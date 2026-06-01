#ifndef CANDLE_INTERP_H
#define CANDLE_INTERP_H

#include "ast.h"
#include "value.h"

struct Env {
    char **names;
    Value *vals;
    int len, cap;
    Env *parent;
};

Env *env_new(Env *parent);
void env_free(Env *e);
void env_define(Env *e, const char *name, Value v);
Value *env_lookup(Env *e, const char *name);
int env_assign(Env *e, const char *name, Value v);

int interp_run(AstNode *program, const char *filename);
int interp_repl(void);

#endif
