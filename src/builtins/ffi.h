#ifndef CANDLE_FFI_H
#define CANDLE_FFI_H

#include "../value.h"

// 加载动态链接库，返回一个 dll 句柄命名空间（V_MAP，含 __dll__ 标记）。
// 加载失败也返回命名空间（句柄为 NULL），以便 content() 仍可用、桩函数优雅降级。
// dotted_path 形如 "system.sqlite3" → 平台库名 sqlite3.dll / libsqlite3.so。
Value ffi_dll_load(const char *dotted_path, const char *alias_name);

// 在所有已加载库中查找符号 sym 并调用。
// ret_type: 返回类型名（"int"/"void"/"double"/"string"/...，可为 NULL）。
// 找不到符号或无库时返回 v_null()，不崩溃。
Value ffi_call(const char *sym, const char *ret_type, int argc, Value *argv);

// 查询某个符号是否已在已加载库中可解析（用于 content() 等）。
int ffi_symbol_exists(const char *sym);

#endif
