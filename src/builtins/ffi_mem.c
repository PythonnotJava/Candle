#include "ffi_mem.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// ── alloc(size) → int ──────────────────────────────────────────────────────
static Value ffi_mem_alloc(int argc, Value *argv) {
    int64_t size;
    void *p;
    if (argc < 1) return v_int(0);
    size = argv[0].as.i;
    if (size <= 0) return v_int(0);
    p = malloc((size_t)size);
    return v_int((int64_t)(intptr_t)p);
}

// ── free(ptr) → void ──────────────────────────────────────────────────────
static Value ffi_mem_free(int argc, Value *argv) {
    int64_t ptr;
    if (argc < 1) return v_null();
    ptr = argv[0].as.i;
    if (ptr) free((void*)(intptr_t)ptr);
    return v_null();
}

// ── memcpy(dst, src, size) → void ─────────────────────────────────────────
static Value ffi_mem_memcpy(int argc, Value *argv) {
    void *dst, *src;
    int64_t size;
    if (argc < 3) return v_null();
    dst  = (void*)(intptr_t)argv[0].as.i;
    src  = (void*)(intptr_t)argv[1].as.i;
    size = argv[2].as.i;
    if (dst && src && size > 0) memcpy(dst, src, (size_t)size);
    return v_null();
}

// ── memset(ptr, byte, size) → void ────────────────────────────────────────
static Value ffi_mem_memset(int argc, Value *argv) {
    void *dst;
    int byte;
    int64_t size;
    if (argc < 3) return v_null();
    dst  = (void*)(intptr_t)argv[0].as.i;
    byte = (int)argv[1].as.i;
    size = argv[2].as.i;
    if (dst && size > 0) memset(dst, byte, (size_t)size);
    return v_null();
}

// ── readInt(ptr) → int ────────────────────────────────────────────────────
static Value ffi_mem_read_int(int argc, Value *argv) {
    int *p;
    if (argc < 1) return v_int(0);
    p = (int*)(intptr_t)argv[0].as.i;
    return p ? v_int(*p) : v_int(0);
}

// ── writeInt(ptr, val) → void ─────────────────────────────────────────────
static Value ffi_mem_write_int(int argc, Value *argv) {
    int *p;
    if (argc < 2) return v_null();
    p = (int*)(intptr_t)argv[0].as.i;
    if (p) *p = (int)argv[1].as.i;
    return v_null();
}

// ── ptrToInt(p) → int ─────────────────────────────────────────────────────
static Value ffi_mem_ptr_to_int(int argc, Value *argv) {
    if (argc < 1) return v_int(0);
    if (argv[0].type == V_INT) return argv[0];
    if (argv[0].type == V_STRING && argv[0].as.s)
        return v_int((int64_t)(intptr_t)argv[0].as.s);
    return v_int(0);
}

// ── intToPtr(n) → int ─────────────────────────────────────────────────────
static Value ffi_mem_int_to_ptr(int argc, Value *argv) {
    if (argc < 1) return v_int(0);
    return v_int(argv[0].as.i);
}

// ── readString(ptr) → string ──────────────────────────────────────────────
static Value ffi_mem_read_string(int argc, Value *argv) {
    const char *p;
    if (argc < 1) return v_null();
    p = (const char*)(intptr_t)argv[0].as.i;
    return p ? v_string(p) : v_null();
}

// ── sizeOf(type) → int ────────────────────────────────────────────────────
static Value ffi_mem_sizeof(int argc, Value *argv) {
    const char *t;
    if (argc < 1) return v_int(0);
    t = argv[0].type == V_STRING ? argv[0].as.s : NULL;
    if (!t) return v_int(0);
    if (strcmp(t, "int")   == 0) return v_int(sizeof(int));
    if (strcmp(t, "double")== 0) return v_int(sizeof(double));
    if (strcmp(t, "float") == 0) return v_int(sizeof(float));
    if (strcmp(t, "ptr")   == 0) return v_int(sizeof(void*));
    if (strcmp(t, "char")  == 0) return v_int(sizeof(char));
    if (strcmp(t, "short") == 0) return v_int(sizeof(short));
    if (strcmp(t, "long")  == 0) return v_int(sizeof(long));
    return v_int(0);
}

// ── 模块构建 ─────────────────────────────────────────────────────────────

Value build_ffi_mem(void) {
    Value m = v_map();
    VMap *vm = m.as.map;
    v_map_set(vm, "alloc",      v_native(ffi_mem_alloc));
    v_map_set(vm, "free",       v_native(ffi_mem_free));
    v_map_set(vm, "memcpy",     v_native(ffi_mem_memcpy));
    v_map_set(vm, "memset",     v_native(ffi_mem_memset));
    v_map_set(vm, "readInt",    v_native(ffi_mem_read_int));
    v_map_set(vm, "writeInt",   v_native(ffi_mem_write_int));
    v_map_set(vm, "ptrToInt",   v_native(ffi_mem_ptr_to_int));
    v_map_set(vm, "intToPtr",   v_native(ffi_mem_int_to_ptr));
    v_map_set(vm, "readString", v_native(ffi_mem_read_string));
    v_map_set(vm, "sizeOf",     v_native(ffi_mem_sizeof));
    return m;
}
