#ifndef INFLUXDB_H
#define INFLUXDB_H

#include "cJSON.h"

typedef struct {
    char* url;
    char* org;
    char* bucket;
    char* token;
    int enabled;
} InfluxDB_Config;

typedef struct {
    char* measurement;
    cJSON* tags;
    cJSON* fields;
    long long timestamp;
} InfluxDB_Point;

InfluxDB_Point* InfluxDB_Point_Create(const char* measurement);
void InfluxDB_Point_Add_Tag(InfluxDB_Point* point, const char* key, const char* value);
void InfluxDB_Point_Add_Field_Int(InfluxDB_Point* point, const char* key, int value);
void InfluxDB_Point_Add_Field_Float(InfluxDB_Point* point, const char* key, double value);
void InfluxDB_Point_Add_Field_Bool(InfluxDB_Point* point, const char* key, int value);
void InfluxDB_Point_Add_Field_String(InfluxDB_Point* point, const char* key, const char* value);
int InfluxDB_Write(InfluxDB_Config* config, InfluxDB_Point* point);
void InfluxDB_Point_Free(InfluxDB_Point* point);
void InfluxDB_Config_Free(InfluxDB_Config* config);

#endif
