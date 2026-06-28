#ifndef CANDLE_RUNTIME_H
#define CANDLE_RUNTIME_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <setjmp.h>

// ── GC abstraction layer ─────────────────────────────────────────────────────
// When CANDLE_USE_BOEHM_GC is defined (interpreter builds), all Value-runtime
// allocations go through Boehm GC.  Transpiled .candle→C programs leave it
// undefined and keep plain malloc/calloc/strdup/realloc/free.

#ifdef CANDLE_USE_BOEHM_GC
  #include <gc.h>
  // GC_INIT is already defined by <gc.h> above
  #define GC_MALLOC(n)         GC_malloc(n)
  #define GC_MALLOC_ATOMIC(n)  GC_malloc_atomic(n)
  #define GC_REALLOC(p, n)     GC_realloc(p, n)
  #define GC_STRDUP(s)         GC_strdup(s)
  // GC_FREE is already defined by <gc.h> above

  // Boehm GC has no calloc; wrap GC_malloc + memset.
  static inline void *gc_calloc(size_t nmemb, size_t size) {
      size_t total = nmemb * size;
      void *p = GC_malloc(total);
      if (p) memset(p, 0, total);
      return p;
  }
  #define GC_CALLOC(n, s)  gc_calloc((n), (s))

#else
  #define GC_INIT()            ((void)0)
  #define GC_MALLOC(n)         malloc(n)
  #define GC_MALLOC_ATOMIC(n)  malloc(n)
  #define GC_CALLOC(n, s)      calloc((n), (s))
  #define GC_STRDUP(s)         strdup(s)
  #define GC_REALLOC(p, n)     realloc((p), (n))
  #define GC_FREE(p)           free(p)
#endif

typedef int64_t     candle_int;
typedef double      candle_double;
typedef int         candle_bool;
typedef const char *candle_string;

static inline candle_string candle_str(const char *s) { return s; }

static inline candle_string candle_str_concat(candle_string a, candle_string b) {
    if (!a) a = "";
    if (!b) b = "";
    size_t la = strlen(a), lb = strlen(b);
    char *buf = GC_MALLOC(la + lb + 1);
    memcpy(buf, a, la);
    memcpy(buf + la, b, lb);
    buf[la + lb] = 0;
    return buf;
}

static inline candle_bool candle_str_eq(candle_string a, candle_string b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    return strcmp(a, b) == 0;
}

typedef struct { candle_int *data; int len; int cap; } CandleList;
typedef struct { candle_string *keys; candle_int *vals; int len; int cap; } CandleMap;

static inline CandleList *candle_list_new(int n, ...) {
    CandleList *l = GC_MALLOC(sizeof(CandleList));
    l->cap = n < 8 ? 8 : n;
    l->data = GC_MALLOC(sizeof(candle_int) * l->cap);
    l->len = n;
    va_list ap; va_start(ap, n);
    for (int i = 0; i < n; i++) l->data[i] = va_arg(ap, candle_int);
    va_end(ap);
    return l;
}

static inline candle_int candle_index(CandleList *l, candle_int i) {
    return l->data[i];
}

static inline CandleMap *candle_map_new(int n, ...) {
    CandleMap *m = GC_MALLOC(sizeof(CandleMap));
    m->cap = n < 4 ? 4 : n;
    m->keys = GC_MALLOC(sizeof(candle_string) * m->cap);
    m->vals = GC_MALLOC(sizeof(candle_int) * m->cap);
    m->len = n;
    va_list ap; va_start(ap, n);
    for (int i = 0; i < n; i++) {
        m->keys[i] = va_arg(ap, candle_string);
        m->vals[i] = va_arg(ap, candle_int);
    }
    va_end(ap);
    return m;
}

#define candle_list_foreach(var, list) \
    for (candle_int _i_##var = 0, _len_##var = (list)->len; _i_##var < _len_##var; _i_##var++) \
        for (candle_int var = (list)->data[_i_##var], _once_##var = 1; _once_##var; _once_##var = 0)

typedef candle_int (*candle_fn_int1)(candle_int);
typedef candle_int (*candle_fn_int2)(candle_int, candle_int);
typedef candle_bool (*candle_fn_pred)(candle_int);
typedef void (*candle_fn_void1)(candle_int);

static inline CandleList *candle_list_map(CandleList *l, candle_fn_int1 fn) {
    CandleList *r = GC_MALLOC(sizeof(CandleList));
    r->cap = l->len > 0 ? l->len : 1;
    r->len = l->len;
    r->data = GC_MALLOC(sizeof(candle_int) * r->cap);
    for (int i = 0; i < l->len; i++) r->data[i] = fn(l->data[i]);
    return r;
}
static inline CandleList *candle_list_filter(CandleList *l, candle_fn_pred fn) {
    CandleList *r = GC_MALLOC(sizeof(CandleList));
    r->cap = l->len > 0 ? l->len : 1;
    r->data = GC_MALLOC(sizeof(candle_int) * r->cap);
    r->len = 0;
    for (int i = 0; i < l->len; i++) if (fn(l->data[i])) r->data[r->len++] = l->data[i];
    return r;
}
static inline candle_int candle_list_reduce(CandleList *l, candle_fn_int2 fn, candle_int init) {
    candle_int acc = init;
    for (int i = 0; i < l->len; i++) acc = fn(acc, l->data[i]);
    return acc;
}
// 2-arg legacy overload
static inline candle_int candle_list_reduce2(CandleList *l, candle_fn_int2 fn) {
    if (l->len == 0) return 0;
    candle_int acc = l->data[0];
    for (int i = 1; i < l->len; i++) acc = fn(acc, l->data[i]);
    return acc;
}
static inline void candle_list_forEach(CandleList *l, candle_fn_void1 fn) {
    for (int i = 0; i < l->len; i++) fn(l->data[i]);
}
static inline candle_bool candle_list_any(CandleList *l, candle_fn_pred fn) {
    for (int i = 0; i < l->len; i++) if (fn(l->data[i])) return 1;
    return 0;
}
static inline candle_bool candle_list_every(CandleList *l, candle_fn_pred fn) {
    for (int i = 0; i < l->len; i++) if (!fn(l->data[i])) return 0;
    return 1;
}

#define candle_str_foreach(var, str) \
    for (const char *_p_##var = (str); *_p_##var; _p_##var++) \
        for (char var = *_p_##var, *_once_##var = (char*)_p_##var; _once_##var; _once_##var = NULL)

typedef struct CandleExc {
    candle_string type;
    candle_string message;
    void *value;
} CandleExc;

typedef struct CandleJmp {
    jmp_buf env;
    struct CandleJmp *prev;
    CandleExc exc;
} CandleJmp;

extern CandleJmp *candle_exc_top;
static CandleJmp *candle_exc_top_dummy_init = NULL;
#ifdef CANDLE_RUNTIME_MAIN
CandleJmp *candle_exc_top = NULL;
#endif

#define candle_try \
    do { CandleJmp _cj; _cj.prev = candle_exc_top; candle_exc_top = &_cj; \
         if (setjmp(_cj.env) == 0)

#define candle_catch_end \
        candle_exc_top = _cj.prev; \
    } while (0)

static inline void candle_throw_str(candle_string type, candle_string msg) {
    if (!candle_exc_top) {
        fprintf(stderr, "uncaught %s: %s\n", type ? type : "Exception", msg ? msg : "");
        exit(1);
    }
    candle_exc_top->exc.type = type;
    candle_exc_top->exc.message = msg;
    longjmp(candle_exc_top->env, 1);
}

#define candle_assert(expr) do { if (!(expr)) candle_throw_str("AssertionError", "assertion failed"); } while(0)
#define candle_throw(v)     candle_throw_str("Exception", (v))

static inline void print_char(char v)        { printf("%c\n", v); }
static inline void print_int(candle_int v)    { printf("%lld\n", (long long)v); }
static inline void print_double(candle_double v) { printf("%g\n", v); }
static inline void print_bool(candle_bool v)  { printf("%s\n", v ? "true" : "false"); }
static inline void print_str(candle_string s) { printf("%s\n", s ? s : "null"); }

static inline candle_string candle_int_to_str(candle_int v) {
    char *b = GC_MALLOC(32); snprintf(b, 32, "%lld", (long long)v); return b;
}
static inline candle_string candle_double_to_str(candle_double v) {
    char *b = GC_MALLOC(32); snprintf(b, 32, "%g", v); return b;
}
static inline candle_string candle_bool_to_str(candle_bool v) { return v ? "true" : "false"; }
static inline candle_string candle_string_to_str(candle_string s) { return s ? s : "null"; }
static inline candle_string candle_char_to_str(char c) {
    char *b = GC_MALLOC(2); b[0] = c; b[1] = 0; return b;
}

#define candle_to_str(x) _Generic((x), \
    char:          candle_char_to_str,   \
    candle_int:    candle_int_to_str,    \
    candle_double: candle_double_to_str, \
    candle_bool:   candle_bool_to_str,   \
    candle_string: candle_string_to_str, \
    default:       candle_string_to_str  \
)(x)

static inline candle_string candle_fmt(const char *fmt, ...) {
    va_list ap1, ap2;
    va_start(ap1, fmt);
    va_copy(ap2, ap1);
    int n = vsnprintf(NULL, 0, fmt, ap1);
    va_end(ap1);
    char *buf = GC_MALLOC(n + 1);
    vsnprintf(buf, n + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

#define print(x) _Generic((x), \
    char:          print_char,  \
    candle_int:    print_int,   \
    candle_double: print_double,\
    candle_bool:   print_bool,  \
    candle_string: print_str,   \
    default:       print_str    \
)(x)

#endif
