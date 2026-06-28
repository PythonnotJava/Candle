// fs.c — std.fs module (file system operations)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef enum { V_NULL, V_INT, V_DOUBLE, V_BOOL, V_STRING, V_LIST, V_TUPLE,
               V_MAP, V_FN, V_LAMBDA, V_CLASS, V_OBJECT, V_NATIVE } ValueType;
typedef struct Value { ValueType type; union { long long i; double d; int b;
    char *s; void *list; void *map; void *fn; void *cls; void *obj;
    struct Value (*native)(int, struct Value*); } as; } Value;
typedef struct { char **keys; Value *vals; int len, cap; } VMap;

static Value v_int(long long x)   { Value v; v.type = V_INT; v.as.i = x; return v; }
static Value v_bool(int x)        { Value v; v.type = V_BOOL; v.as.b = x; return v; }
static Value v_string(const char *s) { Value v; v.type = V_STRING; v.as.s = (char*)s; return v; }
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

// ── FS operations ────────────────────────────────────────────────────────────

#ifdef _WIN32
  #include <direct.h>
  #define fs_mkdir_impl(p) _mkdir(p)
  #define fs_stat_impl _stat
  #define fs_S_ISDIR(m) ((m) & _S_IFDIR)
  #define fs_S_ISREG(m) ((m) & _S_IFREG)
#else
  #include <sys/stat.h>
  #define fs_mkdir_impl(p) mkdir(p, 0755)
  #define fs_stat_impl stat
  #define fs_S_ISDIR(m) S_ISDIR(m)
  #define fs_S_ISREG(m) S_ISREG(m)
#endif

static Value fs_exists(int argc, Value *argv) {
    if (argc < 1) return v_bool(0);
    struct fs_stat_impl st;
    return v_bool(fs_stat_impl(argv[0].as.s, &st) == 0);
}
static Value fs_isDir(int argc, Value *argv) {
    if (argc < 1) return v_bool(0);
    struct fs_stat_impl st;
    return v_bool(fs_stat_impl(argv[0].as.s, &st) == 0 && fs_S_ISDIR(st.st_mode));
}
static Value fs_isFile(int argc, Value *argv) {
    if (argc < 1) return v_bool(0);
    struct fs_stat_impl st;
    return v_bool(fs_stat_impl(argv[0].as.s, &st) == 0 && fs_S_ISREG(st.st_mode));
}
static Value fs_fileSize(int argc, Value *argv) {
    if (argc < 1) return v_int(-1);
    struct fs_stat_impl st;
    if (fs_stat_impl(argv[0].as.s, &st) != 0) return v_int(-1);
    return v_int(st.st_size);
}
static Value fs_readFile(int argc, Value *argv) {
    if (argc < 1) return v_string("");
    FILE *f = fopen(argv[0].as.s, "rb");
    if (!f) return v_string("");
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char*)malloc(sz + 1);
    fread(buf, 1, sz, f);
    buf[sz] = 0;
    fclose(f);
    return v_string(buf);
}
static Value fs_writeFile(int argc, Value *argv) {
    if (argc < 2) return v_null();
    FILE *f = fopen(argv[0].as.s, "wb");
    if (!f) return v_null();
    fprintf(f, "%s", argv[1].as.s);
    fclose(f);
    return v_null();
}
static Value fs_createDir(int argc, Value *argv) {
    if (argc < 1) return v_bool(0);
    return v_bool(fs_mkdir_impl(argv[0].as.s) == 0);
}
static Value fs_delete(int argc, Value *argv) {
    if (argc < 1) return v_null();
    const char *p = argv[0].as.s;
    // Try as file first, then as directory
    if (remove(p) != 0) {
        // remove() only deletes files; use rmdir for directories
        #ifdef _WIN32
        _rmdir(p);
        #else
        rmdir(p);
        #endif
    }
    return v_null();
}
static Value fs_listDir(int argc, Value *argv) {
    // Simplified: returns empty list on Windows (full impl needs FindFirstFile)
#ifdef _WIN32
    return v_null();
#else
    return v_null();
#endif
}

Value build_fs(void) {
    Value mod = v_map();
    VMap *m = (VMap*)mod.as.map;
    v_map_set(m, "exists",    v_native(fs_exists));
    v_map_set(m, "isDir",     v_native(fs_isDir));
    v_map_set(m, "isFile",    v_native(fs_isFile));
    v_map_set(m, "fileSize",  v_native(fs_fileSize));
    v_map_set(m, "readFile",  v_native(fs_readFile));
    v_map_set(m, "writeFile", v_native(fs_writeFile));
    v_map_set(m, "mkdir",     v_native(fs_createDir));
    v_map_set(m, "delete",    v_native(fs_delete));
    v_map_set(m, "listDir",   v_native(fs_listDir));
    return mod;
}
