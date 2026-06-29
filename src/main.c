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
    printf("  --compile, -c  Compile to native executable via GCC\n  --pack          Pack to standalone exe (interpreter + source embedded)\n");
    printf("  --repl       Force REPL mode\n");
    printf("  --help, -h   Show this help\n");
    printf("\n");
    printf("EXAMPLES\n");
    printf("  candlec --run hello.candle\n");
    printf("  candlec -c hello.candle            (? hello.exe)\n");
    printf("  candlec -c hello.candle -o myapp   (? myapp.exe)\n  candlec --pack hello.candle        (? hello.exe)\n");
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
    const char *rt_files[] = {
        "src/std/_builtins_aot_runtime.c",
        "src/std/gui_runtime.c"
    };
    int n_rt = 0;
    char rt_args[1024] = "";
    for (int fi = 0; fi < 2; fi++) {
        FILE *ft = fopen(rt_files[fi], "r");
        if (ft) { fclose(ft); n_rt++; strcat(rt_args, " \""); strcat(rt_args, rt_files[fi]); strcat(rt_args, "\""); }
    }
    if (needs_openmp) {
        if (n_rt > 0)
            snprintf(cmd, sizeof(cmd), "gcc \"%s\"%s -Isrc -fopenmp -lgdi32 -luser32 -o \"%s\"", c_path, rt_args, exe_path);
        else
            snprintf(cmd, sizeof(cmd), "gcc \"%s\" -Isrc -fopenmp -o \"%s\"", c_path, exe_path);
    } else {
        if (n_rt > 0)
            snprintf(cmd, sizeof(cmd), "gcc \"%s\"%s -Isrc -lgdi32 -luser32 -o \"%s\"", c_path, rt_args, exe_path);
        else
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


/* Forward declaration */
static int pack_to_exe(const char *filepath, const char *out_name);
int main(int argc, char **argv) {
    if (argc < 2) {
        return interp_repl();
    }

    int token_mode = 0, ast_mode = 0, emit_c_mode = 0, run_mode = 0;
    int compile_mode = 0, pack_mode = 0, repl_mode = 0;
    const char *filepath = NULL;
    const char *out_name = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0) token_mode = 1;
        else if (strcmp(argv[i], "--ast") == 0) ast_mode = 1;
        else if (strcmp(argv[i], "--emit-c") == 0) emit_c_mode = 1;
        else if (strcmp(argv[i], "--run") == 0) run_mode = 1;
        else if (strcmp(argv[i], "--compile") == 0 || strcmp(argv[i], "-c") == 0) compile_mode = 1;
        else if (strcmp(argv[i], "--pack") == 0) pack_mode = 1;
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
    if (!token_mode && !ast_mode && !emit_c_mode && !run_mode && !compile_mode && !pack_mode)
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
    } else if (pack_mode) {
        rc = pack_to_exe(filepath, out_name);
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

/* Pack: embed source + interpreter into a standalone exe */
static int pack_to_exe(const char *filepath, const char *out_name) {
    /* Read the .candle source */
    FILE *fs = fopen(filepath, "rb");
    if (!fs) { fprintf(stderr, "error: cannot open %s\n", filepath); return 1; }
    fseek(fs, 0, SEEK_END);
    long sz = ftell(fs);
    fseek(fs, 0, SEEK_SET);
    char *src = malloc(sz + 1);
    fread(src, 1, sz, fs);
    src[sz] = 0;
    fclose(fs);

    /* Generate C wrapper */
    char c_path[1024];
    snprintf(c_path, sizeof(c_path), "%s_pack.c", filepath);

    FILE *fc = fopen(c_path, "w");
    if (!fc) { fprintf(stderr, "error: cannot write %s\n", c_path); free(src); return 1; }

    fprintf(fc, "/* Auto-generated by candlec --pack */\n");
    fprintf(fc, "static const char g_source[] =\n");
    /* Embed source as escaped C string literal */
    for (long i = 0; i < sz; i++) {
        if (i == 0) fprintf(fc, "  \"");
        if (src[i] == '\\') fprintf(fc, "\\\\");
        else if (src[i] == '\"') fprintf(fc, "\\\"");
        else if (src[i] == '\n') { fprintf(fc, "\\n\"\n  \""); continue; }
        else if (src[i] == '\r') { fprintf(fc, "\\r\"\n  \""); continue; }
        else if (src[i] == '\t') fprintf(fc, "\\t");
        else fputc(src[i], fc);
    }
    fprintf(fc, "\";\n\n");

    fprintf(fc, "int interp_run_string(const char *source, const char *filename);\n\n");
    fprintf(fc, "int main(void) {\n");
        /* Escape backslashes for C string */
    char *escaped_filepath = malloc(strlen(filepath) * 2 + 1);
    char *dp = escaped_filepath;
    for (const char *sp = filepath; *sp; sp++) {
        if (*sp == '\\') *dp++ = '\\';
        *dp++ = *sp;
    }
    *dp = 0;
    fprintf(fc, "    return interp_run_string(g_source, \"%s\");\n", escaped_filepath);
    free(escaped_filepath);
    fprintf(fc, "}\n");

    fclose(fc);
    free(src);

    /* Determine output name */
    char exe_path[1024];
    if (out_name) {
        snprintf(exe_path, sizeof(exe_path), "%s.exe", out_name);
    } else {
        snprintf(exe_path, sizeof(exe_path), "%s", filepath);
        char *dot = strrchr(exe_path, '.');
        if (dot) *dot = 0;
        strcat(exe_path, ".exe");
    }

    /* Collect all candlec source files to compile */
    const char *engine_src[] = {
        "src/lexer.c", "src/token.c", "src/util.c",
        "src/ast.c", "src/parser.c", "src/sema.c", "src/codegen.c",
        "src/value.c", "src/interp.c", "src/threading.c",
        "src/builtins/builtins.c", "src/builtins/modules.c",
        "src/builtins/ffi.c", "src/builtins/ffi_mem.c",
        "src/builtins/http.c", "src/builtins/time.c",
        "src/builtins/fs.c", "src/builtins/file.c",
        "src/builtins/random.c", "src/builtins/process.c",
        "src/builtins/path.c", "src/builtins/encoding.c",
        "src/builtins/gui.c", "src/builtins/datetime.c",
        "src/builtins/collections.c", "src/builtins/crypto.c",
        "src/vendor/cjson/cJSON.c"
    };
    int n_src = sizeof(engine_src) / sizeof(engine_src[0]);

    char cmd[8192];
    snprintf(cmd, sizeof(cmd), "gcc \"%s\"", c_path);
    int cmd_len = strlen(cmd);
    for (int i = 0; i < n_src; i++) {
        cmd_len += snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, " \"%s\"", engine_src[i]);
    }
    snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len,
             " -Isrc -lgdi32 -luser32 -lws2_32 -o \"%s\"", exe_path);

    printf("[candlec] packing '%s' -> '%s'\n", filepath, exe_path);

    int gcc_rc = system(cmd);
    if (gcc_rc != 0) {
        fprintf(stderr, "[candlec] GCC failed\n");
        return 1;
    }

    /* Clean up generated C file */
    remove(c_path);

    printf("[candlec] done: %s\n", exe_path);
    return 0;
}

