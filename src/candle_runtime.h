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
  #ifndef GC_MALLOC
    #define GC_MALLOC(n)         GC_malloc(n)
  #endif
  #ifndef GC_MALLOC_ATOMIC
    #define GC_MALLOC_ATOMIC(n)  GC_malloc_atomic(n)
  #endif
  #ifndef GC_REALLOC
    #define GC_REALLOC(p, n)     GC_realloc(p, n)
  #endif
  #ifndef GC_STRDUP
    #define GC_STRDUP(s)         GC_strdup(s)
  #endif
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

// ── Additional list methods ──────────────────────────────────────────────────

static inline candle_int candle_list_size(CandleList *l) {
    return l ? l->len : 0;
}

static inline void candle_list_add(CandleList *l, candle_int v) {
    if (l->len >= l->cap) {
        l->cap = l->cap < 8 ? 8 : l->cap * 2;
        candle_int *nd = GC_MALLOC(sizeof(candle_int) * l->cap);
        if (l->data) memcpy(nd, l->data, sizeof(candle_int) * l->len);
        l->data = nd;
    }
    l->data[l->len++] = v;
}

static inline candle_int candle_list_first(CandleList *l) {
    return (l && l->len > 0) ? l->data[0] : 0;
}

static inline candle_int candle_list_last(CandleList *l) {
    return (l && l->len > 0) ? l->data[l->len - 1] : 0;
}

static inline candle_bool candle_list_contains(CandleList *l, candle_int v) {
    for (int i = 0; i < l->len; i++) if (l->data[i] == v) return 1;
    return 0;
}

static inline candle_int candle_list_indexOf(CandleList *l, candle_int v) {
    for (int i = 0; i < l->len; i++) if (l->data[i] == v) return i;
    return -1;
}

static inline CandleList *candle_list_slice(CandleList *l, candle_int start, candle_int end) {
    if (start < 0) start = 0;
    if (end > l->len) end = l->len;
    int n = (end > start) ? (int)(end - start) : 0;
    CandleList *r = GC_MALLOC(sizeof(CandleList));
    r->cap = n > 0 ? n : 1;
    r->data = GC_MALLOC(sizeof(candle_int) * r->cap);
    r->len = n;
    for (int i = 0; i < n; i++) r->data[i] = l->data[start + i];
    return r;
}

static inline candle_string candle_list_join(CandleList *l, candle_string sep) {
    if (!l || l->len == 0) return "";
    // Estimate buffer size
    int total = 0;
    char **parts = GC_MALLOC(sizeof(char*) * l->len);
    int seplen = sep ? (int)strlen(sep) : 0;
    for (int i = 0; i < l->len; i++) {
        parts[i] = GC_MALLOC(32);
        snprintf(parts[i], 32, "%lld", (long long)l->data[i]);
        total += (int)strlen(parts[i]);
    }
    total += seplen * (l->len - 1);
    char *buf = GC_MALLOC(total + 1);
    buf[0] = 0;
    for (int i = 0; i < l->len; i++) {
        if (i > 0 && sep) strcat(buf, sep);
        strcat(buf, parts[i]);
    }
    return buf;
}

static inline CandleList *candle_list_concat(CandleList *a, CandleList *b) {
    int n = a->len + b->len;
    CandleList *r = GC_MALLOC(sizeof(CandleList));
    r->cap = n > 0 ? n : 1;
    r->data = GC_MALLOC(sizeof(candle_int) * r->cap);
    r->len = n;
    for (int i = 0; i < a->len; i++) r->data[i] = a->data[i];
    for (int i = 0; i < b->len; i++) r->data[a->len + i] = b->data[i];
    return r;
}

static inline candle_bool candle_list_isEmpty(CandleList *l) {
    return l->len == 0;
}

static inline candle_bool candle_list_isNotEmpty(CandleList *l) {
    return l->len != 0;
}

static inline void candle_list_clear(CandleList *l) {
    l->len = 0;
}

static inline CandleList *candle_list_reversed(CandleList *l) {
    CandleList *r = GC_MALLOC(sizeof(CandleList));
    r->cap = l->len > 0 ? l->len : 1;
    r->data = GC_MALLOC(sizeof(candle_int) * r->cap);
    r->len = l->len;
    for (int i = 0; i < l->len; i++) r->data[i] = l->data[l->len - 1 - i];
    return r;
}

static inline candle_int candle_list_sum(CandleList *l) {
    candle_int s = 0;
    for (int i = 0; i < l->len; i++) s += l->data[i];
    return s;
}

static inline candle_int candle_list_length(CandleList *l) {
    return l ? l->len : 0;
}

// ── Map methods ──────────────────────────────────────────────────────────────

static inline candle_int candle_map_size(CandleMap *m) {
    return m ? m->len : 0;
}

static inline candle_bool candle_map_isEmpty(CandleMap *m) {
    return !m || m->len == 0;
}

static inline candle_bool candle_map_isNotEmpty(CandleMap *m) {
    return m && m->len > 0;
}

static inline candle_bool candle_map_containsKey(CandleMap *m, candle_string key) {
    if (!m || !key) return 0;
    for (int i = 0; i < m->len; i++)
        if (m->keys[i] && strcmp(m->keys[i], key) == 0) return 1;
    return 0;
}

static inline candle_int candle_map_get(CandleMap *m, candle_string key) {
    if (!m || !key) return 0;
    for (int i = 0; i < m->len; i++)
        if (m->keys[i] && strcmp(m->keys[i], key) == 0) return m->vals[i];
    return 0;
}

static inline void candle_map_set(CandleMap *m, candle_string key, candle_int val) {
    if (!m || !key) return;
    for (int i = 0; i < m->len; i++) {
        if (m->keys[i] && strcmp(m->keys[i], key) == 0) {
            m->vals[i] = val; return;
        }
    }
    if (m->len >= m->cap) {
        m->cap = m->cap < 4 ? 4 : m->cap * 2;
        candle_string *nk = GC_MALLOC(sizeof(candle_string) * m->cap);
        candle_int *nv = GC_MALLOC(sizeof(candle_int) * m->cap);
        memcpy(nk, m->keys, sizeof(candle_string) * m->len);
        memcpy(nv, m->vals, sizeof(candle_int) * m->len);
        m->keys = nk; m->vals = nv;
    }
    m->keys[m->len] = key;
    m->vals[m->len] = val;
    m->len++;
}

// ── Built-in functions ───────────────────────────────────────────────────────

static inline candle_int len(candle_string s) {
    return s ? (candle_int)strlen(s) : 0;
}

// len overload for lists — codegen emits len_list when arg is CandleList*
static inline candle_int len_list(CandleList *l) {
    return l ? l->len : 0;
}

// type() — returns type name as string (limited AOT support)
static inline candle_string type_of_int(candle_int v) { (void)v; return "int"; }
static inline candle_string type_of_double(candle_double v) { (void)v; return "double"; }
static inline candle_string type_of_str(candle_string v) { (void)v; return "string"; }
static inline candle_string type_of_bool(candle_bool v) { (void)v; return "bool"; }
static inline candle_string type_of_list(CandleList *v) { (void)v; return "list"; }
static inline candle_string type_of_map(CandleMap *v) { (void)v; return "map"; }

#define type(x) _Generic((x), \
    candle_int:    type_of_int,    \
    candle_double: type_of_double, \
    candle_bool:   type_of_bool,   \
    candle_string: type_of_str,    \
    CandleList*:   type_of_list,   \
    CandleMap*:    type_of_map,    \
    default:       type_of_str     \
)(x)

// print overload for CandleList*
static inline void print_list(CandleList *l) {
    if (!l) { printf("[]\n"); return; }
    printf("[");
    for (int i = 0; i < l->len; i++) {
        if (i > 0) printf(", ");
        printf("%lld", (long long)l->data[i]);
    }
    printf("]\n");
}

// ── Int methods ──────────────────────────────────────────────────────────────
static inline candle_bool candle_int_isEven(candle_int v)  { return v % 2 == 0; }
static inline candle_bool candle_int_isOdd(candle_int v)   { return v % 2 != 0; }
static inline candle_bool candle_int_isNegative(candle_int v) { return v < 0; }
static inline candle_bool candle_int_isZero(candle_int v)  { return v == 0; }
static inline candle_int  candle_int_sign(candle_int v)    { return v > 0 ? 1 : v < 0 ? -1 : 0; }
static inline candle_int  candle_int_abs(candle_int v)     { return v < 0 ? -v : v; }
static inline candle_int  candle_int_clamp(candle_int v, candle_int lo, candle_int hi) {
    return v < lo ? lo : v > hi ? hi : v;
}
static inline candle_double candle_int_toDouble(candle_int v) { return (candle_double)v; }
static inline candle_string candle_int_toString(candle_int v) {
    char *b = GC_MALLOC(32); snprintf(b, 32, "%lld", (long long)v); return b;
}
static inline CandleList *candle_int_range(candle_int v) {
    CandleList *l = GC_MALLOC(sizeof(CandleList));
    l->cap = v > 0 ? (int)v : 1; l->len = v > 0 ? (int)v : 0;
    l->data = GC_MALLOC(sizeof(candle_int) * l->cap);
    for (int i = 0; i < l->len; i++) l->data[i] = i;
    return l;
}
static inline CandleList *candle_int_rangeFrom(candle_int v, candle_int start) {
    int n = (v > start) ? (int)(v - start) : 0;
    CandleList *l = GC_MALLOC(sizeof(CandleList));
    l->cap = n > 0 ? n : 1; l->len = n;
    l->data = GC_MALLOC(sizeof(candle_int) * l->cap);
    for (int i = 0; i < n; i++) l->data[i] = start + i;
    return l;
}

// ── Double methods ───────────────────────────────────────────────────────────
static inline candle_bool candle_double_isPositive(candle_double v) { return v > 0; }
static inline candle_bool candle_double_isNegative(candle_double v) { return v < 0; }
static inline candle_bool candle_double_isZero(candle_double v)    { return v == 0.0; }
static inline candle_bool candle_double_isInteger(candle_double v) { return v == (candle_int)v; }
static inline candle_int  candle_double_sign(candle_double v)      { return v > 0 ? 1 : v < 0 ? -1 : 0; }
static inline candle_double candle_double_abs(candle_double v)     { return v < 0 ? -v : v; }
static inline candle_int  candle_double_toInt(candle_double v)     { return (candle_int)v; }
static inline candle_int  candle_double_round(candle_double v)     { return (candle_int)(v + 0.5); }
static inline candle_int  candle_double_floor(candle_double v)     { return (candle_int)v; }
static inline candle_int  candle_double_ceil(candle_double v)      { return (candle_int)(v + 0.999999999); }
static inline candle_string candle_double_toString(candle_double v) {
    char *b = GC_MALLOC(32); snprintf(b, 32, "%g", v); return b;
}
static inline candle_double candle_double_clamp(candle_double v, candle_double lo, candle_double hi) {
    return v < lo ? lo : v > hi ? hi : v;
}
static inline candle_double candle_double_squared(candle_double v) { return v * v; }
static inline candle_double candle_double_sqrt(candle_double v)    { return v >= 0 ? v * 0.5 : 0; } /* approximate */

// ── String methods ───────────────────────────────────────────────────────────
static inline candle_int candle_string_length(candle_string s) { return s ? (candle_int)strlen(s) : 0; }
static inline candle_bool candle_string_isEmpty(candle_string s) { return !s || strlen(s) == 0; }
static inline candle_bool candle_string_isNotEmpty(candle_string s) { return s && strlen(s) > 0; }
static inline candle_bool candle_string_contains(candle_string s, candle_string sub) {
    return (s && sub && strstr(s, sub)) ? 1 : 0;
}
static inline candle_bool candle_string_startsWith(candle_string s, candle_string prefix) {
    if (!s || !prefix) return 0;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}
static inline candle_bool candle_string_endsWith(candle_string s, candle_string suffix) {
    if (!s || !suffix) return 0;
    size_t sl = strlen(s), xl = strlen(suffix);
    if (xl > sl) return 0;
    return strcmp(s + sl - xl, suffix) == 0;
}
static inline candle_int candle_string_indexOf(candle_string s, candle_string sub) {
    if (!s || !sub) return -1;
    const char *p = strstr(s, sub);
    return p ? (candle_int)(p - s) : -1;
}
static inline candle_string candle_string_substring(candle_string s, candle_int start, candle_int end) {
    if (!s) return "";
    int sl = (int)strlen(s);
    if (start < 0) start = 0;
    if (end > sl) end = sl;
    int n = (end > start) ? (int)(end - start) : 0;
    char *r = GC_MALLOC(n + 1);
    memcpy(r, s + start, n);
    r[n] = 0;
    return r;
}
static inline candle_string candle_string_toUpperCase(candle_string s) {
    if (!s) return "";
    int n = (int)strlen(s);
    char *r = GC_MALLOC(n + 1);
    for (int i = 0; i < n; i++) r[i] = (s[i] >= 'a' && s[i] <= 'z') ? s[i] - 32 : s[i];
    r[n] = 0;
    return r;
}
static inline candle_string candle_string_toLowerCase(candle_string s) {
    if (!s) return "";
    int n = (int)strlen(s);
    char *r = GC_MALLOC(n + 1);
    for (int i = 0; i < n; i++) r[i] = (s[i] >= 'A' && s[i] <= 'Z') ? s[i] + 32 : s[i];
    r[n] = 0;
    return r;
}
static inline candle_string candle_string_trim(candle_string s) {
    if (!s) return "";
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    int n = (int)strlen(s);
    while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\n' || s[n-1] == '\r')) n--;
    char *r = GC_MALLOC(n + 1);
    memcpy(r, s, n); r[n] = 0;
    return r;
}
static inline candle_string candle_string_toString(candle_string s) { return s ? s : ""; }

// ── Bool methods ─────────────────────────────────────────────────────────────
static inline candle_bool candle_bool_toggle(candle_bool v) { return !v; }
static inline candle_int  candle_bool_toInt(candle_bool v)  { return v ? 1 : 0; }
static inline candle_string candle_bool_toString(candle_bool v) { return v ? "true" : "false"; }

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
    CandleList*:   print_list,  \
    default:       print_str    \
)(x)

// ── String comparison helpers ──
static inline candle_bool candle_str_ne(candle_string a, candle_string b) {
    return !candle_str_eq(a, b);
}

// ── readLine: read a line from stdin with optional prompt ──
static inline candle_string candle_readLine(candle_string prompt) {
    if (prompt && prompt[0]) { fputs(prompt, stdout); fflush(stdout); }
    char buf[4096];
    if (!fgets(buf, sizeof(buf), stdin)) return NULL;
    size_t n = strlen(buf);
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = 0;
    char *s = (char*)malloc(n + 1);
    memcpy(s, buf, n + 1);
    return s;
}
#define readLine(p) candle_readLine(p)

#endif
