#include "parser.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void advance(Parser *p) {
    p->previous = p->current;
    p->current = lexer_next(&p->lexer);
    if (p->current.type == TK_ERROR) {
        error_at(p->filename, p->current.line, p->current.column, "%.*s", p->current.length, p->current.start);
        p->had_error = 1;
    }
}

static int check(Parser *p, TokenType type) {
    return p->current.type == type;
}

static int match(Parser *p, TokenType type) {
    if (!check(p, type)) return 0;
    advance(p);
    return 1;
}

static void consume(Parser *p, TokenType type, const char *msg) {
    if (p->current.type == type) {
        advance(p);
        return;
    }
    error_at(p->filename, p->current.line, p->current.column, "%s", msg);
    p->had_error = 1;
}

static char *token_to_string(Token tok) {
    char *str = malloc(tok.length + 1);
    memcpy(str, tok.start, tok.length);
    str[tok.length] = '\0';
    return str;
}

// Forward declarations
static AstNode *parse_statement(Parser *p);
static AstNode *parse_expression(Parser *p);
static AstNode *parse_assignment(Parser *p);
static AstNode *parse_block(Parser *p);
static int is_type_token(TokenType t);
static char *parse_type_name(Parser *p);

// ── 整数字面量解析（支持 0x/0b/0o 前缀）──────────────────────────────────────
// strtoll(str, NULL, 0) 只认识 0x（十六进制），不认识 0b（二进制）/0o（八进制）。
// 这个包装函数先识别前缀再显式传 base。
static long long parse_int_literal(const char *str) {
    int base = 10;
    const char *p = str;
    if (p[0] == '0' && p[1]) {
        if (p[1] == 'x' || p[1] == 'X') base = 16, p += 2;
        else if (p[1] == 'b' || p[1] == 'B') base = 2, p += 2;
        else if (p[1] == 'o' || p[1] == 'O') base = 8, p += 2;
    }
    return strtoll(p, NULL, base);
}

static AstNode *parse_primary(Parser *p) {
    if (match(p, TK_INT_LIT)) {
        AstNode *node = ast_new(NODE_INT_LIT, p->previous.line, p->previous.column);
        char *str = token_to_string(p->previous);
        node->as.int_lit.value = parse_int_literal(str);
        free(str);
        return node;
    }

    if (match(p, TK_FLOAT_LIT)) {
        AstNode *node = ast_new(NODE_FLOAT_LIT, p->previous.line, p->previous.column);
        char *str = token_to_string(p->previous);
        node->as.float_lit.value = strtod(str, NULL);
        free(str);
        return node;
    }

    if (match(p, TK_STRING_LIT)) {
        AstNode *node = ast_new(NODE_STRING_LIT, p->previous.line, p->previous.column);
        node->as.string_lit.value = token_to_string(p->previous);
        node->as.string_lit.is_fmt = p->previous.start[0] == 'f';
        node->as.string_lit.is_raw = p->previous.start[0] == 'r';
        return node;
    }

    if (match(p, TK_TRUE)) {
        AstNode *node = ast_new(NODE_BOOL_LIT, p->previous.line, p->previous.column);
        node->as.bool_lit.value = 1;
        return node;
    }

    if (match(p, TK_FALSE)) {
        AstNode *node = ast_new(NODE_BOOL_LIT, p->previous.line, p->previous.column);
        node->as.bool_lit.value = 0;
        return node;
    }

    if (match(p, TK_NULL)) {
        return ast_new(NODE_NULL_LIT, p->previous.line, p->previous.column);
    }

    if (match(p, TK_INT) || match(p, TK_DOUBLE) || match(p, TK_BOOL) ||
        match(p, TK_STRING) || match(p, TK_VOID) || match(p, TK_LIST) ||
        match(p, TK_MAP) || match(p, TK_FUNCTION)) {
        AstNode *node = ast_new(NODE_IDENT, p->previous.line, p->previous.column);
        char *base = token_to_string(p->previous);
        // Type Function(params) — 函数类型表达式
        if (check(p, TK_FUNCTION)) {
            advance(p);
            // 跳过参数列表，整体作为类型名
            if (match(p, TK_LPAREN)) {
                int depth = 1;
                while (depth > 0 && !check(p, TK_EOF)) {
                    if (match(p, TK_LPAREN)) depth++;
                    else if (match(p, TK_RPAREN)) depth--;
                    else advance(p);
                }
            }
            free(base);
            node->as.ident.name = strdup("Function");
        } else {
            node->as.ident.name = base;
        }
        return node;
    }

    if (match(p, TK_IDENT)) {
        AstNode *node = ast_new(NODE_IDENT, p->previous.line, p->previous.column);
        node->as.ident.name = token_to_string(p->previous);
        return node;
    }

    if (match(p, TK_THIS)) {
        AstNode *node = ast_new(NODE_IDENT, p->previous.line, p->previous.column);
        node->as.ident.name = strdup("this");
        return node;
    }

    if (match(p, TK_SUPER)) {
        AstNode *node = ast_new(NODE_IDENT, p->previous.line, p->previous.column);
        node->as.ident.name = strdup("super");
        return node;
    }

    if (match(p, TK_LPAREN)) {
        int start_line = p->previous.line, start_col = p->previous.column;

        // 前瞻：判断是否是 lambda 参数列表
        // 形式1: () => ...  或 () { ... }
        // 形式2: (type name, ...) => ...  或 (type name, ...) { ... }
        Lexer saved_lexer = p->lexer;
        Token saved_current = p->current;
        Token saved_previous = p->previous;

        int is_lambda = 0;
        if (check(p, TK_RPAREN)) {
            // () 后跟 => 或 {
            advance(p);
            if (check(p, TK_ARROW) || check(p, TK_LBRACE)) is_lambda = 1;
        } else {
            // 通用前瞻：参数项可为 "type name" 或裸 "name"，逗号分隔，
            // 整个列表后跟 => 或 { 才判定为 lambda
            int ok = 1;
            for (;;) {
                if (is_type_token(p->current.type)) {
                    advance(p);                         // type 或 单个 ident
                    if (check(p, TK_QUESTION)) advance(p);
                    if (check(p, TK_IDENT)) advance(p); // "type name" 形式
                    // 否则是裸 ident 形式（上面 advance 已消耗）
                } else {
                    ok = 0; break;
                }
                if (check(p, TK_COMMA)) { advance(p); continue; }
                break;
            }
            if (ok && check(p, TK_RPAREN)) {
                advance(p);
                if (check(p, TK_ARROW) || check(p, TK_LBRACE)) is_lambda = 1;
            }
        }

        // 恢复状态
        p->lexer = saved_lexer;
        p->current = saved_current;
        p->previous = saved_previous;

        if (is_lambda) {
            AstNode *lambda = ast_new(NODE_LAMBDA, start_line, start_col);
            node_list_init(&lambda->as.lambda.params);

            if (!check(p, TK_RPAREN)) {
                do {
                    AstNode *param = ast_new(NODE_VAR_DECL, p->current.line, p->current.column);
                    // "type name" 或裸 "name"（类型默认 auto）
                    Lexer sl = p->lexer; Token sc = p->current, sp = p->previous;
                    char *tn = parse_type_name(p);
                    if (check(p, TK_IDENT)) {
                        param->as.var.type_name = tn;
                        param->as.var.name = token_to_string(p->current);
                        advance(p);
                    } else {
                        // 裸标识符参数：回退，类型设 auto，名字用该标识符
                        free(tn);
                        p->lexer = sl; p->current = sc; p->previous = sp;
                        param->as.var.type_name = strdup("auto");
                        param->as.var.name = token_to_string(p->current);
                        advance(p);
                    }
                    param->as.var.init = NULL;
                    node_list_push(&lambda->as.lambda.params, param);
                } while (match(p, TK_COMMA));
            }
            consume(p, TK_RPAREN, "expected ')' after lambda parameters");

            if (match(p, TK_ARROW)) {
                if (check(p, TK_LBRACE)) {
                    lambda->as.lambda.body = parse_block(p);
                } else {
                    lambda->as.lambda.body = parse_expression(p);
                }
            } else {
                lambda->as.lambda.body = parse_block(p);
            }
            return lambda;
        }

        AstNode *expr = parse_expression(p);
        // 元组字面量： (a, b, ...) —— 至少两个元素
        if (check(p, TK_COMMA)) {
            AstNode *tuple = ast_new(NODE_TUPLE_LIT, start_line, start_col);
            node_list_init(&tuple->as.list_lit.elements);
            node_list_push(&tuple->as.list_lit.elements, expr);
            while (match(p, TK_COMMA)) {
                if (check(p, TK_RPAREN)) break;  // 允许尾随逗号
                node_list_push(&tuple->as.list_lit.elements, parse_expression(p));
            }
            consume(p, TK_RPAREN, "expected ')' after tuple elements");
            return tuple;
        }
        consume(p, TK_RPAREN, "expected ')' after expression");
        return expr;
    }

    if (match(p, TK_LBRACKET)) {
        AstNode *node = ast_new(NODE_LIST_LIT, p->previous.line, p->previous.column);
        node_list_init(&node->as.list_lit.elements);
        if (!check(p, TK_RBRACKET)) {
            do {
                node_list_push(&node->as.list_lit.elements, parse_expression(p));
            } while (match(p, TK_COMMA));
        }
        consume(p, TK_RBRACKET, "expected ']' after list elements");
        return node;
    }

    if (match(p, TK_LBRACE)) {
        AstNode *node = ast_new(NODE_MAP_LIT, p->previous.line, p->previous.column);
        node_list_init(&node->as.map_lit.keys);
        node_list_init(&node->as.map_lit.values);
        if (!check(p, TK_RBRACE)) {
            do {
                AstNode *key = parse_expression(p);
                consume(p, TK_COLON, "expected ':' after map key");
                AstNode *value = parse_expression(p);
                node_list_push(&node->as.map_lit.keys, key);
                node_list_push(&node->as.map_lit.values, value);
            } while (match(p, TK_COMMA));
        }
        consume(p, TK_RBRACE, "expected '}' after map entries");
        return node;
    }

    error_at(p->filename, p->current.line, p->current.column, "expected expression");
    p->had_error = 1;
    return ast_new(NODE_NULL_LIT, p->current.line, p->current.column);
}

static AstNode *parse_postfix(Parser *p) {
    AstNode *expr = parse_primary(p);

    for (;;) {
        if (match(p, TK_DOT)) {
            if (!check(p, TK_IDENT) && !is_type_token(p->current.type)) {
                error_at(p->filename, p->current.line, p->current.column, "expected identifier after '.'");
                p->had_error = 1;
                break;
            }
            AstNode *member = ast_new(NODE_MEMBER, p->previous.line, p->previous.column);
            member->as.member.object = expr;
            member->as.member.name = token_to_string(p->current);
            advance(p);
            expr = member;
        } else if (match(p, TK_LPAREN)) {
            AstNode *call = ast_new(NODE_CALL, p->previous.line, p->previous.column);
            call->as.call.callee = expr;
            node_list_init(&call->as.call.args);
            if (!check(p, TK_RPAREN)) {
                do {
                    node_list_push(&call->as.call.args, parse_expression(p));
                } while (match(p, TK_COMMA));
            }
            consume(p, TK_RPAREN, "expected ')' after arguments");
            expr = call;
        } else if (match(p, TK_LBRACKET)) {
            AstNode *index = ast_new(NODE_INDEX, p->previous.line, p->previous.column);
            index->as.index_expr.object = expr;
            index->as.index_expr.index = parse_expression(p);
            consume(p, TK_RBRACKET, "expected ']' after index");
            expr = index;
        } else if (check(p, TK_BANG)) {
            // 非空断言后缀 value! —— 解包可空值（运行时为恒等）
            advance(p);
            AstNode *assertion = ast_new(NODE_POSTFIX_UNARY, p->previous.line, p->previous.column);
            assertion->as.unary.op = TK_BANG;
            assertion->as.unary.operand = expr;
            expr = assertion;
        } else if (check(p, TK_INCR) || check(p, TK_DECR)) {
            // 后缀自增/自减 x++ / x--
            TokenType op = p->current.type;
            advance(p);
            AstNode *post = ast_new(NODE_POSTFIX_UNARY, p->previous.line, p->previous.column);
            post->as.unary.op = op;
            post->as.unary.operand = expr;
            expr = post;
        } else {
            break;
        }
    }
    return expr;
}

static AstNode *parse_unary(Parser *p) {
    if (match(p, TK_BANG) || match(p, TK_MINUS) || match(p, TK_BIT_NOT)) {
        AstNode *node = ast_new(NODE_UNARY, p->previous.line, p->previous.column);
        node->as.unary.op = p->previous.type;
        node->as.unary.operand = parse_unary(p);
        return node;
    }
    // 前缀自增/自减 ++x / --x
    if (match(p, TK_INCR) || match(p, TK_DECR)) {
        AstNode *node = ast_new(NODE_UNARY, p->previous.line, p->previous.column);
        node->as.unary.op = p->previous.type;
        node->as.unary.operand = parse_unary(p);
        return node;
    }
    return parse_postfix(p);
}

static AstNode *parse_multiplication(Parser *p) {
    AstNode *left = parse_unary(p);
    while (match(p, TK_STAR) || match(p, TK_SLASH) || match(p, TK_MOD)) {
        AstNode *node = ast_new(NODE_BINARY, p->previous.line, p->previous.column);
        node->as.binary.op = p->previous.type;
        node->as.binary.left = left;
        node->as.binary.right = parse_unary(p);
        left = node;
    }
    return left;
}

static AstNode *parse_addition(Parser *p) {
    AstNode *left = parse_multiplication(p);
    while (match(p, TK_PLUS) || match(p, TK_MINUS)) {
        AstNode *node = ast_new(NODE_BINARY, p->previous.line, p->previous.column);
        node->as.binary.op = p->previous.type;
        node->as.binary.left = left;
        node->as.binary.right = parse_multiplication(p);
        left = node;
    }
    return left;
}

static AstNode *parse_comparison(Parser *p) {
    AstNode *left = parse_addition(p);
    while (match(p, TK_LT) || match(p, TK_LTE) || match(p, TK_GT) || match(p, TK_GTE)) {
        AstNode *node = ast_new(NODE_BINARY, p->previous.line, p->previous.column);
        node->as.binary.op = p->previous.type;
        node->as.binary.left = left;
        node->as.binary.right = parse_addition(p);
        left = node;
    }
    return left;
}

static AstNode *parse_equality(Parser *p) {
    AstNode *left = parse_comparison(p);
    while (match(p, TK_EQ) || match(p, TK_NEQ)) {
        AstNode *node = ast_new(NODE_BINARY, p->previous.line, p->previous.column);
        node->as.binary.op = p->previous.type;
        node->as.binary.left = left;
        node->as.binary.right = parse_comparison(p);
        left = node;
    }
    return left;
}

static AstNode *parse_bitwise_and(Parser *p) {
    AstNode *left = parse_equality(p);
    while (match(p, TK_BIT_AND)) {
        AstNode *node = ast_new(NODE_BINARY, p->previous.line, p->previous.column);
        node->as.binary.op = p->previous.type;
        node->as.binary.left = left;
        node->as.binary.right = parse_equality(p);
        left = node;
    }
    return left;
}

static AstNode *parse_bitwise_xor(Parser *p) {
    AstNode *left = parse_bitwise_and(p);
    while (match(p, TK_BIT_XOR)) {
        AstNode *node = ast_new(NODE_BINARY, p->previous.line, p->previous.column);
        node->as.binary.op = p->previous.type;
        node->as.binary.left = left;
        node->as.binary.right = parse_bitwise_and(p);
        left = node;
    }
    return left;
}

static AstNode *parse_bitwise_or(Parser *p) {
    AstNode *left = parse_bitwise_xor(p);
    while (match(p, TK_BIT_OR)) {
        AstNode *node = ast_new(NODE_BINARY, p->previous.line, p->previous.column);
        node->as.binary.op = p->previous.type;
        node->as.binary.left = left;
        node->as.binary.right = parse_bitwise_xor(p);
        left = node;
    }
    return left;
}

static AstNode *parse_logical_and(Parser *p) {
    AstNode *left = parse_bitwise_or(p);
    while (match(p, TK_AND)) {
        AstNode *node = ast_new(NODE_BINARY, p->previous.line, p->previous.column);
        node->as.binary.op = p->previous.type;
        node->as.binary.left = left;
        node->as.binary.right = parse_bitwise_or(p);
        left = node;
    }
    return left;
}

static AstNode *parse_logical_or(Parser *p) {
    AstNode *left = parse_logical_and(p);
    while (match(p, TK_OR)) {
        AstNode *node = ast_new(NODE_BINARY, p->previous.line, p->previous.column);
        node->as.binary.op = p->previous.type;
        node->as.binary.left = left;
        node->as.binary.right = parse_logical_and(p);
        left = node;
    }
    return left;
}

static AstNode *parse_ternary(Parser *p) {
    AstNode *cond = parse_logical_or(p);
    if (match(p, TK_QUESTION)) {
        AstNode *node = ast_new(NODE_TERNARY, p->previous.line, p->previous.column);
        node->as.ternary.cond = cond;
        node->as.ternary.then_expr = parse_assignment(p);
        consume(p, TK_COLON, "expected ':' in ternary expression");
        node->as.ternary.else_expr = parse_assignment(p);
        return node;
    }
    return cond;
}

static AstNode *parse_assignment(Parser *p) {
    AstNode *expr = parse_ternary(p);
    if (match(p, TK_ASSIGN) || match(p, TK_PLUS_ASSIGN) || match(p, TK_MINUS_ASSIGN) ||
        match(p, TK_STAR_ASSIGN) || match(p, TK_SLASH_ASSIGN) || match(p, TK_MOD_ASSIGN)) {
        AstNode *node = ast_new(NODE_ASSIGN, p->previous.line, p->previous.column);
        node->as.assign.op = p->previous.type;
        node->as.assign.target = expr;
        node->as.assign.value = parse_assignment(p);
        return node;
    }
    return expr;
}

static AstNode *parse_expression(Parser *p) {
    // 单参数无括号 lambda: ident => expr
    if (check(p, TK_IDENT)) {
        Lexer saved_lexer = p->lexer;
        Token saved_current = p->current;
        Token saved_previous = p->previous;
        advance(p);
        if (match(p, TK_ARROW)) {
            AstNode *lambda = ast_new(NODE_LAMBDA, saved_current.line, saved_current.column);
            node_list_init(&lambda->as.lambda.params);
            AstNode *param = ast_new(NODE_VAR_DECL, saved_current.line, saved_current.column);
            param->as.var.type_name = strdup("auto");
            param->as.var.name = token_to_string(saved_current);
            param->as.var.init = NULL;
            node_list_push(&lambda->as.lambda.params, param);
            lambda->as.lambda.body = parse_expression(p);
            return lambda;
        }
        p->lexer = saved_lexer;
        p->current = saved_current;
        p->previous = saved_previous;
    }
    return parse_assignment(p);
}

static AstNode *parse_block(Parser *p) {
    consume(p, TK_LBRACE, "expected '{'");
    AstNode *block = ast_new(NODE_BLOCK, p->previous.line, p->previous.column);
    node_list_init(&block->as.program.stmts);

    while (!check(p, TK_RBRACE) && !check(p, TK_EOF)) {
        Token before = p->current;
        node_list_push(&block->as.program.stmts, parse_statement(p));
        // 防御：解析语句未推进 token 时强制前进，杜绝死循环
        if (p->current.start == before.start && p->current.type == before.type) {
            if (p->current.type == TK_EOF) break;
            advance(p);
        }
    }

    consume(p, TK_RBRACE, "expected '}'");
    return block;
}

static AstNode *parse_if_stmt(Parser *p) {
    AstNode *node = ast_new(NODE_IF, p->previous.line, p->previous.column);
    consume(p, TK_LPAREN, "expected '(' after 'if'");
    node->as.if_stmt.condition = parse_expression(p);
    consume(p, TK_RPAREN, "expected ')' after condition");
    node->as.if_stmt.then_block = parse_block(p);

    node_list_init(&node->as.if_stmt.elif_conds);
    node_list_init(&node->as.if_stmt.elif_blocks);

    while (match(p, TK_ELSE)) {
        if (match(p, TK_IF)) {
            consume(p, TK_LPAREN, "expected '(' after 'else if'");
            node_list_push(&node->as.if_stmt.elif_conds, parse_expression(p));
            consume(p, TK_RPAREN, "expected ')' after condition");
            node_list_push(&node->as.if_stmt.elif_blocks, parse_block(p));
        } else {
            node->as.if_stmt.else_block = parse_block(p);
            break;
        }
    }

    return node;
}

static AstNode *parse_when_stmt(Parser *p) {
    AstNode *node = ast_new(NODE_WHEN, p->previous.line, p->previous.column);
    consume(p, TK_LPAREN, "expected '(' after 'when'");
    node->as.when_stmt.condition = parse_expression(p);
    consume(p, TK_RPAREN, "expected ')' after condition");
    node->as.when_stmt.body = parse_block(p);
    return node;
}

static AstNode *parse_iter_expr(Parser *p);

// 解析单个 iter 参数：可以是普通表达式，也可以是嵌套 iter(...)
static AstNode *parse_iter_arg(Parser *p) {
    if (check(p, TK_ITER)) {
        advance(p);
        return parse_iter_expr(p);
    }
    return parse_expression(p);
}

// 解析 iter(var, args...)，调用前应已消耗 ITER 关键字
static AstNode *parse_iter_expr(Parser *p) {
    consume(p, TK_LPAREN, "expected '(' after 'iter'");
    AstNode *iter = ast_new(NODE_ITER, p->previous.line, p->previous.column);
    consume(p, TK_IDENT, "expected variable name in iter");
    iter->as.iter.var_name = token_to_string(p->previous);
    node_list_init(&iter->as.iter.args);
    while (match(p, TK_COMMA)) {
        node_list_push(&iter->as.iter.args, parse_iter_arg(p));
    }
    consume(p, TK_RPAREN, "expected ')' after iter arguments");
    return iter;
}

static AstNode *parse_for_stmt(Parser *p) {
    AstNode *node = ast_new(NODE_FOR, p->previous.line, p->previous.column);

    consume(p, TK_ITER, "expected 'iter' after 'for'");
    AstNode *iter = parse_iter_expr(p);

    node->as.for_stmt.iter = iter;
    node->as.for_stmt.body = parse_block(p);
    return node;
}

static AstNode *parse_return_stmt(Parser *p) {
    AstNode *node = ast_new(NODE_RETURN, p->previous.line, p->previous.column);
    if (!check(p, TK_SEMI)) {
        node->as.return_stmt.value = parse_expression(p);
    } else {
        node->as.return_stmt.value = NULL;
    }
    consume(p, TK_SEMI, "expected ';' after return");
    return node;
}

static AstNode *parse_break_stmt(Parser *p) {
    AstNode *node = ast_new(NODE_BREAK, p->previous.line, p->previous.column);
    consume(p, TK_SEMI, "expected ';' after break");
    return node;
}

static AstNode *parse_throw_stmt(Parser *p) {
    AstNode *node = ast_new(NODE_THROW, p->previous.line, p->previous.column);
    node->as.throw_stmt.value = parse_expression(p);
    consume(p, TK_SEMI, "expected ';' after throw");
    return node;
}

static AstNode *parse_assert_stmt(Parser *p) {
    AstNode *node = ast_new(NODE_ASSERT, p->previous.line, p->previous.column);
    consume(p, TK_LPAREN, "expected '(' after 'assert'");
    node->as.assert_stmt.expr = parse_expression(p);
    consume(p, TK_RPAREN, "expected ')' after assertion");
    consume(p, TK_SEMI, "expected ';' after assert");
    return node;
}

static AstNode *parse_try_stmt(Parser *p) {
    AstNode *node = ast_new(NODE_TRY, p->previous.line, p->previous.column);
    node->as.try_stmt.body = parse_block(p);

    node_list_init(&node->as.try_stmt.catch_types);
    node_list_init(&node->as.try_stmt.catch_names);
    node_list_init(&node->as.try_stmt.catch_bodies);
    node->as.try_stmt.finally_block = NULL;

    while (match(p, TK_CATCH)) {
        consume(p, TK_LPAREN, "expected '(' after 'catch'");

        // 形态1: catch (Type e)   形态2: catch (e) —— 裸捕获，无类型
        AstNode *type = NULL;
        AstNode *name = NULL;

        // 先读一个标识符
        if (check(p, TK_IDENT)) {
            Token first = p->current;
            advance(p);
            if (check(p, TK_IDENT)) {
                // 两个标识符 → Type name
                type = ast_new(NODE_IDENT, first.line, first.column);
                type->as.ident.name = token_to_string(first);
                name = ast_new(NODE_IDENT, p->current.line, p->current.column);
                name->as.ident.name = token_to_string(p->current);
                advance(p);
            } else {
                // 单标识符 → 裸 catch(e)，类型为 NULL（匹配任意异常）
                name = ast_new(NODE_IDENT, first.line, first.column);
                name->as.ident.name = token_to_string(first);
            }
        } else {
            error_at(p->filename, p->current.line, p->current.column, "expected exception variable name");
            p->had_error = 1;
        }

        node_list_push(&node->as.try_stmt.catch_types, type);
        node_list_push(&node->as.try_stmt.catch_names, name);

        consume(p, TK_RPAREN, "expected ')' after catch parameters");
        node_list_push(&node->as.try_stmt.catch_bodies, parse_block(p));
    }

    // 可选的 final { ... } 块（finally 语义：总会执行）
    if (match(p, TK_FINAL)) {
        node->as.try_stmt.finally_block = parse_block(p);
    }

    return node;
}

static int is_type_token(TokenType t) {
    return t == TK_INT || t == TK_DOUBLE || t == TK_BOOL || t == TK_STRING ||
           t == TK_VOID || t == TK_LIST || t == TK_MAP || t == TK_FUNCTION ||
           t == TK_IDENT;
}

// 可作成员名/方法名的 token：标识符，或类型关键字（map/list/string 等也能当名字，仿 Dart）
static int is_name_token(TokenType t) {
    return t == TK_IDENT || t == TK_INT || t == TK_DOUBLE || t == TK_BOOL ||
           t == TK_STRING || t == TK_VOID || t == TK_LIST || t == TK_MAP ||
           t == TK_FUNCTION;
}

static char *parse_type_name(Parser *p) {
    if (!is_type_token(p->current.type)) return strdup("auto");
    char *name = token_to_string(p->current);
    advance(p);
    if (match(p, TK_QUESTION)) {
        char *nullable = malloc(strlen(name) + 2);
        sprintf(nullable, "%s?", name);
        free(name);
        name = nullable;
    }
    // 函数类型后缀：BaseType Function(TypeList)  →  整体作为类型名 "Function"
    if (check(p, TK_FUNCTION)) {
        advance(p);                       // 消耗 'Function'
        if (match(p, TK_LPAREN)) {        // 跳过参数类型列表（含嵌套括号）
            int depth = 1;
            while (depth > 0 && !check(p, TK_EOF)) {
                if (match(p, TK_LPAREN)) depth++;
                else if (match(p, TK_RPAREN)) depth--;
                else advance(p);
            }
        }
        free(name);
        name = strdup("Function");
    }
    // 联合类型: T1 | T2 | ...  (TK_BIT_OR = '|')
    while (check(p, TK_BIT_OR)) {
        advance(p);  // 消费 '|'
        if (!is_type_token(p->current.type)) break;
        char *next = token_to_string(p->current);
        advance(p);
        // 可空后缀
        if (match(p, TK_QUESTION)) {
            char *nq = malloc(strlen(next) + 2);
            sprintf(nq, "%s?", next);
            free(next);
            next = nq;
        }
        char *combined = malloc(strlen(name) + 1 + strlen(next) + 1);
        sprintf(combined, "%s|%s", name, next);
        free(name); free(next);
        name = combined;
    }
    return name;
}

static AstNode *parse_var_decl(Parser *p) {
    AstNode *node = ast_new(NODE_VAR_DECL, p->current.line, p->current.column);

    if (!is_type_token(p->current.type)) {
        error_at(p->filename, p->current.line, p->current.column, "expected type name");
        p->had_error = 1;
        return node;
    }

    node->as.var.type_name = parse_type_name(p);

    consume(p, TK_IDENT, "expected variable name");
    node->as.var.name = token_to_string(p->previous);

    if (match(p, TK_ASSIGN)) {
        node->as.var.init = parse_expression(p);
    } else {
        node->as.var.init = NULL;
    }

    consume(p, TK_SEMI, "expected ';' after variable declaration");
    return node;
}

static AstNode *parse_alias_decl(Parser *p) {
    AstNode *node = ast_new(NODE_ALIAS, p->previous.line, p->previous.column);
    consume(p, TK_IDENT, "expected alias name");
    node->as.alias.name = token_to_string(p->previous);

    consume(p, TK_ASSIGN, "expected '=' after alias name");
    node->as.alias.value = parse_expression(p);

    if (match(p, TK_ASSERT)) {
        consume(p, TK_LPAREN, "expected '(' after 'assert'");
        if (check(p, TK_INT) || check(p, TK_DOUBLE) || check(p, TK_BOOL) ||
            check(p, TK_STRING) || check(p, TK_VOID) || check(p, TK_LIST) ||
            check(p, TK_MAP) || check(p, TK_FUNCTION) || check(p, TK_IDENT)) {
            node->as.alias.assert_type = token_to_string(p->current);
            advance(p);
        } else {
            error_at(p->filename, p->current.line, p->current.column, "expected type name");
            p->had_error = 1;
            node->as.alias.assert_type = NULL;
        }
        consume(p, TK_RPAREN, "expected ')' after type");
    } else {
        node->as.alias.assert_type = NULL;
    }

    // alias 值若为带 body 的匿名函数（block-body lambda），以 '}' 收尾，分号可选；
    // 其余表达式形式仍要求分号。
    {
        AstNode *av = node->as.alias.value;
        int block_body_lambda = av && av->type == NODE_LAMBDA &&
                                av->as.lambda.body &&
                                av->as.lambda.body->type == NODE_BLOCK;
        if (block_body_lambda)
            match(p, TK_SEMI);            // 可选分号
        else
            consume(p, TK_SEMI, "expected ';' after alias");
    }
    return node;
}

static AstNode *parse_const_decl(Parser *p) {
    AstNode *node = ast_new(NODE_CONST_DECL, p->previous.line, p->previous.column);
    consume(p, TK_IDENT, "expected constant name");
    node->as.constant.name = token_to_string(p->previous);

    consume(p, TK_ASSIGN, "expected '=' after constant name");
    node->as.constant.value = parse_expression(p);

    consume(p, TK_SEMI, "expected ';' after constant");
    return node;
}

static AstNode *parse_func_decl(Parser *p) {
    AstNode *node = ast_new(NODE_FUNC_DECL, p->current.line, p->current.column);

    if (!is_type_token(p->current.type)) {
        error_at(p->filename, p->current.line, p->current.column, "expected return type");
        p->had_error = 1;
        return node;
    }

    node->as.func.return_type = parse_type_name(p);

    consume(p, TK_IDENT, "expected function name");
    node->as.func.name = token_to_string(p->previous);

    consume(p, TK_LPAREN, "expected '(' after function name");
    node_list_init(&node->as.func.params);

    if (!check(p, TK_RPAREN)) {
        do {
            AstNode *param = ast_new(NODE_VAR_DECL, p->current.line, p->current.column);
            if (!is_type_token(p->current.type)) {
                error_at(p->filename, p->current.line, p->current.column, "expected parameter type");
                p->had_error = 1;
                break;
            }
            param->as.var.type_name = parse_type_name(p);
            if (check(p, TK_IDENT)) {
                consume(p, TK_IDENT, "expected parameter name");
                param->as.var.name = token_to_string(p->previous);
            } else {
                param->as.var.name = strdup("_");
            }
            param->as.var.init = NULL;
            node_list_push(&node->as.func.params, param);
        } while (match(p, TK_COMMA));
    }

    consume(p, TK_RPAREN, "expected ')' after parameters");
    if (check(p, TK_SEMI)) {
        advance(p);
        node->as.func.body = NULL;
    } else {
        node->as.func.body = parse_block(p);
    }
    return node;
}

static AstNode *parse_class_decl(Parser *p) {
    AstNode *node = ast_new(NODE_CLASS_DECL, p->previous.line, p->previous.column);
    node->as.class_decl.is_final = 0;
    node->as.class_decl.is_static = 0;
    node->as.class_decl.is_ellipsis = 0;

    consume(p, TK_IDENT, "expected class name");
    node->as.class_decl.name = token_to_string(p->previous);

    if (match(p, TK_INHERIT)) {
        consume(p, TK_IDENT, "expected parent class name");
        node->as.class_decl.parent = token_to_string(p->previous);
    } else {
        node->as.class_decl.parent = NULL;
    }

    consume(p, TK_LBRACE, "expected '{' after class name");
    node_list_init(&node->as.class_decl.members);

    while (!check(p, TK_RBRACE) && !check(p, TK_EOF)) {
        int is_private = 0, is_public = 0, is_static = 0, is_final = 0;

        if (match(p, TK_PRIVATE)) is_private = 1;
        else if (match(p, TK_PUBLIC)) is_public = 1;
        if (match(p, TK_STATIC)) is_static = 1;
        if (match(p, TK_FINAL)) is_final = 1;

        // Constructor: ClassName(params)
        if (check(p, TK_IDENT)) {
            Lexer sl = p->lexer; Token sc = p->current, sp = p->previous;
            advance(p);
            if (check(p, TK_LPAREN)) {
                // constructor
                AstNode *ctor = ast_new(NODE_CONSTRUCTOR, sc.line, sc.column);
                ctor->as.constructor.class_name = token_to_string(sc);
                ctor->as.constructor.body = NULL;
                advance(p); // consume (
                node_list_init(&ctor->as.constructor.params);
                if (!check(p, TK_RPAREN)) {
                    do {
                        AstNode *param = ast_new(NODE_VAR_DECL, p->current.line, p->current.column);
                        // super.field / this.field / type name
                        if (match(p, TK_SUPER)) {
                            consume(p, TK_DOT, "expected '.' after 'super'");
                            param->as.var.type_name = strdup("super");
                            consume(p, TK_IDENT, "expected field name");
                            param->as.var.name = token_to_string(p->previous);
                        } else if (match(p, TK_THIS)) {
                            // this.field — 自动把该参数赋给同名字段
                            consume(p, TK_DOT, "expected '.' after 'this'");
                            param->as.var.type_name = strdup("this");
                            consume(p, TK_IDENT, "expected field name");
                            param->as.var.name = token_to_string(p->previous);
                        } else {
                            param->as.var.type_name = parse_type_name(p);
                            consume(p, TK_IDENT, "expected parameter name");
                            param->as.var.name = token_to_string(p->previous);
                        }
                        param->as.var.init = NULL;
                        node_list_push(&ctor->as.constructor.params, param);
                    } while (match(p, TK_COMMA));
                }
                consume(p, TK_RPAREN, "expected ')' after constructor params");
                // 构造函数可带函数体 { ... } 或以 ; 结尾
                if (check(p, TK_LBRACE)) {
                    ctor->as.constructor.body = parse_block(p);
                } else {
                    consume(p, TK_SEMI, "expected ';' or '{' after constructor declaration");
                }
                node_list_push(&node->as.class_decl.members, ctor);
                continue;
            }
            p->lexer = sl; p->current = sc; p->previous = sp;
        }

        // factory method
        if (match(p, TK_FACTORY)) {
            AstNode *factory = ast_new(NODE_FACTORY_DECL, p->previous.line, p->previous.column);
            factory->as.factory.return_type = parse_type_name(p);
            consume(p, TK_IDENT, "expected factory method name");
            factory->as.factory.name = token_to_string(p->previous);
            consume(p, TK_LPAREN, "expected '('");
            node_list_init(&factory->as.factory.params);
            if (!check(p, TK_RPAREN)) {
                do {
                    AstNode *param = ast_new(NODE_VAR_DECL, p->current.line, p->current.column);
                    param->as.var.type_name = parse_type_name(p);
                    consume(p, TK_IDENT, "expected parameter name");
                    param->as.var.name = token_to_string(p->previous);
                    param->as.var.init = NULL;
                    node_list_push(&factory->as.factory.params, param);
                } while (match(p, TK_COMMA));
            }
            consume(p, TK_RPAREN, "expected ')'");
            factory->as.factory.body = parse_block(p);
            node_list_push(&node->as.class_decl.members, factory);
            continue;
        }

        // field or method: type name ...
        if (is_type_token(p->current.type)) {
            Lexer sl = p->lexer; Token sc = p->current, sp = p->previous;
            char *type = parse_type_name(p);
            if (is_name_token(p->current.type)) {
                char *name_str = token_to_string(p->current);
                advance(p);
                if (check(p, TK_LPAREN)) {
                    // method
                    AstNode *method = ast_new(NODE_METHOD_DECL, sc.line, sc.column);
                    method->as.func.return_type = type;
                    method->as.func.name = name_str;
                    method->as.func.is_private = is_private;
                    method->as.func.is_public = is_public;
                    method->as.func.is_static = is_static;
                    method->as.func.is_final = is_final;
                    advance(p); // consume (
                    node_list_init(&method->as.func.params);
                    if (!check(p, TK_RPAREN)) {
                        do {
                            AstNode *param = ast_new(NODE_VAR_DECL, p->current.line, p->current.column);
                            param->as.var.type_name = parse_type_name(p);
                            consume(p, TK_IDENT, "expected parameter name");
                            param->as.var.name = token_to_string(p->previous);
                            param->as.var.init = NULL;
                            node_list_push(&method->as.func.params, param);
                        } while (match(p, TK_COMMA));
                    }
                    consume(p, TK_RPAREN, "expected ')'");
                    if (check(p, TK_SEMI)) { advance(p); method->as.func.body = NULL; }
                    else method->as.func.body = parse_block(p);
                    node_list_push(&node->as.class_decl.members, method);
                } else {
                    // field
                    AstNode *field = ast_new(NODE_FIELD_DECL, sc.line, sc.column);
                    field->as.var.type_name = type;
                    field->as.var.name = name_str;
                    field->as.var.is_private = is_private;
                    field->as.var.is_static = is_static;
                    field->as.var.is_final = is_final;
                    if (match(p, TK_ASSIGN)) field->as.var.init = parse_expression(p);
                    else field->as.var.init = NULL;
                    consume(p, TK_SEMI, "expected ';' after field");
                    node_list_push(&node->as.class_decl.members, field);
                }
                continue;
            }
            free(type);
            p->lexer = sl; p->current = sc; p->previous = sp;
                        // skip generic type brackets
                if (check(p, TK_LT)) {
                    advance(p);
                    int gdepth = 1;
                    while (gdepth > 0 && !check(p, TK_EOF)) {
                        if (match(p, TK_LT)) gdepth++;
                        else if (match(p, TK_GT)) gdepth--;
                        else advance(p);
                    }
                }
}

        // skip unknown
        advance(p);
    }

    consume(p, TK_RBRACE, "expected '}' after class body");
    return node;
}

static AstNode *parse_parallel_stmt(Parser *p) {
    AstNode *node = ast_new(NODE_PARALLEL, p->previous.line, p->previous.column);
    node_list_init(&node->as.parallel.sections);

    // parallel for iter(...) { }   （iter 外无额外括号，与普通 for 一致）
    if (match(p, TK_FOR)) {
        consume(p, TK_ITER, "expected 'iter' after 'parallel for'");
        consume(p, TK_LPAREN, "expected '(' after 'iter'");
        AstNode *iter = ast_new(NODE_ITER, p->previous.line, p->previous.column);
        consume(p, TK_IDENT, "expected variable name");
        iter->as.iter.var_name = token_to_string(p->previous);
        node_list_init(&iter->as.iter.args);
        while (match(p, TK_COMMA)) node_list_push(&iter->as.iter.args, parse_expression(p));
        consume(p, TK_RPAREN, "expected ')'");
        node->as.parallel.for_iter = iter;
        node->as.parallel.for_body = parse_block(p);
        return node;
    }

    consume(p, TK_LBRACE, "expected '{' after 'parallel'");
    // 段与段之间无逗号分隔，直接读到 '}' 为止
    while (!check(p, TK_RBRACE) && !check(p, TK_EOF)) {
        if (match(p, TK_DELAY)) {
            consume(p, TK_LPAREN, "expected '(' after 'delay'");
            consume(p, TK_IDENT, "expected label");
            AstNode *delay = ast_new(NODE_DELAY, p->previous.line, p->previous.column);
            delay->as.delay.label = token_to_string(p->previous);
            consume(p, TK_RPAREN, "expected ')'");
            delay->as.delay.body = parse_block(p);
            node_list_push(&node->as.parallel.sections, delay);
        } else {
            match(p, TK_SECTION);   // 'section' 关键字可选
            node_list_push(&node->as.parallel.sections, parse_block(p));
        }
    }
    consume(p, TK_RBRACE, "expected '}' after parallel sections");
    return node;
}

static AstNode *parse_signal_stmt(Parser *p) {
    AstNode *node = ast_new(NODE_SIGNAL, p->previous.line, p->previous.column);
    consume(p, TK_LPAREN, "expected '(' after 'signal'");
    consume(p, TK_IDENT, "expected label");
    node->as.signal.label = token_to_string(p->previous);
    consume(p, TK_RPAREN, "expected ')'");
    node->as.signal.body = parse_block(p);
    return node;
}

static AstNode *parse_delay_stmt(Parser *p) {
    AstNode *node = ast_new(NODE_DELAY, p->previous.line, p->previous.column);
    consume(p, TK_LPAREN, "expected '(' after 'delay'");
    consume(p, TK_IDENT, "expected label");
    node->as.delay.label = token_to_string(p->previous);
    consume(p, TK_RPAREN, "expected ')'");
    node->as.delay.body = parse_block(p);
    return node;
}

static AstNode *parse_load_stmt(Parser *p) {
    AstNode *node = ast_new(NODE_LOAD, p->previous.line, p->previous.column);
    // path: ident(.ident)*
    char buf[256] = {0};
    consume(p, TK_IDENT, "expected module path");
    strncat(buf, p->previous.start, p->previous.length);
    while (match(p, TK_DOT)) {
        strncat(buf, ".", 1);
        if (is_type_token(p->current.type) || check(p, TK_IDENT)) {
            strncat(buf, p->current.start, p->current.length);
            advance(p);
        }
    }
    node->as.import.path = strdup(buf);
    if (match(p, TK_ALIAS)) {
        consume(p, TK_IDENT, "expected alias name");
        node->as.import.alias_name = token_to_string(p->previous);
    } else {
        node->as.import.alias_name = NULL;
    }
    consume(p, TK_SEMI, "expected ';' after load");
    return node;
}

static AstNode *parse_dll_stmt(Parser *p) {
    AstNode *node = ast_new(NODE_DLL, p->previous.line, p->previous.column);
    char buf[256] = {0};
    consume(p, TK_IDENT, "expected dll path");
    strncat(buf, p->previous.start, p->previous.length);
    while (match(p, TK_DOT)) {
        strncat(buf, ".", 1);
        if (check(p, TK_IDENT)) { strncat(buf, p->current.start, p->current.length); advance(p); }
    }
    node->as.import.path = strdup(buf);
    if (match(p, TK_ALIAS)) {
        consume(p, TK_IDENT, "expected alias name");
        node->as.import.alias_name = token_to_string(p->previous);
    } else {
        node->as.import.alias_name = NULL;
    }
    consume(p, TK_SEMI, "expected ';' after dll");
    return node;
}

static AstNode *parse_export_stmt(Parser *p) {
    AstNode *node = ast_new(NODE_EXPORT, p->previous.line, p->previous.column);
    node_list_init(&node->as.export_stmt.items);
    node_list_init(&node->as.export_stmt.except_items);
    node->as.export_stmt.is_wildcard = 0;

    // 形态1: export *;  或  export * except {A, B};
    if (match(p, TK_STAR)) {
        node->as.export_stmt.is_wildcard = 1;
        if (match(p, TK_EXCEPT)) {
            consume(p, TK_LBRACE, "expected '{' after 'except'");
            if (!check(p, TK_RBRACE)) {
                do {
                    AstNode *item = ast_new(NODE_IDENT, p->current.line, p->current.column);
                    item->as.ident.name = token_to_string(p->current);
                    advance(p);
                    node_list_push(&node->as.export_stmt.except_items, item);
                } while (match(p, TK_COMMA));
            }
            consume(p, TK_RBRACE, "expected '}' after except list");
        }
        match(p, TK_SEMI); /* optional per PEG */
        return node;
    }

    // 形态2: export {A, B};  （空块 export {}; 表示不导出）
    consume(p, TK_LBRACE, "expected '{' or '*' after 'export'");
    if (!check(p, TK_RBRACE)) {
        do {
            AstNode *item = ast_new(NODE_IDENT, p->current.line, p->current.column);
            item->as.ident.name = token_to_string(p->current);
            advance(p);
            node_list_push(&node->as.export_stmt.items, item);
        } while (match(p, TK_COMMA));
    }
    consume(p, TK_RBRACE, "expected '}' after export list");
    match(p, TK_SEMI); /* optional per PEG */
    return node;
}

static AstNode *parse_reflect_stmt(Parser *p) {
    AstNode *node = ast_new(NODE_REFLECT, p->previous.line, p->previous.column);

    // 目标可以是用户类/实例（标识符），也可以是内置类型（int / double / string / ...）
    if (is_type_token(p->current.type) || check(p, TK_IDENT)) {
        node->as.reflect.target = token_to_string(p->current);
        advance(p);
    } else {
        error_at(p->filename, p->current.line, p->current.column, "expected reflect target (identifier or type)");
        p->had_error = 1;
        node->as.reflect.target = strdup("");
    }

    // body：字段声明 + 方法声明列表，封装进 NODE_BLOCK
    AstNode *body = ast_new(NODE_BLOCK, p->current.line, p->current.column);
    node_list_init(&body->as.program.stmts);

    consume(p, TK_LBRACE, "expected '{' after reflect target");
    while (!check(p, TK_RBRACE) && !check(p, TK_EOF)) {
        // 修饰符：static / private / public / final
        int is_static = 0, is_private = 0, is_public = 0, is_final = 0;
        for (;;) {
            if (match(p, TK_STATIC)) is_static = 1;
            else if (match(p, TK_PRIVATE)) is_private = 1;
            else if (match(p, TK_PUBLIC)) is_public = 1;
            else if (match(p, TK_FINAL)) is_final = 1;
            else break;
        }

        if (is_type_token(p->current.type)) {
            int save_line = p->current.line;
            int save_col = p->current.column;
            char *type_name = parse_type_name(p);
            consume(p, TK_IDENT, "expected name in reflect body");
            char *name = token_to_string(p->previous);

            if (check(p, TK_LPAREN)) {
                // 方法声明：Type name(params) { ... } 或 Type name(params);
                AstNode *method = ast_new(NODE_METHOD_DECL, save_line, save_col);
                method->as.func.return_type = type_name;
                method->as.func.name = name;
                method->as.func.is_static = is_static;
                method->as.func.is_private = is_private;
                method->as.func.is_public = is_public;
                method->as.func.is_final = is_final;
                consume(p, TK_LPAREN, "expected '(' after method name");
                node_list_init(&method->as.func.params);
                if (!check(p, TK_RPAREN)) {
                    do {
                        AstNode *param = ast_new(NODE_VAR_DECL, p->current.line, p->current.column);
                        param->as.var.type_name = parse_type_name(p);
                        consume(p, TK_IDENT, "expected parameter name");
                        param->as.var.name = token_to_string(p->previous);
                        param->as.var.init = NULL;
                        node_list_push(&method->as.func.params, param);
                    } while (match(p, TK_COMMA));
                }
                consume(p, TK_RPAREN, "expected ')' after parameters");
                if (check(p, TK_SEMI)) { advance(p); method->as.func.body = NULL; }
                else method->as.func.body = parse_block(p);
                node_list_push(&body->as.program.stmts, method);
            } else if (match(p, TK_SEMI)) {
                // 字段声明：Type name;
                AstNode *field = ast_new(NODE_FIELD_DECL, save_line, save_col);
                field->as.var.type_name = type_name;
                field->as.var.name = name;
                field->as.var.init = NULL;
                field->as.var.is_static = is_static;
                field->as.var.is_private = is_private;
                node_list_push(&body->as.program.stmts, field);
            } else {
                error_at(p->filename, p->current.line, p->current.column, "expected '(' or ';' after '%s'", name);
                p->had_error = 1;
            }
        } else {
            // 跳过未识别内容（如注释已在词法层处理，此处兜底）
            advance(p);
        }
    }
    consume(p, TK_RBRACE, "expected '}' after reflect body");

    node->as.reflect.body = body;
    return node;
}

static AstNode *parse_statement(Parser *p) {
    if (match(p, TK_IF)) return parse_if_stmt(p);
    if (match(p, TK_WHEN)) return parse_when_stmt(p);
    if (match(p, TK_FOR)) return parse_for_stmt(p);
    if (match(p, TK_RETURN)) return parse_return_stmt(p);
    if (match(p, TK_BREAK)) return parse_break_stmt(p);
    if (match(p, TK_THROW)) return parse_throw_stmt(p);
    if (match(p, TK_ASSERT)) return parse_assert_stmt(p);
    if (match(p, TK_TRY)) return parse_try_stmt(p);
    if (match(p, TK_ALIAS)) return parse_alias_decl(p);
    if (match(p, TK_CONST)) return parse_const_decl(p);
    if (match(p, TK_LOAD)) return parse_load_stmt(p);
    if (match(p, TK_DLL)) return parse_dll_stmt(p);
    if (match(p, TK_EXPORT)) return parse_export_stmt(p);
    if (match(p, TK_REFLECT)) return parse_reflect_stmt(p);
    if (match(p, TK_PARALLEL)) return parse_parallel_stmt(p);
    if (match(p, TK_SIGNAL)) return parse_signal_stmt(p);
    if (match(p, TK_DELAY)) return parse_delay_stmt(p);

    // class with optional modifiers
    {
        int is_final = 0, is_static = 0, is_private = 0, is_public = 0;

        // 收集修饰符（顺序无关）：final / static / private / public
        Lexer mod_lexer = p->lexer;
        Token mod_current = p->current, mod_previous = p->previous;
        for (;;) {
            if (check(p, TK_FINAL))   { advance(p); is_final = 1;   continue; }
            if (check(p, TK_STATIC))  { advance(p); is_static = 1;  continue; }
            if (check(p, TK_PRIVATE)) { advance(p); is_private = 1; continue; }
            if (check(p, TK_PUBLIC))  { advance(p); is_public = 1;  continue; }
            break;
        }

        if (match(p, TK_CLASS)) {
            AstNode *cls = parse_class_decl(p);
            cls->as.class_decl.is_final = is_final;
            cls->as.class_decl.is_static = is_static;
            return cls;
        }

        // 修饰符后是类型 + 标识符 → 顶层函数声明（FuncModifier* Type Identifier ...）
        if ((is_final || is_static || is_private || is_public) && is_type_token(p->current.type)) {
            Lexer sl = p->lexer;
            Token sc = p->current, sp = p->previous;
            advance(p);
            if (check(p, TK_QUESTION)) advance(p);
            if (check(p, TK_IDENT)) {
                advance(p);
                int is_func = check(p, TK_LPAREN);
                p->lexer = sl; p->current = sc; p->previous = sp;
                if (is_func) {
                    AstNode *fn = parse_func_decl(p);
                    fn->as.func.is_final = is_final;
                    fn->as.func.is_static = is_static;
                    fn->as.func.is_private = is_private;
                    fn->as.func.is_public = is_public;
                    return fn;
                }
                p->lexer = sl; p->current = sc; p->previous = sp;
                return parse_var_decl(p);
            }
            p->lexer = sl; p->current = sc; p->previous = sp;
        }

        if (is_final || is_static || is_private || is_public) {
            // 修饰符后既不是 class 也不是声明 → 回退，按表达式处理
            p->lexer = mod_lexer;
            p->current = mod_current;
            p->previous = mod_previous;
        }
    }

    if (match(p, TK_ELLIPSIS)) {
        if (check(p, TK_SEMI)) { advance(p); return ast_new(NODE_ELLIPSIS_EXPR, p->previous.line, p->previous.column); }
        if (check(p, TK_CLASS)) { advance(p); AstNode *cls = parse_class_decl(p); cls->as.class_decl.is_ellipsis = 1; return cls; }
        AstNode *fn = parse_func_decl(p);
        fn->as.func.is_ellipsis = 1;
        return fn;
    }

    if (is_type_token(p->current.type)) {
        Lexer saved_lexer = p->lexer;
        Token saved_current = p->current;
        Token saved_previous = p->previous;
        advance(p);
        if (check(p, TK_QUESTION)) advance(p);
        // 跳过函数类型后缀 Function(...)
        if (check(p, TK_FUNCTION)) {
            advance(p);
            if (match(p, TK_LPAREN)) {
                int depth = 1;
                while (depth > 0 && !check(p, TK_EOF)) {
                    if (match(p, TK_LPAREN)) depth++;
                    else if (match(p, TK_RPAREN)) depth--;
                    else advance(p);
                }
            }
        }
        // union type: consume "| Type" suffixes (int | double | string)
        while (check(p, TK_BIT_OR)) {
            advance(p);
            if (is_type_token(p->current.type)) {
                advance(p);
                if (check(p, TK_QUESTION)) advance(p);
                if (check(p, TK_FUNCTION)) {
                    advance(p);
                    if (match(p, TK_LPAREN)) {
                        int depth = 1;
                        while (depth > 0 && !check(p, TK_EOF)) {
                            if (match(p, TK_LPAREN)) depth++;
                            else if (match(p, TK_RPAREN)) depth--;
                            else advance(p);
                        }
                    }
                }
            }
        }
        if (check(p, TK_IDENT)) {
            advance(p);
            int is_func = check(p, TK_LPAREN);
            p->lexer = saved_lexer;
            p->current = saved_current;
            p->previous = saved_previous;
            return is_func ? parse_func_decl(p) : parse_var_decl(p);
        }
        p->lexer = saved_lexer;
        p->current = saved_current;
        p->previous = saved_previous;
    }

    AstNode *expr = parse_expression(p);
    consume(p, TK_SEMI, "expected ';' after expression");
    AstNode *stmt = ast_new(NODE_EXPR_STMT, expr->line, expr->column);
    node_list_init(&stmt->as.program.stmts);
    node_list_push(&stmt->as.program.stmts, expr);
    return stmt;
}

AstNode *parse(const char *source, const char *filename) {
    Parser p;
    lexer_init(&p.lexer, source, filename);
    p.filename = filename;
    p.had_error = 0;
    p.panic_mode = 0;

    advance(&p);

    AstNode *program = ast_new(NODE_PROGRAM, 1, 1);
    node_list_init(&program->as.program.stmts);

    while (!match(&p, TK_EOF)) {
        Token before = p.current;
        node_list_push(&program->as.program.stmts, parse_statement(&p));
        // 防御：若一次 parse_statement 未消耗任何 token（出错卡住），强制前进，杜绝死循环
        if (p.current.start == before.start && p.current.type == before.type) {
            if (p.current.type == TK_EOF) break;
            advance(&p);
        }
    }

    if (p.had_error) {
        ast_free(program);
        return NULL;
    }

    return program;
}
