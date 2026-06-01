#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void node_list_init(NodeList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void node_list_push(NodeList *list, AstNode *node) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity < 8 ? 8 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(AstNode *) * list->capacity);
    }
    list->items[list->count++] = node;
}

void node_list_free(NodeList *list) {
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

AstNode *ast_new(NodeType type, int line, int column) {
    AstNode *node = calloc(1, sizeof(AstNode));
    node->type = type;
    node->line = line;
    node->column = column;
    return node;
}

void ast_free(AstNode *node) {
    if (!node) return;
    free(node);
}

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) printf("  ");
}

static const char *node_type_str(NodeType type) {
    switch (type) {
        case NODE_PROGRAM:       return "Program";
        case NODE_BLOCK:         return "Block";
        case NODE_FUNC_DECL:     return "FuncDecl";
        case NODE_VAR_DECL:      return "VarDecl";
        case NODE_ALIAS:         return "Alias";
        case NODE_CONST_DECL:    return "ConstDecl";
        case NODE_CLASS_DECL:    return "ClassDecl";
        case NODE_FIELD_DECL:    return "FieldDecl";
        case NODE_METHOD_DECL:   return "MethodDecl";
        case NODE_CONSTRUCTOR:   return "Constructor";
        case NODE_FACTORY_DECL:  return "FactoryDecl";
        case NODE_IF:            return "If";
        case NODE_WHEN:          return "When";
        case NODE_FOR:           return "For";
        case NODE_RETURN:        return "Return";
        case NODE_BREAK:         return "Break";
        case NODE_THROW:         return "Throw";
        case NODE_TRY:           return "Try";
        case NODE_ASSERT:        return "Assert";
        case NODE_EXPR_STMT:     return "ExprStmt";
        case NODE_LOAD:          return "Load";
        case NODE_DLL:           return "Dll";
        case NODE_EXPORT:        return "Export";
        case NODE_REFLECT:       return "Reflect";
        case NODE_PARALLEL:      return "Parallel";
        case NODE_SIGNAL:        return "Signal";
        case NODE_DELAY:         return "Delay";
        case NODE_BINARY:        return "Binary";
        case NODE_TERNARY:       return "Ternary";
        case NODE_UNARY:         return "Unary";
        case NODE_POSTFIX_UNARY: return "PostfixUnary";
        case NODE_CALL:          return "Call";
        case NODE_MEMBER:        return "Member";
        case NODE_INDEX:         return "Index";
        case NODE_ASSIGN:        return "Assign";
        case NODE_LAMBDA:        return "Lambda";
        case NODE_INT_LIT:       return "IntLit";
        case NODE_FLOAT_LIT:     return "FloatLit";
        case NODE_STRING_LIT:    return "StringLit";
        case NODE_BOOL_LIT:      return "BoolLit";
        case NODE_NULL_LIT:      return "NullLit";
        case NODE_IDENT:         return "Ident";
        case NODE_ELLIPSIS_EXPR: return "Ellipsis";
        case NODE_LIST_LIT:      return "ListLit";
        case NODE_TUPLE_LIT:     return "TupleLit";
        case NODE_MAP_LIT:       return "MapLit";
        case NODE_ITER:          return "Iter";
    }
    return "Unknown";
}

void ast_print(AstNode *node, int indent) {
    if (!node) return;

    print_indent(indent);
    printf("%s", node_type_str(node->type));

    switch (node->type) {
        case NODE_PROGRAM:
        case NODE_BLOCK:
            printf("\n");
            for (int i = 0; i < node->as.program.stmts.count; i++)
                ast_print(node->as.program.stmts.items[i], indent + 1);
            break;

        case NODE_FUNC_DECL:
        case NODE_METHOD_DECL:
            printf(" %s -> %s\n", node->as.func.name, node->as.func.return_type);
            for (int i = 0; i < node->as.func.params.count; i++)
                ast_print(node->as.func.params.items[i], indent + 1);
            if (node->as.func.body)
                ast_print(node->as.func.body, indent + 1);
            break;

        case NODE_VAR_DECL:
        case NODE_FIELD_DECL:
            printf(" %s: %s\n", node->as.var.name, node->as.var.type_name);
            if (node->as.var.init)
                ast_print(node->as.var.init, indent + 1);
            break;

        case NODE_ALIAS:
            printf(" %s\n", node->as.alias.name);
            if (node->as.alias.value)
                ast_print(node->as.alias.value, indent + 1);
            break;

        case NODE_CONST_DECL:
            printf(" %s\n", node->as.constant.name);
            if (node->as.constant.value)
                ast_print(node->as.constant.value, indent + 1);
            break;

        case NODE_CLASS_DECL:
            printf(" %s", node->as.class_decl.name);
            if (node->as.class_decl.parent)
                printf(" inherit %s", node->as.class_decl.parent);
            printf("\n");
            for (int i = 0; i < node->as.class_decl.members.count; i++)
                ast_print(node->as.class_decl.members.items[i], indent + 1);
            break;

        case NODE_IF:
            printf("\n");
            print_indent(indent + 1); printf("Cond:\n");
            ast_print(node->as.if_stmt.condition, indent + 2);
            print_indent(indent + 1); printf("Then:\n");
            ast_print(node->as.if_stmt.then_block, indent + 2);
            for (int i = 0; i < node->as.if_stmt.elif_conds.count; i++) {
                print_indent(indent + 1); printf("ElseIf:\n");
                ast_print(node->as.if_stmt.elif_conds.items[i], indent + 2);
                ast_print(node->as.if_stmt.elif_blocks.items[i], indent + 2);
            }
            if (node->as.if_stmt.else_block) {
                print_indent(indent + 1); printf("Else:\n");
                ast_print(node->as.if_stmt.else_block, indent + 2);
            }
            break;

        case NODE_WHEN:
            printf("\n");
            ast_print(node->as.when_stmt.condition, indent + 1);
            ast_print(node->as.when_stmt.body, indent + 1);
            break;

        case NODE_FOR:
            printf("\n");
            ast_print(node->as.for_stmt.iter, indent + 1);
            ast_print(node->as.for_stmt.body, indent + 1);
            break;

        case NODE_ITER:
            printf(" %s\n", node->as.iter.var_name);
            for (int i = 0; i < node->as.iter.args.count; i++)
                ast_print(node->as.iter.args.items[i], indent + 1);
            break;

        case NODE_RETURN:
            printf("\n");
            if (node->as.return_stmt.value)
                ast_print(node->as.return_stmt.value, indent + 1);
            break;

        case NODE_BREAK:
            printf("\n");
            break;

        case NODE_THROW:
            printf("\n");
            ast_print(node->as.throw_stmt.value, indent + 1);
            break;

        case NODE_ASSERT:
            printf("\n");
            ast_print(node->as.assert_stmt.expr, indent + 1);
            break;

        case NODE_TRY:
            printf("\n");
            ast_print(node->as.try_stmt.body, indent + 1);
            for (int i = 0; i < node->as.try_stmt.catch_bodies.count; i++) {
                print_indent(indent + 1); printf("Catch:\n");
                ast_print(node->as.try_stmt.catch_bodies.items[i], indent + 2);
            }
            break;

        case NODE_BINARY:
            printf(" %s\n", token_type_name(node->as.binary.op));
            ast_print(node->as.binary.left, indent + 1);
            ast_print(node->as.binary.right, indent + 1);
            break;

        case NODE_TERNARY:
            printf("\n");
            print_indent(indent + 1); printf("Cond:\n");
            ast_print(node->as.ternary.cond, indent + 2);
            print_indent(indent + 1); printf("Then:\n");
            ast_print(node->as.ternary.then_expr, indent + 2);
            print_indent(indent + 1); printf("Else:\n");
            ast_print(node->as.ternary.else_expr, indent + 2);
            break;

        case NODE_UNARY:
        case NODE_POSTFIX_UNARY:
            printf(" %s\n", token_type_name(node->as.unary.op));
            ast_print(node->as.unary.operand, indent + 1);
            break;

        case NODE_CALL:
            printf("\n");
            ast_print(node->as.call.callee, indent + 1);
            for (int i = 0; i < node->as.call.args.count; i++)
                ast_print(node->as.call.args.items[i], indent + 1);
            break;

        case NODE_MEMBER:
            printf(" .%s\n", node->as.member.name);
            ast_print(node->as.member.object, indent + 1);
            break;

        case NODE_INDEX:
            printf("\n");
            ast_print(node->as.index_expr.object, indent + 1);
            ast_print(node->as.index_expr.index, indent + 1);
            break;

        case NODE_ASSIGN:
            printf(" %s\n", token_type_name(node->as.assign.op));
            ast_print(node->as.assign.target, indent + 1);
            ast_print(node->as.assign.value, indent + 1);
            break;

        case NODE_LAMBDA:
            printf("\n");
            for (int i = 0; i < node->as.lambda.params.count; i++)
                ast_print(node->as.lambda.params.items[i], indent + 1);
            ast_print(node->as.lambda.body, indent + 1);
            break;

        case NODE_INT_LIT:
            printf(" %lld\n", node->as.int_lit.value);
            break;

        case NODE_FLOAT_LIT:
            printf(" %g\n", node->as.float_lit.value);
            break;

        case NODE_STRING_LIT:
            printf(" \"%s\"\n", node->as.string_lit.value);
            break;

        case NODE_BOOL_LIT:
            printf(" %s\n", node->as.bool_lit.value ? "true" : "false");
            break;

        case NODE_NULL_LIT:
            printf("\n");
            break;

        case NODE_IDENT:
            printf(" %s\n", node->as.ident.name);
            break;

        case NODE_ELLIPSIS_EXPR:
            printf("\n");
            break;

        case NODE_LIST_LIT:
            printf("\n");
            for (int i = 0; i < node->as.list_lit.elements.count; i++)
                ast_print(node->as.list_lit.elements.items[i], indent + 1);
            break;

        case NODE_TUPLE_LIT:
            printf("\n");
            for (int i = 0; i < node->as.list_lit.elements.count; i++)
                ast_print(node->as.list_lit.elements.items[i], indent + 1);
            break;

        case NODE_MAP_LIT:
            printf("\n");
            for (int i = 0; i < node->as.map_lit.keys.count; i++) {
                ast_print(node->as.map_lit.keys.items[i], indent + 1);
                ast_print(node->as.map_lit.values.items[i], indent + 1);
            }
            break;

        case NODE_LOAD:
        case NODE_DLL:
            printf(" %s", node->as.import.path);
            if (node->as.import.alias_name)
                printf(" as %s", node->as.import.alias_name);
            printf("\n");
            break;

        case NODE_EXPORT:
            printf("\n");
            break;

        case NODE_REFLECT:
            printf(" %s\n", node->as.reflect.target);
            ast_print(node->as.reflect.body, indent + 1);
            break;

        case NODE_PARALLEL:
            printf("\n");
            for (int i = 0; i < node->as.parallel.sections.count; i++)
                ast_print(node->as.parallel.sections.items[i], indent + 1);
            break;

        case NODE_SIGNAL:
            printf(" %s\n", node->as.signal.label);
            ast_print(node->as.signal.body, indent + 1);
            break;

        case NODE_DELAY:
            printf(" %s\n", node->as.delay.label);
            ast_print(node->as.delay.body, indent + 1);
            break;

        default:
            printf("\n");
            break;
    }
}
