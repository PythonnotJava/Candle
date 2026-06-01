#ifndef CANDLE_MODULES_H
#define CANDLE_MODULES_H

#include "value.h"

// 按模块路径加载一个内置标准库模块（如 "std.math" / "std.io"）。
// 返回一个 V_MAP 值，键为公开名、值为 native 函数 / 常量。
// 若模块不存在，返回 V_NULL（type == V_NULL）。
Value module_load(const char *path);

// 判断给定路径是否为已知的内置模块。
int module_exists(const char *path);

#endif
