// http.h — std.http 模块声明 (轻量，不含 token.h)
#ifndef CANDLE_HTTP_H
#define CANDLE_HTTP_H

// Forward-declare Value (exact layout must match value.h)
typedef enum { V_NULL, V_INT, V_DOUBLE, V_BOOL, V_STRING, V_LIST, V_TUPLE,
               V_MAP, V_FN, V_LAMBDA, V_CLASS, V_OBJECT, V_NATIVE } ValueType;
typedef struct Value { ValueType type; union { long long i; double d; int b;
    char *s; void *list; void *map; void *fn; void *cls; void *obj;
    struct Value (*native)(int, struct Value*); } as; } Value;

Value build_http(void);

#endif
