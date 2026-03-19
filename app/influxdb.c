#include "influxdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <math.h>
#include <curl/curl.h>

#define LOG(fmt, args...)      { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_WARN(fmt, args...) { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args); }
#define MAX_LINE_PROTOCOL 8192

/* Escape commas, equals signs, and spaces in tag keys/values and field keys.
 * Returns number of bytes written (not including NUL). */
static int lp_escape(char* dst, size_t dst_size, const char* src) {
    int n = 0;
    for (; *src && (size_t)n < dst_size - 1; src++) {
        if (*src == ',' || *src == '=' || *src == ' ' || *src == '\\') {
            if ((size_t)n + 2 >= dst_size) break;
            dst[n++] = '\\';
        }
        dst[n++] = *src;
    }
    dst[n] = '\0';
    return n;
}

InfluxDB_Point* InfluxDB_Point_Create(const char* measurement) {
    InfluxDB_Point* point = malloc(sizeof(InfluxDB_Point));
    if (!point) return NULL;

    point->measurement = strdup(measurement);
    point->tags        = cJSON_CreateObject();
    point->fields      = cJSON_CreateObject();
    point->timestamp   = (long long)time(NULL) * 1000000000LL;

    return point;
}

void InfluxDB_Point_Add_Tag(InfluxDB_Point* point, const char* key, const char* value) {
    if (!point || !point->tags) return;
    cJSON_AddStringToObject(point->tags, key, value);
}

void InfluxDB_Point_Add_Field_Int(InfluxDB_Point* point, const char* key, int value) {
    if (!point || !point->fields) return;
    cJSON_AddNumberToObject(point->fields, key, (double)value);
}

void InfluxDB_Point_Add_Field_Float(InfluxDB_Point* point, const char* key, double value) {
    if (!point || !point->fields) return;
    cJSON_AddNumberToObject(point->fields, key, value);
}

void InfluxDB_Point_Add_Field_Bool(InfluxDB_Point* point, const char* key, int value) {
    if (!point || !point->fields) return;
    cJSON_AddBoolToObject(point->fields, key, value ? 1 : 0);
}

void InfluxDB_Point_Add_Field_String(InfluxDB_Point* point, const char* key, const char* value) {
    if (!point || !point->fields) return;
    cJSON_AddStringToObject(point->fields, key, value);
}

static char* point_to_line_protocol(InfluxDB_Point* point) {
    if (!point->fields || !point->fields->child) {
        LOG_WARN("InfluxDB: point '%s' has no fields, skipping\n", point->measurement);
        return NULL;
    }

    char* line = malloc(MAX_LINE_PROTOCOL);
    if (!line) return NULL;

    char escaped[512];
    int offset = 0;

    /* Measurement name — escape commas and spaces */
    lp_escape(escaped, sizeof(escaped), point->measurement);
    offset += sprintf(line + offset, "%s", escaped);

    /* Tags — keys and values both need escaping */
    if (point->tags && point->tags->child) {
        cJSON* tag = point->tags->child;
        while (tag) {
            char ek[256], ev[256];
            lp_escape(ek, sizeof(ek), tag->string);
            lp_escape(ev, sizeof(ev), tag->valuestring ? tag->valuestring : "");
            offset += sprintf(line + offset, ",%s=%s", ek, ev);
            tag = tag->next;
        }
    }

    offset += sprintf(line + offset, " ");

    /* Fields */
    cJSON* field = point->fields->child;
    int first = 1;
    while (field) {
        char fk[256];
        lp_escape(fk, sizeof(fk), field->string);

        if (field->type == cJSON_Number) {
            double v = field->valuedouble;
            /* Skip NaN / Inf — InfluxDB rejects them */
            if (isnan(v) || isinf(v)) { field = field->next; continue; }
            if (!first) offset += sprintf(line + offset, ",");
            /* Always write as float (trailing decimal) to prevent type conflicts */
            offset += sprintf(line + offset, "%s=%.6f", fk, v);
            first = 0;
        } else if (field->type == cJSON_True || field->type == cJSON_False) {
            if (!first) offset += sprintf(line + offset, ",");
            offset += sprintf(line + offset, "%s=%s", fk,
                              field->type == cJSON_True ? "true" : "false");
            first = 0;
        } else if (field->type == cJSON_String) {
            if (!first) offset += sprintf(line + offset, ",");
            offset += sprintf(line + offset, "%s=\"%s\"", fk, field->valuestring);
            first = 0;
        }
        field = field->next;
    }

    /* If all fields were skipped (e.g. all NaN), discard */
    if (first) {
        LOG_WARN("InfluxDB: point '%s' had only invalid field values\n", point->measurement);
        free(line);
        return NULL;
    }

    offset += sprintf(line + offset, " %lld", point->timestamp);
    return line;
}

int InfluxDB_Write(InfluxDB_Config* config, InfluxDB_Point* point) {
    if (!config || !config->enabled || !point) return 0;
    if (!config->url || !config->org || !config->bucket || !config->token) {
        LOG_WARN("InfluxDB: incomplete config\n");
        return 0;
    }

    char* line = point_to_line_protocol(point);
    if (!line) return 0;

    char write_url[512];
    snprintf(write_url, sizeof(write_url),
             "%s/api/v2/write?org=%s&bucket=%s&precision=ns",
             config->url, config->org, config->bucket);

    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Token %s", config->token);

    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_WARN("InfluxDB: curl_easy_init failed\n");
        free(line);
        return 0;
    }

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: text/plain; charset=utf-8");

    curl_easy_setopt(curl, CURLOPT_URL, write_url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, line);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);

    int success = 0;
    if (res != CURLE_OK) {
        LOG_WARN("InfluxDB: write failed: %s\n", curl_easy_strerror(res));
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code == 204) {
            success = 1;
            LOG("InfluxDB: write OK (%s)\n", point->measurement);
        } else {
            LOG_WARN("InfluxDB: write returned HTTP %ld\n", http_code);
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(line);
    return success;
}

void InfluxDB_Point_Free(InfluxDB_Point* point) {
    if (!point) return;
    if (point->measurement) free(point->measurement);
    if (point->tags)        cJSON_Delete(point->tags);
    if (point->fields)      cJSON_Delete(point->fields);
    free(point);
}

void InfluxDB_Config_Free(InfluxDB_Config* config) {
    if (!config) return;
    if (config->url)    { free(config->url);    config->url    = NULL; }
    if (config->org)    { free(config->org);    config->org    = NULL; }
    if (config->bucket) { free(config->bucket); config->bucket = NULL; }
    if (config->token)  { free(config->token);  config->token  = NULL; }
}
