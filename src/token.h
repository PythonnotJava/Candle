#ifndef CANDLE_TOKEN_H
#define CANDLE_TOKEN_H

typedef enum {
    // Keywords
    TK_ALIAS,
    TK_ASSERT,
    TK_BREAK,
    TK_CATCH,
    TK_CLASS,
    TK_CONST,
    TK_DELAY,
    TK_DLL,
    TK_ELSE,
    TK_ELLIPSIS,
    TK_EXCEPT,
    TK_EXPORT,
    TK_FACTORY,
    TK_FALSE,
    TK_FINAL,
    TK_FOR,
    TK_FUNCTION,
    TK_IF,
    TK_INHERIT,
    TK_ITER,
    TK_LOAD,
    TK_NULL,
    TK_PARALLEL,
    TK_PRIVATE,
    TK_PUBLIC,
    TK_REFLECT,
    TK_RETURN,
    TK_SECTION,
    TK_SIGNAL,
    TK_STATIC,
    TK_SUPER,
    TK_THIS,
    TK_THROW,
    TK_TRUE,
    TK_TRY,
    TK_WHEN,

    // Type keywords
    TK_INT,
    TK_DOUBLE,
    TK_STRING,
    TK_BOOL,
    TK_LIST,
    TK_MAP,
    TK_VOID,

    // Literals
    TK_INT_LIT,
    TK_FLOAT_LIT,
    TK_STRING_LIT,

    // Identifier
    TK_IDENT,

    // Operators
    TK_PLUS,        // +
    TK_MINUS,       // -
    TK_STAR,        // *
    TK_SLASH,       // /
    TK_MOD,         // %
    TK_INCR,        // ++
    TK_DECR,        // --
    TK_ASSIGN,      // =
    TK_ARROW,       // =>
    TK_PLUS_ASSIGN, // +=
    TK_MINUS_ASSIGN,// -=
    TK_STAR_ASSIGN, // *=
    TK_SLASH_ASSIGN,// /=
    TK_MOD_ASSIGN,  // %=
    TK_EQ,          // ==
    TK_NEQ,         // !=
    TK_LT,          // <
    TK_GT,          // >
    TK_LTE,         // <=
    TK_GTE,         // >=
    TK_AND,         // &&
    TK_OR,          // ||
    TK_BANG,        // !
    TK_BIT_AND,     // &
    TK_BIT_OR,      // |
    TK_BIT_XOR,     // ^
    TK_BIT_NOT,     // ~

    // Delimiters
    TK_LPAREN,      // (
    TK_RPAREN,      // )
    TK_LBRACE,      // {
    TK_RBRACE,      // }
    TK_LBRACKET,    // [
    TK_RBRACKET,    // ]
    TK_SEMI,        // ;
    TK_COMMA,       // ,
    TK_DOT,         // .
    TK_COLON,       // :
    TK_QUESTION,    // ?
    TK_AT,          // @
    TK_PIPE,        // | (in type union context, same char as BIT_OR)

    // Special
    TK_EOF,
    TK_ERROR
} TokenType;

typedef struct {
    TokenType type;
    const char *start;
    int length;
    int line;
    int column;
} Token;

const char *token_type_name(TokenType type);

#endif
