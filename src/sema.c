#include "sema.h"
#include "token.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ?? Symbol table ??????????????????????????????????????????????????????????????

typedef struct Symbol {
    char *name;
    char *type;       // Candle type string
    struct Symbol *next;
} Symbol;

typedef struct Scope {
    Symbol *head;
    struct Scope *parent;
} Scope;

typedef struct {
    Scope *scope;
    const char *current_class;
    struct { char *cls; char *field; char *type; } fields[512];
    int nfields;
    struct { char *cls; char *method; } methods[512];
    int nmethods;
    char *classes[256];
    int nclasses;
} Sema;

static void sema_class_add(Sema *s, const char *cls) {
    if (s->nclasses >= 256) return;
    s->classes[s->nclasses++] = strdup(cls);
}

static int sema_class_exists(Sema *s, const char *cls) {
    for (int i = 0; i < s->nclasses; i++)
        if (strcmp(s->classes[i], cls) == 0) return 1;
    return 0;
}

static Scope *scope_push(Sema *s) {
    Scope *sc = calloc(1, sizeof(Scope));
    sc->parent = s->scope;
    s->scope = sc;
    return sc;
}

static void scope_pop(Sema *s) {
    Scope *sc = s->scope;
    s->scope = sc->parent;
    Symbol *sym = sc->head;
    while (sym) { Symbol *n = sym->next; free(sym); sym = n; }
    free(sc);
}

static void scope_define(Sema *s, const char *name, const char *type) {
    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = strdup(name);
    sym->type = type ? strdup(type) : NULL;
    sym->next = s->scope->head;
    s->scope->head = sym;
}

static const char *scope_lookup(Sema *s, const char *name) {
    for (Scope *sc = s->scope; sc; sc = sc->parent)
        for (Symbol *sym = sc->head; sym; sym = sym->next)
            if (strcmp(sym->name, name) == 0) return sym->type;
    return NULL;
}

static void sema_field_add(Sema *s, const char *cls, const char *field, const char *type) {
    if (s->nfields >= 512) return;
    s->fields[s->nfields].cls   = strdup(cls);
    s->fields[s->nfields].field = strdup(field);
    s->fields[s->nfields].type  = type ? strdup(type) : NULL;
    s->nfields++;
}

static const char *sema_field_lookup(Sema *s, const char *cls, const char *field) {
    for (int i = 0; i < s->nfields; i++)
        if (strcmp(s->fields[i].cls, cls) == 0 &&
            strcmp(s->fields[i].field, field) == 0)
            return s->fields[i].type;
    return NULL;
}

static void sema_method_add(Sema *s, const char *cls, const char *method) {
    if (s->nmethods >= 512) return;
    s->methods[s->nmethods].cls    = strdup(cls);
    s->methods[s->nmethods].method = strdup(method);
    s->nmethods++;
}

static int sema_method_exists(Sema *s, const char *cls, const char *method) {
    for (int i = 0; i < s->nmethods; i++)
        if (strcmp(s->methods[i].cls, cls) == 0 &&
            strcmp(s->methods[i].method, method) == 0) return 1;
    return 0;
}

// ?? Forward declarations ??????????????????????????????????????????????????????

static void sema_node(Sema *s, AstNode *node);
static const char *sema_expr(Sema *s, AstNode *node);
static void sema_stmts(Sema *s, NodeList *stmts);

// ?? Type helpers ??????????????????????????????????????????????????????????????

static const char *arith_result(const char *a, const char *b) {
    if (!a || !b) return NULL;
    if (strcmp(a, "double") == 0 || strcmp(b, "double") == 0) return "double";
    if (strcmp(a, "int") == 0 && strcmp(b, "int") == 0) return "int";
    return NULL;
}

// ?? Expression type inference ?????????????????????????????????????????????????

static const char *sema_expr(Sema *s, AstNode *node) {
    if (!node) return NULL;
    switch (node->type) {
        case NODE_INT_LIT:    node->resolved_type = "int";    return "int";
        case NODE_FLOAT_LIT:  node->resolved_type = "double"; return "double";
        case NODE_BOOL_LIT:   node->resolved_type = "bool";   return "bool";
        case NODE_NULL_LIT:   node->resolved_type = NULL;     return NULL;
        case NODE_STRING_LIT: node->resolved_type = "string"; return "string";

        case NODE_IDENT: {
            const char *t = scope_lookup(s, node->as.ident.name);
            node->resolved_type = t ? (char*)t : NULL;
            // mark bare field access inside a method
            if (s->current_class &&
                sema_field_lookup(s, s->current_class, node->as.ident.name))
                node->needs_self = 1;
            return t;
        }

        case NODE_BINARY: {
            const char *lt = sema_expr(s, node->as.binary.left);
            const char *rt = sema_expr(s, node->as.binary.right);
            TokenType op = node->as.binary.op;
            const char *t = NULL;
            if (op == TK_EQ || op == TK_NEQ || op == TK_LT || op == TK_LTE ||
                op == TK_GT || op == TK_GTE || op == TK_AND || op == TK_OR)
                t = "bool";
            else if (op == TK_PLUS && lt && rt &&
                     strcmp(lt, "string") == 0 && strcmp(rt, "string") == 0)
                t = "string";
            else
                t = arith_result(lt, rt);
            node->resolved_type = (char*)t;
            return t;
        }

        case NODE_TERNARY: {
            sema_expr(s, node->as.ternary.cond);
            const char *tt = sema_expr(s, node->as.ternary.then_expr);
            const char *et = sema_expr(s, node->as.ternary.else_expr);
            // ??????????????
            const char *t = (tt && et && strcmp(tt, et) == 0) ? tt : NULL;
            node->resolved_type = (char*)t;
            return t;
        }

        case NODE_UNARY: {
            const char *t = sema_expr(s, node->as.unary.operand);
            if (node->as.unary.op == TK_BANG) t = "bool";
            node->resolved_type = (char*)t;
            return t;
        }

        case NODE_CALL: {
            const char *t = NULL;
            if (node->as.call.callee->type == NODE_IDENT) {
                const char *fname = node->as.call.callee->as.ident.name;
                /* Built-in type conversion functions */
                if (strcmp(fname, "String") == 0)       t = "string";
                else if (strcmp(fname, "int") == 0)     t = "int";
                else if (strcmp(fname, "double") == 0)  t = "double";
                else if (strcmp(fname, "bool") == 0)    t = "bool";
                else if (sema_class_exists(s, fname)) {
                    node->is_ctor_call = 1;
                    t = fname;
                } else {
                    t = scope_lookup(s, fname);
                    if (s->current_class && sema_method_exists(s, s->current_class, fname))
                        node->self_call_class = strdup(s->current_class);
                }
            } else {
                sema_expr(s, node->as.call.callee);
            }
            for (int i = 0; i < node->as.call.args.count; i++)
                sema_expr(s, node->as.call.args.items[i]);
            node->resolved_type = (char*)t;
            return t;
        }

        case NODE_MEMBER: {
            const char *obj_type = sema_expr(s, node->as.member.object);
            const char *t = NULL;
            if (obj_type)
                t = sema_field_lookup(s, obj_type, node->as.member.name);
            node->resolved_type = (char*)t;
            return t;
        }

        case NODE_INDEX: {
            sema_expr(s, node->as.index_expr.object);
            sema_expr(s, node->as.index_expr.index);
            // can't infer element type without generics
            node->resolved_type = NULL;
            return NULL;
        }

        case NODE_ASSIGN: {
            const char *t = sema_expr(s, node->as.assign.value);
            sema_expr(s, node->as.assign.target);
            node->resolved_type = (char*)t;
            return t;
        }

        case NODE_LIST_LIT:
            for (int i = 0; i < node->as.list_lit.elements.count; i++)
                sema_expr(s, node->as.list_lit.elements.items[i]);
            node->resolved_type = "list";
            return "list";

        case NODE_MAP_LIT:
            for (int i = 0; i < node->as.map_lit.keys.count; i++) {
                sema_expr(s, node->as.map_lit.keys.items[i]);
                sema_expr(s, node->as.map_lit.values.items[i]);
            }
            node->resolved_type = "map";
            return "map";

        case NODE_LAMBDA: {
            scope_push(s);
            for (int i = 0; i < node->as.lambda.params.count; i++) {
                AstNode *p = node->as.lambda.params.items[i];
                scope_define(s, p->as.var.name, p->as.var.type_name);
            }
            const char *rt = NULL;
            AstNode *body = node->as.lambda.body;
            if (body && body->type == NODE_BLOCK) {
                sema_stmts(s, &body->as.program.stmts);
                for (int i = 0; i < body->as.program.stmts.count; i++) {
                    AstNode *st = body->as.program.stmts.items[i];
                    if (st->type == NODE_RETURN && st->as.return_stmt.value)
                        rt = st->as.return_stmt.value->resolved_type;
                }
            } else if (body) {
                rt = sema_expr(s, body);
            }
            scope_pop(s);
            node->resolved_type = (char*)rt;
            return rt;
        }

        default:
            return NULL;
    }
}

// ?? Statement / declaration analysis ?????????????????????????????????????????

static void sema_stmts(Sema *s, NodeList *stmts) {
    for (int i = 0; i < stmts->count; i++)
        sema_node(s, stmts->items[i]);
}

static void sema_node(Sema *s, AstNode *node) {
    if (!node) return;
    switch (node->type) {
        case NODE_PROGRAM:
        case NODE_BLOCK:
            sema_stmts(s, &node->as.program.stmts);
            break;

        case NODE_VAR_DECL:
            if (node->as.var.init) {
                const char *it = sema_expr(s, node->as.var.init);
                // prefer explicit type annotation
                const char *t = node->as.var.type_name ? node->as.var.type_name : it;
                scope_define(s, node->as.var.name, t);
                node->resolved_type = (char*)t;
            } else {
                scope_define(s, node->as.var.name, node->as.var.type_name);
                node->resolved_type = (char*)node->as.var.type_name;
            }
            break;

        case NODE_CONST_DECL: {
            const char *t = sema_expr(s, node->as.constant.value);
            scope_define(s, node->as.constant.name, t);
            node->resolved_type = (char*)t;
            break;
        }

        case NODE_ALIAS: {
            const char *t = sema_expr(s, node->as.alias.value);
            if (node->as.alias.assert_type) t = node->as.alias.assert_type;
            scope_define(s, node->as.alias.name, t);
            node->resolved_type = (char*)t;
            break;
        }

        case NODE_FUNC_DECL:
        case NODE_METHOD_DECL: {
            // register function name ? return type
            scope_define(s, node->as.func.name, node->as.func.return_type);
            if (node->as.func.body) {
                scope_push(s);
                for (int i = 0; i < node->as.func.params.count; i++) {
                    AstNode *p = node->as.func.params.items[i];
                    scope_define(s, p->as.var.name, p->as.var.type_name);
                }
                sema_stmts(s, &node->as.func.body->as.program.stmts);
                scope_pop(s);
            }
            break;
        }

        case NODE_CLASS_DECL: {
            const char *cls = node->as.class_decl.name;
            sema_class_add(s, cls);
            for (int i = 0; i < node->as.class_decl.members.count; i++) {
                AstNode *m = node->as.class_decl.members.items[i];
                if (m->type == NODE_FIELD_DECL)
                    sema_field_add(s, cls, m->as.var.name, m->as.var.type_name);
                else if (m->type == NODE_METHOD_DECL)
                    sema_method_add(s, cls, m->as.func.name);
            }
            const char *prev = s->current_class;
            s->current_class = cls;
            for (int i = 0; i < node->as.class_decl.members.count; i++) {
                AstNode *m = node->as.class_decl.members.items[i];
                if (m->type == NODE_METHOD_DECL && m->as.func.body) {
                    scope_push(s);
                    scope_define(s, "self", cls);
                    // define fields as locals so bare names resolve
                    for (int j = 0; j < node->as.class_decl.members.count; j++) {
                        AstNode *f = node->as.class_decl.members.items[j];
                        if (f->type == NODE_FIELD_DECL)
                            scope_define(s, f->as.var.name, f->as.var.type_name);
                    }
                    for (int j = 0; j < m->as.func.params.count; j++) {
                        AstNode *p = m->as.func.params.items[j];
                        scope_define(s, p->as.var.name, p->as.var.type_name);
                    }
                    sema_stmts(s, &m->as.func.body->as.program.stmts);
                    scope_pop(s);
                } else if (m->type == NODE_FACTORY_DECL && m->as.factory.body) {
                    scope_push(s);
                    for (int j = 0; j < m->as.factory.params.count; j++) {
                        AstNode *p = m->as.factory.params.items[j];
                        scope_define(s, p->as.var.name, p->as.var.type_name);
                    }
                    sema_stmts(s, &m->as.factory.body->as.program.stmts);
                    scope_pop(s);
                }
            }
            s->current_class = prev;
            break;
        }

        case NODE_IF:
            sema_expr(s, node->as.if_stmt.condition);
            sema_node(s, node->as.if_stmt.then_block);
            for (int i = 0; i < node->as.if_stmt.elif_conds.count; i++) {
                sema_expr(s, node->as.if_stmt.elif_conds.items[i]);
                sema_node(s, node->as.if_stmt.elif_blocks.items[i]);
            }
            sema_node(s, node->as.if_stmt.else_block);
            break;

        case NODE_WHEN:
            sema_expr(s, node->as.when_stmt.condition);
            sema_node(s, node->as.when_stmt.body);
            break;

        case NODE_FOR: {
            AstNode *iter = node->as.for_stmt.iter;
            scope_push(s);
            // infer iter var type from first arg
            const char *var_type = "int";
            if (iter->as.iter.args.count == 1) {
                AstNode *arg = iter->as.iter.args.items[0];
                const char *at = sema_expr(s, arg);
                if (at && strcmp(at, "list") == 0) var_type = NULL; // element type unknown
                else if (at && strcmp(at, "string") == 0) var_type = "string";
                // store inferred collection type on iter node
                iter->resolved_type = (char*)at;
            } else {
                for (int i = 0; i < iter->as.iter.args.count; i++)
                    sema_expr(s, iter->as.iter.args.items[i]);
            }
            scope_define(s, iter->as.iter.var_name, var_type);
            sema_node(s, node->as.for_stmt.body);
            scope_pop(s);
            break;
        }

        case NODE_RETURN:
            sema_expr(s, node->as.return_stmt.value);
            break;

        case NODE_THROW:
            sema_expr(s, node->as.throw_stmt.value);
            break;

        case NODE_ASSERT:
            sema_expr(s, node->as.assert_stmt.expr);
            break;

        case NODE_TRY:
            sema_node(s, node->as.try_stmt.body);
            for (int i = 0; i < node->as.try_stmt.catch_bodies.count; i++)
                sema_node(s, node->as.try_stmt.catch_bodies.items[i]);
            break;

        case NODE_EXPR_STMT:
            if (node->as.program.stmts.count > 0)
                sema_expr(s, node->as.program.stmts.items[0]);
            break;

        case NODE_PARALLEL:
            for (int i = 0; i < node->as.parallel.sections.count; i++)
                sema_node(s, node->as.parallel.sections.items[i]);
            break;

        case NODE_SIGNAL:
        case NODE_DELAY:
            sema_node(s, node->as.signal.body);
            break;

        default:
            break;
    }
}

// ?? Entry point ???????????????????????????????????????????????????????????????

void sema_run(AstNode *program) {
    Sema s = {0};
    scope_push(&s);
    // built-ins
    scope_define(&s, "print",  "void");
    scope_define(&s, "true",   "bool");
    scope_define(&s, "false",  "bool");
    scope_define(&s, "null",   NULL);
    sema_node(&s, program);
    scope_pop(&s);
}
