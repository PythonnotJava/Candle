#ifndef CANDLE_POINTER_H
#define CANDLE_POINTER_H

#include "../value.h"

// Pointer class — wraps an int64 address for structured FFI memory access.
// Built-in class (not a module), auto-injected into global scope.
// Usage: alias p = Pointer.alloc(16);  p.writeInt32(0, 42);  alias v = p.readInt32(0);
Value build_pointer_class(void);

#endif
