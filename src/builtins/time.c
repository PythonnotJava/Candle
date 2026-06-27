// time.c — std.time module (self-contained to avoid TokenType conflict)
// Provides: now() → wall-clock ms (double), sleep(ms) → pause for N ms
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#else
  #include <unistd.h>
  #include <time.h>
#endif

// ── Minimal Value types (mirrors value.h) ──────────────────────────────────────
typedef enum { V_NULL, V_INT, V_DOUBLE, V_BOOL, V_STRING, V_LIST, V_TUPLE,
               V_MAP, V_FN, V_LAMBDA, V_CLASS, V_OBJECT, V_NATIVE } ValueType;

typedef struct Value { ValueType type; union { long long i; double d; int b;
    char *s; void *list; void *map; void *fn; void *cls; void *obj;
    struct Value (*native)(int, struct Value*); } as; } Value;

typedef struct { char **keys; Value *vals; int len, cap; } VMap;

static Value v_double(double x) { Value v; v.type = V_DOUBLE; v.as.d = x; return v; }
static Value v_null(void)        { Value v; v.type = V_NULL; return v; }
static Value v_native(Value (*f)(int, Value*)) { Value v; v.type = V_NATIVE; v.as.native = f; return v; }

static void v_map_set(VMap *m, const char *k, Value v) {
    for (int i = 0; i < m->len; i++)
        if (strcmp(m->keys[i], k) == 0) { m->vals[i] = v; return; }
    if (m->len >= m->cap) {
        m->cap = m->cap ? m->cap * 2 : 4;
        m->keys = realloc(m->keys, sizeof(char*) * m->cap);
        m->vals = realloc(m->vals, sizeof(Value) * m->cap);
    }
    m->keys[m->len] = _strdup(k);
    m->vals[m->len] = v;
    m->len++;
}

static Value v_map_new(void) {
    Value v; v.type = V_MAP;
    VMap *m = calloc(1, sizeof(VMap));
    v.as.map = m;
    return v;
}

// ── Helper: extract double argument ───────────────────────────────────────────
static double arg_d(int argc, Value *argv, int i) {
    if (i >= argc) return 0.0;
    if (argv[i].type == V_DOUBLE) return argv[i].as.d;
    if (argv[i].type == V_INT) return (double)argv[i].as.i;
    return 0.0;
}

// ── now() — wall-clock time in milliseconds (double) ──────────────────────────
static Value time_now(int argc, Value *argv) {
    (void)argc; (void)argv;
#ifdef _WIN32
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER counter;
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return v_double((double)counter.QuadPart * 1000.0 / (double)freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return v_double((double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0);
#endif
}

// ── sleep(ms) — pause for N milliseconds ─────────────────────────────────────
static Value time_sleep(int argc, Value *argv) {
    double ms = arg_d(argc, argv, 0);
    if (ms <= 0) return v_null();
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000.0);
    ts.tv_nsec = (long)((ms - ts.tv_sec * 1000.0) * 1000000.0);
    nanosleep(&ts, NULL);
#endif
    return v_null();
}

// ── Module entry ──────────────────────────────────────────────────────────────
Value build_time(void) {
    Value mod = v_map_new();
    VMap *m = mod.as.map;
    v_map_set(m, "now",   v_native(time_now));
    v_map_set(m, "sleep", v_native(time_sleep));
    return mod;
}
