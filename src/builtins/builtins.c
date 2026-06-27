#include "builtins.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ── 输出 ─────────────────────────────────────────────────────────────────────

static Value bi_print(int argc, Value *argv) {
    for (int i = 0; i < argc; i++) {
        char *s = v_to_string(argv[i]);
        puts(s);
        GC_FREE(s);
    }
    return v_null();
}

// print 的不换行版本
static Value bi_write(int argc, Value *argv) {
    for (int i = 0; i < argc; i++) {
        char *s = v_to_string(argv[i]);
        fputs(s, stdout);
        GC_FREE(s);
    }
    return v_null();
}

// ── 类型转换 ─────────────────────────────────────────────────────────────────

// String(x) / str(x) —— 任意值转字符串
static Value bi_string(int argc, Value *argv) {
    if (argc < 1) return v_string("");
    char *s = v_to_string(argv[0]);
    Value r = v_string(s);
    GC_FREE(s);
    return r;
}

// int(x) —— 转整数（double 截断，string 解析，bool 0/1）
static Value bi_int(int argc, Value *argv) {
    if (argc < 1) return v_int(0);
    Value v = argv[0];
    switch (v.type) {
        case V_INT:    return v;
        case V_DOUBLE: return v_int((int64_t)v.as.d);
        case V_BOOL:   return v_int(v.as.b ? 1 : 0);
        case V_STRING: return v_int(v.as.s ? strtoll(v.as.s, NULL, 0) : 0);
        default:       return v_int(0);
    }
}

// double(x) —— 转浮点
static Value bi_double(int argc, Value *argv) {
    if (argc < 1) return v_double(0.0);
    Value v = argv[0];
    switch (v.type) {
        case V_DOUBLE: return v;
        case V_INT:    return v_double((double)v.as.i);
        case V_BOOL:   return v_double(v.as.b ? 1.0 : 0.0);
        case V_STRING: return v_double(v.as.s ? strtod(v.as.s, NULL) : 0.0);
        default:       return v_double(0.0);
    }
}

// bool(x) —— 转布尔（按真值性）
static Value bi_bool(int argc, Value *argv) {
    if (argc < 1) return v_bool(0);
    return v_bool(v_truthy(argv[0]));
}

// ── 内省 ─────────────────────────────────────────────────────────────────────

// type(x) —— 返回类型名字符串
static Value bi_type(int argc, Value *argv) {
    if (argc < 1) return v_string("null");
    return v_string(v_typename(argv[0].type));
}

// len(x) —— 字符串/列表/映射的长度
static Value bi_len(int argc, Value *argv) {
    if (argc < 1) return v_int(0);
    Value v = argv[0];
    switch (v.type) {
        case V_STRING: return v_int(v.as.s ? (int64_t)strlen(v.as.s) : 0);
        case V_LIST:   return v_int(v.as.list ? v.as.list->len : 0);
        case V_MAP:    return v_int(v.as.map ? v.as.map->len : 0);
        default:       return v_int(0);
    }
}

// content(mod) —— 列出模块/映射公开的名字（返回字符串列表）
static Value bi_content(int argc, Value *argv) {
    Value out = v_list();
    if (argc < 1) return out;
    Value v = argv[0];
    if (v.type == V_MAP && v.as.map) {
        for (int i = 0; i < v.as.map->len; i++)
            v_list_push(out.as.list, v_string(v.as.map->keys[i]));
    } else if (v.type == V_OBJECT && v.as.obj && v.as.obj->fields) {
        VMap *f = v.as.obj->fields;
        for (int i = 0; i < f->len; i++)
            v_list_push(out.as.list, v_string(f->keys[i]));
    }
    return out;
}

// ── 注册 ─────────────────────────────────────────────────────────────────────

void builtins_register(Env *globals) {
    env_define(globals, "print",  v_native(bi_print));
    env_define(globals, "write",  v_native(bi_write));
    env_define(globals, "String", v_native(bi_string));
    env_define(globals, "str",    v_native(bi_string));
    env_define(globals, "int",    v_native(bi_int));
    env_define(globals, "double", v_native(bi_double));
    env_define(globals, "bool",   v_native(bi_bool));
    env_define(globals, "type",   v_native(bi_type));
    env_define(globals, "len",    v_native(bi_len));
    env_define(globals, "content", v_native(bi_content));
}
