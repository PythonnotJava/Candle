#ifndef CANDLE_BUILTINS_H
#define CANDLE_BUILTINS_H

#include "value.h"
#include "interp.h"

// 把所有全局内置函数注册进给定的全局作用域。
// 在解释器启动时（interp_run / interp_repl）调用一次。
void builtins_register(Env *globals);

#endif
