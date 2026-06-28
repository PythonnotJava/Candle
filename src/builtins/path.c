// path.c — std.path module
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { V_NULL, V_INT, V_DOUBLE, V_BOOL, V_STRING, V_LIST, V_TUPLE,
               V_MAP, V_FN, V_LAMBDA, V_CLASS, V_OBJECT, V_NATIVE } ValueType;
typedef struct Value { ValueType type; union { long long i; double d; int b;
    char *s; void *list; void *map; void *fn; void *cls; void *obj;
    struct Value (*native)(int, struct Value*); } as; } Value;
typedef struct { char **keys; Value *vals; int len, cap; } VMap;

static Value v_string(const char *s) { Value v; v.type = V_STRING; v.as.s = (char*)s; return v; }
static Value v_bool(int x) { Value v; v.type = V_BOOL; v.as.b = x; return v; }
static Value v_null(void) { Value v; v.type = V_NULL; return v; }
static Value v_map(void) {
    Value v; v.type = V_MAP;
    VMap *m = (VMap*)calloc(1, sizeof(VMap)); m->cap = 8;
    m->keys = (char**)calloc(m->cap, sizeof(char*));
    m->vals = (Value*)calloc(m->cap, sizeof(Value));
    v.as.map = m; return v;
}
static void v_map_set(VMap *m, const char *k, Value v) {
    for (int i = 0; i < m->len; i++) if (strcmp(m->keys[i], k) == 0) { m->vals[i] = v; return; }
    if (m->len >= m->cap) { m->cap *= 2; m->keys = (char**)realloc(m->keys, m->cap*sizeof(char*)); m->vals = (Value*)realloc(m->vals, m->cap*sizeof(Value)); }
    m->keys[m->len] = strdup(k); m->vals[m->len] = v; m->len++;
}
static Value v_native(Value(*fn)(int, Value*)) { Value v; v.type = V_NATIVE; v.as.native = fn; return v; }

// ── Path utils ───────────────────────────────────────────────────────────────

static Value path_basename(int argc, Value *argv) {
    if (argc < 1) return v_string("");
    const char *p = argv[0].as.s;
    const char *last = p;
    for (const char *c = p; *c; c++) if (*c == '/' || *c == '\\') last = c + 1;
    return v_string(strdup(last));
}

static Value path_dirname(int argc, Value *argv) {
    if (argc < 1) return v_string("");
    const char *p = argv[0].as.s;
    const char *last = p;
    for (const char *c = p; *c; c++) if (*c == '/' || *c == '\\') last = c;
    if (last == p) return v_string("");
    char *s = strdup(p);
    s[last - p] = 0;
    return v_string(s);
}

static Value path_extension(int argc, Value *argv) {
    if (argc < 1) return v_string("");
    const char *p = argv[0].as.s;
    const char *dot = NULL;
    for (const char *c = p; *c; c++) if (*c == '.') dot = c;
    if (!dot || dot == p) return v_string("");
    return v_string(strdup(dot + 1));
}

static Value path_join(int argc, Value *argv) {
    if (argc < 2) return argc >= 1 ? v_string(strdup(argv[0].as.s)) : v_string("");
    size_t total = 0;
    for (int i = 0; i < argc; i++) total += strlen(argv[i].as.s) + 1;
    char *buf = (char*)malloc(total + 1);
    buf[0] = 0;
    for (int i = 0; i < argc; i++) {
        if (i > 0) strcat(buf, "/");
        strcat(buf, argv[i].as.s);
    }
    return v_string(buf);
}

static Value path_isAbsolute(int argc, Value *argv) {
    if (argc < 1) return v_bool(0);
    const char *p = argv[0].as.s;
    return v_bool(p[0] == '/' || p[0] == '\\' || (p[0] && p[1] == ':'));
}

Value build_path(void) {
    Value mod = v_map();
    VMap *m = (VMap*)mod.as.map;
    v_map_set(m, "basename",    v_native(path_basename));
    v_map_set(m, "dirname",     v_native(path_dirname));
    v_map_set(m, "extension",   v_native(path_extension));
    v_map_set(m, "join",        v_native(path_join));
    v_map_set(m, "isAbsolute",  v_native(path_isAbsolute));
    return mod;
}
