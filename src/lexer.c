#include "lexer.h"
#include <string.h>
#include <ctype.h>

typedef struct {
    const char *name;
    int length;
    TokenType type;
} Keyword;

static const Keyword keywords[] = {
    {"alias",    5, TK_ALIAS},
    {"assert",   6, TK_ASSERT},
    {"bool",     4, TK_BOOL},
    {"break",    5, TK_BREAK},
    {"catch",    5, TK_CATCH},
    {"class",    5, TK_CLASS},
    {"const",    5, TK_CONST},
    {"delay",    5, TK_DELAY},
    {"dll",      3, TK_DLL},
    {"double",   6, TK_DOUBLE},
    {"ellipsis", 8, TK_ELLIPSIS},
    {"else",     4, TK_ELSE},
    {"except",   6, TK_EXCEPT},
    {"export",   6, TK_EXPORT},
    {"factory",  7, TK_FACTORY},
    {"false",    5, TK_FALSE},
    {"final",    5, TK_FINAL},
    {"for",      3, TK_FOR},
    {"if",       2, TK_IF},
    {"inherit",  7, TK_INHERIT},
    {"int",      3, TK_INT},
    {"iter",     4, TK_ITER},
    {"list",     4, TK_LIST},
    {"load",     4, TK_LOAD},
    {"map",      3, TK_MAP},
    {"null",     4, TK_NULL},
    {"parallel", 8, TK_PARALLEL},
    {"private",  7, TK_PRIVATE},
    {"public",   6, TK_PUBLIC},
    {"reflect",  7, TK_REFLECT},
    {"return",   6, TK_RETURN},
    {"section",  7, TK_SECTION},
    {"signal",   6, TK_SIGNAL},
    {"static",   6, TK_STATIC},
    {"string",   6, TK_STRING},
    {"super",    5, TK_SUPER},
    {"this",     4, TK_THIS},
    {"throw",    5, TK_THROW},
    {"true",     4, TK_TRUE},
    {"try",      3, TK_TRY},
    {"void",     4, TK_VOID},
    {"when",     4, TK_WHEN},
    {"Function", 8, TK_FUNCTION},
    {NULL,       0, TK_ERROR}
};

void lexer_init(Lexer *lexer, const char *source, const char *filename) {
    lexer->source = source;
    lexer->current = source;
    lexer->start = source;
    lexer->line = 1;
    lexer->column = 1;
    lexer->start_column = 1;
    lexer->filename = filename;
}

static char peek(Lexer *lexer) {
    return *lexer->current;
}

static char peek_next(Lexer *lexer) {
    if (*lexer->current == '\0') return '\0';
    return lexer->current[1];
}

static char advance(Lexer *lexer) {
    char c = *lexer->current++;
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    return c;
}

static int match(Lexer *lexer, char expected) {
    if (*lexer->current == '\0') return 0;
    if (*lexer->current != expected) return 0;
    advance(lexer);
    return 1;
}

static Token make_token(Lexer *lexer, TokenType type) {
    Token tok;
    tok.type = type;
    tok.start = lexer->start;
    tok.length = (int)(lexer->current - lexer->start);
    tok.line = lexer->line;
    tok.column = lexer->start_column;
    return tok;
}

static Token error_token(Lexer *lexer, const char *msg) {
    Token tok;
    tok.type = TK_ERROR;
    tok.start = msg;
    tok.length = (int)strlen(msg);
    tok.line = lexer->line;
    tok.column = lexer->start_column;
    return tok;
}

static void skip_whitespace(Lexer *lexer) {
    for (;;) {
        char c = peek(lexer);
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                advance(lexer);
                break;
            case '/':
                if (peek_next(lexer) == '/') {
                    while (peek(lexer) != '\n' && peek(lexer) != '\0')
                        advance(lexer);
                } else if (peek_next(lexer) == '*') {
                    advance(lexer);
                    advance(lexer);
                    while (!(peek(lexer) == '*' && peek_next(lexer) == '/')) {
                        if (peek(lexer) == '\0') return;
                        advance(lexer);
                    }
                    advance(lexer);
                    advance(lexer);
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static TokenType check_keyword(const char *start, int length) {
    for (int i = 0; keywords[i].name != NULL; i++) {
        if (keywords[i].length == length &&
            memcmp(keywords[i].name, start, length) == 0) {
            return keywords[i].type;
        }
    }
    return TK_IDENT;
}

static Token scan_identifier(Lexer *lexer) {
    while (isalnum(peek(lexer)) || peek(lexer) == '_')
        advance(lexer);

    int length = (int)(lexer->current - lexer->start);
    TokenType type = check_keyword(lexer->start, length);
    return make_token(lexer, type);
}

static Token scan_number(Lexer *lexer) {
    if (lexer->start[0] == '0' && lexer->current == lexer->start + 1) {
        char next = peek(lexer);
        if (next == 'x' || next == 'X') {
            advance(lexer);
            while (isxdigit(peek(lexer))) advance(lexer);
            return make_token(lexer, TK_INT_LIT);
        }
        if (next == 'b' || next == 'B') {
            advance(lexer);
            while (peek(lexer) == '0' || peek(lexer) == '1') advance(lexer);
            return make_token(lexer, TK_INT_LIT);
        }
        if (next == 'o' || next == 'O') {
            advance(lexer);
            while (peek(lexer) >= '0' && peek(lexer) <= '7') advance(lexer);
            return make_token(lexer, TK_INT_LIT);
        }
    }

    while (isdigit(peek(lexer))) advance(lexer);

    if (peek(lexer) == '.' && isdigit(peek_next(lexer))) {
        advance(lexer);
        while (isdigit(peek(lexer))) advance(lexer);

        if (peek(lexer) == 'e' || peek(lexer) == 'E') {
            advance(lexer);
            if (peek(lexer) == '+' || peek(lexer) == '-') advance(lexer);
            while (isdigit(peek(lexer))) advance(lexer);
        }
        return make_token(lexer, TK_FLOAT_LIT);
    }

    return make_token(lexer, TK_INT_LIT);
}

static Token scan_string(Lexer *lexer, char quote) {
    while (peek(lexer) != quote && peek(lexer) != '\0') {
        if (peek(lexer) == '\\') advance(lexer);
        advance(lexer);
    }

    if (peek(lexer) == '\0') {
        return error_token(lexer, "unterminated string");
    }

    advance(lexer);
    return make_token(lexer, TK_STRING_LIT);
}

Token lexer_next(Lexer *lexer) {
    skip_whitespace(lexer);

    lexer->start = lexer->current;
    lexer->start_column = lexer->column;

    if (*lexer->current == '\0') {
        return make_token(lexer, TK_EOF);
    }

    char c = advance(lexer);

    if (c == 'f' && (peek(lexer) == '\'' || peek(lexer) == '"')) {
        char quote = advance(lexer);
        return scan_string(lexer, quote);
    }
    if (c == 'r' && (peek(lexer) == '\'' || peek(lexer) == '"')) {
        char quote = advance(lexer);
        return scan_string(lexer, quote);
    }

    if (isalpha(c) || c == '_') return scan_identifier(lexer);
    if (isdigit(c)) return scan_number(lexer);

    switch (c) {
        case '(': return make_token(lexer, TK_LPAREN);
        case ')': return make_token(lexer, TK_RPAREN);
        case '{': return make_token(lexer, TK_LBRACE);
        case '}': return make_token(lexer, TK_RBRACE);
        case '[': return make_token(lexer, TK_LBRACKET);
        case ']': return make_token(lexer, TK_RBRACKET);
        case ';': return make_token(lexer, TK_SEMI);
        case ',': return make_token(lexer, TK_COMMA);
        case '.': return make_token(lexer, TK_DOT);
        case ':': return make_token(lexer, TK_COLON);
        case '?': return make_token(lexer, TK_QUESTION);
        case '@': return make_token(lexer, TK_AT);
        case '~': return make_token(lexer, TK_BIT_NOT);
        case '^': return make_token(lexer, TK_BIT_XOR);

        case '+':
            if (match(lexer, '+')) return make_token(lexer, TK_INCR);
            if (match(lexer, '=')) return make_token(lexer, TK_PLUS_ASSIGN);
            return make_token(lexer, TK_PLUS);
        case '-':
            if (match(lexer, '-')) return make_token(lexer, TK_DECR);
            if (match(lexer, '=')) return make_token(lexer, TK_MINUS_ASSIGN);
            return make_token(lexer, TK_MINUS);
        case '*':
            if (match(lexer, '=')) return make_token(lexer, TK_STAR_ASSIGN);
            return make_token(lexer, TK_STAR);
        case '/':
            if (match(lexer, '=')) return make_token(lexer, TK_SLASH_ASSIGN);
            return make_token(lexer, TK_SLASH);
        case '%':
            if (match(lexer, '=')) return make_token(lexer, TK_MOD_ASSIGN);
            return make_token(lexer, TK_MOD);

        case '=':
            if (match(lexer, '>')) return make_token(lexer, TK_ARROW);
            if (match(lexer, '=')) return make_token(lexer, TK_EQ);
            return make_token(lexer, TK_ASSIGN);
        case '!':
            if (match(lexer, '=')) return make_token(lexer, TK_NEQ);
            return make_token(lexer, TK_BANG);
        case '<':
            if (match(lexer, '=')) return make_token(lexer, TK_LTE);
            return make_token(lexer, TK_LT);
        case '>':
            if (match(lexer, '=')) return make_token(lexer, TK_GTE);
            return make_token(lexer, TK_GT);

        case '&':
            if (match(lexer, '&')) return make_token(lexer, TK_AND);
            return make_token(lexer, TK_BIT_AND);
        case '|':
            if (match(lexer, '|')) return make_token(lexer, TK_OR);
            return make_token(lexer, TK_BIT_OR);

        case '\'':
        case '"':
            return scan_string(lexer, c);
    }

    return error_token(lexer, "unexpected character");
}
