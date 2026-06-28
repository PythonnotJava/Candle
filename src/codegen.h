#ifndef CANDLE_CODEGEN_H
#define CANDLE_CODEGEN_H

#include "ast.h"
#include <stdio.h>

typedef struct {
    FILE *out;
    int indent;
    int had_error;
    const char *filename;
    const char *module_prefix;    /* non-NULL when emitting a user module ? rewrites internal calls */
} Codegen;

void codegen_preprocess(AstNode *program);
void codegen_run(AstNode *program, FILE *out, const char *filename);

#endif
