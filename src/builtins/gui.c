// gui.c ─ Candle GUI module (Win32 window + double-buffered canvas)
// Includes: createWindow, isOpen, close, setColor, setPenWidth, clear,
//           fillRect, drawLine, drawText, refresh, getMouseX/Y, isMouseDown/KeyDown
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "../value.h"

// ── Global GUI state ───────────────────────────────────────────────────────
static HWND      g_hwnd     = NULL;
static HDC       g_mem_dc   = NULL;
static HBITMAP   g_mem_bmp  = NULL;
static HBITMAP   g_old_bmp  = NULL;
static int       g_width    = 800;
static int       g_height   = 600;
static int       g_open     = 0;
static int       g_mx = 0, g_my = 0;
static int       g_mdown    = 0;
static COLORREF  g_pen_color = RGB(255,0,0);
static int       g_pen_width = 2;
static CRITICAL_SECTION g_lock;

// ── Window procedure ───────────────────────────────────────────────────────
static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CLOSE:
        g_open = 0;
        DestroyWindow(hwnd);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        EnterCriticalSection(&g_lock);
        if (g_mem_dc)
            BitBlt(hdc, 0, 0, g_width, g_height, g_mem_dc, 0, 0, SRCCOPY);
        LeaveCriticalSection(&g_lock);
        EndPaint(hwnd, &ps);
        break;
    }
    default:
        return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

// ── Message pump thread ────────────────────────────────────────────────────
static DWORD WINAPI msg_thread(LPVOID param) {
    (void)param;
    MSG msg;
    while (g_open && GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    g_open = 0;
    return 0;
}

// ── createWindow(title, width, height) → bool ─────────────────────────────
static Value gui_create(int argc, Value *argv) {
    if (g_hwnd) return v_bool(0);

    const char *title = (argc > 0 && argv[0].type == V_STRING) ? argv[0].as.s : "Candle GUI";
    g_width  = (int)((argc > 1 && argv[1].type == V_INT) ? argv[1].as.i : 800);
    g_height = (int)((argc > 2 && argv[2].type == V_INT) ? argv[2].as.i : 600);

    InitializeCriticalSection(&g_lock);

    HINSTANCE hi = GetModuleHandle(NULL);
    WNDCLASSEX wc = {0};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = wnd_proc;
    wc.hInstance     = hi;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "CandleGUI";
    RegisterClassEx(&wc);

    RECT r = {0, 0, g_width, g_height};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, 0);

    g_hwnd = CreateWindowEx(0, "CandleGUI", title,
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top,
        NULL, NULL, hi, NULL);

    if (!g_hwnd) return v_bool(0);

    // Create double-buffer
    HDC screen_dc = GetDC(NULL);
    g_mem_dc = CreateCompatibleDC(screen_dc);
    g_mem_bmp = CreateCompatibleBitmap(screen_dc, g_width, g_height);
    g_old_bmp = (HBITMAP)SelectObject(g_mem_dc, g_mem_bmp);
    ReleaseDC(NULL, screen_dc);

    // Fill white
    HBRUSH wb = CreateSolidBrush(RGB(255,255,255));
    RECT fr = {0, 0, g_width, g_height};
    FillRect(g_mem_dc, &fr, wb);
    DeleteObject(wb);

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    g_open = 1;
    CreateThread(NULL, 0, msg_thread, NULL, 0, NULL);

    return v_bool(1);
}

// ── isOpen() → bool ────────────────────────────────────────────────────────
static Value gui_isopen(int argc, Value *argv) {
    (void)argc; (void)argv;
    return v_bool(g_open && g_hwnd);
}

// ── close() ────────────────────────────────────────────────────────────────
static Value gui_close(int argc, Value *argv) {
    (void)argc; (void)argv;
    if (g_hwnd) {
        g_open = 0;
        PostMessage(g_hwnd, WM_CLOSE, 0, 0);
    }
    return v_null();
}

// ── setColor(r, g, b) ──────────────────────────────────────────────────────
static Value gui_setcolor(int argc, Value *argv) {
    int r = (argc > 0) ? (int)(argv[0].as.i & 0xFF) : 255;
    int g = (argc > 1) ? (int)(argv[1].as.i & 0xFF) : 0;
    int b = (argc > 2) ? (int)(argv[2].as.i & 0xFF) : 0;
    g_pen_color = RGB(r, g, b);
    return v_null();
}

// ── setPenWidth(w) ─────────────────────────────────────────────────────────
static Value gui_setwidth(int argc, Value *argv) {
    g_pen_width = (argc > 0) ? (int)argv[0].as.i : 2;
    if (g_pen_width < 1) g_pen_width = 1;
    if (g_pen_width > 20) g_pen_width = 20;
    return v_null();
}

// ── clear(r, g, b) ─────────────────────────────────────────────────────────
static Value gui_clear(int argc, Value *argv) {
    int r = (argc > 0) ? (int)(argv[0].as.i & 0xFF) : 255;
    int g = (argc > 1) ? (int)(argv[1].as.i & 0xFF) : 255;
    int b = (argc > 2) ? (int)(argv[2].as.i & 0xFF) : 255;
    EnterCriticalSection(&g_lock);
    if (g_mem_dc) {
        HBRUSH br = CreateSolidBrush(RGB(r,g,b));
        RECT fr = {0, 0, g_width, g_height};
        FillRect(g_mem_dc, &fr, br);
        DeleteObject(br);
    }
    LeaveCriticalSection(&g_lock);
    return v_null();
}

// ── fillRect(x, y, w, h) ───────────────────────────────────────────────────
static Value gui_fillrect(int argc, Value *argv) {
    int x = (argc > 0) ? (int)argv[0].as.i : 0;
    int y = (argc > 1) ? (int)argv[1].as.i : 0;
    int w = (argc > 2) ? (int)argv[2].as.i : 10;
    int h = (argc > 3) ? (int)argv[3].as.i : 10;
    EnterCriticalSection(&g_lock);
    if (g_mem_dc) {
        HBRUSH br = CreateSolidBrush(g_pen_color);
        RECT fr = {x, y, x+w, y+h};
        FillRect(g_mem_dc, &fr, br);
        DeleteObject(br);
    }
    LeaveCriticalSection(&g_lock);
    return v_null();
}

// ── drawLine(x1, y1, x2, y2) ───────────────────────────────────────────────
static Value gui_line(int argc, Value *argv) {
    int x1 = (argc > 0) ? (int)argv[0].as.i : 0;
    int y1 = (argc > 1) ? (int)argv[1].as.i : 0;
    int x2 = (argc > 2) ? (int)argv[2].as.i : 0;
    int y2 = (argc > 3) ? (int)argv[3].as.i : 0;
    EnterCriticalSection(&g_lock);
    if (g_mem_dc) {
        HPEN pen = CreatePen(PS_SOLID, g_pen_width, g_pen_color);
        HPEN old = (HPEN)SelectObject(g_mem_dc, pen);
        MoveToEx(g_mem_dc, x1, y1, NULL);
        LineTo(g_mem_dc, x2, y2);
        SelectObject(g_mem_dc, old);
        DeleteObject(pen);
    }
    LeaveCriticalSection(&g_lock);
    return v_null();
}

// ── drawText(x, y, text) ───────────────────────────────────────────────────
static Value gui_text(int argc, Value *argv) {
    int x = (argc > 0) ? (int)argv[0].as.i : 0;
    int y = (argc > 1) ? (int)argv[1].as.i : 0;
    const char *txt = (argc > 2 && argv[2].type == V_STRING) ? argv[2].as.s : "";
    EnterCriticalSection(&g_lock);
    if (g_mem_dc) {
        SetBkMode(g_mem_dc, TRANSPARENT);
        SetTextColor(g_mem_dc, g_pen_color);
        TextOutA(g_mem_dc, x, y, txt, (int)strlen(txt));
    }
    LeaveCriticalSection(&g_lock);
    return v_null();
}

// ── refresh() — flush buffer and poll input ────────────────────────────────
static Value gui_refresh(int argc, Value *argv) {
    (void)argc; (void)argv;
    // Poll mouse
    POINT pt;
    GetCursorPos(&pt);
    if (g_hwnd) ScreenToClient(g_hwnd, &pt);
    g_mx = pt.x;
    g_my = pt.y;
    g_mdown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) ? 1 : 0;

    // Flush
    if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);

    // Process pending messages
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return v_null();
}

// ── Input polling ──────────────────────────────────────────────────────────
static Value gui_mousex(int argc, Value *argv) { (void)argc; (void)argv; return v_int(g_mx); }
static Value gui_mousey(int argc, Value *argv) { (void)argc; (void)argv; return v_int(g_my); }
static Value gui_mdown(int argc, Value *argv)  { (void)argc; (void)argv; return v_bool(g_mdown); }
static Value gui_key(int argc, Value *argv) {
    int vk = (argc > 0) ? (int)argv[0].as.i : 0;
    return v_bool((GetAsyncKeyState(vk) & 0x8000) ? 1 : 0);
}
static Value gui_width(int argc, Value *argv)  { (void)argc; (void)argv; return v_int(g_width); }
static Value gui_height(int argc, Value *argv) { (void)argc; (void)argv; return v_int(g_height); }

// ── Module build ───────────────────────────────────────────────────────────
Value build_gui(void) {
    Value mod = v_map();
    VMap *m = mod.as.map;

    v_map_set(m, "createWindow", v_native(gui_create));
    v_map_set(m, "isOpen",       v_native(gui_isopen));
    v_map_set(m, "close",        v_native(gui_close));
    v_map_set(m, "setColor",     v_native(gui_setcolor));
    v_map_set(m, "setPenWidth",  v_native(gui_setwidth));
    v_map_set(m, "clear",        v_native(gui_clear));
    v_map_set(m, "fillRect",     v_native(gui_fillrect));
    v_map_set(m, "drawLine",     v_native(gui_line));
    v_map_set(m, "drawText",     v_native(gui_text));
    v_map_set(m, "refresh",      v_native(gui_refresh));
    v_map_set(m, "getMouseX",    v_native(gui_mousex));
    v_map_set(m, "getMouseY",    v_native(gui_mousey));
    v_map_set(m, "isMouseDown",  v_native(gui_mdown));
    v_map_set(m, "isKeyDown",    v_native(gui_key));
    v_map_set(m, "getWidth",     v_native(gui_width));
    v_map_set(m, "getHeight",    v_native(gui_height));
    return mod;
}
