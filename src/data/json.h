#ifndef CANDLE_DATA_JSON_H
#define CANDLE_DATA_JSON_H

#include "../candle_runtime.h"
#include "../vendor/cjson/cJSON.h"

typedef cJSON JsonValue;

static inline JsonValue *json_parse(candle_string text) {
    return cJSON_Parse(text ? text : "");
}

static inline candle_string json_stringify(JsonValue *v) {
    if (!v) return "null";
    char *s = cJSON_PrintUnformatted(v);
    if (!s) return "null";
    size_t n = strlen(s);
    char *buf = GC_MALLOC(n + 1);
    memcpy(buf, s, n + 1);
    cJSON_free(s);
    return buf;
}

static inline candle_string json_get_string(JsonValue *v, candle_string key) {
    if (!v) return NULL;
    cJSON *item = cJSON_GetObjectItemCaseSensitive(v, key);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static inline candle_int json_get_int(JsonValue *v, candle_string key) {
    if (!v) return 0;
    cJSON *item = cJSON_GetObjectItemCaseSensitive(v, key);
    return cJSON_IsNumber(item) ? (candle_int)item->valuedouble : 0;
}

static inline candle_double json_get_double(JsonValue *v, candle_string key) {
    if (!v) return 0.0;
    cJSON *item = cJSON_GetObjectItemCaseSensitive(v, key);
    return cJSON_IsNumber(item) ? item->valuedouble : 0.0;
}

static inline candle_bool json_get_bool(JsonValue *v, candle_string key) {
    if (!v) return 0;
    cJSON *item = cJSON_GetObjectItemCaseSensitive(v, key);
    return cJSON_IsBool(item) ? cJSON_IsTrue(item) : 0;
}

static inline JsonValue *json_get(JsonValue *v, candle_string key) {
    return v ? cJSON_GetObjectItemCaseSensitive(v, key) : NULL;
}

static inline JsonValue *json_at(JsonValue *v, candle_int i) {
    return v ? cJSON_GetArrayItem(v, (int)i) : NULL;
}

static inline candle_int json_size(JsonValue *v) {
    return v ? cJSON_GetArraySize(v) : 0;
}

static inline void json_free(JsonValue *v) { if (v) cJSON_Delete(v); }

static inline JsonValue *json_object(void)  { return cJSON_CreateObject(); }
static inline JsonValue *json_array(void)   { return cJSON_CreateArray(); }
static inline void json_set_string(JsonValue *v, candle_string k, candle_string s) { cJSON_AddStringToObject(v, k, s); }
static inline void json_set_int(JsonValue *v, candle_string k, candle_int n) { cJSON_AddNumberToObject(v, k, (double)n); }
static inline void json_set_double(JsonValue *v, candle_string k, candle_double n) { cJSON_AddNumberToObject(v, k, n); }
static inline void json_set_bool(JsonValue *v, candle_string k, candle_bool b) { cJSON_AddBoolToObject(v, k, b); }

#endif
