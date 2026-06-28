// random.c — std.random module
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum { V_NULL, V_INT, V_DOUBLE, V_BOOL, V_STRING, V_LIST, V_TUPLE,
               V_MAP, V_FN, V_LAMBDA, V_CLASS, V_OBJECT, V_NATIVE } ValueType;
typedef struct Value { ValueType type; union { long long i; double d; int b;
    char *s; void *list; void *map; void *fn; void *cls; void *obj;
    struct Value (*native)(int, struct Value*); } as; } Value;
typedef struct { char **keys; Value *vals; int len, cap; } VMap;

static Value v_int(long long x)   { Value v; v.type = V_INT; v.as.i = x; return v; }
static Value v_double(double x)   { Value v; v.type = V_DOUBLE; v.as.d = x; return v; }
static Value v_null(void)         { Value v; v.type = V_NULL; return v; }
static Value v_map(void) {
    Value v; v.type = V_MAP;
    VMap *m = (VMap*)calloc(1, sizeof(VMap)); m->cap = 16;
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
static Value v_string(const char *s) { Value v; v.type = V_STRING; v.as.s = (char*)s; return v; }

// ── Random ───────────────────────────────────────────────────────────────────
static int g_seeded = 0;

static void ensure_seed(void) {
    if (!g_seeded) { srand((unsigned)time(NULL)); g_seeded = 1; }
}

static Value rnd_seed(int argc, Value *argv) {
    if (argc >= 1) srand((unsigned)argv[0].as.i);
    else srand((unsigned)time(NULL));
    g_seeded = 1;
    return v_null();
}

// randomInt(lo, hi) — inclusive
static Value rnd_int(int argc, Value *argv) {
    ensure_seed();
    long long lo = argc >= 1 ? argv[0].as.i : 0;
    long long hi = argc >= 2 ? argv[1].as.i : 2147483647;
    if (lo > hi) { long long t = lo; lo = hi; hi = t; }
    long long range = hi - lo + 1;
    return v_int(lo + (rand() % range));
}

// randomDouble() — [0, 1)
static Value rnd_double(int argc, Value *argv) {
    ensure_seed();
    return v_double((double)rand() / (double)RAND_MAX);
}

// shuffle(list) — in-place
static Value rnd_shuffle(int argc, Value *argv) {
    ensure_seed();
    // shuffle is complex with the minimal Value type; implement in Candle instead
    return v_null();
}

Value build_random(void) {
    Value mod = v_map();
    VMap *m = (VMap*)mod.as.map;
    v_map_set(m, "seed",        v_native(rnd_seed));
    v_map_set(m, "randomInt",    v_native(rnd_int));
    v_map_set(m, "randomDouble", v_native(rnd_double));
    v_map_set(m, "shuffle",      v_native(rnd_shuffle));
    return mod;
}
