// process.c �?std.process module
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { V_NULL, V_INT, V_DOUBLE, V_BOOL, V_STRING, V_LIST, V_TUPLE,
               V_MAP, V_FN, V_LAMBDA, V_CLASS, V_OBJECT, V_NATIVE } ValueType;
typedef struct Value { ValueType type; union { long long i; double d; int b;
    char *s; void *list; void *map; void *fn; void *cls; void *obj;
    struct Value (*native)(int, struct Value*); } as; } Value;
typedef struct { char **keys; Value *vals; int len, cap; } VMap;

static Value v_int(long long x)   { Value v; v.type = V_INT; v.as.i = x; return v; }
static Value v_string(const char *s) { Value v; v.type = V_STRING; v.as.s = (char*)s; return v; }
static Value v_null(void)         { Value v; v.type = V_NULL; return v; }
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

// ── Process ──────────────────────────────────────────────────────────────────

// exec(command) -> exit code
static Value proc_exec(int argc, Value *argv) {
    if (argc < 1) return v_int(-1);
#ifdef _WIN32
    return v_int(system(argv[0].as.s));
#else
    // On POSIX, system() returns status; normalize to exit code
    int status = system(argv[0].as.s);
    if (WIFEXITED(status)) return v_int(WEXITSTATUS(status));
    return v_int(status);
#endif
}

// execCapture(command) -> stdout string
static Value proc_execCapture(int argc, Value *argv) {
    if (argc < 1) return v_string("");
#ifdef _WIN32
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "%s 2>nul", argv[0].as.s);
#else
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "%s 2>/dev/null", argv[0].as.s);
#endif
    FILE *f = _popen(cmd, "r");
    if (!f) return v_string("");
    char *buf = (char*)malloc(8192);
    size_t total = 0, cap = 8192;
    while (!feof(f)) {
        if (total + 1024 >= cap) { cap *= 2; buf = (char*)realloc(buf, cap); }
        total += fread(buf + total, 1, cap - total - 1, f);
    }
    buf[total] = 0;
    _pclose(f);
    Value v = v_string(buf);
    return v;
}

Value build_process(void) {
    Value mod = v_map();
    VMap *m = (VMap*)mod.as.map;
    v_map_set(m, "exec",        v_native(proc_exec));
    v_map_set(m, "execCapture", v_native(proc_execCapture));
    return mod;
}
