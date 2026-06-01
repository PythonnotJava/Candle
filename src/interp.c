#include "interp.h"
#include "token.h"
#include "parser.h"
#include "util.h"
#include "ast.h"
#include "builtins/builtins.h"
#include "builtins/modules.h"
#include "builtins/ffi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <math.h>
#include <ctype.h>

// ── Env ──────────────────────────────────────────────────────────────────────

Env *env_new(Env *parent) {
    Env *e = calloc(1, sizeof(Env));
    e->cap = 8;
    e->names = calloc(e->cap, sizeof(char*));
    e->vals = calloc(e->cap, sizeof(Value));
    e->parent = parent;
    return e;
}

void env_free(Env *e) {
    if (!e) return;
    for (int i = 0; i < e->len; i++) free(e->names[i]);
    free(e->names); free(e->vals); free(e);
}

void env_define(Env *e, const char *name, Value v) {
    for (int i = 0; i < e->len; i++) {
        if (strcmp(e->names[i], name) == 0) { e->vals[i] = v; return; }
    }
    if (e->len >= e->cap) {
        e->cap *= 2;
        e->names = realloc(e->names, sizeof(char*) * e->cap);
        e->vals = realloc(e->vals, sizeof(Value) * e->cap);
    }
    e->names[e->len] = strdup(name);
    e->vals[e->len] = v;
    e->len++;
}

Value *env_lookup(Env *e, const char *name) {
    for (Env *cur = e; cur; cur = cur->parent) {
        for (int i = 0; i < cur->len; i++) {
            if (strcmp(cur->names[i], name) == 0) return &cur->vals[i];
        }
    }
    return NULL;
}

int env_assign(Env *e, const char *name, Value v) {
    for (Env *cur = e; cur; cur = cur->parent) {
        for (int i = 0; i < cur->len; i++) {
            if (strcmp(cur->names[i], name) == 0) { cur->vals[i] = v; return 1; }
        }
    }
    return 0;
}

// ── Interpreter state ────────────────────────────────────────────────────────

typedef struct {
    const char *filename;
    int had_error;
    jmp_buf return_jmp;
    int return_active;
    Value return_value;
    int break_active;
    int exc_active;
    Value exc_value;
} Interp;

static Value eval(Interp *I, Env *env, AstNode *node);
static void exec_block(Interp *I, Env *env, NodeList *stmts);
// 加载一个 .candle 源码模块，返回其导出符号组成的命名空间（V_MAP），失败返回 V_NULL
static Value load_candle_module(Interp *I, const char *dotted_path);

// ── Class helpers ────────────────────────────────────────────────────────────

static Env *g_class_env = NULL; // for parent class lookup

static AstNode *find_class(const char *name) {
    if (!g_class_env || !name) return NULL;
    Value *v = env_lookup(g_class_env, name);
    if (v && v->type == V_CLASS) return v->as.cls;
    return NULL;
}

// ── Reflect: 扩展方法表 (target type name → method decl) ──────────────────────
typedef struct {
    char *target;       // 目标类型名："Vector" / "int" / "string" ...
    AstNode *method;    // NODE_METHOD_DECL
} ReflectEntry;

static ReflectEntry g_reflect[512];
static int g_nreflect = 0;

// ── 静态字段存储 (key = "Class.field") ───────────────────────────────────────
typedef struct { char *key; Value val; } StaticEntry;
static StaticEntry g_statics[512];
static int g_nstatics = 0;

static Value *static_slot(const char *cls, const char *field) {
    char key[256];
    snprintf(key, sizeof(key), "%s.%s", cls, field);
    for (int i = 0; i < g_nstatics; i++)
        if (strcmp(g_statics[i].key, key) == 0) return &g_statics[i].val;
    return NULL;
}

static Value *static_define(const char *cls, const char *field, Value v) {
    Value *exist = static_slot(cls, field);
    if (exist) { *exist = v; return exist; }
    if (g_nstatics >= 512) return NULL;
    char key[256];
    snprintf(key, sizeof(key), "%s.%s", cls, field);
    g_statics[g_nstatics].key = strdup(key);
    g_statics[g_nstatics].val = v;
    return &g_statics[g_nstatics++].val;
}

// ── alias 类型锁定 ────────────────────────────────────────────────────────────
// `alias x = expr` 会推断并锁定 x 的类型，后续给 x 赋不兼容类型的值时抛错。
typedef struct { char *name; ValueType locked; } AliasLock;
static AliasLock g_alias_locks[256];
static int g_nalias_locks = 0;

static void alias_lock_register(const char *name, ValueType t) {
    for (int i = 0; i < g_nalias_locks; i++)
        if (strcmp(g_alias_locks[i].name, name) == 0) { g_alias_locks[i].locked = t; return; }
    if (g_nalias_locks >= 256) return;
    g_alias_locks[g_nalias_locks].name = strdup(name);
    g_alias_locks[g_nalias_locks].locked = t;
    g_nalias_locks++;
}

// 返回锁定类型；未锁定返回 -1
static int alias_lock_type(const char *name) {
    for (int i = 0; i < g_nalias_locks; i++)
        if (strcmp(g_alias_locks[i].name, name) == 0) return (int)g_alias_locks[i].locked;
    return -1;
}

// int/double 视为数值，互相兼容；其余须类型完全一致
static int alias_type_compatible(ValueType locked, ValueType actual) {
    if (locked == actual) return 1;
    if ((locked == V_INT || locked == V_DOUBLE) &&
        (actual == V_INT || actual == V_DOUBLE)) return 1;
    return 0;
}

static void reflect_register(const char *target, AstNode *method) {
    if (g_nreflect >= 512) return;
    g_reflect[g_nreflect].target = strdup(target);
    g_reflect[g_nreflect].method = method;
    g_nreflect++;
}

// 从当前执行环境定位一个静态字段槽：优先 self 的类，再 __class__
static Value *static_in_env(Env *env, const char *field) {
    Value *self = env_lookup(env, "self");
    if (self && self->type == V_OBJECT) {
        Value *st = static_slot(self->as.obj->class_node->as.class_decl.name, field);
        if (st) return st;
    }
    Value *cls = env_lookup(env, "__class__");
    if (cls && cls->type == V_STRING) {
        Value *st = static_slot(cls->as.s, field);
        if (st) return st;
    }
    return NULL;
}

static AstNode *reflect_lookup(const char *target, const char *name) {
    if (!target || !name) return NULL;
    for (int i = 0; i < g_nreflect; i++)
        if (strcmp(g_reflect[i].target, target) == 0 &&
            strcmp(g_reflect[i].method->as.func.name, name) == 0)
            return g_reflect[i].method;
    return NULL;
}

// 把一个 Value 的运行时类型映射成 reflect 目标名（用于内置类型扩展）
static const char *reflect_type_of(Value v) {
    switch (v.type) {
        case V_INT:    return "int";
        case V_DOUBLE: return "double";
        case V_BOOL:   return "bool";
        case V_STRING: return "string";
        case V_LIST:   return "list";
        case V_MAP:    return "map";
        case V_OBJECT: return v.as.obj->class_node->as.class_decl.name;
        default:       return NULL;
    }
}

static AstNode *find_method(AstNode *cls, const char *name) {
    while (cls) {
        for (int i = 0; i < cls->as.class_decl.members.count; i++) {
            AstNode *m = cls->as.class_decl.members.items[i];
            if (m->type == NODE_METHOD_DECL && strcmp(m->as.func.name, name) == 0)
                return m;
            if (m->type == NODE_FACTORY_DECL && strcmp(m->as.factory.name, name) == 0)
                return m;
        }
        cls = find_class(cls->as.class_decl.parent);
    }
    return NULL;
}

static AstNode *find_constructor(AstNode *cls) {
    for (int i = 0; i < cls->as.class_decl.members.count; i++) {
        AstNode *m = cls->as.class_decl.members.items[i];
        if (m->type == NODE_CONSTRUCTOR) return m;
    }
    return NULL;
}

static int class_has_field(AstNode *cls, const char *name) {
    while (cls) {
        for (int i = 0; i < cls->as.class_decl.members.count; i++) {
            AstNode *m = cls->as.class_decl.members.items[i];
            if (m->type == NODE_FIELD_DECL && !m->as.var.is_static &&
                strcmp(m->as.var.name, name) == 0)
                return 1;
        }
        cls = find_class(cls->as.class_decl.parent);
    }
    return 0;
}

static void runtime_error(Interp *I, AstNode *node, const char *msg) {
    fprintf(stderr, "%s:%d: runtime error: %s\n",
            I->filename, node ? node->line : 0, msg);
    I->had_error = 1;
}

// ── String literal decode (strip quotes + handle escapes) ───────────────────

static char *decode_str_lit(const char *raw) {
    if (!raw) return strdup("");
    size_t n = strlen(raw);
    int prefix = 0;
    if (n > 0 && (raw[0] == 'f' || raw[0] == 'r')) prefix = 1;
    int is_raw = prefix && raw[0] == 'r';
    if (n - prefix < 2) return strdup("");
    char q = raw[prefix];
    if (q != '\'' && q != '"') return strdup(raw);
    const char *p = raw + prefix + 1;
    size_t len = n - prefix - 2;
    char *out = malloc(len + 1);
    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        char c = p[i];
        if (!is_raw && c == '\\' && i + 1 < len) {
            char nx = p[++i];
            switch (nx) {
                case 'n': out[o++] = '\n'; break;
                case 't': out[o++] = '\t'; break;
                case 'r': out[o++] = '\r'; break;
                case '\\': out[o++] = '\\'; break;
                case '\'': out[o++] = '\''; break;
                case '"': out[o++] = '"'; break;
                case '0': out[o++] = '\0'; break;
                default: out[o++] = nx; break;
            }
        } else {
            out[o++] = c;
        }
    }
    out[o] = 0;
    return out;
}

// ── f-string interpolation: replace {Expression} with value ─────────────────

static char *fstring_interpolate(Interp *I, Env *env, const char *tpl, AstNode *node);

static char *fstring_interpolate(Interp *I, Env *env, const char *tpl, AstNode *node) {
    (void)node;
    size_t cap = strlen(tpl) + 32;
    char *out = malloc(cap);
    size_t len = 0;
    for (size_t i = 0; tpl[i]; i++) {
        if (tpl[i] == '{') {
            // find matching '}' (balance braces inside)
            size_t j = i + 1;
            int depth = 1;
            while (tpl[j] && depth > 0) {
                if (tpl[j] == '{') depth++;
                else if (tpl[j] == '}') { depth--; if (depth == 0) break; }
                j++;
            }
            if (!tpl[j]) {
                if (len + 1 >= cap) { cap *= 2; out = realloc(out, cap); }
                out[len++] = tpl[i]; continue;
            }
            // extract sub-expression text between { and }
            size_t elen = j - i - 1;
            char *expr_src = malloc(elen + 4);
            memcpy(expr_src, tpl + i + 1, elen);
            expr_src[elen] = ';';
            expr_src[elen + 1] = 0;
            // try parse as a small program
            Value v = v_null();
            AstNode *sub = parse(expr_src, "<fstring>");
            if (sub && sub->type == NODE_PROGRAM && sub->as.program.stmts.count > 0) {
                AstNode *st = sub->as.program.stmts.items[0];
                AstNode *e = NULL;
                if (st->type == NODE_EXPR_STMT && st->as.program.stmts.count > 0)
                    e = st->as.program.stmts.items[0];
                if (e) v = eval(I, env, e);
            }
            if (sub) ast_free(sub);
            free(expr_src);
            char *s = v_to_string(v);
            size_t sl = strlen(s);
            if (len + sl + 1 >= cap) { cap = (len + sl + 1) * 2; out = realloc(out, cap); }
            memcpy(out + len, s, sl); len += sl;
            free(s);
            i = j;
        } else {
            if (len + 1 >= cap) { cap *= 2; out = realloc(out, cap); }
            out[len++] = tpl[i];
        }
    }
    out[len] = 0;
    return out;
}

// ── List built-in methods ────────────────────────────────────────────────────

// 调用一个 reflect 注入的方法：把 receiver 绑定到 self，执行方法体
static Value reflect_invoke(Interp *I, Env *env, AstNode *mth, Value receiver, int argc, Value *argv) {
    Env *call_env = env_new(env);
    env_define(call_env, "self", receiver);
    NodeList *params = &mth->as.func.params;
    int n = params->count < argc ? params->count : argc;
    for (int i = 0; i < n; i++)
        env_define(call_env, params->items[i]->as.var.name, argv[i]);
    Value result = v_null();
    if (mth->as.func.body) {
        exec_block(I, call_env, &mth->as.func.body->as.program.stmts);
        if (I->return_active) { result = I->return_value; I->return_active = 0; }
    }
    return result;
}

static Value call_func_value(Interp *I, Value fn, int argc, Value *argv) {
    if (fn.type == V_NATIVE) return fn.as.native(argc, argv);
    if (fn.type != V_FN && fn.type != V_LAMBDA) return v_null();
    AstNode *decl = fn.as.fn->decl;
    NodeList *params;
    AstNode *body;
    if (decl->type == NODE_LAMBDA) {
        params = &decl->as.lambda.params; body = decl->as.lambda.body;
    } else if (decl->type == NODE_FACTORY_DECL) {
        params = &decl->as.factory.params; body = decl->as.factory.body;
    } else {
        params = &decl->as.func.params; body = decl->as.func.body;
    }
    Env *call_env = env_new(fn.as.fn->closure);
    int n = params->count < argc ? params->count : argc;
    for (int i = 0; i < n; i++) {
        AstNode *p = params->items[i];
        env_define(call_env, p->as.var.name, argv[i]);
    }
    Value result = v_null();
    if (body) {
        if (body->type == NODE_BLOCK)
            exec_block(I, call_env, &body->as.program.stmts);
        else
            result = eval(I, call_env, body);
        if (I->return_active) { result = I->return_value; I->return_active = 0; }
    }
    return result;
}

// ── 类型句柄（type handle）──────────────────────────────────────────────────
// 类型别名（alias Int64 = int）在运行时绑定为一个含静态方法的命名空间(V_MAP)，
// 复用 map 成员访问 + native 调用机制。支持 int/double/bool/string 的静态构造。

// int.parse / Int64.parser："435" → 435（支持 bool/double 入参的宽松转换）
static Value th_int_parse(int argc, Value *argv) {
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
static Value th_double_parse(int argc, Value *argv) {
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
static Value th_bool_parse(int argc, Value *argv) {
    if (argc < 1) return v_bool(0);
    Value v = argv[0];
    if (v.type == V_STRING && v.as.s)
        return v_bool(strcmp(v.as.s, "true") == 0 || strcmp(v.as.s, "1") == 0);
    return v_bool(v_truthy(v));
}
static Value th_string_parse(int argc, Value *argv) {
    if (argc < 1) return v_string("");
    char *s = v_to_string(argv[0]);
    Value r = v_string(s);
    free(s);
    return r;
}

// 构造类型句柄：返回带静态方法 + __type__ 标记的命名空间。
// __type__ 存底层类型名，便于 type() 比较与文档展示。
static Value make_type_handle(const char *type_name) {
    Value h = v_map();
    VMap *m = h.as.map;
    v_map_set(m, "__type__", v_string(type_name));
    NativeFn parse = NULL;
    if (strcmp(type_name, "int") == 0)         parse = th_int_parse;
    else if (strcmp(type_name, "double") == 0) parse = th_double_parse;
    else if (strcmp(type_name, "bool") == 0)   parse = th_bool_parse;
    else if (strcmp(type_name, "string") == 0) parse = th_string_parse;
    if (parse) {
        v_map_set(m, "parse",  v_native(parse));   // Dart 风格 int.parse(...)
        v_map_set(m, "parser", v_native(parse));   // 示例使用的 parser 别名
    }
    return h;
}

static Value list_method(Interp *I, Value lst, const char *name, int argc, Value *argv) {
    VList *l = lst.as.list;
    if (!l) return v_null();

    // ── 查询 ──
    if (strcmp(name, "size") == 0 || strcmp(name, "length") == 0)
        return v_int(l->len);
    if (strcmp(name, "isEmpty") == 0)    return v_bool(l->len == 0);
    if (strcmp(name, "isNotEmpty") == 0) return v_bool(l->len != 0);
    if (strcmp(name, "first") == 0)      return l->len ? l->items[0] : v_null();
    if (strcmp(name, "last") == 0)       return l->len ? l->items[l->len-1] : v_null();
    if (strcmp(name, "get") == 0 || strcmp(name, "at") == 0 ||
        strcmp(name, "elementAt") == 0) {
        int i = argc > 0 && argv[0].type == V_INT ? (int)argv[0].as.i : 0;
        if (i < 0) i += l->len;
        return v_list_get(l, i);
    }
    if (strcmp(name, "contains") == 0) {
        if (argc < 1) return v_bool(0);
        for (int i = 0; i < l->len; i++)
            if (v_equals(l->items[i], argv[0])) return v_bool(1);
        return v_bool(0);
    }
    if (strcmp(name, "indexOf") == 0) {
        if (argc < 1) return v_int(-1);
        for (int i = 0; i < l->len; i++)
            if (v_equals(l->items[i], argv[0])) return v_int(i);
        return v_int(-1);
    }

    // ── 修改 ──
    if (strcmp(name, "add") == 0 || strcmp(name, "push") == 0 || strcmp(name, "append") == 0) {
        for (int i = 0; i < argc; i++) v_list_push(l, argv[i]);
        return lst;
    }
    if (strcmp(name, "addAll") == 0) {
        if (argc >= 1 && argv[0].type == V_LIST && argv[0].as.list) {
            VList *o = argv[0].as.list;
            for (int i = 0; i < o->len; i++) v_list_push(l, o->items[i]);
        }
        return lst;
    }
    if (strcmp(name, "pop") == 0 || strcmp(name, "removeLast") == 0) {
        if (l->len == 0) return v_null();
        return l->items[--l->len];
    }
    if (strcmp(name, "removeAt") == 0) {
        int i = argc > 0 && argv[0].type == V_INT ? (int)argv[0].as.i : 0;
        if (i < 0 || i >= l->len) return v_null();
        Value removed = l->items[i];
        for (int k = i; k < l->len - 1; k++) l->items[k] = l->items[k+1];
        l->len--;
        return removed;
    }
    if (strcmp(name, "insert") == 0) {
        if (argc < 2 || argv[0].type != V_INT) return lst;
        int idx = (int)argv[0].as.i;
        if (idx < 0) idx = 0; if (idx > l->len) idx = l->len;
        v_list_push(l, v_null());  // 扩容一格
        for (int k = l->len - 1; k > idx; k--) l->items[k] = l->items[k-1];
        l->items[idx] = argv[1];
        return lst;
    }
    if (strcmp(name, "clear") == 0) { l->len = 0; return lst; }
    if (strcmp(name, "reverse") == 0 || strcmp(name, "reversed") == 0) {
        Value out = v_list();
        for (int i = l->len - 1; i >= 0; i--) v_list_push(out.as.list, l->items[i]);
        return out;
    }
    if (strcmp(name, "sublist") == 0) {
        int start = argc > 0 && argv[0].type == V_INT ? (int)argv[0].as.i : 0;
        int end   = argc > 1 && argv[1].type == V_INT ? (int)argv[1].as.i : l->len;
        if (start < 0) start = 0; if (end > l->len) end = l->len;
        Value out = v_list();
        for (int i = start; i < end; i++) v_list_push(out.as.list, l->items[i]);
        return out;
    }
    if (strcmp(name, "join") == 0) {
        const char *sep = (argc >= 1 && argv[0].type == V_STRING) ? argv[0].as.s : "";
        size_t cap = 32, len = 0; char *buf = malloc(cap); buf[0] = 0;
        for (int i = 0; i < l->len; i++) {
            char *part = v_to_string(l->items[i]);
            size_t need = len + strlen(part) + strlen(sep) + 1;
            if (need > cap) { cap = need * 2; buf = realloc(buf, cap); }
            if (i > 0) { strcat(buf, sep); len += strlen(sep); }
            strcat(buf, part); len += strlen(part);
            free(part);
        }
        Value r = v_string(buf); free(buf); return r;
    }

    // ── 高阶函数 ──
    if (strcmp(name, "map") == 0 || strcmp(name, "mapList") == 0) {
        if (argc < 1) return v_null();
        Value out = v_list();
        for (int i = 0; i < l->len; i++)
            v_list_push(out.as.list, call_func_value(I, argv[0], 1, &l->items[i]));
        return out;
    }
    if (strcmp(name, "where") == 0 || strcmp(name, "filter") == 0) {
        if (argc < 1) return v_null();
        Value out = v_list();
        for (int i = 0; i < l->len; i++)
            if (v_truthy(call_func_value(I, argv[0], 1, &l->items[i])))
                v_list_push(out.as.list, l->items[i]);
        return out;
    }
    if (strcmp(name, "reduce") == 0) {
        if (argc < 1 || l->len == 0) return v_null();
        Value acc = l->items[0];
        for (int i = 1; i < l->len; i++) {
            Value pair[2] = { acc, l->items[i] };
            acc = call_func_value(I, argv[0], 2, pair);
        }
        return acc;
    }
    if (strcmp(name, "fold") == 0) {
        if (argc < 2) return v_null();
        Value acc = argv[0];
        for (int i = 0; i < l->len; i++) {
            Value pair[2] = { acc, l->items[i] };
            acc = call_func_value(I, argv[1], 2, pair);
        }
        return acc;
    }
    if (strcmp(name, "forEach") == 0) {
        if (argc < 1) return v_null();
        for (int i = 0; i < l->len; i++)
            call_func_value(I, argv[0], 1, &l->items[i]);
        return v_null();
    }
    if (strcmp(name, "any") == 0) {
        if (argc < 1) return v_bool(0);
        for (int i = 0; i < l->len; i++)
            if (v_truthy(call_func_value(I, argv[0], 1, &l->items[i]))) return v_bool(1);
        return v_bool(0);
    }
    if (strcmp(name, "every") == 0) {
        if (argc < 1) return v_bool(1);
        for (int i = 0; i < l->len; i++)
            if (!v_truthy(call_func_value(I, argv[0], 1, &l->items[i]))) return v_bool(0);
        return v_bool(1);
    }
    // ── 数值聚合 ──
    if (strcmp(name, "sum") == 0) {
        int is_d = 0; double ds = 0; int64_t is = 0;
        for (int i = 0; i < l->len; i++) {
            Value e = l->items[i];
            if (e.type == V_DOUBLE) { is_d = 1; ds += e.as.d; }
            else if (e.type == V_INT) { is += e.as.i; ds += (double)e.as.i; }
        }
        return is_d ? v_double(ds) : v_int(is);
    }
    if (strcmp(name, "product") == 0) {
        int is_d = 0; double dp = 1; int64_t ip = 1;
        for (int i = 0; i < l->len; i++) {
            Value e = l->items[i];
            if (e.type == V_DOUBLE) { is_d = 1; dp *= e.as.d; }
            else if (e.type == V_INT) { ip *= e.as.i; dp *= (double)e.as.i; }
        }
        return is_d ? v_double(dp) : v_int(ip);
    }
    if (strcmp(name, "maxValue") == 0 || strcmp(name, "minValue") == 0) {
        if (l->len == 0) return v_null();
        int want_max = (name[1] == 'a');  // "max" vs "min"
        Value best = l->items[0];
        double bd = best.type == V_DOUBLE ? best.as.d : (double)best.as.i;
        for (int i = 1; i < l->len; i++) {
            Value e = l->items[i];
            double ed = e.type == V_DOUBLE ? e.as.d : (double)e.as.i;
            if ((want_max && ed > bd) || (!want_max && ed < bd)) { best = e; bd = ed; }
        }
        return best;
    }
    // ── 变换（返回新列表）──
    if (strcmp(name, "sorted") == 0) {
        Value out = v_list();
        for (int i = 0; i < l->len; i++) v_list_push(out.as.list, l->items[i]);
        VList *o = out.as.list;
        // 插入排序（数值比较）
        for (int i = 1; i < o->len; i++) {
            Value key = o->items[i];
            double kd = key.type == V_DOUBLE ? key.as.d : (double)key.as.i;
            int j = i - 1;
            while (j >= 0) {
                double jd = o->items[j].type == V_DOUBLE ? o->items[j].as.d : (double)o->items[j].as.i;
                if (jd <= kd) break;
                o->items[j+1] = o->items[j];
                j--;
            }
            o->items[j+1] = key;
        }
        return out;
    }
    if (strcmp(name, "distinct") == 0) {
        Value out = v_list();
        for (int i = 0; i < l->len; i++) {
            int dup = 0;
            for (int k = 0; k < out.as.list->len; k++)
                if (v_equals(out.as.list->items[k], l->items[i])) { dup = 1; break; }
            if (!dup) v_list_push(out.as.list, l->items[i]);
        }
        return out;
    }
    if (strcmp(name, "take") == 0) {
        int n = argc > 0 && argv[0].type == V_INT ? (int)argv[0].as.i : 0;
        Value out = v_list();
        for (int i = 0; i < n && i < l->len; i++) v_list_push(out.as.list, l->items[i]);
        return out;
    }
    if (strcmp(name, "skip") == 0) {
        int n = argc > 0 && argv[0].type == V_INT ? (int)argv[0].as.i : 0;
        Value out = v_list();
        for (int i = n; i < l->len; i++) v_list_push(out.as.list, l->items[i]);
        return out;
    }
    if (strcmp(name, "zip") == 0) {
        Value out = v_list();
        if (argc >= 1 && (argv[0].type == V_LIST || argv[0].type == V_TUPLE) && argv[0].as.list) {
            VList *o = argv[0].as.list;
            int n = l->len < o->len ? l->len : o->len;
            for (int i = 0; i < n; i++) {
                Value t = v_tuple();
                v_list_push(t.as.list, l->items[i]);
                v_list_push(t.as.list, o->items[i]);
                v_list_push(out.as.list, t);
            }
        }
        return out;
    }
    if (strcmp(name, "toString") == 0) {
        char *s = v_to_string(lst);
        Value r = v_string(s);
        free(s);
        return r;
    }
    return v_null();
}

static Value string_method(Value s, const char *name, int argc, Value *argv) {
    if (s.type != V_STRING || !s.as.s) return v_null();
    const char *str = s.as.s;
    size_t slen = strlen(str);

    // ── 查询 ──
    if (strcmp(name, "size") == 0 || strcmp(name, "length") == 0)
        return v_int((int64_t)slen);
    if (strcmp(name, "isEmpty") == 0)    return v_bool(slen == 0);
    if (strcmp(name, "isNotEmpty") == 0) return v_bool(slen != 0);
    if (argc >= 1 && strcmp(name, "startsWith") == 0) {
        if (argv[0].type != V_STRING) return v_bool(0);
        size_t lb = strlen(argv[0].as.s);
        if (lb > slen) return v_bool(0);
        return v_bool(memcmp(str, argv[0].as.s, lb) == 0);
    }
    if (argc >= 1 && strcmp(name, "endsWith") == 0) {
        if (argv[0].type != V_STRING) return v_bool(0);
        size_t lb = strlen(argv[0].as.s);
        if (lb > slen) return v_bool(0);
        return v_bool(memcmp(str + slen - lb, argv[0].as.s, lb) == 0);
    }
    if (argc >= 1 && strcmp(name, "contains") == 0) {
        if (argv[0].type != V_STRING) return v_bool(0);
        return v_bool(strstr(str, argv[0].as.s) != NULL);
    }
    if (argc >= 1 && strcmp(name, "indexOf") == 0) {
        if (argv[0].type != V_STRING) return v_int(-1);
        const char *p = strstr(str, argv[0].as.s);
        return v_int(p ? (int64_t)(p - str) : -1);
    }

    // ── 变换 ──
    if (strcmp(name, "toUpperCase") == 0) {
        char *r = malloc(slen + 1);
        for (size_t i = 0; i < slen; i++) r[i] = (char)toupper((unsigned char)str[i]);
        r[slen] = 0;
        Value v = v_string(r); free(r); return v;
    }
    if (strcmp(name, "toLowerCase") == 0) {
        char *r = malloc(slen + 1);
        for (size_t i = 0; i < slen; i++) r[i] = (char)tolower((unsigned char)str[i]);
        r[slen] = 0;
        Value v = v_string(r); free(r); return v;
    }
    if (strcmp(name, "trim") == 0) {
        size_t a = 0, b = slen;
        while (a < b && isspace((unsigned char)str[a])) a++;
        while (b > a && isspace((unsigned char)str[b-1])) b--;
        char *r = malloc(b - a + 1);
        memcpy(r, str + a, b - a); r[b-a] = 0;
        Value v = v_string(r); free(r); return v;
    }
    if (strcmp(name, "substring") == 0) {
        int start = argc > 0 && argv[0].type == V_INT ? (int)argv[0].as.i : 0;
        int end   = argc > 1 && argv[1].type == V_INT ? (int)argv[1].as.i : (int)slen;
        if (start < 0) start = 0; if (end > (int)slen) end = (int)slen;
        if (start > end) start = end;
        char *r = malloc(end - start + 1);
        memcpy(r, str + start, end - start); r[end-start] = 0;
        Value v = v_string(r); free(r); return v;
    }
    if (strcmp(name, "split") == 0) {
        Value out = v_list();
        const char *sep = (argc >= 1 && argv[0].type == V_STRING) ? argv[0].as.s : "";
        if (!*sep) {  // 空分隔符：逐字符
            for (size_t i = 0; i < slen; i++) {
                char ch[2] = { str[i], 0 };
                v_list_push(out.as.list, v_string(ch));
            }
            return out;
        }
        size_t seplen = strlen(sep);
        const char *cur = str, *hit;
        while ((hit = strstr(cur, sep)) != NULL) {
            size_t n = hit - cur;
            char *part = malloc(n + 1); memcpy(part, cur, n); part[n] = 0;
            v_list_push(out.as.list, v_string(part)); free(part);
            cur = hit + seplen;
        }
        v_list_push(out.as.list, v_string(cur));
        return out;
    }
    if (strcmp(name, "replaceAll") == 0) {
        if (argc < 2 || argv[0].type != V_STRING || argv[1].type != V_STRING) return s;
        const char *from = argv[0].as.s, *to = argv[1].as.s;
        size_t flen = strlen(from), tlen = strlen(to);
        if (flen == 0) return s;
        size_t cap = slen + 1, len = 0; char *out = malloc(cap);
        const char *cur = str, *hit;
        while ((hit = strstr(cur, from)) != NULL) {
            size_t n = hit - cur;
            size_t need = len + n + tlen + 1;
            if (need > cap) { cap = need * 2; out = realloc(out, cap); }
            memcpy(out + len, cur, n); len += n;
            memcpy(out + len, to, tlen); len += tlen;
            cur = hit + flen;
        }
        size_t rest = strlen(cur);
        if (len + rest + 1 > cap) { cap = len + rest + 1; out = realloc(out, cap); }
        memcpy(out + len, cur, rest); len += rest; out[len] = 0;
        Value v = v_string(out); free(out); return v;
    }
    if (strcmp(name, "codeUnitAt") == 0) {
        int i = argc > 0 && argv[0].type == V_INT ? (int)argv[0].as.i : 0;
        if (i < 0 || i >= (int)slen) return v_int(0);
        return v_int((unsigned char)str[i]);
    }
    if (strcmp(name, "toString") == 0) return s;
    // ── 核心扩展 ──
    if (strcmp(name, "repeated") == 0) {
        int times = argc > 0 && argv[0].type == V_INT ? (int)argv[0].as.i : 0;
        if (times <= 0) return v_string("");
        char *out = malloc(slen * times + 1);
        for (int i = 0; i < times; i++) memcpy(out + i*slen, str, slen);
        out[slen*times] = 0;
        Value v = v_string(out); free(out); return v;
    }
    if (strcmp(name, "reversed") == 0) {
        char *out = malloc(slen + 1);
        for (size_t i = 0; i < slen; i++) out[i] = str[slen-1-i];
        out[slen] = 0;
        Value v = v_string(out); free(out); return v;
    }
    if (strcmp(name, "padLeft") == 0 || strcmp(name, "padRight") == 0) {
        int width = argc > 0 && argv[0].type == V_INT ? (int)argv[0].as.i : 0;
        const char *pad = (argc > 1 && argv[1].type == V_STRING) ? argv[1].as.s : " ";
        size_t plen = strlen(pad);
        if ((int)slen >= width || plen == 0) return s;
        int left = (strcmp(name, "padLeft") == 0);
        size_t total = slen + (width - slen) * plen;  // 上界
        char *out = malloc(total + 1); out[0] = 0;
        size_t cur = 0;
        if (left) {
            while ((int)(cur + slen) < width) { memcpy(out+cur, pad, plen); cur += plen; }
            memcpy(out+cur, str, slen); cur += slen;
        } else {
            memcpy(out, str, slen); cur = slen;
            while ((int)cur < width) { memcpy(out+cur, pad, plen); cur += plen; }
        }
        out[cur] = 0;
        Value v = v_string(out); free(out); return v;
    }
    if (strcmp(name, "count") == 0) {
        if (argc < 1 || argv[0].type != V_STRING) return v_int(0);
        const char *sub = argv[0].as.s; size_t sublen = strlen(sub);
        if (sublen == 0) return v_int(0);
        int n = 0; const char *cur = str, *hit;
        while ((hit = strstr(cur, sub)) != NULL) { n++; cur = hit + sublen; }
        return v_int(n);
    }
    if (strcmp(name, "isPalindrome") == 0) {
        for (size_t i = 0; i < slen/2; i++)
            if (str[i] != str[slen-1-i]) return v_bool(0);
        return v_bool(1);
    }
    return v_null();
}

// ── Map<K,V> 方法 ─────────────────────────────────────────────────────────────

static Value map_method(Interp *I, Value mp, const char *name, int argc, Value *argv) {
    (void)I;
    VMap *m = mp.as.map;
    if (!m) return v_null();
    if (strcmp(name, "size") == 0 || strcmp(name, "length") == 0)
        return v_int(m->len);
    if (strcmp(name, "isEmpty") == 0)    return v_bool(m->len == 0);
    if (strcmp(name, "isNotEmpty") == 0) return v_bool(m->len != 0);
    if (strcmp(name, "containsKey") == 0) {
        if (argc < 1) return v_bool(0);
        char *k = v_to_string(argv[0]);
        int found = 0;
        for (int i = 0; i < m->len; i++) if (strcmp(m->keys[i], k) == 0) { found = 1; break; }
        free(k);
        return v_bool(found);
    }
    if (strcmp(name, "get") == 0) {
        if (argc < 1) return v_null();
        char *k = v_to_string(argv[0]);
        Value r = v_map_get(m, k); free(k); return r;
    }
    if (strcmp(name, "set") == 0 || strcmp(name, "put") == 0) {
        if (argc < 2) return mp;
        char *k = v_to_string(argv[0]);
        v_map_set(m, k, argv[1]); free(k); return mp;
    }
    if (strcmp(name, "remove") == 0) {
        if (argc < 1) return v_null();
        char *k = v_to_string(argv[0]);
        Value removed = v_null();
        for (int i = 0; i < m->len; i++) {
            if (strcmp(m->keys[i], k) == 0) {
                removed = m->vals[i];
                free(m->keys[i]);
                for (int j = i; j < m->len - 1; j++) { m->keys[j] = m->keys[j+1]; m->vals[j] = m->vals[j+1]; }
                m->len--;
                break;
            }
        }
        free(k);
        return removed;
    }
    if (strcmp(name, "keys") == 0) {
        Value out = v_list();
        for (int i = 0; i < m->len; i++) v_list_push(out.as.list, v_string(m->keys[i]));
        return out;
    }
    if (strcmp(name, "values") == 0) {
        Value out = v_list();
        for (int i = 0; i < m->len; i++) v_list_push(out.as.list, m->vals[i]);
        return out;
    }
    if (strcmp(name, "clear") == 0) {
        for (int i = 0; i < m->len; i++) free(m->keys[i]);
        m->len = 0;
        return mp;
    }
    if (strcmp(name, "toString") == 0) {
        char *s = v_to_string(mp);
        Value r = v_string(s);
        free(s);
        return r;
    }
    return v_null();
}

// ── int / double 数值方法 ─────────────────────────────────────────────────────

static Value num_method(Value n, const char *name, int argc, Value *argv) {
    int is_int = (n.type == V_INT);
    double d = is_int ? (double)n.as.i : n.as.d;
    int64_t i = is_int ? n.as.i : (int64_t)n.as.d;

    if (strcmp(name, "isEven") == 0) return v_bool((i % 2) == 0);
    if (strcmp(name, "isOdd") == 0)  return v_bool((i % 2) != 0);
    if (strcmp(name, "abs") == 0)
        return is_int ? v_int(i < 0 ? -i : i) : v_double(d < 0 ? -d : d);
    if (strcmp(name, "toDouble") == 0) return v_double(d);
    if (strcmp(name, "toInt") == 0)    return v_int(i);
    if (strcmp(name, "toString") == 0) {
        char *s = v_to_string(n); Value r = v_string(s); free(s); return r;
    }
    if (strcmp(name, "isNegative") == 0) return v_bool(d < 0);
    if (strcmp(name, "isZero") == 0)     return v_bool(d == 0);
    if (strcmp(name, "clamp") == 0) {
        if (argc < 2) return n;
        double lo = argv[0].type == V_DOUBLE ? argv[0].as.d : (double)argv[0].as.i;
        double hi = argv[1].type == V_DOUBLE ? argv[1].as.d : (double)argv[1].as.i;
        double v = d < lo ? lo : (d > hi ? hi : d);
        return is_int ? v_int((int64_t)v) : v_double(v);
    }
    if (strcmp(name, "sign") == 0)
        return v_int(d > 0 ? 1 : (d < 0 ? -1 : 0));
    // ── 浮点取整（对 int 也合法，返回相应值）──
    if (strcmp(name, "floor") == 0) return v_int((int64_t)floor(d));
    if (strcmp(name, "ceil") == 0)  return v_int((int64_t)ceil(d));
    if (strcmp(name, "round") == 0) return v_int((int64_t)round(d));
    if (strcmp(name, "truncate") == 0) return v_int((int64_t)d);
    if (strcmp(name, "isFinite") == 0) return v_bool(isfinite(d));
    if (strcmp(name, "isNaN") == 0)    return v_bool(isnan(d));
    return v_null();  // 未命中 → 交给 reflect
}

// ── bool 方法 ─────────────────────────────────────────────────────────────────

static Value bool_method(Value b, const char *name, int argc, Value *argv) {
    int v = b.as.b;
    if (strcmp(name, "toggle") == 0) return v_bool(!v);
    if (strcmp(name, "toInt") == 0)  return v_int(v ? 1 : 0);
    if (strcmp(name, "toString") == 0) return v_string(v ? "true" : "false");
    if (strcmp(name, "and") == 0) return v_bool(v && (argc > 0 && v_truthy(argv[0])));
    if (strcmp(name, "or") == 0)  return v_bool(v || (argc > 0 && v_truthy(argv[0])));
    return v_null();  // 未命中 → 交给 reflect
}

// ── Operators ────────────────────────────────────────────────────────────────

static Value bin_arith(Interp *I, AstNode *node, TokenType op, Value a, Value b) {
    if (op == TK_PLUS && a.type == V_STRING && b.type == V_STRING) {
        size_t la = strlen(a.as.s), lb = strlen(b.as.s);
        char *r = malloc(la + lb + 1);
        memcpy(r, a.as.s, la); memcpy(r + la, b.as.s, lb); r[la+lb] = 0;
        Value v = { .type = V_STRING }; v.as.s = r; return v;
    }
    int is_d = (a.type == V_DOUBLE || b.type == V_DOUBLE);
    double da = a.type == V_DOUBLE ? a.as.d : (double)a.as.i;
    double db = b.type == V_DOUBLE ? b.as.d : (double)b.as.i;
    int64_t ia = a.type == V_DOUBLE ? (int64_t)a.as.d : a.as.i;
    int64_t ib = b.type == V_DOUBLE ? (int64_t)b.as.d : b.as.i;
    switch (op) {
        case TK_PLUS:  return is_d ? v_double(da+db) : v_int(ia+ib);
        case TK_MINUS: return is_d ? v_double(da-db) : v_int(ia-ib);
        case TK_STAR:  return is_d ? v_double(da*db) : v_int(ia*ib);
        case TK_SLASH:
            if ((is_d ? db == 0 : ib == 0)) { runtime_error(I, node, "division by zero"); return v_null(); }
            return is_d ? v_double(da/db) : v_int(ia/ib);
        case TK_MOD:
            if (ib == 0) { runtime_error(I, node, "modulo by zero"); return v_null(); }
            return v_int(ia % ib);
        case TK_LT:  return v_bool(is_d ? da<db : ia<ib);
        case TK_LTE: return v_bool(is_d ? da<=db : ia<=ib);
        case TK_GT:  return v_bool(is_d ? da>db : ia>ib);
        case TK_GTE: return v_bool(is_d ? da>=db : ia>=ib);
        case TK_BIT_AND: return v_int(ia & ib);
        case TK_BIT_OR:  return v_int(ia | ib);
        case TK_BIT_XOR: return v_int(ia ^ ib);
        default: runtime_error(I, node, "unsupported binary op"); return v_null();
    }
}

// ── Eval ─────────────────────────────────────────────────────────────────────

// 对 int/double 值做 +1 / -1
static Value step_value(Value v, int delta) {
    if (v.type == V_DOUBLE) return v_double(v.as.d + delta);
    return v_int(v.as.i + delta);
}

static Value eval(Interp *I, Env *env, AstNode *node);

// 自增/自减：读取 lvalue 旧值，写回新值。is_prefix 决定返回旧值还是新值。
static Value incr_decr(Interp *I, Env *env, AstNode *target, int delta, int is_prefix) {
    // 标识符
    if (target->type == NODE_IDENT) {
        Value *slot = env_lookup(env, target->as.ident.name);
        if (slot) {
            Value oldv = *slot;
            Value newv = step_value(oldv, delta);
            *slot = newv;
            return is_prefix ? newv : oldv;
        }
        // self 字段
        Value *self = env_lookup(env, "self");
        if (self && self->type == V_OBJECT &&
            class_has_field(self->as.obj->class_node, target->as.ident.name)) {
            Value oldv = v_map_get(self->as.obj->fields, target->as.ident.name);
            Value newv = step_value(oldv, delta);
            v_map_set(self->as.obj->fields, target->as.ident.name, newv);
            return is_prefix ? newv : oldv;
        }
        // 静态字段
        Value *st = static_in_env(env, target->as.ident.name);
        if (st) {
            Value oldv = *st;
            *st = step_value(oldv, delta);
            return is_prefix ? *st : oldv;
        }
        runtime_error(I, target, "undefined variable in ++/--");
        return v_null();
    }
    // 成员 obj.field
    if (target->type == NODE_MEMBER) {
        Value obj = eval(I, env, target->as.member.object);
        VMap *fields = NULL;
        if (obj.type == V_OBJECT) fields = obj.as.obj->fields;
        else if (obj.type == V_MAP) fields = obj.as.map;
        if (fields) {
            Value oldv = v_map_get(fields, target->as.member.name);
            Value newv = step_value(oldv, delta);
            v_map_set(fields, target->as.member.name, newv);
            return is_prefix ? newv : oldv;
        }
        runtime_error(I, target, "invalid target for ++/--");
        return v_null();
    }
    // 下标 a[i]
    if (target->type == NODE_INDEX) {
        Value obj = eval(I, env, target->as.index_expr.object);
        Value idx = eval(I, env, target->as.index_expr.index);
        if (obj.type == V_LIST) {
            int i = idx.type == V_INT ? (int)idx.as.i : 0;
            if (obj.as.list && i < 0) i += obj.as.list->len;
            if (obj.as.list && i >= 0 && i < obj.as.list->len) {
                Value oldv = obj.as.list->items[i];
                Value newv = step_value(oldv, delta);
                obj.as.list->items[i] = newv;
                return is_prefix ? newv : oldv;
            }
        } else if (obj.type == V_MAP) {
            char *ks = v_to_string(idx);
            Value oldv = v_map_get(obj.as.map, ks);
            Value newv = step_value(oldv, delta);
            v_map_set(obj.as.map, ks, newv);
            free(ks);
            return is_prefix ? newv : oldv;
        }
        runtime_error(I, target, "invalid index target for ++/--");
        return v_null();
    }
    runtime_error(I, target, "invalid operand for ++/--");
    return v_null();
}

static Value eval(Interp *I, Env *env, AstNode *node) {
    if (!node || I->had_error) return v_null();
    switch (node->type) {
        case NODE_INT_LIT:    return v_int(node->as.int_lit.value);
        case NODE_FLOAT_LIT:  return v_double(node->as.float_lit.value);
        case NODE_BOOL_LIT:   return v_bool(node->as.bool_lit.value);
        case NODE_NULL_LIT:   return v_null();
        case NODE_STRING_LIT: {
            char *decoded = decode_str_lit(node->as.string_lit.value);
            if (node->as.string_lit.is_fmt) {
                char *interp_s = fstring_interpolate(I, env, decoded, node);
                free(decoded);
                Value v = { .type = V_STRING };
                v.as.s = interp_s;
                return v;
            }
            Value v = { .type = V_STRING };
            v.as.s = decoded;
            return v;
        }
        case NODE_IDENT: {
            // 'this' / 'self' both refer to the current method's receiver
            if (strcmp(node->as.ident.name, "this") == 0 ||
                strcmp(node->as.ident.name, "self") == 0) {
                Value *self = env_lookup(env, "self");
                if (self) return *self;
                runtime_error(I, node, "'this' used outside method");
                return v_null();
            }
            Value *p = env_lookup(env, node->as.ident.name);
            if (p) return *p;
            // bare field access inside a method: check self's fields
            Value *self = env_lookup(env, "self");
            if (self && self->type == V_OBJECT) {
                VObject *o = self->as.obj;
                Value v = v_map_get(o->fields, node->as.ident.name);
                if (v.type != V_NULL) return v;
                // even null fields are valid if declared
                if (class_has_field(o->class_node, node->as.ident.name))
                    return v_null();
                // 实例方法内访问静态字段：用对象的类名查
                Value *st = static_slot(o->class_node->as.class_decl.name, node->as.ident.name);
                if (st) return *st;
            }
            // 静态方法内访问静态字段：用 __class__ 查
            Value *cls = env_lookup(env, "__class__");
            if (cls && cls->type == V_STRING) {
                Value *st = static_slot(cls->as.s, node->as.ident.name);
                if (st) return *st;
            }
            runtime_error(I, node, node->as.ident.name);
            return v_null();
        }
        case NODE_ELLIPSIS_EXPR:
            // placeholder (`ellipsis;` inside a body) — no-op
            return v_null();
        case NODE_BINARY: {
            TokenType op = node->as.binary.op;
            if (op == TK_AND) {
                Value l = eval(I, env, node->as.binary.left);
                return v_truthy(l) ? eval(I, env, node->as.binary.right) : l;
            }
            if (op == TK_OR) {
                Value l = eval(I, env, node->as.binary.left);
                return v_truthy(l) ? l : eval(I, env, node->as.binary.right);
            }
            Value a = eval(I, env, node->as.binary.left);
            Value b = eval(I, env, node->as.binary.right);
            if (op == TK_EQ) return v_bool(v_equals(a, b));
            if (op == TK_NEQ) return v_bool(!v_equals(a, b));
            return bin_arith(I, node, op, a, b);
        }
        case NODE_TERNARY: {
            Value c = eval(I, env, node->as.ternary.cond);
            return v_truthy(c)
                ? eval(I, env, node->as.ternary.then_expr)
                : eval(I, env, node->as.ternary.else_expr);
        }
        case NODE_UNARY: {
            // 前缀自增/自减
            if (node->as.unary.op == TK_INCR)
                return incr_decr(I, env, node->as.unary.operand, +1, 1);
            if (node->as.unary.op == TK_DECR)
                return incr_decr(I, env, node->as.unary.operand, -1, 1);
            Value v = eval(I, env, node->as.unary.operand);
            if (node->as.unary.op == TK_BANG) return v_bool(!v_truthy(v));
            if (node->as.unary.op == TK_MINUS) {
                if (v.type == V_INT) return v_int(-v.as.i);
                if (v.type == V_DOUBLE) return v_double(-v.as.d);
            }
            if (node->as.unary.op == TK_BIT_NOT && v.type == V_INT) return v_int(~v.as.i);
            return v_null();
        }
        case NODE_POSTFIX_UNARY: {
            // 后缀自增/自减
            if (node->as.unary.op == TK_INCR)
                return incr_decr(I, env, node->as.unary.operand, +1, 0);
            if (node->as.unary.op == TK_DECR)
                return incr_decr(I, env, node->as.unary.operand, -1, 0);
            // 后缀 value! —— 非空断言：解包可空值，运行时为恒等返回
            Value v = eval(I, env, node->as.unary.operand);
            return v;
        }
        case NODE_ASSIGN: {
            Value rhs = eval(I, env, node->as.assign.value);
            AstNode *t = node->as.assign.target;
            if (t->type == NODE_IDENT) {
                // alias 类型锁定检查：给锁定别名赋不兼容类型的值会抛异常
                int locked = alias_lock_type(t->as.ident.name);
                if (locked >= 0 && !alias_type_compatible((ValueType)locked, rhs.type)) {
                    char buf[160];
                    snprintf(buf, sizeof(buf),
                             "TypeError: cannot assign %s to alias '%s' locked as %s",
                             v_typename(rhs.type), t->as.ident.name,
                             v_typename((ValueType)locked));
                    I->exc_value = v_string(buf);
                    I->exc_active = 1;
                    return v_null();
                }
                if (node->as.assign.op == TK_ASSIGN) {
                    if (!env_assign(env, t->as.ident.name, rhs)) {
                        // bare field assignment inside method
                        Value *self = env_lookup(env, "self");
                        if (self && self->type == V_OBJECT &&
                            class_has_field(self->as.obj->class_node, t->as.ident.name)) {
                            v_map_set(self->as.obj->fields, t->as.ident.name, rhs);
                        } else {
                            // 静态字段写入
                            Value *st = static_in_env(env, t->as.ident.name);
                            if (st) *st = rhs;
                            else env_define(env, t->as.ident.name, rhs);
                        }
                    }
                } else {
                    Value *cur = env_lookup(env, t->as.ident.name);
                    Value old;
                    int is_field = 0;
                    int is_static = 0;
                    Value *self = NULL;
                    Value *staticp = NULL;
                    if (!cur) {
                        self = env_lookup(env, "self");
                        if (self && self->type == V_OBJECT &&
                            class_has_field(self->as.obj->class_node, t->as.ident.name)) {
                            old = v_map_get(self->as.obj->fields, t->as.ident.name);
                            is_field = 1;
                        } else if ((staticp = static_in_env(env, t->as.ident.name)) != NULL) {
                            old = *staticp;
                            is_static = 1;
                        } else {
                            runtime_error(I, node, "undefined"); return v_null();
                        }
                    } else { old = *cur; }
                    TokenType bop = TK_PLUS;
                    switch (node->as.assign.op) {
                        case TK_PLUS_ASSIGN:  bop = TK_PLUS; break;
                        case TK_MINUS_ASSIGN: bop = TK_MINUS; break;
                        case TK_STAR_ASSIGN:  bop = TK_STAR; break;
                        case TK_SLASH_ASSIGN: bop = TK_SLASH; break;
                        case TK_MOD_ASSIGN:   bop = TK_MOD; break;
                        default: break;
                    }
                    Value nv = bin_arith(I, node, bop, old, rhs);
                    if (is_field) v_map_set(self->as.obj->fields, t->as.ident.name, nv);
                    else if (is_static) *staticp = nv;
                    else *cur = nv;
                }
                return rhs;
            }
            if (t->type == NODE_MEMBER) {
                Value obj = eval(I, env, t->as.member.object);
                if (obj.type == V_OBJECT) {
                    Value newv = rhs;
                    if (node->as.assign.op != TK_ASSIGN) {
                        Value old = v_map_get(obj.as.obj->fields, t->as.member.name);
                        TokenType bop = TK_PLUS;
                        switch (node->as.assign.op) {
                            case TK_PLUS_ASSIGN:  bop = TK_PLUS; break;
                            case TK_MINUS_ASSIGN: bop = TK_MINUS; break;
                            case TK_STAR_ASSIGN:  bop = TK_STAR; break;
                            case TK_SLASH_ASSIGN: bop = TK_SLASH; break;
                            case TK_MOD_ASSIGN:   bop = TK_MOD; break;
                            default: break;
                        }
                        newv = bin_arith(I, node, bop, old, rhs);
                    }
                    v_map_set(obj.as.obj->fields, t->as.member.name, newv);
                    return newv;
                }
                if (obj.type == V_MAP) {
                    Value newv = rhs;
                    if (node->as.assign.op != TK_ASSIGN) {
                        Value old = v_map_get(obj.as.map, t->as.member.name);
                        TokenType bop = TK_PLUS;
                        switch (node->as.assign.op) {
                            case TK_PLUS_ASSIGN:  bop = TK_PLUS; break;
                            case TK_MINUS_ASSIGN: bop = TK_MINUS; break;
                            case TK_STAR_ASSIGN:  bop = TK_STAR; break;
                            case TK_SLASH_ASSIGN: bop = TK_SLASH; break;
                            case TK_MOD_ASSIGN:   bop = TK_MOD; break;
                            default: break;
                        }
                        newv = bin_arith(I, node, bop, old, rhs);
                    }
                    v_map_set(obj.as.map, t->as.member.name, newv);
                    return newv;
                }
                runtime_error(I, node, "member assignment on non-object");
                return v_null();
            }
            if (t->type == NODE_INDEX) {
                Value obj = eval(I, env, t->as.index_expr.object);
                Value idx = eval(I, env, t->as.index_expr.index);
                Value newv = rhs;
                if (obj.type == V_LIST) {
                    int i = idx.type == V_INT ? (int)idx.as.i : 0;
                    if (obj.as.list && i < 0) i += obj.as.list->len;
                    if (!obj.as.list || i < 0 || i >= obj.as.list->len) {
                        runtime_error(I, node, "list index out of range");
                        return v_null();
                    }
                    if (node->as.assign.op != TK_ASSIGN) {
                        Value old = obj.as.list->items[i];
                        TokenType bop = TK_PLUS;
                        switch (node->as.assign.op) {
                            case TK_PLUS_ASSIGN:  bop = TK_PLUS; break;
                            case TK_MINUS_ASSIGN: bop = TK_MINUS; break;
                            case TK_STAR_ASSIGN:  bop = TK_STAR; break;
                            case TK_SLASH_ASSIGN: bop = TK_SLASH; break;
                            case TK_MOD_ASSIGN:   bop = TK_MOD; break;
                            default: break;
                        }
                        newv = bin_arith(I, node, bop, old, rhs);
                    }
                    obj.as.list->items[i] = newv;
                    return newv;
                }
                if (obj.type == V_MAP) {
                    char *ks = v_to_string(idx);
                    if (node->as.assign.op != TK_ASSIGN) {
                        Value old = v_map_get(obj.as.map, ks);
                        TokenType bop = TK_PLUS;
                        switch (node->as.assign.op) {
                            case TK_PLUS_ASSIGN:  bop = TK_PLUS; break;
                            case TK_MINUS_ASSIGN: bop = TK_MINUS; break;
                            case TK_STAR_ASSIGN:  bop = TK_STAR; break;
                            case TK_SLASH_ASSIGN: bop = TK_SLASH; break;
                            case TK_MOD_ASSIGN:   bop = TK_MOD; break;
                            default: break;
                        }
                        newv = bin_arith(I, node, bop, old, rhs);
                    }
                    v_map_set(obj.as.map, ks, newv);
                    free(ks);
                    return newv;
                }
                runtime_error(I, node, "index assignment on non-indexable value");
                return v_null();
            }
            runtime_error(I, node, "unsupported assignment target");
            return v_null();
        }
        case NODE_CALL: {
            // Resolve method/static call: obj.method(args) or Class.method(args)
            int is_member_call = (node->as.call.callee->type == NODE_MEMBER);
            Value receiver = v_null();
            int has_receiver = 0;
            const char *method_name = NULL;
            Value callee = v_null();
            const char *static_call_class = NULL;  // 静态方法所属类名

            if (is_member_call) {
                AstNode *m = node->as.call.callee;

                // super.method(...) → 在父类中查找方法，self 仍为当前对象
                if (m->as.member.object->type == NODE_IDENT &&
                    strcmp(m->as.member.object->as.ident.name, "super") == 0) {
                    Value *self = env_lookup(env, "self");
                    if (self && self->type == V_OBJECT) {
                        AstNode *parent = find_class(self->as.obj->class_node->as.class_decl.parent);
                        AstNode *mth = parent ? find_method(parent, m->as.member.name) : NULL;
                        if (mth) {
                            int argc = node->as.call.args.count;
                            Value *argv = calloc(argc, sizeof(Value));
                            for (int i = 0; i < argc; i++)
                                argv[i] = eval(I, env, node->as.call.args.items[i]);
                            Env *call_env = env_new(env);
                            env_define(call_env, "self", *self);
                            NodeList *params = &mth->as.func.params;
                            int n = params->count < argc ? params->count : argc;
                            for (int i = 0; i < n; i++)
                                env_define(call_env, params->items[i]->as.var.name, argv[i]);
                            Value result = v_null();
                            if (mth->as.func.body) {
                                exec_block(I, call_env, &mth->as.func.body->as.program.stmts);
                                if (I->return_active) { result = I->return_value; I->return_active = 0; }
                            }
                            free(argv);
                            return result;
                        }
                    }
                    runtime_error(I, node, "super method not found");
                    return v_null();
                }

                receiver = eval(I, env, m->as.member.object);
                method_name = m->as.member.name;

                // 基本类型（int/double/bool）：先内置数值方法，未命中再查 reflect
                if (receiver.type == V_INT || receiver.type == V_DOUBLE || receiver.type == V_BOOL) {
                    int argc = node->as.call.args.count;
                    Value *argv = calloc(argc, sizeof(Value));
                    for (int i = 0; i < argc; i++)
                        argv[i] = eval(I, env, node->as.call.args.items[i]);
                    AstNode *rm = reflect_lookup(reflect_type_of(receiver), method_name);
                    Value r;
                    if (rm) {
                        r = reflect_invoke(I, env, rm, receiver, argc, argv);
                    } else if (receiver.type == V_BOOL) {
                        r = bool_method(receiver, method_name, argc, argv);
                    } else {
                        r = num_method(receiver, method_name, argc, argv);
                    }
                    free(argv);
                    return r;
                }
                if (receiver.type == V_LIST || receiver.type == V_TUPLE) {
                    int argc = node->as.call.args.count;
                    Value *argv = calloc(argc, sizeof(Value));
                    for (int i = 0; i < argc; i++)
                        argv[i] = eval(I, env, node->as.call.args.items[i]);
                    // 优先内建方法；不存在的方法名回退到 reflect 扩展表
                    AstNode *rm = reflect_lookup("list", method_name);
                    Value r = rm ? reflect_invoke(I, env, rm, receiver, argc, argv)
                                 : list_method(I, receiver, method_name, argc, argv);
                    free(argv);
                    return r;
                }
                if (receiver.type == V_STRING) {
                    int argc = node->as.call.args.count;
                    Value *argv = calloc(argc, sizeof(Value));
                    for (int i = 0; i < argc; i++)
                        argv[i] = eval(I, env, node->as.call.args.items[i]);
                    AstNode *rm = reflect_lookup("string", method_name);
                    Value r = rm ? reflect_invoke(I, env, rm, receiver, argc, argv)
                                 : string_method(receiver, method_name, argc, argv);
                    free(argv);
                    return r;
                }
                if (receiver.type == V_MAP) {
                    // 优先：模块命名空间 / map 持有可调用值（m.func(args)）
                    Value fld = v_map_get(receiver.as.map, method_name);
                    if (fld.type == V_NATIVE || fld.type == V_FN || fld.type == V_LAMBDA) {
                        callee = fld;
                    } else {
                        // 否则按 Map<K,V> 内建方法处理（containsKey/keys/values/...）
                        int argc = node->as.call.args.count;
                        Value *argv = calloc(argc, sizeof(Value));
                        for (int i = 0; i < argc; i++)
                            argv[i] = eval(I, env, node->as.call.args.items[i]);
                        Value r = map_method(I, receiver, method_name, argc, argv);
                        free(argv);
                        return r;
                    }
                }
                if (receiver.type == V_OBJECT) {
                    AstNode *mth = find_method(receiver.as.obj->class_node, method_name);
                    if (mth) {
                        VFunc *f = calloc(1, sizeof(VFunc));
                        f->decl = mth; f->closure = env;
                        callee.type = V_FN; callee.as.fn = f;
                        has_receiver = 1;
                    } else {
                        // try field that holds a function
                        Value fld = v_map_get(receiver.as.obj->fields, method_name);
                        if (fld.type == V_FN || fld.type == V_LAMBDA || fld.type == V_NATIVE) {
                            callee = fld;
                        } else {
                            // reflect 扩展方法回退（沿继承链查目标类型）
                            AstNode *cn = receiver.as.obj->class_node;
                            AstNode *rm = NULL;
                            while (cn && !rm) {
                                rm = reflect_lookup(cn->as.class_decl.name, method_name);
                                cn = find_class(cn->as.class_decl.parent);
                            }
                            if (rm) {
                                int argc = node->as.call.args.count;
                                Value *argv = calloc(argc, sizeof(Value));
                                for (int i = 0; i < argc; i++)
                                    argv[i] = eval(I, env, node->as.call.args.items[i]);
                                Value r = reflect_invoke(I, env, rm, receiver, argc, argv);
                                free(argv);
                                return r;
                            }
                        }
                    }
                } else if (receiver.type == V_CLASS) {
                    // 静态字段访问：ClassName.field
                    AstNode *mth = find_method(receiver.as.cls, method_name);
                    if (mth && (mth->type == NODE_FACTORY_DECL || mth->as.func.is_static)) {
                        VFunc *f = calloc(1, sizeof(VFunc));
                        f->decl = mth; f->closure = env;
                        callee.type = V_FN; callee.as.fn = f;
                        // 记录所属类名，供静态方法体内访问静态字段
                        static_call_class = receiver.as.cls->as.class_decl.name;
                    }
                }
                if (callee.type == V_NULL && !has_receiver) {
                    runtime_error(I, node, "no such method");
                    return v_null();
                }
            } else {
                AstNode *cnode = node->as.call.callee;
                // bare method call inside a method body → self.method(args)
                if (cnode->type == NODE_IDENT) {
                    Value *direct = env_lookup(env, cnode->as.ident.name);
                    if (!direct) {
                        Value *self = env_lookup(env, "self");
                        if (self && self->type == V_OBJECT) {
                            AstNode *mth = find_method(self->as.obj->class_node, cnode->as.ident.name);
                            if (mth) {
                                VFunc *f = calloc(1, sizeof(VFunc));
                                f->decl = mth; f->closure = env;
                                callee.type = V_FN; callee.as.fn = f;
                                receiver = *self;
                                has_receiver = 1;
                            }
                        }
                    }
                }
                if (callee.type == V_NULL)
                    callee = eval(I, env, cnode);
            }

            // Class(args) → constructor
            if (callee.type == V_CLASS) {
                AstNode *cls = callee.as.cls;
                AstNode *ctor = find_constructor(cls);
                Value obj_v = { .type = V_OBJECT };
                VObject *obj = calloc(1, sizeof(VObject));
                obj->class_node = cls;
                Value mp = v_map();
                obj->fields = mp.as.map;
                obj_v.as.obj = obj;

                int argc = node->as.call.args.count;
                Value *argv = calloc(argc, sizeof(Value));
                for (int i = 0; i < argc; i++)
                    argv[i] = eval(I, env, node->as.call.args.items[i]);

                if (ctor) {
                    int n = ctor->as.constructor.params.count;
                    // 1. 处理参数：this./super. 始终赋字段；普通参数若存在同名字段则自动赋值
                    for (int i = 0; i < n && i < argc; i++) {
                        AstNode *p = ctor->as.constructor.params.items[i];
                        const char *tn = p->as.var.type_name;
                        if (tn && (strcmp(tn, "this") == 0 || strcmp(tn, "super") == 0)) {
                            v_map_set(obj->fields, p->as.var.name, argv[i]);
                        } else if (class_has_field(cls, p->as.var.name)) {
                            v_map_set(obj->fields, p->as.var.name, argv[i]);
                        }
                    }
                    // 2. 若构造函数有函数体，绑定 self + 参数为局部变量后执行
                    if (ctor->as.constructor.body) {
                        Env *ctor_env = env_new(env);
                        env_define(ctor_env, "self", obj_v);
                        for (int i = 0; i < n && i < argc; i++) {
                            AstNode *p = ctor->as.constructor.params.items[i];
                            env_define(ctor_env, p->as.var.name, argv[i]);
                        }
                        exec_block(I, ctor_env, &ctor->as.constructor.body->as.program.stmts);
                        I->return_active = 0;
                    }
                }
                free(argv);
                return obj_v;
            }

            int argc = node->as.call.args.count;
            Value *argv = calloc(argc, sizeof(Value));
            for (int i = 0; i < argc; i++)
                argv[i] = eval(I, env, node->as.call.args.items[i]);
            Value result = v_null();
            if (callee.type == V_NATIVE) {
                result = callee.as.native(argc, argv);
            } else if (callee.type == V_FN || callee.type == V_LAMBDA) {
                AstNode *decl = callee.as.fn->decl;
                // ellipsis 外部函数桩：函数体由 dll 提供，转发到 FFI 调用
                if (decl->type == NODE_FUNC_DECL && decl->as.func.is_ellipsis) {
                    result = ffi_call(decl->as.func.name,
                                      decl->as.func.return_type, argc, argv);
                    free(argv);
                    return result;
                }
                NodeList *params;
                AstNode *body;
                if (decl->type == NODE_LAMBDA) {
                    params = &decl->as.lambda.params;
                    body = decl->as.lambda.body;
                } else if (decl->type == NODE_FACTORY_DECL) {
                    params = &decl->as.factory.params;
                    body = decl->as.factory.body;
                } else {
                    params = &decl->as.func.params;
                    body = decl->as.func.body;
                }
                Env *call_env = env_new(callee.as.fn->closure);
                if (has_receiver)
                    env_define(call_env, "self", receiver);
                if (static_call_class)
                    env_define(call_env, "__class__", v_string(static_call_class));
                int n = params->count < argc ? params->count : argc;
                for (int i = 0; i < n; i++) {
                    AstNode *p = params->items[i];
                    env_define(call_env, p->as.var.name, argv[i]);
                }
                if (body) {
                    if (body->type == NODE_BLOCK) {
                        exec_block(I, call_env, &body->as.program.stmts);
                    } else {
                        result = eval(I, call_env, body);
                    }
                    if (I->return_active) {
                        result = I->return_value;
                        I->return_active = 0;
                    }
                }
            } else {
                runtime_error(I, node, "value not callable");
            }
            free(argv);
            return result;
        }
        case NODE_MEMBER: {
            Value obj = eval(I, env, node->as.member.object);
            if (obj.type == V_OBJECT) {
                Value v = v_map_get(obj.as.obj->fields, node->as.member.name);
                if (v.type != V_NULL) return v;
                // 实例上访问静态字段
                Value *st = static_slot(obj.as.obj->class_node->as.class_decl.name, node->as.member.name);
                if (st) return *st;
                return v;
            }
            if (obj.type == V_MAP)
                return v_map_get(obj.as.map, node->as.member.name);
            if (obj.type == V_CLASS) {
                // ClassName.staticField
                Value *st = static_slot(obj.as.cls->as.class_decl.name, node->as.member.name);
                if (st) return *st;
                return v_null();
            }
            // 无括号属性访问：list/tuple/string 的 first/last/length/isEmpty 等
            if (obj.type == V_LIST || obj.type == V_TUPLE) {
                const char *nm = node->as.member.name;
                VList *l = obj.as.list;
                if (strcmp(nm, "length") == 0 || strcmp(nm, "size") == 0) return v_int(l ? l->len : 0);
                if (strcmp(nm, "isEmpty") == 0)    return v_bool(!l || l->len == 0);
                if (strcmp(nm, "isNotEmpty") == 0) return v_bool(l && l->len > 0);
                if (strcmp(nm, "first") == 0)      return (l && l->len) ? l->items[0] : v_null();
                if (strcmp(nm, "last") == 0)       return (l && l->len) ? l->items[l->len-1] : v_null();
                return v_null();
            }
            if (obj.type == V_STRING) {
                const char *nm = node->as.member.name;
                if (strcmp(nm, "length") == 0 || strcmp(nm, "size") == 0)
                    return v_int(obj.as.s ? (int64_t)strlen(obj.as.s) : 0);
                if (strcmp(nm, "isEmpty") == 0)    return v_bool(!obj.as.s || !obj.as.s[0]);
                if (strcmp(nm, "isNotEmpty") == 0) return v_bool(obj.as.s && obj.as.s[0]);
                return v_null();
            }
            runtime_error(I, node, "member access on non-object");
            return v_null();
        }
        case NODE_CLASS_DECL: {
            Value v = { .type = V_CLASS };
            v.as.cls = node;
            env_define(env, node->as.class_decl.name, v);
            // 初始化静态字段（static 字段属于类，所有实例共享）
            for (int i = 0; i < node->as.class_decl.members.count; i++) {
                AstNode *m = node->as.class_decl.members.items[i];
                if (m->type == NODE_FIELD_DECL && m->as.var.is_static) {
                    Value init;
                    if (m->as.var.init) {
                        init = eval(I, env, m->as.var.init);
                    } else {
                        // 无初始化器：按声明类型给默认值
                        const char *t = m->as.var.type_name;
                        if (t && strcmp(t, "int") == 0) init = v_int(0);
                        else if (t && strcmp(t, "double") == 0) init = v_double(0.0);
                        else if (t && strcmp(t, "bool") == 0) init = v_bool(0);
                        else if (t && strcmp(t, "string") == 0) init = v_string("");
                        else init = v_null();
                    }
                    static_define(node->as.class_decl.name, m->as.var.name, init);
                }
            }
            return v_null();
        }
        case NODE_BLOCK:
            exec_block(I, env, &node->as.program.stmts);
            return v_null();
        case NODE_EXPR_STMT:
            if (node->as.program.stmts.count > 0)
                return eval(I, env, node->as.program.stmts.items[0]);
            return v_null();
        case NODE_VAR_DECL: {
            Value v = node->as.var.init
                ? eval(I, env, node->as.var.init)
                : v_null();
            env_define(env, node->as.var.name, v);
            return v_null();
        }
        case NODE_ALIAS: {
            // 类型别名: alias Int64 = int → 绑定类型句柄（含 parse/parser 静态方法）
            AstNode *av = node->as.alias.value;
            if (av && av->type == NODE_IDENT) {
                const char *n = av->as.ident.name;
                if (strcmp(n, "int") == 0 || strcmp(n, "double") == 0 ||
                    strcmp(n, "bool") == 0 || strcmp(n, "string") == 0 ||
                    strcmp(n, "void") == 0 || strcmp(n, "list") == 0 ||
                    strcmp(n, "map") == 0 || strcmp(n, "Function") == 0) {
                    env_define(env, node->as.alias.name, make_type_handle(n));
                    return v_null();
                }
            }
            // union/intersection type alias → no runtime effect
            if (av && av->type == NODE_BINARY &&
                (av->as.binary.op == TK_BIT_OR || av->as.binary.op == TK_BIT_AND))
                return v_null();
            Value v = av ? eval(I, env, av) : v_null();
            env_define(env, node->as.alias.name, v);
            // 值别名：推断并锁定类型，后续赋不兼容类型的值会抛错
            alias_lock_register(node->as.alias.name, v.type);
            return v_null();
        }
        case NODE_CONST_DECL: {
            Value v = node->as.constant.value
                ? eval(I, env, node->as.constant.value)
                : v_null();
            env_define(env, node->as.constant.name, v);
            return v_null();
        }
        case NODE_IF: {
            Value c = eval(I, env, node->as.if_stmt.condition);
            if (v_truthy(c)) {
                eval(I, env, node->as.if_stmt.then_block);
            } else {
                int taken = 0;
                for (int i = 0; i < node->as.if_stmt.elif_conds.count; i++) {
                    Value ec = eval(I, env, node->as.if_stmt.elif_conds.items[i]);
                    if (v_truthy(ec)) {
                        eval(I, env, node->as.if_stmt.elif_blocks.items[i]);
                        taken = 1; break;
                    }
                }
                if (!taken && node->as.if_stmt.else_block)
                    eval(I, env, node->as.if_stmt.else_block);
            }
            return v_null();
        }
        case NODE_RETURN: {
            I->return_value = node->as.return_stmt.value
                ? eval(I, env, node->as.return_stmt.value)
                : v_null();
            I->return_active = 1;
            return v_null();
        }
        case NODE_BREAK:
            I->break_active = 1;
            return v_null();
        case NODE_FOR: {
            AstNode *iter = node->as.for_stmt.iter;
            const char *var = iter->as.iter.var_name;
            int argc = iter->as.iter.args.count;
            Env *loop_env = env_new(env);

            if (argc == 1) {
                Value seq = eval(I, env, iter->as.iter.args.items[0]);
                if (seq.type == V_LIST || seq.type == V_TUPLE) {
                    env_define(loop_env, var, v_null());
                    for (int i = 0; i < seq.as.list->len; i++) {
                        Value *slot = env_lookup(loop_env, var);
                        *slot = seq.as.list->items[i];
                        eval(I, loop_env, node->as.for_stmt.body);
                        if (I->had_error || I->return_active || I->break_active) break;
                    }
                } else if (seq.type == V_STRING) {
                    env_define(loop_env, var, v_null());
                    for (size_t i = 0; seq.as.s && seq.as.s[i]; i++) {
                        char buf[2] = { seq.as.s[i], 0 };
                        Value *slot = env_lookup(loop_env, var);
                        *slot = v_string(buf);
                        eval(I, loop_env, node->as.for_stmt.body);
                        if (I->had_error || I->return_active || I->break_active) break;
                    }
                } else if (seq.type == V_INT) {
                    int64_t end = seq.as.i;
                    env_define(loop_env, var, v_int(0));
                    for (int64_t i = 0; i < end; i++) {
                        Value *slot = env_lookup(loop_env, var);
                        *slot = v_int(i);
                        eval(I, loop_env, node->as.for_stmt.body);
                        if (I->had_error || I->return_active || I->break_active) break;
                    }
                }
            } else if (argc >= 2) {
                Value sv = eval(I, env, iter->as.iter.args.items[0]);
                Value ev = eval(I, env, iter->as.iter.args.items[1]);
                int64_t start = sv.type == V_INT ? sv.as.i : (int64_t)sv.as.d;
                int64_t end   = ev.type == V_INT ? ev.as.i : (int64_t)ev.as.d;
                int64_t step  = 1;
                if (argc >= 3) {
                    Value stp = eval(I, env, iter->as.iter.args.items[2]);
                    step = stp.type == V_INT ? stp.as.i : (int64_t)stp.as.d;
                    if (step == 0) step = 1;
                }
                env_define(loop_env, var, v_int(start));
                if (step > 0) {
                    for (int64_t i = start; i < end; i += step) {
                        Value *slot = env_lookup(loop_env, var);
                        *slot = v_int(i);
                        eval(I, loop_env, node->as.for_stmt.body);
                        if (I->had_error || I->return_active || I->break_active) break;
                    }
                } else {
                    for (int64_t i = start; i > end; i += step) {
                        Value *slot = env_lookup(loop_env, var);
                        *slot = v_int(i);
                        eval(I, loop_env, node->as.for_stmt.body);
                        if (I->had_error || I->return_active || I->break_active) break;
                    }
                }
            }
            I->break_active = 0;
            return v_null();
        }
        case NODE_WHEN: {
            // when 是条件循环（相当于 while）
            while (1) {
                Value c = eval(I, env, node->as.when_stmt.condition);
                if (!v_truthy(c)) break;
                eval(I, env, node->as.when_stmt.body);
                if (I->had_error || I->return_active || I->exc_active) break;
                if (I->break_active) { I->break_active = 0; break; }
            }
            return v_null();
        }
        case NODE_THROW: {
            AstNode *v = node->as.throw_stmt.value;
            // throw NameOfError(...)  with unknown class → string "NameOfError: msg"
            if (v && v->type == NODE_CALL && v->as.call.callee->type == NODE_IDENT) {
                const char *cn = v->as.call.callee->as.ident.name;
                if (!find_class(cn) && !env_lookup(env, cn)) {
                    char *prefix = strdup(cn);
                    if (v->as.call.args.count > 0) {
                        Value arg = eval(I, env, v->as.call.args.items[0]);
                        char *as = v_to_string(arg);
                        char *combined = malloc(strlen(prefix) + strlen(as) + 4);
                        sprintf(combined, "%s: %s", prefix, as);
                        free(prefix); free(as);
                        I->exc_value = v_string(combined);
                        free(combined);
                    } else {
                        I->exc_value = v_string(prefix);
                        free(prefix);
                    }
                    I->exc_active = 1;
                    return v_null();
                }
            }
            I->exc_value = v ? eval(I, env, v) : v_string("Exception");
            I->exc_active = 1;
            return v_null();
        }
        case NODE_PARALLEL: {
            // Sequential fallback in interpreter mode.
            for (int i = 0; i < node->as.parallel.sections.count; i++)
                eval(I, env, node->as.parallel.sections.items[i]);
            if (node->as.parallel.for_iter && node->as.parallel.for_body) {
                AstNode pf;
                memset(&pf, 0, sizeof(pf));
                pf.type = NODE_FOR;
                pf.as.for_stmt.iter = node->as.parallel.for_iter;
                pf.as.for_stmt.body = node->as.parallel.for_body;
                eval(I, env, &pf);
            }
            return v_null();
        }
        case NODE_SIGNAL:
            // 顺序退化语义：直接执行 signal 体（发信号点的逻辑）
            if (node->as.signal.body)
                eval(I, env, node->as.signal.body);
            return v_null();
        case NODE_DELAY:
            // 顺序退化语义：直接执行 delay 体（等到信号后运行的逻辑）
            if (node->as.delay.body)
                eval(I, env, node->as.delay.body);
            return v_null();
        case NODE_DLL: {
            // dll path alias name → 加载动态链接库（FFI），绑定 dll 句柄命名空间
            const char *path = node->as.import.path;
            const char *alias = node->as.import.alias_name;
            Value ns = ffi_dll_load(path, alias);
            const char *bind = alias;
            char namebuf[128];
            if (!bind) {
                const char *dot = strrchr(path, '.');
                snprintf(namebuf, sizeof(namebuf), "%s", dot ? dot + 1 : path);
                bind = namebuf;
            }
            env_define(env, bind, ns);
            return v_null();
        }
        case NODE_EXPORT:
            // codegen-only; no-op in interpreter
            return v_null();
        case NODE_LOAD: {
            // 先查 C 内置模块，再回退到 .candle 源码模块
            const char *path = node->as.import.path;
            Value mod = module_load(path);
            if (mod.type == V_NULL)
                mod = load_candle_module(I, path);
            if (mod.type == V_NULL) {
                runtime_error(I, node, path);  // 模块找不到
                return v_null();
            }
            const char *bind = node->as.import.alias_name;
            char namebuf[128];
            if (!bind) {
                const char *dot = strrchr(path, '.');
                snprintf(namebuf, sizeof(namebuf), "%s", dot ? dot + 1 : path);
                bind = namebuf;
            }
            env_define(env, bind, mod);
            return v_null();
        }
        case NODE_REFLECT: {
            // 把 reflect 体内的方法登记进扩展方法表
            const char *target = node->as.reflect.target;
            AstNode *body = node->as.reflect.body;
            if (body && body->type == NODE_BLOCK) {
                for (int i = 0; i < body->as.program.stmts.count; i++) {
                    AstNode *m = body->as.program.stmts.items[i];
                    if (m->type == NODE_METHOD_DECL)
                        reflect_register(target, m);
                }
            }
            return v_null();
        }
        case NODE_TRY: {
            eval(I, env, node->as.try_stmt.body);
            if (I->exc_active && node->as.try_stmt.catch_bodies.count > 0) {
                Value exc = I->exc_value;
                I->exc_active = 0;

                // wrap raw values into { type, message } so e.message / e.type work
                if (exc.type != V_OBJECT && exc.type != V_MAP) {
                    Value wrap = v_map();
                    char *full = v_to_string(exc);
                    char *colon = strchr(full, ':');
                    if (colon) {
                        *colon = 0;
                        v_map_set(wrap.as.map, "type", v_string(full));
                        char *msg = colon + 1;
                        while (*msg == ' ') msg++;
                        v_map_set(wrap.as.map, "message", v_string(msg));
                    } else {
                        v_map_set(wrap.as.map, "type", v_string("Exception"));
                        v_map_set(wrap.as.map, "message", v_string(full));
                    }
                    free(full);
                    exc = wrap;
                }

                // 取异常类型名（用于匹配带类型的 catch）
                const char *exc_type = NULL;
                if (exc.type == V_MAP) {
                    Value t = v_map_get(exc.as.map, "type");
                    if (t.type == V_STRING) exc_type = t.as.s;
                } else if (exc.type == V_OBJECT) {
                    exc_type = exc.as.obj->class_node->as.class_decl.name;
                }

                // 选择匹配的 catch 分支：类型相符 或 裸 catch(e)（type==NULL，捕获任意）
                int chosen = -1;
                for (int i = 0; i < node->as.try_stmt.catch_bodies.count; i++) {
                    AstNode *ct = node->as.try_stmt.catch_types.items[i];
                    if (ct == NULL) { chosen = i; break; }           // 裸 catch 兜底
                    if (exc_type && strcmp(ct->as.ident.name, exc_type) == 0) { chosen = i; break; }
                }

                if (chosen >= 0) {
                    Env *catch_env = env_new(env);
                    AstNode *name = node->as.try_stmt.catch_names.items[chosen];
                    if (name) env_define(catch_env, name->as.ident.name, exc);
                    eval(I, catch_env, node->as.try_stmt.catch_bodies.items[chosen]);
                } else {
                    // 没有匹配的 catch → 重新激活异常向上传播
                    I->exc_value = exc;
                    I->exc_active = 1;
                }
            }
            // finally：无论是否发生异常都执行（保留 exc/return 状态在执行后恢复）
            if (node->as.try_stmt.finally_block) {
                int saved_exc = I->exc_active;
                Value saved_exc_val = I->exc_value;
                int saved_ret = I->return_active;
                Value saved_ret_val = I->return_value;
                I->exc_active = 0;
                I->return_active = 0;
                eval(I, env, node->as.try_stmt.finally_block);
                // finally 自身未抛新异常/return 时，恢复之前挂起的状态
                if (!I->exc_active && saved_exc) { I->exc_active = 1; I->exc_value = saved_exc_val; }
                if (!I->return_active && saved_ret) { I->return_active = 1; I->return_value = saved_ret_val; }
            }
            return v_null();
        }
        case NODE_ASSERT: {
            Value v = eval(I, env, node->as.assert_stmt.expr);
            if (!v_truthy(v)) {
                // 断言失败 → 抛出可捕获的 AssetError 异常（与语言规范一致）
                Value exc = v_map();
                v_map_set(exc.as.map, "type", v_string("AssetError"));
                v_map_set(exc.as.map, "message", v_string("assertion failed"));
                I->exc_value = exc;
                I->exc_active = 1;
            }
            return v_null();
        }
        case NODE_FUNC_DECL: {
            VFunc *f = calloc(1, sizeof(VFunc));
            f->decl = node;
            f->closure = env;
            Value v = { .type = V_FN }; v.as.fn = f;
            env_define(env, node->as.func.name, v);
            return v_null();
        }
        case NODE_LAMBDA: {
            VFunc *f = calloc(1, sizeof(VFunc));
            f->decl = node;
            f->closure = env;
            Value v = { .type = V_LAMBDA }; v.as.fn = f;
            return v;
        }
        case NODE_LIST_LIT: {
            Value lst = v_list();
            for (int i = 0; i < node->as.list_lit.elements.count; i++)
                v_list_push(lst.as.list, eval(I, env, node->as.list_lit.elements.items[i]));
            return lst;
        }
        case NODE_TUPLE_LIT: {
            Value tup = v_tuple();
            for (int i = 0; i < node->as.list_lit.elements.count; i++)
                v_list_push(tup.as.list, eval(I, env, node->as.list_lit.elements.items[i]));
            return tup;
        }
        case NODE_MAP_LIT: {
            Value mp = v_map();
            int n = node->as.map_lit.keys.count;
            for (int i = 0; i < n; i++) {
                Value k = eval(I, env, node->as.map_lit.keys.items[i]);
                Value vv = eval(I, env, node->as.map_lit.values.items[i]);
                char *ks = v_to_string(k);
                v_map_set(mp.as.map, ks, vv);
                free(ks);
            }
            return mp;
        }
        case NODE_INDEX: {
            Value obj = eval(I, env, node->as.index_expr.object);
            Value idx = eval(I, env, node->as.index_expr.index);
            if (obj.type == V_LIST || obj.type == V_TUPLE) {
                int i = idx.type == V_INT ? (int)idx.as.i : 0;
                if (obj.as.list && i < 0) i += obj.as.list->len;
                return v_list_get(obj.as.list, i);
            }
            if (obj.type == V_MAP) {
                char *ks = v_to_string(idx);
                Value r = v_map_get(obj.as.map, ks);
                free(ks);
                return r;
            }
            if (obj.type == V_STRING) {
                int i = idx.type == V_INT ? (int)idx.as.i : 0;
                size_t len = obj.as.s ? strlen(obj.as.s) : 0;
                if (i < 0) i += (int)len;
                if (i < 0 || (size_t)i >= len) return v_null();
                char buf[2] = { obj.as.s[i], 0 };
                return v_string(buf);
            }
            runtime_error(I, node, "index access on non-indexable value");
            return v_null();
        }
        default:
            return v_null();
    }
}

static void exec_block(Interp *I, Env *env, NodeList *stmts) {
    for (int i = 0; i < stmts->count; i++) {
        if (I->had_error || I->return_active || I->break_active || I->exc_active) return;
        eval(I, env, stmts->items[i]);
    }
}

// ── Entry ────────────────────────────────────────────────────────────────────

static void run_program(Interp *I, Env *globals, AstNode *program) {
    if (!program || program->type != NODE_PROGRAM) return;
    NodeList *stmts = &program->as.program.stmts;
    // 先提升类与函数（reflect 也提前注册，使方法在调用前可见）
    for (int i = 0; i < stmts->count; i++) {
        AstNode *s = stmts->items[i];
        if (s->type == NODE_FUNC_DECL || s->type == NODE_CLASS_DECL || s->type == NODE_REFLECT)
            eval(I, globals, s);
    }
    for (int i = 0; i < stmts->count; i++) {
        if (I->had_error || I->return_active || I->exc_active) break;
        AstNode *s = stmts->items[i];
        if (s->type != NODE_FUNC_DECL && s->type != NODE_CLASS_DECL && s->type != NODE_REFLECT)
            eval(I, globals, s);
    }

    // 入口约定：若定义了 main 函数则自动调用（Candle 程序以 main 为入口）
    if (!I->had_error && !I->exc_active) {
        Value *mainfn = env_lookup(globals, "main");
        if (mainfn && (mainfn->type == V_FN || mainfn->type == V_LAMBDA)) {
            AstNode *decl = mainfn->as.fn->decl;
            if (decl->type == NODE_FUNC_DECL && decl->as.func.body) {
                Env *call_env = env_new(mainfn->as.fn->closure);
                exec_block(I, call_env, &decl->as.func.body->as.program.stmts);
                I->return_active = 0;
            }
        }
    }
}

// 把点号路径解析为候选文件路径并尝试读取。std.* 走 lib/std/，其它走相对路径。
// 静默读取（不打印 read_file 的找不到错误），用于探测多个候选路径。
static char *quiet_read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, size, f);
    buf[rd] = 0;
    fclose(f);
    return buf;
}

static char *resolve_module_source(const char *dotted) {
    // 构造 a/b/c 形式
    char rel[256];
    size_t j = 0;
    for (size_t i = 0; dotted[i] && j < sizeof(rel) - 1; i++)
        rel[j++] = (dotted[i] == '.') ? '/' : dotted[i];
    rel[j] = 0;

    char candidate[512];
    // 模块搜索路径：library/ 优先（Candle 标准库所在），再 lib/，再相对路径
    const char *prefixes[] = { "library/", "lib/", "", "./" };
    for (int k = 0; k < 4; k++) {
        snprintf(candidate, sizeof(candidate), "%s%s.candle", prefixes[k], rel);
        char *src = quiet_read_file(candidate);
        if (src) return src;
    }
    return NULL;
}

// 模块缓存：避免重复加载（也防循环加载）
typedef struct { char *path; Value mod; } ModuleCacheEntry;
static ModuleCacheEntry g_modcache[128];
static int g_nmodcache = 0;

static Value load_candle_module(Interp *I, const char *dotted_path) {
    // 查缓存
    for (int i = 0; i < g_nmodcache; i++)
        if (strcmp(g_modcache[i].path, dotted_path) == 0)
            return g_modcache[i].mod;

    char *src = resolve_module_source(dotted_path);
    if (!src) return v_null();

    AstNode *ast = parse(src, dotted_path);
    free(src);
    if (!ast) return v_null();

    // 在独立模块作用域里执行顶层声明（不调用 main）
    Env *modenv = env_new(g_class_env);  // 共享全局内置 + 类查找
    NodeList *stmts = &ast->as.program.stmts;
    // 提升类/函数/reflect
    for (int i = 0; i < stmts->count; i++) {
        AstNode *s = stmts->items[i];
        if (s->type == NODE_FUNC_DECL || s->type == NODE_CLASS_DECL || s->type == NODE_REFLECT)
            eval(I, modenv, s);
    }
    // 执行其余顶层语句（const/alias/var 等），跳过 main
    for (int i = 0; i < stmts->count; i++) {
        AstNode *s = stmts->items[i];
        if (s->type == NODE_FUNC_DECL || s->type == NODE_CLASS_DECL || s->type == NODE_REFLECT)
            continue;
        if (s->type == NODE_FUNC_DECL) continue;
        eval(I, modenv, s);
        if (I->had_error || I->exc_active) break;
    }

    // 收集导出符号：若有 export 声明则按白名单，否则导出全部顶层绑定（main 除外）
    Value ns = v_map();
    AstNode *export_node = NULL;
    for (int i = 0; i < stmts->count; i++)
        if (stmts->items[i]->type == NODE_EXPORT) { export_node = stmts->items[i]; break; }

    if (export_node && !export_node->as.export_stmt.is_wildcard &&
        export_node->as.export_stmt.items.count > 0) {
        // 白名单导出
        for (int i = 0; i < export_node->as.export_stmt.items.count; i++) {
            const char *nm = export_node->as.export_stmt.items.items[i]->as.ident.name;
            Value *v = env_lookup(modenv, nm);
            if (v) v_map_set(ns.as.map, nm, *v);
        }
    } else {
        // 默认：导出模块作用域内所有绑定（排除 main 与从父级继承的内置）
        for (int i = 0; i < modenv->len; i++) {
            const char *nm = modenv->names[i];
            if (strcmp(nm, "main") == 0) continue;
            // export * except {...}
            int excluded = 0;
            if (export_node && export_node->as.export_stmt.is_wildcard) {
                for (int e = 0; e < export_node->as.export_stmt.except_items.count; e++)
                    if (strcmp(export_node->as.export_stmt.except_items.items[e]->as.ident.name, nm) == 0)
                        { excluded = 1; break; }
            }
            if (!excluded) v_map_set(ns.as.map, nm, modenv->vals[i]);
        }
    }

    // 入缓存（注意：AST 与 modenv 故意不释放，导出的函数闭包仍引用它们）
    if (g_nmodcache < 128) {
        g_modcache[g_nmodcache].path = strdup(dotted_path);
        g_modcache[g_nmodcache].mod = ns;
        g_nmodcache++;
    }
    return ns;
}

int interp_run(AstNode *program, const char *filename) {
    Interp I = { .filename = filename };
    Env *globals = env_new(NULL);
    g_class_env = globals;
    builtins_register(globals);

    run_program(&I, globals, program);

    if (I.exc_active) {
        char *m = v_to_string(I.exc_value);
        fprintf(stderr, "%s: uncaught exception: %s\n", filename, m);
        free(m);
        I.had_error = 1;
    }

    int rc = I.had_error ? 1 : 0;
    env_free(globals);
    return rc;
}

// ── REPL ─────────────────────────────────────────────────────────────────────

int interp_repl(void) {
    Interp I = { .filename = "<repl>" };
    Env *globals = env_new(NULL);
    g_class_env = globals;
    builtins_register(globals);

    printf("Candle REPL — :quit to exit, :help for help\n");
    char line[4096];
    int n = 1;
    for (;;) {
        printf("[%d] >> ", n); fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) { printf("\n"); break; }
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = 0;
        if (len == 0) continue;
        if (strcmp(line, ":quit") == 0 || strcmp(line, ":q") == 0) break;
        if (strcmp(line, ":help") == 0 || strcmp(line, ":h") == 0) {
            printf("  :quit / :q   exit\n");
            printf("  :help / :h   this help\n");
            printf("  any candle expression or statement (auto ';' appended)\n");
            continue;
        }

        // Auto append ';' if not block-end / not already terminated
        char buf[4200];
        char tail = line[len-1];
        if (tail != ';' && tail != '}')
            snprintf(buf, sizeof(buf), "%s;", line);
        else
            snprintf(buf, sizeof(buf), "%s", line);

        AstNode *ast = parse(buf, "<repl>");
        if (!ast) { n++; continue; }

        // If it's a single expression statement, eval and echo non-null result
        if (ast->type == NODE_PROGRAM && ast->as.program.stmts.count == 1) {
            AstNode *s = ast->as.program.stmts.items[0];
            if (s->type == NODE_EXPR_STMT && s->as.program.stmts.count == 1) {
                AstNode *expr = s->as.program.stmts.items[0];
                if (expr->type != NODE_ASSIGN) {
                    Value v = eval(&I, globals, expr);
                    if (!I.had_error && !I.exc_active && v.type != V_NULL) {
                        char *r = v_to_string(v);
                        printf("=> %s\n", r);
                        free(r);
                    }
                    if (I.exc_active) {
                        char *m = v_to_string(I.exc_value);
                        fprintf(stderr, "uncaught: %s\n", m);
                        free(m);
                        I.exc_active = 0;
                    }
                    I.had_error = 0;
                    ast_free(ast);
                    n++;
                    continue;
                }
            }
        }

        I.had_error = 0;
        I.return_active = 0;
        I.break_active = 0;
        I.exc_active = 0;
        run_program(&I, globals, ast);

        if (I.exc_active) {
            char *m = v_to_string(I.exc_value);
            fprintf(stderr, "uncaught: %s\n", m);
            free(m);
            I.exc_active = 0;
        }
        ast_free(ast);
        n++;
    }
    env_free(globals);
    return 0;
}
