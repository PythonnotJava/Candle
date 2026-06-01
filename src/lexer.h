#ifndef CANDLE_LEXER_H
#define CANDLE_LEXER_H

#include "token.h"

typedef struct {
    const char *source;
    const char *current;
    const char *start;
    int line;
    int column;
    int start_column;
    const char *filename;
} Lexer;

void lexer_init(Lexer *lexer, const char *source, const char *filename);
Token lexer_next(Lexer *lexer);

#endif
