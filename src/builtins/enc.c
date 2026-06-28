// enc.c — std.encoding module (UTF-8 / ASCII / Latin-1)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef enum { V_NULL, V_INT, V_DOUBLE, V_BOOL, V_STRING, V_LIST, V_TUPLE,
               V_MAP, V_FN, V_LAMBDA, V_CLASS, V_OBJECT, V_NATIVE } ValueType;
typedef struct Value { ValueType type; union { long long i; double d; int b;
    char *s; void *list; void *map; void *fn; void *cls; void *obj;
    struct Value (*native)(int, struct Value*); } as; } Value;
typedef struct { char **keys; Value *vals; int len, cap; } VMap;

static Value v_string(const char *s) { Value v; v.type = V_STRING; v.as.s = (char*)s; return v; }
static Value v_bool(int x) { Value v; v.type = V_BOOL; v.as.b = x; return v; }
static Value v_int(long long x) { Value v; v.type = V_INT; v.as.i = x; return v; }
static Value v_null(void) { Value v; v.type = V_NULL; return v; }
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
static char *v_strdup(const char *s) { if(!s)return NULL; char *d=(char*)malloc(strlen(s)+1); strcpy(d,s); return d; }

// Uint8List helpers — returns a list value
// We use a minimal list representation
typedef struct { long long *data; int len, cap; } CList;
static Value v_list_new(void) {
    CList *l = (CList*)calloc(1, sizeof(CList));
    l->cap = 8; l->data = (long long*)calloc(l->cap, sizeof(long long));
    Value v; v.type = V_LIST; v.as.list = l; return v;
}
static void v_list_push(CList *l, long long x) {
    if (l->len >= l->cap) { l->cap *= 2; l->data = (long long*)realloc(l->data, l->cap * sizeof(long long)); }
    l->data[l->len++] = x;
}

// ── UTF-8 encode / decode ───────────────────────────────────────────────────

// encode: string → list<int> (UTF-8 bytes)
static Value enc_utf8_encode(int argc, Value *argv) {
    if (argc < 1 || !argv[0].as.s) return v_list_new();
    const char *s = argv[0].as.s;
    CList *l = (CList*)calloc(1, sizeof(CList)); l->cap = 16;
    l->data = (long long*)calloc(l->cap, sizeof(long long));
    for (const unsigned char *p = (const unsigned char*)s; *p; p++)
        v_list_push(l, (long long)*p);
    Value v; v.type = V_LIST; v.as.list = l; return v;
}

// decode: list<int> → string (UTF-8)
static Value enc_utf8_decode(int argc, Value *argv) {
    if (argc < 1 || argv[0].type != V_LIST) return v_string("");
    CList *l = (CList*)argv[0].as.list;
    char *buf = (char*)malloc(l->len + 1);
    for (int i = 0; i < l->len; i++) buf[i] = (char)l->data[i];
    buf[l->len] = 0;
    return v_string(buf);
}

// decode bytes with length — safer
static Value enc_decode(int argc, Value *argv) {
    if (argc < 1 || argv[0].type != V_LIST) return v_string("");
    CList *l = (CList*)argv[0].as.list;
    char *buf = (char*)malloc(l->len + 1);
    for (int i = 0; i < l->len; i++) buf[i] = (char)l->data[i];
    buf[l->len] = 0;
    return v_string(buf);
}

// ── Latin-1 / ASCII ─────────────────────────────────────────────────────────

static Value enc_latin1_encode(int argc, Value *argv) {
    if (argc < 1 || !argv[0].as.s) return v_list_new();
    const char *s = argv[0].as.s;
    CList *l = (CList*)calloc(1, sizeof(CList)); l->cap = 16;
    l->data = (long long*)calloc(l->cap, sizeof(long long));
    for (const unsigned char *p = (const unsigned char*)s; *p; p++)
        v_list_push(l, (*p < 256) ? (long long)*p : 63); // '?' for >255
    Value v; v.type = V_LIST; v.as.list = l; return v;
}

static Value enc_ascii_encode(int argc, Value *argv) {
    if (argc < 1 || !argv[0].as.s) return v_list_new();
    const char *s = argv[0].as.s;
    CList *l = (CList*)calloc(1, sizeof(CList)); l->cap = 16;
    l->data = (long long*)calloc(l->cap, sizeof(long long));
    for (const unsigned char *p = (const unsigned char*)s; *p; p++)
        v_list_push(l, (*p < 128) ? (long long)*p : 63);
    Value v; v.type = V_LIST; v.as.list = l; return v;
}

Value build_encoding(void) {
    Value mod = v_map();
    VMap *m = (VMap*)mod.as.map;

    // Functions
    v_map_set(m, "decode",    v_native(enc_utf8_decode));
    v_map_set(m, "encode",    v_native(enc_utf8_encode));

    // Named encodings (as maps with name + encode/decode)
    // UTF-8
    Value utf8 = v_map();
    VMap *u8 = (VMap*)utf8.as.map;
    v_map_set(u8, "name",     v_string("UTF-8"));
    v_map_set(u8, "encode",   v_native(enc_utf8_encode));
    v_map_set(u8, "decode",   v_native(enc_utf8_decode));
    v_map_set(m, "utf8", utf8);
    v_map_set(m, "UTF8", utf8);

    // Latin-1
    Value latin1 = v_map();
    VMap *l1 = (VMap*)latin1.as.map;
    v_map_set(l1, "name",     v_string("Latin-1"));
    v_map_set(l1, "encode",   v_native(enc_latin1_encode));
    v_map_set(l1, "decode",   v_native(enc_utf8_decode)); // Latin-1 decode = ASCII subset
    v_map_set(m, "latin1", latin1);
    v_map_set(m, "LATIN1", latin1);

    // ASCII
    Value ascii = v_map();
    VMap *a = (VMap*)ascii.as.map;
    v_map_set(a, "name",     v_string("ASCII"));
    v_map_set(a, "encode",   v_native(enc_ascii_encode));
    v_map_set(a, "decode",   v_native(enc_utf8_decode));
    v_map_set(m, "ascii", ascii);
    v_map_set(m, "ASCII", ascii);

    return mod;
}
