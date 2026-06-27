#include "codegen.h"
#include "token.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static void emit(Codegen *g, const char *fmt, ...);
static void emit_node(Codegen *g, AstNode *node);
static void emit_expr(Codegen *g, AstNode *node);
static void emit_stmts(Codegen *g, NodeList *stmts);

static char *g_classes[256];
static AstNode *g_class_nodes[256];
static int g_nclasses = 0;
static void register_class(const char *name) {
    if (g_nclasses >= 256) return;
    for (int i = 0; i < g_nclasses; i++)
        if (strcmp(g_classes[i], name) == 0) return;
    g_classes[g_nclasses] = strdup(name);
    g_class_nodes[g_nclasses] = NULL;
    g_nclasses++;
}
static void register_class_node(AstNode *n) {
    register_class(n->as.class_decl.name);
    for (int i = 0; i < g_nclasses; i++)
        if (strcmp(g_classes[i], n->as.class_decl.name) == 0)
            g_class_nodes[i] = n;
}
static AstNode *get_class_node(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < g_nclasses; i++)
        if (strcmp(g_classes[i], name) == 0) return g_class_nodes[i];
    return NULL;
}
static const char *find_field_type(const char *cls, const char *field) {
    AstNode *cn = get_class_node(cls);
    while (cn) {
        for (int i = 0; i < cn->as.class_decl.members.count; i++) {
            AstNode *m = cn->as.class_decl.members.items[i];
            if (m->type == NODE_FIELD_DECL && strcmp(m->as.var.name, field) == 0)
                return m->as.var.type_name;
        }
        cn = get_class_node(cn->as.class_decl.parent);
    }
    return NULL;
}
static int is_class(const char *name) {
    if (!name) return 0;
    for (int i = 0; i < g_nclasses; i++)
        if (strcmp(g_classes[i], name) == 0) return 1;
    return 0;
}

static const char *g_current_class = NULL;
static int s_field_in_class(const char *name) {
    if (!g_current_class) return 0;
    return find_field_type(g_current_class, name) != NULL;
}

static char *g_module_aliases[64];
static int g_nmodule_aliases = 0;
static void register_module_alias(const char *name) {
    if (!name || g_nmodule_aliases >= 64) return;
    for (int i = 0; i < g_nmodule_aliases; i++)
        if (strcmp(g_module_aliases[i], name) == 0) return;
    g_module_aliases[g_nmodule_aliases++] = strdup(name);
}
static int is_module_alias(const char *name) {
    if (!name) return 0;
    for (int i = 0; i < g_nmodule_aliases; i++)
        if (strcmp(g_module_aliases[i], name) == 0) return 1;
    return 0;
}

static AstNode *g_lambdas[512];
static int g_nlambdas = 0;
static int collect_lambda(AstNode *n) {
    if (g_nlambdas >= 512) return -1;
    n->lambda_id = g_nlambdas;
    g_lambdas[g_nlambdas++] = n;
    return n->lambda_id;
}
static void collect_lambdas_walk(AstNode *n) {
    if (!n) return;
    if (n->type == NODE_LAMBDA) collect_lambda(n);
    switch (n->type) {
        case NODE_PROGRAM: case NODE_BLOCK:
            for (int i = 0; i < n->as.program.stmts.count; i++) collect_lambdas_walk(n->as.program.stmts.items[i]);
            break;
        case NODE_FUNC_DECL: case NODE_METHOD_DECL:
            collect_lambdas_walk(n->as.func.body);
            break;
        case NODE_CLASS_DECL:
            for (int i = 0; i < n->as.class_decl.members.count; i++) collect_lambdas_walk(n->as.class_decl.members.items[i]);
            break;
        case NODE_FACTORY_DECL:
            collect_lambdas_walk(n->as.factory.body); break;
        case NODE_VAR_DECL: case NODE_FIELD_DECL:
            collect_lambdas_walk(n->as.var.init); break;
        case NODE_CONST_DECL:
            collect_lambdas_walk(n->as.constant.value); break;
        case NODE_ALIAS:
            collect_lambdas_walk(n->as.alias.value); break;
        case NODE_IF:
            collect_lambdas_walk(n->as.if_stmt.condition);
            collect_lambdas_walk(n->as.if_stmt.then_block);
            for (int i = 0; i < n->as.if_stmt.elif_conds.count; i++) collect_lambdas_walk(n->as.if_stmt.elif_conds.items[i]);
            for (int i = 0; i < n->as.if_stmt.elif_blocks.count; i++) collect_lambdas_walk(n->as.if_stmt.elif_blocks.items[i]);
            collect_lambdas_walk(n->as.if_stmt.else_block);
            break;
        case NODE_WHEN:
            collect_lambdas_walk(n->as.when_stmt.condition);
            collect_lambdas_walk(n->as.when_stmt.body); break;
        case NODE_FOR:
            for (int i = 0; i < n->as.for_stmt.iter->as.iter.args.count; i++)
                collect_lambdas_walk(n->as.for_stmt.iter->as.iter.args.items[i]);
            collect_lambdas_walk(n->as.for_stmt.body); break;
        case NODE_RETURN: collect_lambdas_walk(n->as.return_stmt.value); break;
        case NODE_THROW: collect_lambdas_walk(n->as.throw_stmt.value); break;
        case NODE_ASSERT: collect_lambdas_walk(n->as.assert_stmt.expr); break;
        case NODE_TRY:
            collect_lambdas_walk(n->as.try_stmt.body);
            for (int i = 0; i < n->as.try_stmt.catch_bodies.count; i++) collect_lambdas_walk(n->as.try_stmt.catch_bodies.items[i]);
            break;
        case NODE_BINARY:
            collect_lambdas_walk(n->as.binary.left);
            collect_lambdas_walk(n->as.binary.right); break;
        case NODE_TERNARY:
            collect_lambdas_walk(n->as.ternary.cond);
            collect_lambdas_walk(n->as.ternary.then_expr);
            collect_lambdas_walk(n->as.ternary.else_expr); break;
        case NODE_UNARY: case NODE_POSTFIX_UNARY:
            collect_lambdas_walk(n->as.unary.operand); break;
        case NODE_CALL:
            collect_lambdas_walk(n->as.call.callee);
            for (int i = 0; i < n->as.call.args.count; i++) collect_lambdas_walk(n->as.call.args.items[i]);
            break;
        case NODE_MEMBER: collect_lambdas_walk(n->as.member.object); break;
        case NODE_INDEX:
            collect_lambdas_walk(n->as.index_expr.object);
            collect_lambdas_walk(n->as.index_expr.index); break;
        case NODE_ASSIGN:
            collect_lambdas_walk(n->as.assign.target);
            collect_lambdas_walk(n->as.assign.value); break;
        case NODE_LAMBDA:
            collect_lambdas_walk(n->as.lambda.body); break;
        case NODE_LIST_LIT:
            for (int i = 0; i < n->as.list_lit.elements.count; i++) collect_lambdas_walk(n->as.list_lit.elements.items[i]);
            break;
        case NODE_TUPLE_LIT:
            for (int i = 0; i < n->as.list_lit.elements.count; i++) collect_lambdas_walk(n->as.list_lit.elements.items[i]);
            break;
        case NODE_MAP_LIT:
            for (int i = 0; i < n->as.map_lit.keys.count; i++) collect_lambdas_walk(n->as.map_lit.keys.items[i]);
            for (int i = 0; i < n->as.map_lit.values.count; i++) collect_lambdas_walk(n->as.map_lit.values.items[i]);
            break;
        case NODE_EXPR_STMT:
            for (int i = 0; i < n->as.program.stmts.count; i++) collect_lambdas_walk(n->as.program.stmts.items[i]);
            break;
        case NODE_PARALLEL:
            for (int i = 0; i < n->as.parallel.sections.count; i++) collect_lambdas_walk(n->as.parallel.sections.items[i]);
            break;
        case NODE_SIGNAL: case NODE_DELAY:
            collect_lambdas_walk(n->as.signal.body); break;
        default: break;
    }
}

static void indent(Codegen *g) {
    for (int i = 0; i < g->indent; i++) fprintf(g->out, "    ");
}

static void emit(Codegen *g, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(g->out, fmt, args);
    va_end(args);
}

// Map Candle type to C type
static const char *c_type(const char *t) {
    if (!t) return "void";
    if (strcmp(t, "int") == 0)    return "candle_int";
    if (strcmp(t, "double") == 0) return "candle_double";
    if (strcmp(t, "bool") == 0)   return "candle_bool";
    if (strcmp(t, "string") == 0) return "candle_string";
    if (strcmp(t, "void") == 0)   return "void";
    if (strcmp(t, "auto") == 0)   return "candle_int";
    if (strcmp(t, "list") == 0)   return "CandleList*";
    if (strcmp(t, "map") == 0)    return "CandleMap*";
    if (strncmp(t, "int?", 4) == 0)    return "candle_int*";
    if (strncmp(t, "double?", 7) == 0) return "candle_double*";
    if (strncmp(t, "bool?", 5) == 0)   return "candle_bool*";
    if (strncmp(t, "string?", 7) == 0) return "candle_string*";
    if (is_class(t)) {
        static char buf[128];
        snprintf(buf, sizeof(buf), "%s*", t);
        return buf;
    }
    return t;
}

static const char *op_str(TokenType op) {
    switch (op) {
        case TK_PLUS:    return "+";
        case TK_MINUS:   return "-";
        case TK_STAR:    return "*";
        case TK_SLASH:   return "/";
        case TK_MOD:     return "%";
        case TK_EQ:      return "==";
        case TK_NEQ:     return "!=";
        case TK_LT:      return "<";
        case TK_LTE:     return "<=";
        case TK_GT:      return ">";
        case TK_GTE:     return ">=";
        case TK_AND:     return "&&";
        case TK_OR:      return "||";
        case TK_BIT_AND: return "&";
        case TK_BIT_OR:  return "|";
        case TK_BIT_XOR: return "^";
        case TK_ASSIGN:       return "=";
        case TK_PLUS_ASSIGN:  return "+=";
        case TK_MINUS_ASSIGN: return "-=";
        case TK_STAR_ASSIGN:  return "*=";
        case TK_SLASH_ASSIGN: return "/=";
        case TK_MOD_ASSIGN:   return "%=";
        default: return "?";
    }
}

static void emit_expr(Codegen *g, AstNode *node) {
    if (!node) return;
    switch (node->type) {
        case NODE_INT_LIT:
            emit(g, "((candle_int)%lld)", node->as.int_lit.value);
            break;
        case NODE_FLOAT_LIT:
            emit(g, "%g", node->as.float_lit.value);
            break;
        case NODE_BOOL_LIT:
            emit(g, "%s", node->as.bool_lit.value ? "((candle_bool)1)" : "((candle_bool)0)");
            break;
        case NODE_NULL_LIT:
            emit(g, "NULL");
            break;
        case NODE_STRING_LIT: {
            const char *s = node->as.string_lit.value;
            int len = strlen(s);
            if (len < 2) { emit(g, "candle_str(\"\")"); break; }
            int start = 0;
            int is_fmt = 0;
            if (s[0] == 'f') { start = 1; is_fmt = 1; }
            else if (s[0] == 'r') { start = 1; }
            char open = s[start];
            const char *inner = s + start + 1;
            int inner_len = len - start - 2;
            if (inner_len < 0) inner_len = 0;
            if (!is_fmt) {
                emit(g, "candle_str(\"");
                for (int i = 0; i < inner_len; i++) {
                    char c = inner[i];
                    if (c == '"') emit(g, "\\\"");
                    else if (c == '\\') emit(g, "\\\\");
                    else if (open == '\'' && c == '\'') emit(g, "'");
                    else fputc(c, g->out);
                }
                emit(g, "\")");
                break;
            }
            char names[32][128];
            int nargs = 0;
            emit(g, "candle_fmt(\"");
            for (int i = 0; i < inner_len; i++) {
                char c = inner[i];
                if (c == '{' && i + 1 < inner_len && inner[i+1] == '{') { emit(g, "{"); i++; continue; }
                if (c == '}' && i + 1 < inner_len && inner[i+1] == '}') { emit(g, "}"); i++; continue; }
                if (c == '{') {
                    int j = i + 1, k = 0;
                    while (j < inner_len && inner[j] != '}' && k < 127) {
                        names[nargs][k++] = inner[j++];
                    }
                    names[nargs][k] = 0;
                    if (j < inner_len) j++;
                    emit(g, "%%s");
                    nargs++;
                    i = j - 1;
                    continue;
                }
                if (c == '"') emit(g, "\\\"");
                else if (c == '\\') emit(g, "\\\\");
                else if (open == '\'' && c == '\'') emit(g, "'");
                else fputc(c, g->out);
            }
            emit(g, "\"");
            for (int a = 0; a < nargs; a++) {
                emit(g, ", candle_to_str(");
                if (s_field_in_class(names[a])) emit(g, "self->%s", names[a]);
                else emit(g, "%s", names[a]);
                emit(g, ")");
            }
            emit(g, ")");
            break;
        }
        case NODE_IDENT:
            if (node->needs_self)
                emit(g, "self->%s", node->as.ident.name);
            else
                emit(g, "%s", strcmp(node->as.ident.name, "this") == 0 ? "self" : node->as.ident.name);
            break;
        case NODE_BINARY: {
            const char *lt = node->as.binary.left->resolved_type;
            const char *rt = node->as.binary.right->resolved_type;
            if (node->as.binary.op == TK_PLUS &&
                lt && rt &&
                strcmp(lt, "string") == 0 && strcmp(rt, "string") == 0) {
                emit(g, "candle_str_concat(");
                emit_expr(g, node->as.binary.left);
                emit(g, ", ");
                emit_expr(g, node->as.binary.right);
                emit(g, ")");
            } else {
                emit(g, "(");
                emit_expr(g, node->as.binary.left);
                emit(g, " %s ", op_str(node->as.binary.op));
                emit_expr(g, node->as.binary.right);
                emit(g, ")");
            }
            break;
        }
        case NODE_UNARY:
            if (node->as.unary.op == TK_BANG) emit(g, "!");
            else if (node->as.unary.op == TK_MINUS) emit(g, "-");
            else if (node->as.unary.op == TK_BIT_NOT) emit(g, "~");
            else if (node->as.unary.op == TK_INCR) emit(g, "++");
            else if (node->as.unary.op == TK_DECR) emit(g, "--");
            emit_expr(g, node->as.unary.operand);
            break;
        case NODE_TERNARY:
            emit(g, "(");
            emit_expr(g, node->as.ternary.cond);
            emit(g, " ? ");
            emit_expr(g, node->as.ternary.then_expr);
            emit(g, " : ");
            emit_expr(g, node->as.ternary.else_expr);
            emit(g, ")");
            break;
        case NODE_ASSIGN: {
            const char *tt = node->as.assign.target->resolved_type;
            const char *vt = node->as.assign.value->resolved_type;
            if (node->as.assign.op == TK_PLUS_ASSIGN &&
                tt && vt &&
                strcmp(tt, "string") == 0 && strcmp(vt, "string") == 0) {
                emit_expr(g, node->as.assign.target);
                emit(g, " = candle_str_concat(");
                emit_expr(g, node->as.assign.target);
                emit(g, ", ");
                emit_expr(g, node->as.assign.value);
                emit(g, ")");
            } else {
                emit_expr(g, node->as.assign.target);
                emit(g, " %s ", op_str(node->as.assign.op));
                emit_expr(g, node->as.assign.value);
            }
            break;
        }
        case NODE_CALL:
            if (node->is_ctor_call && node->as.call.callee->type == NODE_IDENT) {
                emit(g, "%s__new(", node->as.call.callee->as.ident.name);
                for (int i = 0; i < node->as.call.args.count; i++) {
                    if (i > 0) emit(g, ", ");
                    emit_expr(g, node->as.call.args.items[i]);
                }
                emit(g, ")");
            } else if (node->self_call_class && node->as.call.callee->type == NODE_IDENT) {
                emit(g, "%s__%s(self", node->self_call_class, node->as.call.callee->as.ident.name);
                for (int i = 0; i < node->as.call.args.count; i++) {
                    emit(g, ", ");
                    emit_expr(g, node->as.call.args.items[i]);
                }
                emit(g, ")");
            } else if (node->as.call.callee->type == NODE_MEMBER) {
                AstNode *m = node->as.call.callee;
                AstNode *obj = m->as.member.object;
                const char *ot = obj->resolved_type;
                if (obj->type == NODE_IDENT && is_module_alias(obj->as.ident.name)) {
                    emit(g, "%s_%s(", obj->as.ident.name, m->as.member.name);
                    for (int i = 0; i < node->as.call.args.count; i++) {
                        if (i > 0) emit(g, ", ");
                        emit_expr(g, node->as.call.args.items[i]);
                    }
                    emit(g, ")");
                } else if (ot && strcmp(ot, "list") == 0) {
                    emit(g, "candle_list_%s(", m->as.member.name);
                    emit_expr(g, obj);
                    for (int i = 0; i < node->as.call.args.count; i++) {
                        emit(g, ", ");
                        emit_expr(g, node->as.call.args.items[i]);
                    }
                    emit(g, ")");
                } else if (ot && is_class(ot)) {
                    emit(g, "%s__%s(", ot, m->as.member.name);
                    emit_expr(g, obj);
                    for (int i = 0; i < node->as.call.args.count; i++) {
                        emit(g, ", ");
                        emit_expr(g, node->as.call.args.items[i]);
                    }
                    emit(g, ")");
                } else if (obj->type == NODE_IDENT && is_class(obj->as.ident.name)) {
                    emit(g, "%s__%s(", obj->as.ident.name, m->as.member.name);
                    for (int i = 0; i < node->as.call.args.count; i++) {
                        if (i > 0) emit(g, ", ");
                        emit_expr(g, node->as.call.args.items[i]);
                    }
                    emit(g, ")");
                } else {
                    emit_expr(g, m);
                    emit(g, "(");
                    for (int i = 0; i < node->as.call.args.count; i++) {
                        if (i > 0) emit(g, ", ");
                        emit_expr(g, node->as.call.args.items[i]);
                    }
                    emit(g, ")");
                }
            } else {
                emit_expr(g, node->as.call.callee);
                emit(g, "(");
                for (int i = 0; i < node->as.call.args.count; i++) {
                    if (i > 0) emit(g, ", ");
                    emit_expr(g, node->as.call.args.items[i]);
                }
                emit(g, ")");
            }
            break;
        case NODE_MEMBER:
            emit_expr(g, node->as.member.object);
            emit(g, "->%s", node->as.member.name);
            break;
        case NODE_INDEX:
            emit(g, "candle_index(");
            emit_expr(g, node->as.index_expr.object);
            emit(g, ", ");
            emit_expr(g, node->as.index_expr.index);
            emit(g, ")");
            break;
        case NODE_LIST_LIT:
            emit(g, "candle_list_new(%d", node->as.list_lit.elements.count);
            for (int i = 0; i < node->as.list_lit.elements.count; i++) {
                emit(g, ", (candle_int)(");
                emit_expr(g, node->as.list_lit.elements.items[i]);
                emit(g, ")");
            }
            emit(g, ")");
            break;
        case NODE_MAP_LIT:
            emit(g, "candle_map_new(%d", node->as.map_lit.keys.count);
            for (int i = 0; i < node->as.map_lit.keys.count; i++) {
                emit(g, ", ");
                emit_expr(g, node->as.map_lit.keys.items[i]);
                emit(g, ", ");
                emit_expr(g, node->as.map_lit.values.items[i]);
            }
            emit(g, ")");
            break;
        case NODE_LAMBDA:
            emit(g, "__lambda_%d", node->lambda_id);
            break;
        case NODE_POSTFIX_UNARY:
            if (node->as.unary.op == TK_INCR) {
                emit_expr(g, node->as.unary.operand);
                emit(g, "++");
            } else if (node->as.unary.op == TK_DECR) {
                emit_expr(g, node->as.unary.operand);
                emit(g, "--");
            } else {
                // value! 非空断言 —— 运行时恒等
                emit_expr(g, node->as.unary.operand);
            }
            break;
        default:
            emit(g, "/* expr? */");
            break;
    }
}

static void emit_node(Codegen *g, AstNode *node) {
    if (!node) return;
    switch (node->type) {
        case NODE_VAR_DECL:
            indent(g);
            emit(g, "%s %s", c_type(node->as.var.type_name), node->as.var.name);
            if (node->as.var.init) {
                emit(g, " = ");
                emit_expr(g, node->as.var.init);
            }
            emit(g, ";\n");
            break;

        case NODE_CONST_DECL: {
            indent(g);
            const char *ct = "candle_int";
            if (node->as.constant.value) {
                NodeType vt = node->as.constant.value->type;
                if (vt == NODE_FLOAT_LIT) ct = "candle_double";
                else if (vt == NODE_STRING_LIT) ct = "candle_string";
                else if (vt == NODE_BOOL_LIT) ct = "candle_bool";
            }
            emit(g, "const %s %s = ", ct, node->as.constant.name);
            emit_expr(g, node->as.constant.value);
            emit(g, ";\n");
            break;
        }

        case NODE_ALIAS: {
            AstNode *av = node->as.alias.value;
            // type alias: alias Foo = int  →  typedef candle_int Foo;
            if (av && av->type == NODE_IDENT) {
                const char *name = av->as.ident.name;
                if (strcmp(name, "int") == 0 || strcmp(name, "double") == 0 ||
                    strcmp(name, "bool") == 0 || strcmp(name, "string") == 0 ||
                    strcmp(name, "void") == 0 || strcmp(name, "list") == 0 ||
                    strcmp(name, "map") == 0) {
                    emit(g, "typedef %s %s;\n", c_type(name), node->as.alias.name);
                    break;
                }
                if (strcmp(name, "Function") == 0) {
                    emit(g, "typedef void* %s;\n", node->as.alias.name);
                    break;
                }
            }
            // union/intersection type alias → comment (only if both sides are idents)
            if (av && av->type == NODE_BINARY &&
                (av->as.binary.op == TK_BIT_OR || av->as.binary.op == TK_BIT_AND) &&
                av->as.binary.left->type == NODE_IDENT &&
                av->as.binary.right->type == NODE_IDENT) {
                emit(g, "/* type alias %s */\n", node->as.alias.name);
                break;
            }
            indent(g);
            const char *rt = node->resolved_type ? node->resolved_type
                           : node->as.alias.assert_type;
            // lambda values are function pointers — let __auto_type infer
            if (av && av->type == NODE_LAMBDA) rt = NULL;
            if (rt)
                emit(g, "%s %s = ", c_type(rt), node->as.alias.name);
            else
                emit(g, "__auto_type %s = ", node->as.alias.name);
            emit_expr(g, node->as.alias.value);
            emit(g, ";\n");
            break;
        }

        case NODE_FUNC_DECL:
        case NODE_METHOD_DECL: {
            const char *ret = c_type(node->as.func.return_type);
            int is_user_main = (node->type == NODE_FUNC_DECL &&
                                node->as.func.name &&
                                strcmp(node->as.func.name, "main") == 0);
            const char *fname = is_user_main ? "candle_user_main" : node->as.func.name;
            if (is_user_main) ret = "int";
            if (node->as.func.is_static && !is_user_main) emit(g, "static ");
            emit(g, "%s %s(", ret, fname);
            for (int i = 0; i < node->as.func.params.count; i++) {
                if (i > 0) emit(g, ", ");
                AstNode *p = node->as.func.params.items[i];
                emit(g, "%s %s", c_type(p->as.var.type_name), p->as.var.name);
            }
            emit(g, ")");
            if (!node->as.func.body) {
                emit(g, ";\n");
            } else {
                emit(g, " {\n");
                g->indent++;
                emit_stmts(g, &node->as.func.body->as.program.stmts);
                g->indent--;
                emit(g, "}\n\n");
            }
            break;
        }

        case NODE_RETURN:
            indent(g);
            emit(g, "return");
            if (node->as.return_stmt.value) {
                emit(g, " ");
                emit_expr(g, node->as.return_stmt.value);
            }
            emit(g, ";\n");
            break;

        case NODE_BREAK:
            indent(g);
            emit(g, "break;\n");
            break;

        case NODE_EXPR_STMT:
            indent(g);
            if (node->as.program.stmts.count > 0)
                emit_expr(g, node->as.program.stmts.items[0]);
            emit(g, ";\n");
            break;

        case NODE_IF:
            indent(g);
            emit(g, "if (");
            emit_expr(g, node->as.if_stmt.condition);
            emit(g, ") {\n");
            g->indent++;
            emit_stmts(g, &node->as.if_stmt.then_block->as.program.stmts);
            g->indent--;
            indent(g); emit(g, "}");
            for (int i = 0; i < node->as.if_stmt.elif_conds.count; i++) {
                emit(g, " else if (");
                emit_expr(g, node->as.if_stmt.elif_conds.items[i]);
                emit(g, ") {\n");
                g->indent++;
                emit_stmts(g, &node->as.if_stmt.elif_blocks.items[i]->as.program.stmts);
                g->indent--;
                indent(g); emit(g, "}");
            }
            if (node->as.if_stmt.else_block) {
                emit(g, " else {\n");
                g->indent++;
                emit_stmts(g, &node->as.if_stmt.else_block->as.program.stmts);
                g->indent--;
                indent(g); emit(g, "}");
            }
            emit(g, "\n");
            break;

        case NODE_WHEN:
            indent(g);
            emit(g, "while (");
            emit_expr(g, node->as.when_stmt.condition);
            emit(g, ") {\n");
            g->indent++;
            emit_stmts(g, &node->as.when_stmt.body->as.program.stmts);
            g->indent--;
            indent(g); emit(g, "}\n");
            break;

        case NODE_FOR: {
            AstNode *iter = node->as.for_stmt.iter;
            int argc = iter->as.iter.args.count;
            AstNode *arg0 = argc >= 1 ? iter->as.iter.args.items[0] : NULL;
            indent(g);
            if (argc == 1 && arg0 && arg0->type == NODE_STRING_LIT) {
                // for iter(ch, 'str') → candle_str_foreach
                emit(g, "candle_str_foreach(%s, ", iter->as.iter.var_name);
                emit_expr(g, arg0);
                emit(g, ") {\n");
            } else if (argc == 1 && arg0 && arg0->type == NODE_IDENT &&
                       iter->resolved_type && strcmp(iter->resolved_type, "list") == 0) {
                emit(g, "candle_list_foreach(%s, ", iter->as.iter.var_name);
                emit_expr(g, arg0);
                emit(g, ") {\n");
            } else if (argc == 1 && arg0 && arg0->type == NODE_IDENT &&
                       iter->resolved_type && strcmp(iter->resolved_type, "string") == 0) {
                emit(g, "candle_str_foreach(%s, ", iter->as.iter.var_name);
                emit_expr(g, arg0);
                emit(g, ") {\n");
            } else {
                emit(g, "for (candle_int %s = ", iter->as.iter.var_name);
                if (argc >= 2) emit_expr(g, arg0);
                else emit(g, "0");
                emit(g, "; %s < ", iter->as.iter.var_name);
                if (argc >= 2) emit_expr(g, iter->as.iter.args.items[1]);
                else if (argc >= 1) emit_expr(g, arg0);
                emit(g, "; %s += ", iter->as.iter.var_name);
                if (argc >= 3) emit_expr(g, iter->as.iter.args.items[2]);
                else emit(g, "1");
                emit(g, ") {\n");
            }
            g->indent++;
            emit_stmts(g, &node->as.for_stmt.body->as.program.stmts);
            g->indent--;
            indent(g); emit(g, "}\n");
            break;
        }

        case NODE_ASSERT:
            indent(g);
            emit(g, "candle_assert(");
            emit_expr(g, node->as.assert_stmt.expr);
            emit(g, ");\n");
            break;

        case NODE_THROW: {
            indent(g);
            AstNode *v = node->as.throw_stmt.value;
            if (v && v->type == NODE_CALL && v->as.call.callee->type == NODE_IDENT &&
                !is_class(v->as.call.callee->as.ident.name)) {
                emit(g, "candle_throw_str(\"%s\", ", v->as.call.callee->as.ident.name);
                if (v->as.call.args.count > 0) emit_expr(g, v->as.call.args.items[0]);
                else emit(g, "candle_str(\"\")");
                emit(g, ");\n");
            } else {
                emit(g, "candle_throw_str(\"Exception\", ");
                emit_expr(g, v);
                emit(g, ");\n");
            }
            break;
        }

        case NODE_TRY: {
            indent(g);
            emit(g, "candle_try {\n");
            g->indent++;
            emit_stmts(g, &node->as.try_stmt.body->as.program.stmts);
            g->indent--;
            indent(g); emit(g, "} else {\n");
            g->indent++;
            for (int i = 0; i < node->as.try_stmt.catch_bodies.count; i++) {
                AstNode *ct = node->as.try_stmt.catch_types.items[i];
                AstNode *cn = node->as.try_stmt.catch_names.items[i];
                AstNode *cb = node->as.try_stmt.catch_bodies.items[i];
                const char *tname = ct ? ct->as.ident.name : "Exception";
                const char *vname = cn ? cn->as.ident.name : "e";
                indent(g);
                if (i > 0) emit(g, "else ");
                emit(g, "if (strcmp(_cj.exc.type, \"%s\") == 0 || strcmp(\"%s\", \"Exception\") == 0) {\n", tname, tname);
                g->indent++;
                indent(g); emit(g, "CandleExc %s = _cj.exc;\n", vname);
                indent(g); emit(g, "(void)%s;\n", vname);
                emit_stmts(g, &cb->as.program.stmts);
                g->indent--;
                indent(g); emit(g, "}\n");
            }
            g->indent--;
            indent(g); emit(g, "} candle_catch_end;\n");
            break;
        }

        case NODE_CLASS_DECL: {
            const char *name = node->as.class_decl.name;
            emit(g, "/* class %s */\n", name);
            emit(g, "typedef struct %s %s;\n", name, name);
            emit(g, "struct %s {\n", name);
            // inherited fields (walk parent chain, outer-most first)
            {
                const char *parents[16]; int np = 0;
                const char *p = node->as.class_decl.parent;
                while (p && np < 16) { parents[np++] = p; AstNode *pn = get_class_node(p); p = pn ? pn->as.class_decl.parent : NULL; }
                for (int k = np - 1; k >= 0; k--) {
                    AstNode *pn = get_class_node(parents[k]);
                    if (!pn) continue;
                    for (int i = 0; i < pn->as.class_decl.members.count; i++) {
                        AstNode *m = pn->as.class_decl.members.items[i];
                        if (m->type == NODE_FIELD_DECL)
                            emit(g, "    %s %s;\n", c_type(m->as.var.type_name), m->as.var.name);
                    }
                }
            }
            // own fields
            AstNode *ctor = NULL;
            for (int i = 0; i < node->as.class_decl.members.count; i++) {
                AstNode *m = node->as.class_decl.members.items[i];
                if (m->type == NODE_FIELD_DECL) {
                    emit(g, "    %s %s;\n", c_type(m->as.var.type_name), m->as.var.name);
                } else if (m->type == NODE_CONSTRUCTOR) {
                    ctor = m;
                }
            }
            emit(g, "};\n\n");
            // constructor — ClassName__new(args) → allocates and assigns matching fields
            emit(g, "static %s *%s__new(", name, name);
            if (ctor && ctor->as.constructor.params.count > 0) {
                for (int i = 0; i < ctor->as.constructor.params.count; i++) {
                    if (i > 0) emit(g, ", ");
                    AstNode *p = ctor->as.constructor.params.items[i];
                    const char *ptype = p->as.var.type_name;
                    if (ptype && strcmp(ptype, "super") == 0) {
                        ptype = find_field_type(node->as.class_decl.parent, p->as.var.name);
                    } else if (ptype && strcmp(ptype, "this") == 0) {
                        ptype = find_field_type(name, p->as.var.name);
                    }
                    emit(g, "%s %s", c_type(ptype), p->as.var.name);
                }
            } else {
                // default: param per field
                for (int i = 0, n = 0; i < node->as.class_decl.members.count; i++) {
                    AstNode *m = node->as.class_decl.members.items[i];
                    if (m->type != NODE_FIELD_DECL) continue;
                    if (n++ > 0) emit(g, ", ");
                    emit(g, "%s %s", c_type(m->as.var.type_name), m->as.var.name);
                }
            }
            emit(g, ") {\n");
            emit(g, "    %s *self = GC_MALLOC(sizeof(%s));\n", name, name);
            if (ctor && ctor->as.constructor.params.count > 0) {
                // this./super./同名字段的参数自动赋值
                for (int i = 0; i < ctor->as.constructor.params.count; i++) {
                    AstNode *p = ctor->as.constructor.params.items[i];
                    const char *ptype = p->as.var.type_name;
                    int is_forward = ptype && (strcmp(ptype, "this") == 0 || strcmp(ptype, "super") == 0);
                    if (is_forward || find_field_type(name, p->as.var.name))
                        emit(g, "    self->%s = %s;\n", p->as.var.name, p->as.var.name);
                }
                // 构造函数体（this.field = ...; 等）
                if (ctor->as.constructor.body) {
                    const char *pc = g_current_class;
                    g_current_class = name;
                    g->indent++;
                    emit_stmts(g, &ctor->as.constructor.body->as.program.stmts);
                    g->indent--;
                    g_current_class = pc;
                }
            } else {
                for (int i = 0; i < node->as.class_decl.members.count; i++) {
                    AstNode *m = node->as.class_decl.members.items[i];
                    if (m->type != NODE_FIELD_DECL) continue;
                    emit(g, "    self->%s = %s;\n", m->as.var.name, m->as.var.name);
                }
            }
            emit(g, "    return self;\n");
            emit(g, "}\n\n");
            const char *prev_class = g_current_class;
            g_current_class = name;
            for (int i = 0; i < node->as.class_decl.members.count; i++) {
                AstNode *m = node->as.class_decl.members.items[i];
                if (m->type == NODE_METHOD_DECL) {
                    if (m->as.func.is_static || node->as.class_decl.is_static) {
                        emit(g, "static %s %s__%s(", c_type(m->as.func.return_type), name, m->as.func.name);
                    } else {
                        emit(g, "%s %s__%s(%s *self", c_type(m->as.func.return_type), name, m->as.func.name, name);
                        if (m->as.func.params.count > 0) emit(g, ", ");
                    }
                    for (int j = 0; j < m->as.func.params.count; j++) {
                        if (j > 0) emit(g, ", ");
                        AstNode *p = m->as.func.params.items[j];
                        emit(g, "%s %s", c_type(p->as.var.type_name), p->as.var.name);
                    }
                    emit(g, ")");
                    if (!m->as.func.body) { emit(g, ";\n"); continue; }
                    emit(g, " {\n");
                    g->indent++;
                    emit_stmts(g, &m->as.func.body->as.program.stmts);
                    g->indent--;
                    emit(g, "}\n\n");
                }
            }
            for (int i = 0; i < node->as.class_decl.members.count; i++) {
                AstNode *m = node->as.class_decl.members.items[i];
                if (m->type == NODE_FACTORY_DECL) {
                    emit(g, "static %s *%s__%s(", name, name, m->as.factory.name);
                    for (int j = 0; j < m->as.factory.params.count; j++) {
                        if (j > 0) emit(g, ", ");
                        AstNode *p = m->as.factory.params.items[j];
                        emit(g, "%s %s", c_type(p->as.var.type_name), p->as.var.name);
                    }
                    emit(g, ") {\n");
                    g->indent++;
                    emit_stmts(g, &m->as.factory.body->as.program.stmts);
                    g->indent--;
                    emit(g, "}\n\n");
                }
            }
            g_current_class = prev_class;
            break;
        }

        case NODE_LOAD: {
            char path[256];
            strncpy(path, node->as.import.path, sizeof(path)-1);
            path[sizeof(path)-1] = 0;
            for (char *p = path; *p; p++) if (*p == '.') *p = '/';
            emit(g, "#include \"%s.h\"\n", path);
            if (node->as.import.alias_name)
                register_module_alias(node->as.import.alias_name);
            else {
                const char *p = strrchr(node->as.import.path, '.');
                register_module_alias(p ? p + 1 : node->as.import.path);
            }
            break;
        }

        case NODE_DLL: {
            if (node->as.import.alias_name)
                register_module_alias(node->as.import.alias_name);
            else {
                const char *p = strrchr(node->as.import.path, '.');
                register_module_alias(p ? p + 1 : node->as.import.path);
            }
            break;
        }

        case NODE_ELLIPSIS_EXPR:
            indent(g);
            emit(g, "/* ellipsis */\n");
            break;

        case NODE_PARALLEL:
            if (node->as.parallel.for_iter) {
                AstNode *it = node->as.parallel.for_iter;
                indent(g); emit(g, "_Pragma(\"omp parallel for\")\n");
                indent(g);
                if (it->as.iter.args.count == 0) {
                    emit(g, "for (candle_int %s = 0; %s < 0; %s++) ", it->as.iter.var_name, it->as.iter.var_name, it->as.iter.var_name);
                } else if (it->as.iter.args.count == 1) {
                    emit(g, "for (candle_int %s = 0; %s < ", it->as.iter.var_name, it->as.iter.var_name);
                    emit_expr(g, it->as.iter.args.items[0]);
                    emit(g, "; %s++) ", it->as.iter.var_name);
                } else {
                    emit(g, "for (candle_int %s = ", it->as.iter.var_name);
                    emit_expr(g, it->as.iter.args.items[0]);
                    emit(g, "; %s < ", it->as.iter.var_name);
                    emit_expr(g, it->as.iter.args.items[1]);
                    emit(g, "; %s += ", it->as.iter.var_name);
                    if (it->as.iter.args.count >= 3) emit_expr(g, it->as.iter.args.items[2]);
                    else emit(g, "1");
                    emit(g, ") ");
                }
                AstNode *body = node->as.parallel.for_body;
                emit(g, "{\n");
                g->indent++;
                if (body && body->type == NODE_BLOCK)
                    emit_stmts(g, &body->as.program.stmts);
                else
                    emit_node(g, body);
                g->indent--;
                indent(g); emit(g, "}\n");
            } else {
                indent(g); emit(g, "_Pragma(\"omp parallel sections\")\n");
                indent(g); emit(g, "{\n");
                g->indent++;
                for (int i = 0; i < node->as.parallel.sections.count; i++) {
                    AstNode *s = node->as.parallel.sections.items[i];
                    indent(g); emit(g, "_Pragma(\"omp section\")\n");
                    AstNode *body = s;
                    if (s->type == NODE_DELAY) {
                        indent(g); emit(g, "/* delay(%s) */\n", s->as.delay.label);
                        body = s->as.delay.body;
                    }
                    if (body && body->type == NODE_BLOCK) {
                        indent(g); emit(g, "{\n");
                        g->indent++;
                        emit_stmts(g, &body->as.program.stmts);
                        g->indent--;
                        indent(g); emit(g, "}\n");
                    } else {
                        emit_node(g, body);
                    }
                }
                g->indent--;
                indent(g); emit(g, "}\n");
            }
            break;

        case NODE_SIGNAL:
            indent(g);
            emit(g, "/* signal(%s) */\n", node->as.signal.label);
            emit_stmts(g, &node->as.signal.body->as.program.stmts);
            break;

        case NODE_DELAY:
            indent(g);
            emit(g, "/* delay(%s) — runs as a parallel section */\n", node->as.delay.label);
            indent(g); emit(g, "_Pragma(\"omp section\")\n");
            indent(g); emit(g, "{\n");
            g->indent++;
            emit_stmts(g, &node->as.delay.body->as.program.stmts);
            g->indent--;
            indent(g); emit(g, "}\n");
            break;

        case NODE_REFLECT:
            indent(g);
            emit(g, "/* reflect %s */\n", node->as.reflect.target);
            break;

        case NODE_EXPORT:
            break;

        default:
            break;
    }
}

static int is_type_alias(AstNode *node) {
    if (node->type != NODE_ALIAS) return 0;
    AstNode *av = node->as.alias.value;
    if (!av) return 0;
    if (av->type == NODE_IDENT) {
        const char *name = av->as.ident.name;
        if (strcmp(name, "int") == 0 || strcmp(name, "double") == 0 ||
            strcmp(name, "bool") == 0 || strcmp(name, "string") == 0 ||
            strcmp(name, "void") == 0 || strcmp(name, "list") == 0 ||
            strcmp(name, "map") == 0 || strcmp(name, "Function") == 0) return 1;
    }
    if (av->type == NODE_BINARY &&
        (av->as.binary.op == TK_BIT_OR || av->as.binary.op == TK_BIT_AND) &&
        av->as.binary.left->type == NODE_IDENT &&
        av->as.binary.right->type == NODE_IDENT) return 1;
    return 0;
}

static void emit_stmts(Codegen *g, NodeList *stmts) {
    for (int i = 0; i < stmts->count; i++)
        emit_node(g, stmts->items[i]);
}

void codegen_preprocess(AstNode *program) {
    for (int i = 0; i < program->as.program.stmts.count; i++) {
        AstNode *node = program->as.program.stmts.items[i];
        if (node->type == NODE_CLASS_DECL)
            register_class_node(node);
    }

    for (int i = 0; i < program->as.program.stmts.count; i++) {
        AstNode *node = program->as.program.stmts.items[i];
        if (node->type != NODE_REFLECT) continue;
        const char *target_class = NULL;
        if (is_class(node->as.reflect.target)) {
            target_class = node->as.reflect.target;
        } else {
            for (int j = 0; j < program->as.program.stmts.count; j++) {
                AstNode *v = program->as.program.stmts.items[j];
                if (v->type == NODE_VAR_DECL && v->as.var.name &&
                    strcmp(v->as.var.name, node->as.reflect.target) == 0) {
                    if (v->as.var.type_name && is_class(v->as.var.type_name)) {
                        target_class = v->as.var.type_name;
                    } else if (v->as.var.init && v->as.var.init->type == NODE_CALL &&
                               v->as.var.init->as.call.callee->type == NODE_IDENT &&
                               is_class(v->as.var.init->as.call.callee->as.ident.name)) {
                        target_class = v->as.var.init->as.call.callee->as.ident.name;
                    }
                    break;
                }
                if (v->type == NODE_ALIAS && v->as.alias.name &&
                    strcmp(v->as.alias.name, node->as.reflect.target) == 0 &&
                    v->as.alias.value && v->as.alias.value->type == NODE_CALL &&
                    v->as.alias.value->as.call.callee->type == NODE_IDENT &&
                    is_class(v->as.alias.value->as.call.callee->as.ident.name)) {
                    target_class = v->as.alias.value->as.call.callee->as.ident.name;
                    break;
                }
            }
        }
        if (!target_class) continue;
        AstNode *cls = get_class_node(target_class);
        if (!cls || !node->as.reflect.body) continue;
        NodeList *body = &node->as.reflect.body->as.program.stmts;
        for (int k = 0; k < body->count; k++) {
            AstNode *m = body->items[k];
            if (m->type == NODE_VAR_DECL) {
                AstNode *fld = ast_new(NODE_FIELD_DECL, m->line, m->column);
                fld->as.var.type_name = m->as.var.type_name ? strdup(m->as.var.type_name) : NULL;
                fld->as.var.name = strdup(m->as.var.name);
                fld->as.var.init = m->as.var.init;
                node_list_push(&cls->as.class_decl.members, fld);
            } else if (m->type == NODE_FUNC_DECL || m->type == NODE_METHOD_DECL) {
                AstNode *mth = ast_new(NODE_METHOD_DECL, m->line, m->column);
                mth->as.func.name = strdup(m->as.func.name);
                mth->as.func.return_type = m->as.func.return_type ? strdup(m->as.func.return_type) : NULL;
                mth->as.func.params = m->as.func.params;
                mth->as.func.body = m->as.func.body;
                mth->as.func.is_static = m->as.func.is_static;
                node_list_push(&cls->as.class_decl.members, mth);
            }
        }
        node->type = NODE_EXPORT;
        node_list_init(&node->as.export_stmt.items);
        node_list_init(&node->as.export_stmt.except_items);
        node->as.export_stmt.is_wildcard = 0;
    }
}

void codegen_run(AstNode *program, FILE *out, const char *filename) {
    Codegen g = { .out = out, .indent = 0, .had_error = 0, .filename = filename };

    emit(&g, "#define CANDLE_RUNTIME_MAIN\n");
    emit(&g, "#include \"candle_runtime.h\"\n\n");

    for (int i = 0; i < program->as.program.stmts.count; i++) {
        AstNode *node = program->as.program.stmts.items[i];
        if (node->type == NODE_CLASS_DECL)
            register_class_node(node);
    }

    // pre-pass: collect lambdas and emit each as a top-level function
    g_nlambdas = 0;
    collect_lambdas_walk(program);
    for (int i = 0; i < g_nlambdas; i++) {
        AstNode *l = g_lambdas[i];
        const char *rt = l->resolved_type;
        if (!rt) {
            int has_return = 0;
            if (l->as.lambda.body && l->as.lambda.body->type == NODE_BLOCK) {
                for (int j = 0; j < l->as.lambda.body->as.program.stmts.count; j++) {
                    AstNode *st = l->as.lambda.body->as.program.stmts.items[j];
                    if (st->type == NODE_RETURN && st->as.return_stmt.value) { has_return = 1; break; }
                }
            } else if (l->as.lambda.body) {
                has_return = 1;
            }
            rt = has_return ? "candle_int" : "void";
        }
        emit(&g, "static %s __lambda_%d(", c_type(rt), i);
        for (int j = 0; j < l->as.lambda.params.count; j++) {
            if (j > 0) emit(&g, ", ");
            AstNode *p = l->as.lambda.params.items[j];
            const char *pt = p->as.var.type_name ? p->as.var.type_name : "candle_int";
            emit(&g, "%s %s", c_type(pt), p->as.var.name);
        }
        emit(&g, ") {\n");
        g.indent = 1;
        if (l->as.lambda.body && l->as.lambda.body->type == NODE_BLOCK) {
            emit_stmts(&g, &l->as.lambda.body->as.program.stmts);
        } else {
            indent(&g);
            emit(&g, "return ");
            emit_expr(&g, l->as.lambda.body);
            emit(&g, ";\n");
        }
        g.indent = 0;
        emit(&g, "}\n\n");
    }

    // forward declarations for functions
    for (int i = 0; i < program->as.program.stmts.count; i++) {
        AstNode *node = program->as.program.stmts.items[i];
        if (node->type == NODE_FUNC_DECL) {
            const char *fret = c_type(node->as.func.return_type);
            const char *fname = node->as.func.name;
            int is_user_main = fname && strcmp(fname, "main") == 0;
            if (is_user_main) { fret = "int"; fname = "candle_user_main"; }
            emit(&g, "%s %s(", fret, fname);
            for (int j = 0; j < node->as.func.params.count; j++) {
                if (j > 0) emit(&g, ", ");
                AstNode *p = node->as.func.params.items[j];
                emit(&g, "%s %s", c_type(p->as.var.type_name), p->as.var.name);
            }
            emit(&g, ");\n");
        }
    }
    emit(&g, "\n");

    // global vars and class declarations first (including type aliases)
    for (int i = 0; i < program->as.program.stmts.count; i++) {
        AstNode *node = program->as.program.stmts.items[i];
        if (node->type == NODE_CLASS_DECL || node->type == NODE_LOAD ||
            node->type == NODE_DLL || node->type == NODE_CONST_DECL ||
            is_type_alias(node)) {
            emit_node(&g, node);
        }
    }

    // collect top-level statements that go into candle_main
    int has_main = 0;
    for (int i = 0; i < program->as.program.stmts.count; i++) {
        AstNode *node = program->as.program.stmts.items[i];
        if (node->type == NODE_FUNC_DECL &&
            strcmp(node->as.func.name, "main") == 0) {
            has_main = 1;
        }
    }

    // emit functions
    for (int i = 0; i < program->as.program.stmts.count; i++) {
        AstNode *node = program->as.program.stmts.items[i];
        if (node->type == NODE_FUNC_DECL) {
            emit_node(&g, node);
        }
    }

    // top-level statements → candle_init()
    int has_toplevel = 0;
    for (int i = 0; i < program->as.program.stmts.count; i++) {
        AstNode *node = program->as.program.stmts.items[i];
        if (node->type != NODE_FUNC_DECL && node->type != NODE_CLASS_DECL &&
            node->type != NODE_LOAD && node->type != NODE_DLL &&
            node->type != NODE_CONST_DECL && !is_type_alias(node)) {
            has_toplevel = 1;
            break;
        }
    }

    if (has_toplevel) {
        emit(&g, "static void candle_init(void) {\n");
        g.indent = 1;
        for (int i = 0; i < program->as.program.stmts.count; i++) {
            AstNode *node = program->as.program.stmts.items[i];
            if (node->type != NODE_FUNC_DECL && node->type != NODE_CLASS_DECL &&
                node->type != NODE_LOAD && node->type != NODE_DLL &&
                node->type != NODE_CONST_DECL && !is_type_alias(node)) {
                emit_node(&g, node);
            }
        }
        g.indent = 0;
        emit(&g, "}\n\n");
    }

    emit(&g, "int main(void) {\n");
    emit(&g, "    GC_INIT();\n");
    if (has_toplevel) emit(&g, "    candle_init();\n");
    if (has_main) emit(&g, "    return candle_user_main();\n");
    else emit(&g, "    return 0;\n");
    emit(&g, "}\n");
}
