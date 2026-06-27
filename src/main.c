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

static void print_help(void) {
    printf("Candle compiler & interpreter — v0.1\n");
    printf("\n");
    printf("USAGE\n");
    printf("  candlec [mode] <file.candle>\n");
    printf("  candlec                        (enter REPL)\n");
    printf("\n");
    printf("MODES\n");
    printf("  --tokens     Lexer: print token stream\n");
    printf("  --ast        Parser: print syntax tree (AST)\n");
    printf("  --emit-c     Codegen: transpile to C source (stdout)\n");
    printf("  --run        Interpreter: execute directly (default)\n");
    printf("  --repl       Force REPL mode\n");
    printf("  --help, -h   Show this help\n");
    printf("\n");
    printf("EXAMPLES\n");
    printf("  candlec --run hello.candle\n");
    printf("  candlec --tokens test.candle\n");
    printf("  candlec --emit-c app.candle > app.c && gcc app.c -Isrc -o app\n");
    printf("  candlec\n");
    printf("\n");
    printf("PROJECT\n");
    printf("  Candle — a user-designed, AI-assisted programming language\n");
    printf("  Transpiles to C (GCC backend) · tree-walk interpreter · Boehm GC\n");
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
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help();
            return 0;
        }
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
