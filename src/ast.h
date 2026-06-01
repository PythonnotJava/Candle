#ifndef CANDLE_AST_H
#define CANDLE_AST_H

#include "token.h"
#include <stddef.h>

typedef enum {
    NODE_PROGRAM,
    NODE_BLOCK,

    // Declarations
    NODE_FUNC_DECL,
    NODE_VAR_DECL,
    NODE_ALIAS,
    NODE_CONST_DECL,
    NODE_CLASS_DECL,
    NODE_FIELD_DECL,
    NODE_METHOD_DECL,
    NODE_CONSTRUCTOR,
    NODE_FACTORY_DECL,

    // Statements
    NODE_IF,
    NODE_WHEN,
    NODE_FOR,
    NODE_RETURN,
    NODE_BREAK,
    NODE_THROW,
    NODE_TRY,
    NODE_ASSERT,
    NODE_EXPR_STMT,
    NODE_LOAD,
    NODE_DLL,
    NODE_EXPORT,
    NODE_REFLECT,
    NODE_PARALLEL,
    NODE_SIGNAL,
    NODE_DELAY,

    // Expressions
    NODE_BINARY,
    NODE_TERNARY,
    NODE_UNARY,
    NODE_POSTFIX_UNARY,
    NODE_CALL,
    NODE_MEMBER,
    NODE_INDEX,
    NODE_ASSIGN,
    NODE_LAMBDA,
    NODE_INT_LIT,
    NODE_FLOAT_LIT,
    NODE_STRING_LIT,
    NODE_BOOL_LIT,
    NODE_NULL_LIT,
    NODE_IDENT,
    NODE_ELLIPSIS_EXPR,
    NODE_LIST_LIT,
    NODE_TUPLE_LIT,
    NODE_MAP_LIT,
    NODE_ITER,
} NodeType;

typedef struct AstNode AstNode;

typedef struct {
    AstNode **items;
    int count;
    int capacity;
} NodeList;

struct AstNode {
    NodeType type;
    int line;
    int column;
    char *resolved_type;  // set by sema pass, NULL if unknown
    int needs_self;       // bare field access inside a method → self->field
    char *self_call_class; // bare method call inside a method → Class__method(self,...)
    int is_ctor_call;     // call where callee is a class name → ClassName__new(...)
    int lambda_id;        // codegen-assigned id for lifted lambda
    union {
        // NODE_PROGRAM, NODE_BLOCK
        struct { NodeList stmts; } program;

        // NODE_FUNC_DECL, NODE_METHOD_DECL
        struct {
            char *name;
            char *return_type;
            NodeList params;     // NODE_VAR_DECL (type + name)
            AstNode *body;      // NODE_BLOCK or NULL
            int is_static;
            int is_private;
            int is_public;
            int is_final;
            int is_ellipsis;
        } func;

        // NODE_VAR_DECL, NODE_FIELD_DECL
        struct {
            char *type_name;
            char *name;
            AstNode *init;      // nullable
            int is_private;
            int is_static;
            int is_final;
        } var;

        // NODE_ALIAS
        struct {
            char *name;
            AstNode *value;     // expression or type
            char *assert_type;  // nullable
        } alias;

        // NODE_CONST_DECL
        struct {
            char *name;
            AstNode *value;
        } constant;

        // NODE_CLASS_DECL
        struct {
            char *name;
            char *parent;       // nullable (inherit)
            NodeList members;
            int is_final;
            int is_static;
            int is_ellipsis;
        } class_decl;

        // NODE_CONSTRUCTOR
        struct {
            char *class_name;
            NodeList params;
            AstNode *body;      // nullable: 构造函数体（this.field=... 等）
        } constructor;

        // NODE_FACTORY_DECL
        struct {
            char *return_type;
            char *name;
            NodeList params;
            AstNode *body;
        } factory;

        // NODE_IF
        struct {
            AstNode *condition;
            AstNode *then_block;
            NodeList elif_conds;
            NodeList elif_blocks;
            AstNode *else_block; // nullable
        } if_stmt;

        // NODE_WHEN
        struct {
            AstNode *condition;
            AstNode *body;
        } when_stmt;

        // NODE_FOR
        struct {
            AstNode *iter;      // NODE_ITER
            AstNode *body;
        } for_stmt;

        // NODE_ITER
        struct {
            char *var_name;
            NodeList args;
        } iter;

        // NODE_RETURN
        struct { AstNode *value; } return_stmt;

        // NODE_THROW
        struct { AstNode *value; } throw_stmt;

        // NODE_ASSERT
        struct { AstNode *expr; } assert_stmt;

        // NODE_TRY
        struct {
            AstNode *body;
            NodeList catch_types;   // identifiers (type names), 元素可为 NULL 表示裸 catch(e)
            NodeList catch_names;   // identifiers (var names)
            NodeList catch_bodies;  // blocks
            AstNode *finally_block; // nullable: final { ... } 块，总会执行
        } try_stmt;

        // NODE_BINARY
        struct {
            AstNode *left;
            AstNode *right;
            TokenType op;
        } binary;

        // NODE_TERNARY  (cond ? then_expr : else_expr)
        struct {
            AstNode *cond;
            AstNode *then_expr;
            AstNode *else_expr;
        } ternary;

        // NODE_UNARY, NODE_POSTFIX_UNARY
        struct {
            AstNode *operand;
            TokenType op;
        } unary;

        // NODE_CALL
        struct {
            AstNode *callee;
            NodeList args;
        } call;

        // NODE_MEMBER
        struct {
            AstNode *object;
            char *name;
        } member;

        // NODE_INDEX
        struct {
            AstNode *object;
            AstNode *index;
        } index_expr;

        // NODE_ASSIGN
        struct {
            AstNode *target;
            AstNode *value;
            TokenType op;       // TK_ASSIGN, TK_PLUS_ASSIGN, etc.
        } assign;

        // NODE_LAMBDA
        struct {
            NodeList params;
            AstNode *body;      // expression or block
        } lambda;

        // NODE_INT_LIT
        struct { long long value; } int_lit;

        // NODE_FLOAT_LIT
        struct { double value; } float_lit;

        // NODE_STRING_LIT
        struct { char *value; int is_fmt; int is_raw; } string_lit;

        // NODE_BOOL_LIT
        struct { int value; } bool_lit;

        // NODE_IDENT
        struct { char *name; } ident;

        // NODE_LIST_LIT
        struct { NodeList elements; } list_lit;

        // NODE_MAP_LIT
        struct { NodeList keys; NodeList values; } map_lit;

        // NODE_LOAD, NODE_DLL
        struct { char *path; char *alias_name; } import;

        // NODE_EXPORT
        struct { NodeList items; NodeList except_items; int is_wildcard; } export_stmt;

        // NODE_REFLECT
        struct { char *target; AstNode *body; } reflect;

        // NODE_PARALLEL
        struct { NodeList sections; AstNode *for_iter; AstNode *for_body; } parallel;

        // NODE_SIGNAL
        struct { char *label; AstNode *body; } signal;

        // NODE_DELAY
        struct { char *label; AstNode *body; } delay;
    } as;
};

// Node list operations
void node_list_init(NodeList *list);
void node_list_push(NodeList *list, AstNode *node);
void node_list_free(NodeList *list);

// Node constructors
AstNode *ast_new(NodeType type, int line, int column);
void ast_free(AstNode *node);

// Debug printing
void ast_print(AstNode *node, int indent);

#endif
