#include "builtins.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

extern AstNode *find_class(const char *name);

// -- Output --------------------------------------------------------

static Value bi_print(int argc, Value *argv) {
    for (int i = 0; i < argc; i++) {
        char *s = v_to_string(argv[i]);
        puts(s);
        GC_FREE(s);
    }
    return v_null();
}

static Value bi_write(int argc, Value *argv) {
    for (int i = 0; i < argc; i++) {
        char *s = v_to_string(argv[i]);
        fputs(s, stdout);
        GC_FREE(s);
    }
    return v_null();
}

// -- Type conversion ------------------------------------------------

static Value bi_string(int argc, Value *argv) {
    if (argc < 1) return v_string("");
    char *s = v_to_string(argv[0]);
    Value r = v_string(s);
    GC_FREE(s);
    return r;
}

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

static Value bi_bool(int argc, Value *argv) {
    if (argc < 1) return v_bool(0);
    return v_bool(v_truthy(argv[0]));
}

// -- Introspection -------------------------------------------------

static Value bi_type(int argc, Value *argv) {
    if (argc < 1) return v_string("null");
    return v_string(v_typename(argv[0].type));
}

static Value bi_len(int argc, Value *argv) {
    if (argc < 1) return v_int(0);
    Value v = argv[0];
    switch (v.type) {
        case V_STRING: return v_int(v.as.s ? (int64_t)strlen(v.as.s) : 0);
        case V_LIST:   return v_int(v.as.list ? v.as.list->len : 0);
        case V_TUPLE:  return v_int(v.as.list ? v.as.list->len : 0);
        case V_MAP:    return v_int(v.as.map ? v.as.map->len : 0);
        default:       return v_int(0);
    }
}

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


// -- Python-style builtins -----------------------------------------

// chr(n) -> codepoint to single char
static Value bi_chr(int argc, Value *argv) {
    if (argc < 1) return v_string("");
    int64_t code = argv[0].type == V_INT ? argv[0].as.i : 0;
    char buf[2] = { (char)(code & 0xFF), 0 };
    return v_string(buf);
}

// ord(c) -> codepoint of first char
static Value bi_ord(int argc, Value *argv) {
    if (argc < 1 || argv[0].type != V_STRING || !argv[0].as.s) return v_int(-1);
    return v_int((unsigned char)argv[0].as.s[0]);
}

// input(prompt?) -> print prompt and read a line from stdin
static Value bi_input(int argc, Value *argv) {
    if (argc >= 1 && argv[0].type == V_STRING && argv[0].as.s)
        fputs(argv[0].as.s, stdout);
    fflush(stdout);
    char buf[4096];
    if (!fgets(buf, sizeof(buf), stdin)) return v_string("");
    size_t n = strlen(buf);
    while (n && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = 0;
    return v_string(buf);
}

// exit(code?) -> exit process
static Value bi_exit(int argc, Value *argv) {
    int code = (argc >= 1 && argv[0].type == V_INT) ? (int)argv[0].as.i : 0;
    exit(code);
    return v_null();
}

// is(x, type_name) -> runtime type check
static Value bi_is(int argc, Value *argv) {
    if (argc < 2) return v_bool(0);
    const char *want = NULL;
    if (argv[1].type == V_STRING)
        want = argv[1].as.s;
    else if (argv[1].type == V_CLASS && argv[1].as.cls)
        want = argv[1].as.cls->as.class_decl.name;
    if (!want) return v_bool(0);
    Value x = argv[0];
    if (strcmp(want, "int") == 0)    return v_bool(x.type == V_INT);
    if (strcmp(want, "double") == 0) return v_bool(x.type == V_DOUBLE);
    if (strcmp(want, "bool") == 0)   return v_bool(x.type == V_BOOL);
    if (strcmp(want, "string") == 0) return v_bool(x.type == V_STRING);
    if (strcmp(want, "list") == 0)   return v_bool(x.type == V_LIST);
    if (strcmp(want, "tuple") == 0)  return v_bool(x.type == V_TUPLE);
    if (strcmp(want, "map") == 0)    return v_bool(x.type == V_MAP);
    if (strcmp(want, "object") == 0) return v_bool(x.type == V_OBJECT);
    if (strcmp(want, "function") == 0) return v_bool(x.type == V_FN || x.type == V_LAMBDA || x.type == V_NATIVE);
    if (strcmp(want, "null") == 0)   return v_bool(x.type == V_NULL);
    AstNode *cn = NULL;
    if (x.type == V_OBJECT && x.as.obj && x.as.obj->class_node)
        cn = x.as.obj->class_node;
    else if (x.type == V_CLASS && x.as.cls)
        cn = x.as.cls;
    while (cn) {
        if (strcmp(cn->as.class_decl.name, want) == 0) return v_bool(1);
        cn = find_class(cn->as.class_decl.parent);
    }
    return v_bool(0);
}

// -- Tuple ---------------------------------------------------------

// tuple(e1, e2, ...) -> immutable tuple
static Value bi_tuple(int argc, Value *argv) {
    Value v = v_tuple();
    VList *l = v.as.list;
    for (int i = 0; i < argc; i++)
        v_list_push(l, argv[i]);
    return v;
}

// -- Byte / Bit ops ------------------------------------------------

// byteAnd(a, b) -> a & b (byte range 0-255)
static Value bi_byte_and(int argc, Value *argv) {
    int64_t a = (argc > 0 && argv[0].type == V_INT) ? argv[0].as.i : 0;
    int64_t b = (argc > 1 && argv[1].type == V_INT) ? argv[1].as.i : 0;
    return v_int((a & 0xFF) & (b & 0xFF));
}

static Value bi_byte_or(int argc, Value *argv) {
    int64_t a = (argc > 0 && argv[0].type == V_INT) ? argv[0].as.i : 0;
    int64_t b = (argc > 1 && argv[1].type == V_INT) ? argv[1].as.i : 0;
    return v_int(((a & 0xFF) | (b & 0xFF)) & 0xFF);
}

static Value bi_byte_xor(int argc, Value *argv) {
    int64_t a = (argc > 0 && argv[0].type == V_INT) ? argv[0].as.i : 0;
    int64_t b = (argc > 1 && argv[1].type == V_INT) ? argv[1].as.i : 0;
    return v_int(((a & 0xFF) ^ (b & 0xFF)) & 0xFF);
}

static Value bi_byte_not(int argc, Value *argv) {
    int64_t a = (argc > 0 && argv[0].type == V_INT) ? argv[0].as.i : 0;
    return v_int((~(a & 0xFF)) & 0xFF);
}

static Value bi_byte_shl(int argc, Value *argv) {
    int64_t a = (argc > 0 && argv[0].type == V_INT) ? argv[0].as.i : 0;
    int64_t n = (argc > 1 && argv[1].type == V_INT) ? argv[1].as.i : 0;
    return v_int(((a & 0xFF) << (n & 7)) & 0xFF);
}

static Value bi_byte_shr(int argc, Value *argv) {
    int64_t a = (argc > 0 && argv[0].type == V_INT) ? argv[0].as.i : 0;
    int64_t n = (argc > 1 && argv[1].type == V_INT) ? argv[1].as.i : 0;
    return v_int(((a & 0xFF) >> (n & 7)) & 0xFF);
}

// byteToHex(b) -> "0a"
static Value bi_byte_to_hex(int argc, Value *argv) {
    int64_t b = (argc > 0 && argv[0].type == V_INT) ? (argv[0].as.i & 0xFF) : 0;
    char buf[3];
    static const char hex[] = "0123456789abcdef";
    buf[0] = hex[b >> 4];
    buf[1] = hex[b & 0xF];
    buf[2] = 0;
    return v_string(buf);
}

// byteToBin(b) -> "00001010"
static Value bi_byte_to_bin(int argc, Value *argv) {
    int64_t b = (argc > 0 && argv[0].type == V_INT) ? (argv[0].as.i & 0xFF) : 0;
    char buf[9];
    for (int i = 7; i >= 0; i--)
        buf[7-i] = ((unsigned)b >> i) & 1 ? '1' : '0';
    buf[8] = 0;
    return v_string(buf);
}

// -- Hex / Base64 globals (auto-available, no load needed) ---------

static Value bi_hex_encode(int argc, Value *argv) {
    if (argc < 1 || argv[0].type != V_LIST || !argv[0].as.list) return v_string("");
    VList *bytes = argv[0].as.list;
    if (bytes->len == 0) return v_string("");
    char *buf = (char*)GC_MALLOC((size_t)bytes->len * 2 + 1);
    for (int i = 0; i < bytes->len; i++) {
        unsigned char b = (unsigned char)(bytes->items[i].as.i & 0xFF);
        const char hx[] = "0123456789abcdef";
        buf[i*2]   = hx[b >> 4];
        buf[i*2+1] = hx[b & 0xF];
    }
    buf[bytes->len * 2] = 0;
    Value v = v_string(buf);
    GC_FREE(buf);
    return v;
}

static Value bi_hex_decode(int argc, Value *argv) {
    if (argc < 1 || argv[0].type != V_STRING || !argv[0].as.s) return v_list();
    const char *s = argv[0].as.s;
    size_t slen = strlen(s);
    Value result = v_list();
    for (size_t i = 0; i + 1 < slen; i += 2) {
        char hi = s[i], lo = s[i+1];
        int hval = (hi >= '0' && hi <= '9') ? (hi - '0') :
                   (hi >= 'a' && hi <= 'f') ? (hi - 'a' + 10) :
                   (hi >= 'A' && hi <= 'F') ? (hi - 'A' + 10) : -1;
        int lval = (lo >= '0' && lo <= '9') ? (lo - '0') :
                   (lo >= 'a' && lo <= 'f') ? (lo - 'a' + 10) :
                   (lo >= 'A' && lo <= 'F') ? (lo - 'A' + 10) : -1;
        if (hval < 0 || lval < 0) break;
        v_list_push(result.as.list, v_int((int64_t)((hval << 4) | lval)));
    }
    return result;
}

static int b64_char_val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static Value bi_base64_encode(int argc, Value *argv) {
    if (argc < 1 || argv[0].type != V_LIST || !argv[0].as.list) return v_string("");
    VList *bytes = argv[0].as.list;
    if (bytes->len == 0) return v_string("");
    int out_len = ((bytes->len + 2) / 3) * 4;
    char *buf = (char*)GC_MALLOC((size_t)out_len + 1);
    const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int j = 0;
    for (int i = 0; i < bytes->len; i += 3) {
        unsigned char a = (unsigned char)(bytes->items[i].as.i & 0xFF);
        unsigned char b = (i+1 < bytes->len) ? (unsigned char)(bytes->items[i+1].as.i & 0xFF) : 0;
        unsigned char c = (i+2 < bytes->len) ? (unsigned char)(bytes->items[i+2].as.i & 0xFF) : 0;
        unsigned triple = ((unsigned)a << 16) | ((unsigned)b << 8) | c;
        buf[j++] = tbl[(triple >> 18) & 0x3F];
        buf[j++] = tbl[(triple >> 12) & 0x3F];
        buf[j++] = (i+1 < bytes->len) ? tbl[(triple >> 6) & 0x3F] : '=';
        buf[j++] = (i+2 < bytes->len) ? tbl[triple & 0x3F] : '=';
    }
    buf[j] = 0;
    Value v = v_string(buf);
    GC_FREE(buf);
    return v;
}

static Value bi_base64_decode(int argc, Value *argv) {
    if (argc < 1 || argv[0].type != V_STRING || !argv[0].as.s) return v_list();
    const char *s = argv[0].as.s;
    size_t slen = strlen(s);
    Value result = v_list();
    unsigned acc = 0; int bits = 0;
    for (size_t i = 0; i < slen; i++) {
        if (s[i] == '=') break;
        int v = b64_char_val(s[i]);
        if (v < 0) continue;
        acc = (acc << 6) | (unsigned)v;
        bits += 6;
        if (bits >= 8) { bits -= 8;
            v_list_push(result.as.list, v_int((int64_t)((acc >> bits) & 0xFF))); }
    }
    return result;
}

// -- Object builtins ------------------------------------------------

static Value bi_object_identical(int argc, Value *argv) {
    if (argc < 2) return v_bool(0);
    if (argv[0].type == V_OBJECT && argv[1].type == V_OBJECT)
        return v_bool(argv[0].as.obj == argv[1].as.obj);
    return v_bool(0);
}

static Value bi_object_hash(int argc, Value *argv) {
    if (argc < 1 || argv[0].type != V_OBJECT) return v_int(0);
    return v_int((int64_t)(intptr_t)argv[0].as.obj);
}

static Value bi_object_type_name(int argc, Value *argv) {
    if (argc < 1) return v_string("?");
    if (argv[0].type == V_OBJECT && argv[0].as.obj && argv[0].as.obj->class_node)
        return v_string(argv[0].as.obj->class_node->as.class_decl.name);
    return v_string(v_typename(argv[0].type));
}

// -- Register -------------------------------------------------------

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
    env_define(globals, "chr",    v_native(bi_chr));
    env_define(globals, "ord",    v_native(bi_ord));
    env_define(globals, "input",  v_native(bi_input));
    env_define(globals, "exit",   v_native(bi_exit));
    env_define(globals, "is",     v_native(bi_is));
    env_define(globals, "tuple",  v_native(bi_tuple));

    // Byte bit ops
    env_define(globals, "byteAnd",    v_native(bi_byte_and));
    env_define(globals, "byteOr",     v_native(bi_byte_or));
    env_define(globals, "byteXor",    v_native(bi_byte_xor));
    env_define(globals, "byteNot",    v_native(bi_byte_not));
    env_define(globals, "byteShl",    v_native(bi_byte_shl));
    env_define(globals, "byteShr",    v_native(bi_byte_shr));
    env_define(globals, "byteToHex",  v_native(bi_byte_to_hex));
    env_define(globals, "byteToBin",  v_native(bi_byte_to_bin));

    // Hex / Base64 globals
    env_define(globals, "hexEncode",    v_native(bi_hex_encode));
    env_define(globals, "hexDecode",    v_native(bi_hex_decode));
    env_define(globals, "base64Encode", v_native(bi_base64_encode));
    env_define(globals, "base64Decode", v_native(bi_base64_decode));

    // Object internals
    env_define(globals, "__object_identical",  v_native(bi_object_identical));
    env_define(globals, "__object_hash",       v_native(bi_object_hash));
    env_define(globals, "__object_type_name",  v_native(bi_object_type_name));

    // Builtin class Object
    {   AstNode *obj_cls = ast_new(NODE_CLASS_DECL, 0, 0);
        obj_cls->as.class_decl.name = strdup("Object");
        obj_cls->as.class_decl.parent = NULL;
        obj_cls->as.class_decl.is_final = 0;
        obj_cls->as.class_decl.is_static = 0;
        obj_cls->as.class_decl.is_ellipsis = 0;
        node_list_init(&obj_cls->as.class_decl.members);
        Value v; v.type = V_CLASS; v.as.cls = obj_cls;
        env_define(globals, "Object", v);
    }
}
