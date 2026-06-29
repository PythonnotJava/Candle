// std/gui.h — AOT header for candle_gui runtime
// Maps candle_gui_* calls to the same implementation that gui.c provides.
// Linked by candlec -c for Candle AOT programs using load std.gui.
#ifndef CANDLE_GUI_RUNTIME_H
#define CANDLE_GUI_RUNTIME_H

#include "candle_runtime.h"

// All GUI functions return candle_int (bool for isOpen/isMouseDown/isKeyDown)
candle_int candle_gui_createWindow(candle_string title, candle_int width, candle_int height);
candle_int candle_gui_isOpen(void);
candle_int candle_gui_close(void);
candle_int candle_gui_setColor(candle_int r, candle_int g, candle_int b);
candle_int candle_gui_setPenWidth(candle_int w);
candle_int candle_gui_clear(candle_int r, candle_int g, candle_int b);
candle_int candle_gui_fillRect(candle_int x, candle_int y, candle_int w, candle_int h);
candle_int candle_gui_drawLine(candle_int x1, candle_int y1, candle_int x2, candle_int y2);
candle_int candle_gui_drawText(candle_int x, candle_int y, candle_string text);
candle_int candle_gui_refresh(void);
candle_int candle_gui_getMouseX(void);
candle_int candle_gui_getMouseY(void);
candle_int candle_gui_isMouseDown(void);
candle_int candle_gui_isKeyDown(candle_int vk);
candle_int candle_gui_getWidth(void);
candle_int candle_gui_getHeight(void);


// Convenience wrappers — codegen emits gui_xxx, map to candle_gui_xxx
#define gui_createWindow candle_gui_createWindow
#define gui_isOpen       candle_gui_isOpen
#define gui_close        candle_gui_close
#define gui_setColor     candle_gui_setColor
#define gui_setPenWidth  candle_gui_setPenWidth
#define gui_clear        candle_gui_clear
#define gui_fillRect     candle_gui_fillRect
#define gui_drawLine     candle_gui_drawLine
#define gui_drawText     candle_gui_drawText
#define gui_refresh      candle_gui_refresh
#define gui_getMouseX    candle_gui_getMouseX
#define gui_getMouseY    candle_gui_getMouseY
#define gui_isMouseDown  candle_gui_isMouseDown
#define gui_isKeyDown    candle_gui_isKeyDown
#define gui_getWidth     candle_gui_getWidth
#define gui_getHeight    candle_gui_getHeight


// Convenience wrappers — codegen emits gui_xxx, map to candle_gui_xxx
#define gui_createWindow candle_gui_createWindow
#define gui_isOpen       candle_gui_isOpen
#define gui_close        candle_gui_close
#define gui_setColor     candle_gui_setColor
#define gui_setPenWidth  candle_gui_setPenWidth
#define gui_clear        candle_gui_clear
#define gui_fillRect     candle_gui_fillRect
#define gui_drawLine     candle_gui_drawLine
#define gui_drawText     candle_gui_drawText
#define gui_refresh      candle_gui_refresh
#define gui_getMouseX    candle_gui_getMouseX
#define gui_getMouseY    candle_gui_getMouseY
#define gui_isMouseDown  candle_gui_isMouseDown
#define gui_isKeyDown    candle_gui_isKeyDown
#define gui_getWidth     candle_gui_getWidth
#define gui_getHeight    candle_gui_getHeight

#endif
