#ifndef CANDLE_GUI_H
#define CANDLE_GUI_H

#include "../value.h"

// GUI module — Win32 window + double-buffered canvas
// Provides: createWindow, isOpen, beginPaint, endPaint, setPixel, fillRect,
//           drawLine, drawText, getMouseX, getMouseY, isMouseDown, isKeyDown, close

Value build_gui(void);

#endif
