#ifndef CANDLE_FFI_MEM_H
#define CANDLE_FFI_MEM_H

#include "../value.h"

// FFI memory management — allows Candle to manually manage C-allocated memory.
// Entry point: build_ffi_mem() returns a VMap with bound functions.

Value build_ffi_mem(void);

#endif
