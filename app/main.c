#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <glib.h>
#include <glib-unix.h>
#include <signal.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <unistd.h>
#include <curl/curl.h>

#include "ACAP.h"
#include "cJSON.h"
#include "influxdb.h"

#define APP_PACKAGE "acap_influxdb"

#define LOG(fmt, args...)      { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_WARN(fmt, args...) { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args); }

static InfluxDB_Config influxdb_config;
static GMainLoop *main_loop = NULL;
static guint collect_timer_id = 0;

static gboolean collect_and_send(gpointer user_data);

/*-----------------------------------------------------
 * Data collectors — system
 *-----------------------------------------------------*/

static double get_memory_usage_percent(void) {
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f) return -1.0;

    long mem_total = 0, mem_available = 0;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        long val;
        if (sscanf(line, "MemTotal: %ld kB", &val) == 1)      mem_total     = val;
        if (sscanf(line, "MemAvailable: %ld kB", &val) == 1)  mem_available = val;
        if (mem_total && mem_available) break;
    }
    fclose(f);
    if (mem_total <= 0) return -1.0;
    return (double)(mem_total - mem_available) / (double)mem_total * 100.0;
}

static double get_temperature(void) {
    int fd = open("/sys/class/thermal/thermal_zone0/temp", O_RDONLY);
    if (fd < 0) return -999.0;
    char buf[32] = {0};
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return -999.0;
    int millideg = 0;
    sscanf(buf, "%d", &millideg);
    return (double)millideg / 1000.0;
}

static double get_storage_usage_percent(void) {
    struct statvfs st;
    const char* paths[] = {
        "/var/spool/storage/SD_DISK",
        "/mnt/SD_DISK",
        "/sdcard",
        NULL
    };
    for (int i = 0; paths[i]; i++) {
        if (statvfs(paths[i], &st) == 0 && st.f_blocks > 0) {
            return (double)(st.f_blocks - st.f_bfree) / (double)st.f_blocks * 100.0;
        }
    }
    return -1.0;
}

/*-----------------------------------------------------
 * Data collectors — VAPIX (write their own points, one per zone/channel)
 *-----------------------------------------------------*/

static void collect_thermometry(InfluxDB_Config* config, const char* serial) {
    const char* body = "{\"apiVersion\":\"1.0\",\"method\":\"getAreaStatus\"}";
    char* resp = ACAP_VAPIX_Post("thermometry.cgi", body);
    if (!resp) return;

    cJSON* json = cJSON_Parse(resp);
    free(resp);
    if (!json) return;

    cJSON* data     = cJSON_GetObjectItem(json, "data");
    cJSON* areaList = data ? cJSON_GetObjectItem(data, "areaList") : NULL;

    if (areaList && cJSON_IsArray(areaList)) {
        int idx = 0;
        cJSON* area;
        cJSON_ArrayForEach(area, areaList) {
            InfluxDB_Point* pt = InfluxDB_Point_Create("thermal_zones");
            if (!pt) { idx++; continue; }

            if (serial) InfluxDB_Point_Add_Tag(pt, "serial", serial);

            /* Tag the zone by name if available, otherwise by numeric id */
            cJSON* name_item = cJSON_GetObjectItem(area, "name");
            cJSON* id_item   = cJSON_GetObjectItem(area, "id");
            char zone_tag[64];
            if (name_item && name_item->valuestring && name_item->valuestring[0])
                snprintf(zone_tag, sizeof(zone_tag), "%s", name_item->valuestring);
            else if (id_item)
                snprintf(zone_tag, sizeof(zone_tag), "%d", (int)id_item->valuedouble);
            else
                snprintf(zone_tag, sizeof(zone_tag), "%d", idx);
            InfluxDB_Point_Add_Tag(pt, "zone", zone_tag);

            /* Support both field naming conventions from different camera/zone types */
            cJSON* f_avg = cJSON_GetObjectItem(area, "averageTemperature");
            if (!f_avg) f_avg = cJSON_GetObjectItem(area, "avg");
            cJSON* f_max = cJSON_GetObjectItem(area, "maxTemperature");
            if (!f_max) f_max = cJSON_GetObjectItem(area, "max");
            cJSON* f_min = cJSON_GetObjectItem(area, "minTemperature");
            if (!f_min) f_min = cJSON_GetObjectItem(area, "min");
            cJSON* f_trig = cJSON_GetObjectItem(area, "triggered");

            if (f_avg)  InfluxDB_Point_Add_Field_Float(pt, "avg_c",     f_avg->valuedouble);
            if (f_max)  InfluxDB_Point_Add_Field_Float(pt, "max_c",     f_max->valuedouble);
            if (f_min)  InfluxDB_Point_Add_Field_Float(pt, "min_c",     f_min->valuedouble);
            if (f_trig) InfluxDB_Point_Add_Field_Bool(pt,  "triggered",
                            cJSON_IsTrue(f_trig) ? 1 : 0);

            if (!InfluxDB_Write(config, pt))
                LOG_WARN("Failed to write thermal_zones point (zone=%s)\n", zone_tag);
            InfluxDB_Point_Free(pt);
            idx++;
        }
    }
    cJSON_Delete(json);
}


static void collect_spot_temperature(InfluxDB_Config* config, const char* serial) {
    const char* body =
        "{\"apiVersion\":\"1.3\",\"method\":\"getSpotTemperature\","
        "\"params\":{\"coordinateSystem\":\"coord_neg1_1\",\"imagesource\":0}}";
    char* resp = ACAP_VAPIX_Post("thermometry.cgi", body);
    if (!resp) return;

    cJSON* json = cJSON_Parse(resp);
    free(resp);
    if (!json) return;

    cJSON* data     = cJSON_GetObjectItem(json, "data");
    cJSON* f_temp = data ? cJSON_GetObjectItem(data, "spotTemperature") : NULL;

    if (f_temp) {
        InfluxDB_Point* pt = InfluxDB_Point_Create("thermal_spot");
        if (pt) {
            if (serial) InfluxDB_Point_Add_Tag(pt, "serial", serial);
            InfluxDB_Point_Add_Field_Float(pt, "temperature_c", f_temp->valuedouble);
            if (!InfluxDB_Write(config, pt))
                LOG_WARN("Failed to write thermal_spot point\n");
            InfluxDB_Point_Free(pt);
        }
    }
    cJSON_Delete(json);
}


/*-----------------------------------------------------
 * Data collector — AXIS D6310 air quality sensor
 *-----------------------------------------------------*/

static const struct { const char* category; const char* field; } AQ_METRICS[] = {
    { "TEMPERATURE", "Temperature"  },
    { "HUMIDITY",    "Humidity"   },
    { "CO2",         "CO2"        },
    { "VOC",         "VOC"      },
    { "NOX",         "NOX"      },
    { "AQI",         "AQI"            },
    { "PM1_0",       "PM1_0"     },
    { "PM2_5",       "PM2_5"     },
    { "PM4_0",       "PM4_0"     },
    { "PM10_0",      "PM10_0"    },
    { NULL, NULL }
};

static void collect_air_quality(InfluxDB_Config* config, const char* serial) {
    char* sensors_resp = ACAP_VAPIX_Get_Path("/config/rest/airqualitymonitor/v1beta/sensors");
    if (!sensors_resp) return;

    cJSON* sensors_root = cJSON_Parse(sensors_resp);
    free(sensors_resp);
    if (!sensors_root) {
        LOG_WARN("AirQuality: failed to parse sensors response\n");
        return;
    }

    /* Response: {"status":"success","data":[...]} */
    cJSON* data_field = cJSON_GetObjectItem(sensors_root, "data");
    cJSON* sensors = cJSON_IsArray(data_field) ? data_field
                   : cJSON_IsArray(sensors_root) ? sensors_root
                   : NULL;
    if (!sensors) {
        LOG_WARN("AirQuality: no sensor array in response\n");
        cJSON_Delete(sensors_root);
        return;
    }

    cJSON* sensor;
    cJSON_ArrayForEach(sensor, sensors) {
        cJSON* id_item   = cJSON_GetObjectItem(sensor, "sensorId");
        cJSON* connected = cJSON_GetObjectItem(sensor, "connected");
        if (!id_item || !id_item->valuestring || !cJSON_IsTrue(connected))
            continue;

        const char* sensor_id = id_item->valuestring;
        LOG("AirQuality: found connected sensor id=%s\n", sensor_id);

        InfluxDB_Point* pt = InfluxDB_Point_Create("air_quality");
        if (!pt) continue;

        if (serial)    InfluxDB_Point_Add_Tag(pt, "serial",    serial);
        InfluxDB_Point_Add_Tag(pt, "sensor_id", sensor_id);

        int has_fields = 0;

        long long now_s   = (long long)time(NULL);
        long long start_s = now_s - 120;

        for (int i = 0; AQ_METRICS[i].category; i++) {
            char path[256];
            snprintf(path, sizeof(path),
                "/config/rest/airqualitymonitor/v1beta/sensors/%s/getHistoryData",
                sensor_id);

            /* Body must be wrapped in "data": {...} and timestamps are Unix seconds */
            char body[256];
            snprintf(body, sizeof(body),
                "{\"data\":{\"category\":\"%s\",\"startTime\":%lld,\"endTime\":%lld}}",
                AQ_METRICS[i].category, start_s, now_s);

            char* resp = ACAP_VAPIX_Post_Path(path, body);
            if (!resp) {
                LOG_WARN("AirQuality: no response for %s\n", AQ_METRICS[i].category);
                continue;
            }

            cJSON* json = cJSON_Parse(resp);
            if (!json) {
                LOG_WARN("AirQuality: invalid JSON for %s: %.120s\n",
                         AQ_METRICS[i].category, resp);
                free(resp);
                continue;
            }
            free(resp);

            cJSON* status = cJSON_GetObjectItem(json, "status");
            if (status && status->valuestring &&
                strcmp(status->valuestring, "success") == 0) {
                cJSON* data         = cJSON_GetObjectItem(json, "data");
                cJSON* measurements = data ? cJSON_GetObjectItem(data, "measurement") : NULL;
                if (measurements && cJSON_IsArray(measurements)) {
                    int count = cJSON_GetArraySize(measurements);
                    if (count > 0) {
                        cJSON* last = cJSON_GetArrayItem(measurements, count - 1);
                        if (last) {
                            InfluxDB_Point_Add_Field_Float(pt, AQ_METRICS[i].field,
                                                           last->valuedouble);
                            has_fields = 1;
                        }
                    }
                }
            } else {
                cJSON* err = cJSON_GetObjectItem(json, "error");
                cJSON* msg = err ? cJSON_GetObjectItem(err, "message") : NULL;
                LOG_WARN("AirQuality: %s error: %s\n", AQ_METRICS[i].category,
                         msg && msg->valuestring ? msg->valuestring : "unknown");
            }
            cJSON_Delete(json);
        }

        if (has_fields) {
            if (!InfluxDB_Write(config, pt))
                LOG_WARN("Failed to write air_quality point (sensor=%s)\n", sensor_id);
        }
        InfluxDB_Point_Free(pt);
    }
    cJSON_Delete(sensors_root);
}


/*-----------------------------------------------------
 * Config
 *-----------------------------------------------------*/

static void apply_influxdb_config(cJSON* influx) {
    if (!influx) return;

    InfluxDB_Config_Free(&influxdb_config);
    memset(&influxdb_config, 0, sizeof(InfluxDB_Config));

    const char* url    = cJSON_GetStringValue(cJSON_GetObjectItem(influx, "url"));
    const char* org    = cJSON_GetStringValue(cJSON_GetObjectItem(influx, "org"));
    const char* bucket = cJSON_GetStringValue(cJSON_GetObjectItem(influx, "bucket"));
    const char* token  = cJSON_GetStringValue(cJSON_GetObjectItem(influx, "token"));

    influxdb_config.url     = strdup(url    ? url    : "");
    influxdb_config.org     = strdup(org    ? org    : "");
    influxdb_config.bucket  = strdup(bucket ? bucket : "");
    influxdb_config.token   = strdup(token  ? token  : "");
    influxdb_config.enabled = cJSON_IsTrue(cJSON_GetObjectItem(influx, "enabled")) ? 1 : 0;

    LOG("InfluxDB config: enabled=%d url=%s org=%s bucket=%s\n",
        influxdb_config.enabled, influxdb_config.url,
        influxdb_config.org, influxdb_config.bucket);
}

static void apply_poll_interval(cJSON* dc) {
    if (!dc) return;
    cJSON* item = cJSON_GetObjectItem(dc, "pollIntervalSeconds");
    if (!item) return;
    int interval = (int)item->valuedouble;
    if (interval < 10) interval = 10;
    if (collect_timer_id) {
        g_source_remove(collect_timer_id);
        collect_timer_id = 0;
    }
    collect_timer_id = g_timeout_add_seconds(interval, collect_and_send, NULL);
    LOG("Poll interval updated: %ds\n", interval);
}

static void Settings_Updated_Callback(const char* service, cJSON* data) {
    if (strcmp(service, "influxdb") == 0)
        apply_influxdb_config(data);
    else if (strcmp(service, "dataCollection") == 0)
        apply_poll_interval(data);
}

/*-----------------------------------------------------
 * Periodic data collection
 *-----------------------------------------------------*/

static gboolean collect_and_send(gpointer user_data) {
    if (!influxdb_config.enabled) {
        LOG("InfluxDB disabled, skipping write\n");
        return G_SOURCE_CONTINUE;
    }

    cJSON* settings = ACAP_Get_Config("settings");
    cJSON* dc       = settings ? cJSON_GetObjectItem(settings, "dataCollection") : NULL;
    cJSON* types    = dc ? cJSON_GetObjectItem(dc, "selectedDataTypes") : NULL;
    if (!cJSON_IsObject(types)) types = NULL;

    const char* serial = ACAP_DEVICE_Prop("serial");

    /* device_metrics — only write if at least one system metric is enabled */
    int any_system = !types
        || cJSON_IsTrue(cJSON_GetObjectItem(types, "cpu"))
        || cJSON_IsTrue(cJSON_GetObjectItem(types, "memory"))
        || cJSON_IsTrue(cJSON_GetObjectItem(types, "network"))
        || cJSON_IsTrue(cJSON_GetObjectItem(types, "temperature"))
        || cJSON_IsTrue(cJSON_GetObjectItem(types, "uptime"))
        || cJSON_IsTrue(cJSON_GetObjectItem(types, "storage"));

    if (any_system) {
        InfluxDB_Point* point = InfluxDB_Point_Create("device_metrics");
        if (!point) return G_SOURCE_CONTINUE;

        if (serial) InfluxDB_Point_Add_Tag(point, "serial", serial);

        if (!types || cJSON_IsTrue(cJSON_GetObjectItem(types, "cpu")))
            InfluxDB_Point_Add_Field_Float(point, "cpu_usage_pct",
                                           ACAP_DEVICE_CPU_Average() * 100.0);

        if (!types || cJSON_IsTrue(cJSON_GetObjectItem(types, "memory"))) {
            double mem = get_memory_usage_percent();
            if (mem >= 0.0)
                InfluxDB_Point_Add_Field_Float(point, "memory_usage_pct", mem);
        }

        if (!types || cJSON_IsTrue(cJSON_GetObjectItem(types, "network")))
            InfluxDB_Point_Add_Field_Float(point, "network_kbps",
                                           ACAP_DEVICE_Network_Average());

        if (!types || cJSON_IsTrue(cJSON_GetObjectItem(types, "temperature"))) {
            double temp = get_temperature();
            if (temp > -999.0)
                InfluxDB_Point_Add_Field_Float(point, "temperature_c", temp);
        }

        if (types && cJSON_IsTrue(cJSON_GetObjectItem(types, "uptime")))
            InfluxDB_Point_Add_Field_Float(point, "uptime_seconds", ACAP_DEVICE_Uptime());

        if (types && cJSON_IsTrue(cJSON_GetObjectItem(types, "storage"))) {
            double storage = get_storage_usage_percent();
            if (storage >= 0.0)
                InfluxDB_Point_Add_Field_Float(point, "storage_usage_pct", storage);
        }

        if (!InfluxDB_Write(&influxdb_config, point))
            LOG_WARN("Failed to write device_metrics\n");

        InfluxDB_Point_Free(point);
    }

    if (types && cJSON_IsTrue(cJSON_GetObjectItem(types, "thermometry")))
        collect_thermometry(&influxdb_config, serial);

    if (types && cJSON_IsTrue(cJSON_GetObjectItem(types, "spot_temperature")))
        collect_spot_temperature(&influxdb_config, serial);

    if (types && cJSON_IsTrue(cJSON_GetObjectItem(types, "air_quality")))
        collect_air_quality(&influxdb_config, serial);
    return G_SOURCE_CONTINUE;
}

/*-----------------------------------------------------
 * Debug endpoint — exposes raw D6310 API responses
 *-----------------------------------------------------*/

static void debug_add_post(cJSON* out, const char* key,
                           const char* path, const char* body) {
    long http_code = 0;
    char* resp = ACAP_VAPIX_Post_Path_Raw(path, body, &http_code);

    char code_key[128];
    snprintf(code_key, sizeof(code_key), "%s_http", key);
    cJSON_AddNumberToObject(out, code_key, (double)http_code);
    cJSON_AddStringToObject(out, key, resp ? resp : "(null — curl error)");
    if (resp) {
        char parsed_key[128];
        snprintf(parsed_key, sizeof(parsed_key), "%s_parsed", key);
        cJSON* p = cJSON_Parse(resp);
        cJSON_AddItemToObject(out, parsed_key, p ? p : cJSON_CreateNull());
        free(resp);
    }
}

static void HTTP_Debug_AirQuality(const ACAP_HTTP_Response response,
                                  const ACAP_HTTP_Request  request) {
    cJSON* out = cJSON_CreateObject();

    /* 1. List sensors */
    char* sensors_resp = ACAP_VAPIX_Get_Path(
        "/config/rest/airqualitymonitor/v1beta/sensors");
    cJSON_AddStringToObject(out, "sensors_raw",
        sensors_resp ? sensors_resp : "NULL");
    if (sensors_resp) {
        cJSON* p = cJSON_Parse(sensors_resp);
        cJSON_AddItemToObject(out, "sensors_parsed", p ? p : cJSON_CreateNull());
        free(sensors_resp);
    }

    /* 2. GET on sensor/0 directly */
    char* s0 = ACAP_VAPIX_Get_Path(
        "/config/rest/airqualitymonitor/v1beta/sensors/0");
    cJSON_AddStringToObject(out, "sensor0_get", s0 ? s0 : "NULL");
    if (s0) { cJSON_AddItemToObject(out, "sensor0_get_parsed",
        cJSON_Parse(s0) ?: cJSON_CreateNull()); free(s0); }

    /* getHistoryData: body = {"data":{"category":...,"startTime":seconds,"endTime":seconds}} */
    long long now_s   = (long long)time(NULL);
    long long start_s = now_s - 120;
    const char* hist_path =
        "/config/rest/airqualitymonitor/v1beta/sensors/0/getHistoryData";

    char co2_body[256];
    snprintf(co2_body, sizeof(co2_body),
        "{\"data\":{\"category\":\"CO2\",\"startTime\":%lld,\"endTime\":%lld}}",
        start_s, now_s);
    debug_add_post(out, "historyData_CO2", hist_path, co2_body);

    char temp_body[256];
    snprintf(temp_body, sizeof(temp_body),
        "{\"data\":{\"category\":\"TEMPERATURE\",\"startTime\":%lld,\"endTime\":%lld}}",
        start_s, now_s);
    debug_add_post(out, "historyData_TEMPERATURE", hist_path, temp_body);

    ACAP_HTTP_Respond_JSON(response, out);
    cJSON_Delete(out);
}

/*-----------------------------------------------------
 * Test connection endpoint
 *-----------------------------------------------------*/

static void HTTP_Test_Connection(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
    if (!influxdb_config.url || strlen(influxdb_config.url) == 0) {
        ACAP_HTTP_Respond_Error(response, 400, "No URL configured — save settings first");
        return;
    }

    char ping_url[512];
    snprintf(ping_url, sizeof(ping_url), "%s/ping", influxdb_config.url);

    CURL* curl = curl_easy_init();
    if (!curl) {
        ACAP_HTTP_Respond_Error(response, 500, "curl init failed");
        return;
    }

    struct curl_slist* headers = NULL;
    char auth_header[512] = "";
    if (influxdb_config.token && strlen(influxdb_config.token) > 0) {
        snprintf(auth_header, sizeof(auth_header), "Authorization: Token %s", influxdb_config.token);
        headers = curl_slist_append(headers, auth_header);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    curl_easy_setopt(curl, CURLOPT_URL, ping_url);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Connection error: %s", curl_easy_strerror(res));
        if (headers) curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        ACAP_HTTP_Respond_Error(response, 502, msg);
        return;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (http_code == 204 || http_code == 200) {
        ACAP_HTTP_Respond_Text(response, "OK");
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "HTTP %ld", http_code);
        ACAP_HTTP_Respond_Error(response, 502, msg);
    }
}

/*-----------------------------------------------------
 * Signal handler & main
 *-----------------------------------------------------*/

static gboolean signal_handler(gpointer user_data) {
    LOG("Received SIGTERM, shutting down\n");
    if (main_loop && g_main_loop_is_running(main_loop))
        g_main_loop_quit(main_loop);
    return G_SOURCE_REMOVE;
}

int main(void) {
    openlog(APP_PACKAGE, LOG_PID | LOG_CONS, LOG_USER);
    LOG("------ Starting %s ------\n", APP_PACKAGE);

    memset(&influxdb_config, 0, sizeof(InfluxDB_Config));

    cJSON* settings = ACAP_Init(APP_PACKAGE, Settings_Updated_Callback);
    if (!settings) {
        LOG_WARN("ACAP_Init failed\n");
        return EXIT_FAILURE;
    }

    cJSON* influx = cJSON_GetObjectItem(settings, "influxdb");
    if (influx && !influxdb_config.url)
        apply_influxdb_config(influx);

    ACAP_STATUS_SetString("app", "status", "Running");

    cJSON* dc = cJSON_GetObjectItem(settings, "dataCollection");
    int poll_interval = 30;
    if (dc) {
        cJSON* item = cJSON_GetObjectItem(dc, "pollIntervalSeconds");
        if (item) poll_interval = (int)item->valuedouble;
    }
    if (poll_interval < 10) poll_interval = 10;

    ACAP_HTTP_Node("test", HTTP_Test_Connection);
    ACAP_HTTP_Node("debug", HTTP_Debug_AirQuality);
    if (!collect_timer_id)
        collect_timer_id = g_timeout_add_seconds(poll_interval, collect_and_send, NULL);

    main_loop = g_main_loop_new(NULL, FALSE);
    GSource* sig = g_unix_signal_source_new(SIGTERM);
    if (sig) {
        g_source_set_callback(sig, signal_handler, NULL, NULL);
        g_source_attach(sig, NULL);
    }

    LOG("Entering main loop (poll interval: %ds)\n", poll_interval);
    g_main_loop_run(main_loop);

    LOG("Cleaning up %s\n", APP_PACKAGE);
    InfluxDB_Config_Free(&influxdb_config);
    ACAP_Cleanup();
    closelog();
    return EXIT_SUCCESS;
}
