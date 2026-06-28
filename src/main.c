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
    printf("Candle compiler & interpreter ? v0.1\n");
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
    printf("  --compile, -c  Compile to native executable via GCC\n");
    printf("  --repl       Force REPL mode\n");
    printf("  --help, -h   Show this help\n");
    printf("\n");
    printf("EXAMPLES\n");
    printf("  candlec --run hello.candle\n");
    printf("  candlec -c hello.candle            (? hello.exe)\n");
    printf("  candlec -c hello.candle -o myapp   (? myapp.exe)\n");
    printf("  candlec --emit-c app.candle > app.c\n");
    printf("  candlec\n");
    printf("\n");
    printf("PROJECT\n");
    printf("  Candle ? a user-designed, AI-assisted programming language\n");
    printf("  Transpiles to C (GCC backend) ? tree-walk interpreter ? Boehm GC\n");
}

/* Write the generated C to a temp file, compile with GCC, delete the .c */
static int compile_to_exe(AstNode *ast, const char *filepath, const char *out_name) {
    /* Generate C to a .c file next to the source */
    char c_path[1024];
    snprintf(c_path, sizeof(c_path), "%s.c", filepath);

    FILE *f = fopen(c_path, "w");
    if (!f) {
        fprintf(stderr, "error: cannot write %s\n", c_path);
        return 1;
    }
    codegen_preprocess(ast);
    sema_run(ast);
    codegen_run(ast, f, filepath);
    fclose(f);

    /* Detect if OpenMP is needed */
    int needs_openmp = 0;
    FILE *fc = fopen(c_path, "r");
    if (fc) {
        char buf[4096];
        while (fgets(buf, sizeof(buf), fc)) {
            if (strstr(buf, "_Pragma(\"omp")) { needs_openmp = 1; break; }
        }
        fclose(fc);
    }

    /* Determine output name */
    char exe_path[1024];
    if (out_name) {
        snprintf(exe_path, sizeof(exe_path), "%s.exe", out_name);
    } else {
        /* Derive from source: strip .candle, append .exe */
        snprintf(exe_path, sizeof(exe_path), "%s", filepath);
        char *dot = strrchr(exe_path, '.');
        if (dot) *dot = '\0';
        strcat(exe_path, ".exe");
    }

    /* Compile */
    char cmd[2048];
    if (needs_openmp) {
        snprintf(cmd, sizeof(cmd), "gcc \"%s\" -Isrc -fopenmp -o \"%s\"", c_path, exe_path);
    } else {
        snprintf(cmd, sizeof(cmd), "gcc \"%s\" -Isrc -o \"%s\"", c_path, exe_path);
    }

    printf("[candlec] compiling '%s' -> '%s'%s\n",
           filepath, exe_path,
           needs_openmp ? "  (with OpenMP)" : "");

    int gcc_rc = system(cmd);
    remove(c_path);  /* clean up .c file */

    if (gcc_rc == 0) {
        printf("[candlec] done: %s\n", exe_path);
    }
    return gcc_rc;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        return interp_repl();
    }

    int token_mode = 0, ast_mode = 0, emit_c_mode = 0, run_mode = 0;
    int compile_mode = 0, repl_mode = 0;
    const char *filepath = NULL;
    const char *out_name = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0) token_mode = 1;
        else if (strcmp(argv[i], "--ast") == 0) ast_mode = 1;
        else if (strcmp(argv[i], "--emit-c") == 0) emit_c_mode = 1;
        else if (strcmp(argv[i], "--run") == 0) run_mode = 1;
        else if (strcmp(argv[i], "--compile") == 0 || strcmp(argv[i], "-c") == 0) compile_mode = 1;
        else if (strcmp(argv[i], "--repl") == 0) repl_mode = 1;
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help();
            return 0;
        }
        else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) out_name = argv[++i];
            else { fprintf(stderr, "error: -o requires an argument\n"); return 1; }
        }
        else filepath = argv[i];
    }

    if (repl_mode) return interp_repl();

    if (!filepath) { fprintf(stderr, "error: no input file\n"); return 1; }

    char *source = read_file(filepath);
    if (!source) return 1;

    /* Default mode: if no explicit mode given, treat as --run */
    if (!token_mode && !ast_mode && !emit_c_mode && !run_mode && !compile_mode)
        run_mode = 1;

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
    } else if (compile_mode) {
        AstNode *ast = parse(source, filepath);
        if (ast) { rc = compile_to_exe(ast, filepath, out_name); ast_free(ast); }
        else { fprintf(stderr, "error: parsing failed\n"); rc = 1; }
    } else if (run_mode) {
        AstNode *ast = parse(source, filepath);
        if (ast) { rc = interp_run(ast, filepath); ast_free(ast); }
        else { fprintf(stderr, "error: parsing failed\n"); rc = 1; }
    } else {
        fprintf(stderr, "Usage: candlec [--tokens|--ast|--emit-c|--compile|--run] <file.candle>\n");
    }

    free(source);
    return rc;
}
