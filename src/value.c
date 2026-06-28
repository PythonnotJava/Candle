#include "value.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Value v_null(void) { Value v = {0}; v.type = V_NULL; return v; }
Value v_int(int64_t x) { Value v; v.type = V_INT; v.as.i = x; return v; }
Value v_double(double x) { Value v; v.type = V_DOUBLE; v.as.d = x; return v; }
Value v_bool(int x) { Value v; v.type = V_BOOL; v.as.b = x ? 1 : 0; return v; }
Value v_string(const char *s) {
    Value v; v.type = V_STRING;
    v.as.s = GC_STRDUP(s ? s : "");
    return v;
}
Value v_list(void) {
    Value v; v.type = V_LIST;
    VList *l = GC_CALLOC(1, sizeof(VList));
    l->cap = 8;
    l->items = GC_CALLOC(l->cap, sizeof(Value));
    v.as.list = l;
    return v;
}
Value v_tuple(void) {
    Value v = v_list();
    v.type = V_TUPLE;   // ?? VList ???????????? + ??? (..)?
    return v;
}
Value v_map(void) {
    Value v; v.type = V_MAP;
    VMap *m = GC_CALLOC(1, sizeof(VMap));
    m->cap = 8;
    m->keys = GC_CALLOC(m->cap, sizeof(char*));
    m->vals = GC_CALLOC(m->cap, sizeof(Value));
    v.as.map = m;
    return v;
}
Value v_native(NativeFn fn) { Value v; v.type = V_NATIVE; v.as.native = fn; return v; }

const char *v_typename(ValueType t) {
    switch (t) {
        case V_NULL: return "null";
        case V_INT: return "int";
        case V_DOUBLE: return "double";
        case V_BOOL: return "bool";
        case V_STRING: return "string";
        case V_LIST: return "list";
        case V_TUPLE: return "tuple";
        case V_MAP: return "map";
        case V_FN: return "function";
        case V_LAMBDA: return "lambda";
        case V_CLASS: return "class";
        case V_OBJECT: return "object";
        case V_NATIVE: return "native";
    }
    return "?";
}

int v_truthy(Value v) {
    switch (v.type) {
        case V_NULL: return 0;
        case V_BOOL: return v.as.b;
        case V_INT: return v.as.i != 0;
        case V_DOUBLE: return v.as.d != 0.0;
        case V_STRING: return v.as.s && v.as.s[0];
        case V_LIST: return v.as.list && v.as.list->len > 0;
        case V_TUPLE: return v.as.list && v.as.list->len > 0;
        case V_MAP: return v.as.map && v.as.map->len > 0;
        default: return 1;
    }
}

int v_equals(Value a, Value b) {
    if (a.type != b.type) {
        if ((a.type == V_INT && b.type == V_DOUBLE) ||
            (a.type == V_DOUBLE && b.type == V_INT)) {
            double da = a.type == V_INT ? (double)a.as.i : a.as.d;
            double db = b.type == V_INT ? (double)b.as.i : b.as.d;
            return da == db;
        }
        // type() ??? vs ????(map ? __type__): ????????
        // ???????? __type__=="Function" ?? type() ? "lambda"/"function"?
        {
            Value str_v = (a.type == V_STRING) ? a : (b.type == V_STRING ? b : a);
            Value map_v = (a.type == V_MAP) ? a : (b.type == V_MAP ? b : a);
            if (str_v.type == V_STRING && map_v.type == V_MAP && map_v.as.map) {
                Value th = v_map_get(map_v.as.map, "__type__");
                if (th.type == V_STRING && th.as.s && str_v.as.s) {
                    const char *tn = th.as.s, *sn = str_v.as.s;
                    if (strcmp(tn, sn) == 0) return 1;
                    if (strcmp(tn, "Function") == 0 &&
                        (strcmp(sn, "lambda") == 0 || strcmp(sn, "function") == 0))
                        return 1;
                }
            }
        }
        return 0;
    }
    switch (a.type) {
        case V_NULL: return 1;
        case V_INT: return a.as.i == b.as.i;
        case V_DOUBLE: return a.as.d == b.as.d;
        case V_BOOL: return a.as.b == b.as.b;
        case V_STRING:
            if (a.as.s == b.as.s) return 1;
            if (!a.as.s || !b.as.s) return 0;
            return strcmp(a.as.s, b.as.s) == 0;
        case V_TUPLE:
        case V_LIST: {
            VList *la = a.as.list, *lb = b.as.list;
            if (!la || !lb) return la == lb;
            if (la->len != lb->len) return 0;
            for (int i = 0; i < la->len; i++)
                if (!v_equals(la->items[i], lb->items[i])) return 0;
            return 1;
        }
        default: return 0;
    }
}

char *v_to_string(Value v) {
    char buf[64];
    switch (v.type) {
        case V_NULL: return GC_STRDUP("null");
        case V_INT: snprintf(buf, sizeof(buf), "%lld", (long long)v.as.i); return GC_STRDUP(buf);
        case V_DOUBLE: snprintf(buf, sizeof(buf), "%g", v.as.d); return GC_STRDUP(buf);
        case V_BOOL: return GC_STRDUP(v.as.b ? "true" : "false");
        case V_STRING: return GC_STRDUP(v.as.s ? v.as.s : "");
        case V_LIST: {
            if (!v.as.list) return GC_STRDUP("[]");
            size_t cap = 64; char *r = GC_MALLOC(cap); r[0] = '['; r[1] = 0; size_t len = 1;
            for (int i = 0; i < v.as.list->len; i++) {
                char *part = v_to_string(v.as.list->items[i]);
                size_t need = len + strlen(part) + 4;
                if (need > cap) { cap = need * 2; r = GC_REALLOC(r, cap); }
                if (i > 0) { strcat(r, ", "); len += 2; }
                strcat(r, part); len += strlen(part);
                GC_FREE(part);
            }
            strcat(r, "]");
            return r;
        }
        case V_TUPLE: {
            if (!v.as.list) return GC_STRDUP("()");
            size_t cap = 64; char *r = GC_MALLOC(cap); r[0] = '('; r[1] = 0; size_t len = 1;
            for (int i = 0; i < v.as.list->len; i++) {
                char *part = v_to_string(v.as.list->items[i]);
                size_t need = len + strlen(part) + 4;
                if (need > cap) { cap = need * 2; r = GC_REALLOC(r, cap); }
                if (i > 0) { strcat(r, ", "); len += 2; }
                strcat(r, part); len += strlen(part);
                GC_FREE(part);
            }
            strcat(r, ")");
            return r;
        }
        case V_MAP: {
            VMap *m = v.as.map;
            if (!m || m->len == 0) return GC_STRDUP("{}");
            // ?????? type/message ????? "type: message"
            int ti = -1, mi = -1;
            for (int i = 0; i < m->len; i++) {
                if (strcmp(m->keys[i], "type") == 0) ti = i;
                else if (strcmp(m->keys[i], "message") == 0) mi = i;
            }
            if (ti >= 0 && mi >= 0 && m->len == 2) {
                char *tp = v_to_string(m->vals[ti]);
                char *ms = v_to_string(m->vals[mi]);
                size_t n = strlen(tp) + strlen(ms) + 4;
                char *r = GC_MALLOC(n);
                snprintf(r, n, "%s: %s", tp, ms);
                GC_FREE(tp); GC_FREE(ms);
                return r;
            }
            // ?? map?{k: v, k2: v2}
            size_t cap = 64; char *r = GC_MALLOC(cap); r[0] = '{'; r[1] = 0; size_t len = 1;
            for (int i = 0; i < m->len; i++) {
                char *val = v_to_string(m->vals[i]);
                size_t need = len + strlen(m->keys[i]) + strlen(val) + 8;
                if (need > cap) { cap = need * 2; r = GC_REALLOC(r, cap); }
                if (i > 0) { strcat(r, ", "); len += 2; }
                strcat(r, m->keys[i]); len += strlen(m->keys[i]);
                strcat(r, ": "); len += 2;
                strcat(r, val); len += strlen(val);
                GC_FREE(val);
            }
            strcat(r, "}");
            return r;
        }
        case V_FN: case V_LAMBDA: return GC_STRDUP("<function>");
        case V_CLASS: return GC_STRDUP("<class>");
        case V_OBJECT: return GC_STRDUP("<object>");
        case V_NATIVE: return GC_STRDUP("<native>");
    }
    return GC_STRDUP("?");
}

void v_list_push(VList *l, Value v) {
    if (l->len >= l->cap) {
        l->cap = l->cap ? l->cap * 2 : 8;
        l->items = GC_REALLOC(l->items, sizeof(Value) * l->cap);
    }
    l->items[l->len++] = v;
}

Value v_list_get(VList *l, int i) {
    if (!l || i < 0 || i >= l->len) return v_null();
    return l->items[i];
}

void v_map_set(VMap *m, const char *k, Value v) {
    for (int i = 0; i < m->len; i++) {
        if (strcmp(m->keys[i], k) == 0) { m->vals[i] = v; return; }
    }
    if (m->len >= m->cap) {
        m->cap = m->cap ? m->cap * 2 : 8;
        m->keys = GC_REALLOC(m->keys, sizeof(char*) * m->cap);
        m->vals = GC_REALLOC(m->vals, sizeof(Value) * m->cap);
    }
    m->keys[m->len] = GC_STRDUP(k);
    m->vals[m->len] = v;
    m->len++;
}

Value v_map_get(VMap *m, const char *k) {
    if (!m) return v_null();
    for (int i = 0; i < m->len; i++) {
        if (strcmp(m->keys[i], k) == 0) return m->vals[i];
    }
    return v_null();
}
