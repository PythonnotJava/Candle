// encoding.c ? std.encoding module
// Encoding constants (UTF8/ASCII/LATIN1) + encode/decode/hex/base64 functions
// Standalone compilation (separate from interpreter core to avoid TokenType conflicts)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// -- Local Value type (must match candle_runtime.h layout) --------------------
typedef enum { V_NULL, V_INT, V_DOUBLE, V_BOOL, V_STRING, V_LIST, V_TUPLE,
               V_MAP, V_FN, V_LAMBDA, V_CLASS, V_OBJECT, V_NATIVE } ValueType;
typedef struct Value Value;
typedef Value (*NativeFn)(int argc, Value *argv);
typedef struct { Value *items; int len, cap; } VList;
typedef struct { char **keys; Value *vals; int len, cap; } VMap;
struct Value {
    ValueType type;
    union { int64_t i; double d; int b; char *s; VList *list;
            VMap *map; void *fn; void *cls; void *obj; NativeFn native; } as;
};

static Value v_int(int64_t x)   { Value v; v.type = V_INT; v.as.i = x; return v; }
static Value v_string(const char *s) { Value v; v.type = V_STRING; v.as.s = (char*)s; return v; }
static Value v_null(void)       { Value v; v.type = V_NULL; return v; }
static Value v_bool(int x)      { Value v; v.type = V_BOOL; v.as.b = x ? 1 : 0; return v; }
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

static Value v_map(void) {
    Value v; v.type = V_MAP;
    VMap *m = (VMap*)calloc(1, sizeof(VMap));
    m->cap = 16;
    m->keys = (char**)calloc(m->cap, sizeof(char*));
    m->vals = (Value*)calloc(m->cap, sizeof(Value));
    v.as.map = m; return v;
}
static void v_map_set(VMap *m, const char *k, Value val) {
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

// -- UTF-8 helper -------------------------------------------------------------

// Decode one UTF-8 codepoint; returns bytes consumed (0 = error)
static int utf8_decode(const unsigned char *p, unsigned *cp) {
    if ((p[0] & 0x80) == 0) {
        *cp = p[0]; return 1;
    }
    if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
        *cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F); return 2;
    }
    if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        *cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); return 3;
    }
    if ((p[0] & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
        *cp = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F); return 4;
    }
    *cp = 0xFFFD; return 0; // invalid
}

// Encode a codepoint into UTF-8; returns bytes written
static int utf8_encode(unsigned cp, unsigned char out[4]) {
    if (cp < 0x80) { out[0] = (unsigned char)cp; return 1; }
    if (cp < 0x800) {
        out[0] = (unsigned char)(0xC0 | (cp >> 6));
        out[1] = (unsigned char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (unsigned char)(0xE0 | (cp >> 12));
        out[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (unsigned char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (unsigned char)(0xF0 | (cp >> 18));
    out[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (unsigned char)(0x80 | (cp & 0x3F));
    return 4;
}

// -- encode(string, encoding) -> list<int> -------------------------------------

static Value enc_encode(int argc, Value *argv) {
    if (argc < 2) return v_null();
    const char *str = (argc > 0 && argv[0].type == V_STRING) ? argv[0].as.s : "";
    if (!str) str = "";
    const char *enc = (argc > 1 && argv[1].type == V_STRING) ? argv[1].as.s : "UTF-8";
    if (!enc) enc = "UTF-8";

    Value result = v_list();
    VList *l = result.as.list;

    if (strcmp(enc, "UTF-8") == 0 || strcmp(enc, "utf-8") == 0 ||
        strcmp(enc, "UTF8") == 0  || strcmp(enc, "utf8") == 0) {
        // UTF-8: string is already UTF-8 bytes (char*)
        for (const unsigned char *p = (const unsigned char*)str; *p; p++)
            v_list_push(l, v_int((int64_t)*p));

    } else if (strcmp(enc, "ASCII") == 0 || strcmp(enc, "ascii") == 0) {
        // ASCII: decode UTF-8 codepoints, replace non-ASCII with '?'
        const unsigned char *p = (const unsigned char*)str;
        while (*p) {
            unsigned cp;
            int adv = utf8_decode(p, &cp);
            if (adv <= 0) { v_list_push(l, v_int('?')); p++; }
            else { v_list_push(l, v_int(cp < 128 ? (int64_t)cp : '?')); p += adv; }
        }

    } else if (strcmp(enc, "Latin-1") == 0 || strcmp(enc, "latin-1") == 0 ||
               strcmp(enc, "LATIN1") == 0 || strcmp(enc, "latin1") == 0 ||
               strcmp(enc, "ISO-8859-1") == 0 || strcmp(enc, "iso-8859-1") == 0) {
        // Latin-1: decode UTF-8, map to byte 0-255, '?' for >255
        const unsigned char *p = (const unsigned char*)str;
        while (*p) {
            unsigned cp;
            int adv = utf8_decode(p, &cp);
            if (adv <= 0) { v_list_push(l, v_int('?')); p++; }
            else { v_list_push(l, v_int(cp < 256 ? (int64_t)cp : '?')); p += adv; }
        }
    } else {
        // Unknown encoding -> treat as UTF-8
        for (const unsigned char *p = (const unsigned char*)str; *p; p++)
            v_list_push(l, v_int((int64_t)*p));
    }
    return result;
}

// -- decode(list<int>, encoding) -> string -------------------------------------

static Value enc_decode(int argc, Value *argv) {
    if (argc < 2) return v_string("");
    if (argv[0].type != V_LIST || !argv[0].as.list) return v_string("");

    VList *bytes = argv[0].as.list;
    const char *enc = (argv[1].type == V_STRING && argv[1].as.s) ? argv[1].as.s : "UTF-8";

    if (bytes->len == 0) return v_string("");

    if (strcmp(enc, "UTF-8") == 0 || strcmp(enc, "utf-8") == 0 ||
        strcmp(enc, "UTF8") == 0  || strcmp(enc, "utf8") == 0) {
        // UTF-8: bytes already represent UTF-8 codepoints
        char *buf = (char*)malloc((size_t)bytes->len + 1);
        for (int i = 0; i < bytes->len; i++) {
            int64_t b = bytes->items[i].as.i;
            buf[i] = (char)(unsigned char)(b & 0xFF);
        }
        buf[bytes->len] = 0;
        Value v = v_string(buf);
        return v;

    } else if (strcmp(enc, "ASCII") == 0 || strcmp(enc, "ascii") == 0) {
        // ASCII -> UTF-8 (sub-128 bytes are already valid UTF-8)
        char *buf = (char*)malloc((size_t)bytes->len + 1);
        int j = 0;
        for (int i = 0; i < bytes->len; i++) {
            unsigned char b = (unsigned char)(bytes->items[i].as.i & 0xFF);
            buf[j++] = (b < 128) ? (char)b : '?';
        }
        buf[j] = 0;
        Value v = v_string(buf);
        return v;

    } else if (strcmp(enc, "Latin-1") == 0 || strcmp(enc, "latin-1") == 0 ||
               strcmp(enc, "LATIN1") == 0 || strcmp(enc, "latin1") == 0 ||
               strcmp(enc, "ISO-8859-1") == 0 || strcmp(enc, "iso-8859-1") == 0) {
        // Latin-1 -> UTF-8: each byte 0-255 -> encoded UTF-8 sequence
        char *buf = (char*)malloc((size_t)bytes->len * 2 + 1);
        int j = 0;
        for (int i = 0; i < bytes->len; i++) {
            unsigned cp = (unsigned)(bytes->items[i].as.i & 0xFF);
            unsigned char out[4];
            int n = utf8_encode(cp, out);
            for (int k = 0; k < n; k++) buf[j++] = (char)out[k];
        }
        buf[j] = 0;
        Value v = v_string(buf);
        return v;
    }

    // Unknown encoding: passthrough as raw
    char *buf = (char*)malloc((size_t)bytes->len + 1);
    for (int i = 0; i < bytes->len; i++)
        buf[i] = (char)(unsigned char)(bytes->items[i].as.i & 0xFF);
    buf[bytes->len] = 0;
    Value v = v_string(buf);
    return v;
}

// -- hex_encode(list<int>) -> string -------------------------------------------

static Value enc_hex_encode(int argc, Value *argv) {
    if (argc < 1 || argv[0].type != V_LIST || !argv[0].as.list)
        return v_string("");
    VList *bytes = argv[0].as.list;
    if (bytes->len == 0) return v_string("");
    char *buf = (char*)malloc((size_t)bytes->len * 2 + 1);
    for (int i = 0; i < bytes->len; i++) {
        unsigned char b = (unsigned char)(bytes->items[i].as.i & 0xFF);
        static const char hex[] = "0123456789abcdef";
        buf[i*2]   = hex[b >> 4];
        buf[i*2+1] = hex[b & 0xF];
    }
    buf[bytes->len * 2] = 0;
    Value v = v_string(buf);
    return v;
}

// -- hex_decode(string) -> list<int> -------------------------------------------

static Value enc_hex_decode(int argc, Value *argv) {
    if (argc < 1 || argv[0].type != V_STRING || !argv[0].as.s)
        return v_list();
    const char *s = argv[0].as.s;
    size_t slen = strlen(s);
    Value result = v_list();
    VList *l = result.as.list;
    for (size_t i = 0; i + 1 < slen; i += 2) {
        char hi = s[i], lo = s[i+1];
        int hval = (hi >= '0' && hi <= '9') ? (hi - '0') :
                   (hi >= 'a' && hi <= 'f') ? (hi - 'a' + 10) :
                   (hi >= 'A' && hi <= 'F') ? (hi - 'A' + 10) : -1;
        int lval = (lo >= '0' && lo <= '9') ? (lo - '0') :
                   (lo >= 'a' && lo <= 'f') ? (lo - 'a' + 10) :
                   (lo >= 'A' && lo <= 'F') ? (lo - 'A' + 10) : -1;
        if (hval < 0 || lval < 0) break;
        v_list_push(l, v_int((int64_t)((hval << 4) | lval)));
    }
    return result;
}

// -- base64_encode(list<int>) -> string ----------------------------------------

static const char b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static Value enc_base64_encode(int argc, Value *argv) {
    if (argc < 1 || argv[0].type != V_LIST || !argv[0].as.list)
        return v_string("");
    VList *bytes = argv[0].as.list;
    if (bytes->len == 0) return v_string("");
    int out_len = ((bytes->len + 2) / 3) * 4;
    char *buf = (char*)malloc((size_t)out_len + 1);
    int j = 0;
    for (int i = 0; i < bytes->len; i += 3) {
        unsigned char a = (unsigned char)(bytes->items[i].as.i & 0xFF);
        unsigned char b = (i+1 < bytes->len) ? (unsigned char)(bytes->items[i+1].as.i & 0xFF) : 0;
        unsigned char c = (i+2 < bytes->len) ? (unsigned char)(bytes->items[i+2].as.i & 0xFF) : 0;
        unsigned triple = ((unsigned)a << 16) | ((unsigned)b << 8) | c;
        buf[j++] = b64_table[(triple >> 18) & 0x3F];
        buf[j++] = b64_table[(triple >> 12) & 0x3F];
        buf[j++] = (i+1 < bytes->len) ? b64_table[(triple >> 6) & 0x3F] : '=';
        buf[j++] = (i+2 < bytes->len) ? b64_table[triple & 0x3F] : '=';
    }
    buf[j] = 0;
    Value v = v_string(buf);
    return v;
}

// -- base64_decode(string) -> list<int> ----------------------------------------

static int b64_val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static Value enc_base64_decode(int argc, Value *argv) {
    if (argc < 1 || argv[0].type != V_STRING || !argv[0].as.s)
        return v_list();
    const char *s = argv[0].as.s;
    size_t slen = strlen(s);
    Value result = v_list();
    VList *l = result.as.list;
    unsigned acc = 0;
    int bits = 0;
    for (size_t i = 0; i < slen; i++) {
        if (s[i] == '=') break;
        int v = b64_val(s[i]);
        if (v < 0) continue;
        acc = (acc << 6) | (unsigned)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            v_list_push(l, v_int((int64_t)((acc >> bits) & 0xFF)));
        }
    }
    return result;
}

// -- build_encoding() ---------------------------------------------------------

Value build_encoding(void) {
    Value mod = v_map();
    VMap *m = mod.as.map;

    // Encoding name constants
    v_map_set(m, "UTF8",   v_string("UTF-8"));
    v_map_set(m, "ASCII",  v_string("ASCII"));
    v_map_set(m, "LATIN1", v_string("Latin-1"));

    // Encode / decode functions
    v_map_set(m, "encode",       v_native(enc_encode));
    v_map_set(m, "decode",       v_native(enc_decode));
    v_map_set(m, "hexEncode",    v_native(enc_hex_encode));
    v_map_set(m, "hexDecode",    v_native(enc_hex_decode));
    v_map_set(m, "base64Encode", v_native(enc_base64_encode));
    v_map_set(m, "base64Decode", v_native(enc_base64_decode));

    return mod;
}
