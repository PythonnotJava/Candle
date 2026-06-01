#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "token.h"
#include "util.h"
#include "parser.h"
#include "ast.h"
#include "codegen.h"
#include "sema.h"
#include "interp.h"

static void print_tokens(const char *source, const char *filename) {
    Lexer lexer;
    lexer_init(&lexer, source, filename);

    printf("%-6s %-4s %-16s %s\n", "LINE", "COL", "TYPE", "TEXT");
    printf("------ ---- ---------------- ----------------\n");

    for (;;) {
        Token tok = lexer_next(&lexer);
        printf("%-6d %-4d %-16s %.*s\n",
               tok.line, tok.column,
               token_type_name(tok.type),
               tok.length, tok.start);

        if (tok.type == TK_EOF || tok.type == TK_ERROR) break;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        // No args → REPL
        return interp_repl();
    }

    int token_mode = 0, ast_mode = 0, emit_c_mode = 0, run_mode = 0, repl_mode = 0;
    const char *filepath = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0) token_mode = 1;
        else if (strcmp(argv[i], "--ast") == 0) ast_mode = 1;
        else if (strcmp(argv[i], "--emit-c") == 0) emit_c_mode = 1;
        else if (strcmp(argv[i], "--run") == 0) run_mode = 1;
        else if (strcmp(argv[i], "--repl") == 0) repl_mode = 1;
        else filepath = argv[i];
    }

    if (repl_mode) return interp_repl();

    if (!filepath) { fprintf(stderr, "error: no input file\n"); return 1; }

    char *source = read_file(filepath);
    if (!source) return 1;

    int rc = 0;
    if (token_mode) {
        print_tokens(source, filepath);
    } else if (ast_mode) {
        AstNode *ast = parse(source, filepath);
        if (ast) { ast_print(ast, 0); ast_free(ast); }
        else { fprintf(stderr, "error: parsing failed\n"); rc = 1; }
    } else if (emit_c_mode) {
        AstNode *ast = parse(source, filepath);
        if (ast) { codegen_preprocess(ast); sema_run(ast); codegen_run(ast, stdout, filepath); ast_free(ast); }
        else { fprintf(stderr, "error: parsing failed\n"); rc = 1; }
    } else if (run_mode) {
        AstNode *ast = parse(source, filepath);
        if (ast) { rc = interp_run(ast, filepath); ast_free(ast); }
        else { fprintf(stderr, "error: parsing failed\n"); rc = 1; }
    } else {
        fprintf(stderr, "Usage: candlec [--tokens|--ast|--emit-c|--run] <file.candle>\n");
    }

    free(source);
    return rc;
}
