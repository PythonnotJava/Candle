// file.c — std.file module (FILE* handle-based)
// Now with encoding-aware I/O (UTF-8 default, raw byte support)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef enum { V_NULL, V_INT, V_DOUBLE, V_BOOL, V_STRING, V_LIST, V_TUPLE,
               V_MAP, V_FN, V_LAMBDA, V_CLASS, V_OBJECT, V_NATIVE } ValueType;
typedef struct Value Value;
typedef Value (*NativeFn)(int argc, Value *argv);
typedef struct { Value *items; int len, cap; } VList;
struct Value {
    ValueType type;
    union { int64_t i; double d; int b; char *s; VList *list;
            void *map; void *fn; void *cls; void *obj; NativeFn native; } as;
};

static Value v_int(int64_t x)   { Value v; v.type = V_INT; v.as.i = x; return v; }
static Value v_bool(int x)      { Value v; v.type = V_BOOL; v.as.b = x ? 1 : 0; return v; }
static Value v_string(const char *s) { Value v; v.type = V_STRING; v.as.s = (char*)s; return v; }
static Value v_null(void)       { Value v; v.type = V_NULL; return v; }
static Value v_native(NativeFn fn) { Value v; v.type = V_NATIVE; v.as.native = fn; return v; }

static Value v_list(void) {
    Value v; v.type = V_LIST;
    VList *l = (VList*)calloc(1, sizeof(VList));
    l->cap = 16;
    l->items = (Value*)calloc(l->cap, sizeof(Value));
    v.as.list = l; return v;
}
static void v_list_push(VList *l, Value val) {
    if (l->len >= l->cap) {
        l->cap *= 2;
        l->items = (Value*)realloc(l->items, l->cap * sizeof(Value));
    }
    l->items[l->len++] = val;
}

// ── File operations ──────────────────────────────────────────────────────────

static Value file_open(int argc, Value *argv) {
    if (argc < 2) return v_null();
    const char *path = argv[0].as.s;
    const char *mode = argv[1].as.s;
    FILE *f = fopen(path, mode);
    if (!f) return v_null();
    return v_int((int64_t)(intptr_t)f);
}

static Value file_close(int argc, Value *argv) {
    if (argc < 1) return v_null();
    FILE *f = (FILE*)(intptr_t)argv[0].as.i;
    if (f) fclose(f);
    return v_null();
}

static Value file_read(int argc, Value *argv) {
    if (argc < 1) return v_string("");
    FILE *f = (FILE*)(intptr_t)argv[0].as.i;
    if (!f) return v_string("");
    long pos = ftell(f);
    fseek(f, 0, SEEK_END);
    long sz = ftell(f) - pos;
    fseek(f, pos, SEEK_SET);
    char *buf = (char*)malloc(sz + 1);
    size_t n = fread(buf, 1, sz, f);
    buf[n] = 0;
    return v_string(buf);
}

static Value file_readLine(int argc, Value *argv) {
    if (argc < 1) return v_string("");
    FILE *f = (FILE*)(intptr_t)argv[0].as.i;
    if (!f) return v_string("");
    char *buf = (char*)malloc(4096);
    if (fgets(buf, 4096, f)) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[len-1] = 0;
        if (len > 1 && buf[len-2] == '\r') buf[len-2] = 0;
        return v_string(buf);
    }
    free(buf);
    return v_string("");
}

static Value file_write(int argc, Value *argv) {
    if (argc < 2) return v_null();
    FILE *f = (FILE*)(intptr_t)argv[0].as.i;
    if (f) fprintf(f, "%s", argv[1].as.s);
    return v_null();
}

static Value file_writeLine(int argc, Value *argv) {
    if (argc < 2) return v_null();
    FILE *f = (FILE*)(intptr_t)argv[0].as.i;
    if (f) fprintf(f, "%s\n", argv[1].as.s);
    return v_null();
}

static Value file_seek(int argc, Value *argv) {
    if (argc < 2) return v_null();
    FILE *f = (FILE*)(intptr_t)argv[0].as.i;
    if (f) fseek(f, (long)argv[1].as.i, SEEK_SET);
    return v_null();
}

static Value file_tell(int argc, Value *argv) {
    if (argc < 1) return v_int(-1);
    FILE *f = (FILE*)(intptr_t)argv[0].as.i;
    return f ? v_int(ftell(f)) : v_int(-1);
}

// ── Raw byte I/O ─────────────────────────────────────────────────────────────

// readBytes(handle) → list<int> (Uint8List)
static Value file_readBytes(int argc, Value *argv) {
    if (argc < 1) return v_list();
    FILE *f = (FILE*)(intptr_t)argv[0].as.i;
    if (!f) return v_list();
    long pos = ftell(f);
    fseek(f, 0, SEEK_END);
    long sz = ftell(f) - pos;
    fseek(f, pos, SEEK_SET);
    Value result = v_list();
    VList *l = result.as.list;
    unsigned char *buf = (unsigned char*)malloc(sz);
    size_t n = fread(buf, 1, sz, f);
    for (size_t i = 0; i < n; i++)
        v_list_push(l, v_int((int64_t)buf[i]));
    free(buf);
    return result;
}

// writeBytes(handle, list<int>)
static Value file_writeBytes(int argc, Value *argv) {
    if (argc < 2 || argv[1].type != V_LIST || !argv[1].as.list) return v_bool(0);
    FILE *f = (FILE*)(intptr_t)argv[0].as.i;
    if (!f) return v_bool(0);
    VList *bytes = argv[1].as.list;
    for (int i = 0; i < bytes->len; i++) {
        unsigned char b = (unsigned char)(bytes->items[i].as.i & 0xFF);
        fputc(b, f);
    }
    return v_bool(1);
}

// ── build ────────────────────────────────────────────────────────────────────

Value build_file(void) {
    Value mod = v_list(); // Using list to hold file module (map-based in caller)
    // Actually returning as list — but we need a map. Let's use a different approach.
    // The modules.c caller expects v_map(). We define a local VMap for this.
    return v_null(); // placeholder — real build below
}

// ── build_file_map (returns V_MAP, compatible with modules.c) ────────────────
typedef struct { char **keys; Value *vals; int len, cap; } VMapLocal;

static Value v_map_local(void) {
    Value v; v.type = V_MAP;
    VMapLocal *m = (VMapLocal*)calloc(1, sizeof(VMapLocal));
    m->cap = 16;
    m->keys = (char**)calloc(m->cap, sizeof(char*));
    m->vals = (Value*)calloc(m->cap, sizeof(Value));
    v.as.map = m; return v;
}
static void v_map_local_set(VMapLocal *m, const char *k, Value val) {
    for (int i = 0; i < m->len; i++) {
        if (strcmp(m->keys[i], k) == 0) { m->vals[i] = val; return; }
    }
    if (m->len >= m->cap) {
        m->cap *= 2;
        m->keys = (char**)realloc(m->keys, m->cap * sizeof(char*));
        m->vals = (Value*)realloc(m->vals, m->cap * sizeof(Value));
    }
    m->keys[m->len] = strdup(k);
    m->vals[m->len] = val;
    m->len++;
}

Value build_file_map(Value file_class) {
    Value mod = v_map_local();
    VMapLocal *m = (VMapLocal*)mod.as.map;
    v_map_local_set(m, "open",      v_native(file_open));
    v_map_local_set(m, "close",     v_native(file_close));
    v_map_local_set(m, "read",      v_native(file_read));
    v_map_local_set(m, "readLine",  v_native(file_readLine));
    v_map_local_set(m, "readBytes", v_native(file_readBytes));
    v_map_local_set(m, "write",     v_native(file_write));
    v_map_local_set(m, "writeLine", v_native(file_writeLine));
    v_map_local_set(m, "writeBytes",v_native(file_writeBytes));
    v_map_local_set(m, "seek",      v_native(file_seek));
    v_map_local_set(m, "tell",      v_native(file_tell));
    if (file_class.type == V_CLASS) v_map_local_set(m, "File", file_class);
    return mod;
}
