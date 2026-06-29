#ifndef CANDLE_VALUE_H
#define CANDLE_VALUE_H

#include "candle_runtime.h"

/* Forward declarations — avoid pulling token.h into standalone builtins */
typedef struct AstNode AstNode;
typedef struct Env Env;

typedef enum {
    V_NULL,
    V_INT,
    V_DOUBLE,
    V_BOOL,
    V_STRING,
    V_LIST,
    V_TUPLE,
    V_MAP,
    V_FN,
    V_LAMBDA,
    V_CLASS,
    V_OBJECT,
    V_NATIVE
} ValueType;

typedef struct Value Value;

typedef Value (*NativeFn)(int argc, Value *argv);

typedef struct {
    Value *items;
    int len, cap;
} VList;

typedef struct {
    char **keys;
    Value *vals;
    int len, cap;
} VMap;

typedef struct {
    AstNode *decl;
    Env *closure;
} VFunc;

typedef struct {
    AstNode *class_node;
    VMap *fields;
    void *extra;   /* extension data (File*, etc.) */
} VObject;

struct Value {
    ValueType type;
    union {
        int64_t i;
        double d;
        int b;
        char *s;
        VList *list;
        VMap *map;
        VFunc *fn;
        AstNode *cls;
        VObject *obj;
        NativeFn native;
    } as;
};

Value v_null(void);
Value v_int(int64_t x);
Value v_double(double x);
Value v_bool(int x);
Value v_string(const char *s);
Value v_list(void);
Value v_tuple(void);
Value v_map(void);
Value v_native(NativeFn fn);

const char *v_typename(ValueType t);
int v_truthy(Value v);
int v_equals(Value a, Value b);
char *v_to_string(Value v);

void v_list_push(VList *l, Value v);
Value v_list_get(VList *l, int i);
void v_map_set(VMap *m, const char *k, Value v);
Value v_map_get(VMap *m, const char *k);

#endif
