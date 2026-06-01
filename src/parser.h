#ifndef CANDLE_PARSER_H
#define CANDLE_PARSER_H

#include "ast.h"
#include "lexer.h"

typedef struct {
    Lexer lexer;
    Token current;
    Token previous;
    const char *filename;
    int had_error;
    int panic_mode;
} Parser;

AstNode *parse(const char *source, const char *filename);

#endif
