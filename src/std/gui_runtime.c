// gui_runtime.c — AOT implementation of std.gui (Win32 double-buffered window)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "candle_runtime.h"

static HWND gwnd=NULL; static HDC gdc=NULL; static HBITMAP gbmp=NULL, gold=NULL;
static int gw=800,gh=600,gopen=0,gmx=0,gmy=0,gmdown=0;
static COLORREF gcol=RGB(255,0,0); static int gpw=2;
static CRITICAL_SECTION glk;

static LRESULT CALLBACK wp(HWND h,UINT m,WPARAM w,LPARAM l){
 switch(m){case WM_CLOSE:gopen=0;DestroyWindow(h);break;case WM_DESTROY:PostQuitMessage(0);break;
 case WM_PAINT:{PAINTSTRUCT p;HDC d=BeginPaint(h,&p);EnterCriticalSection(&glk);
 if(gdc)BitBlt(d,0,0,gw,gh,gdc,0,0,SRCCOPY);LeaveCriticalSection(&glk);EndPaint(h,&p);break;}
 default:return DefWindowProc(h,m,w,l);}return 0;
}
static DWORD WINAPI mt(LPVOID p){(void)p;MSG m;while(gopen&&GetMessage(&m,NULL,0,0)>0){TranslateMessage(&m);DispatchMessage(&m);}gopen=0;return 0;}

candle_int candle_gui_createWindow(candle_string t,candle_int w,candle_int h){
 if(gwnd)return 0;if(!t)t="Candle";gw=(int)w;gh=(int)h;InitializeCriticalSection(&glk);
 HINSTANCE hi=GetModuleHandle(NULL);WNDCLASSEX wc={0};wc.cbSize=sizeof(wc);wc.style=CS_HREDRAW|CS_VREDRAW;
 wc.lpfnWndProc=wp;wc.hInstance=hi;wc.hCursor=LoadCursor(NULL,IDC_ARROW);
 wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);wc.lpszClassName="CandleGUI";RegisterClassEx(&wc);
 RECT r={0,0,gw,gh};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,0);
 gwnd=CreateWindowEx(0,"CandleGUI",t,WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,
  r.right-r.left,r.bottom-r.top,NULL,NULL,hi,NULL);if(!gwnd)return 0;
 HDC sd=GetDC(NULL);gdc=CreateCompatibleDC(sd);gbmp=CreateCompatibleBitmap(sd,gw,gh);
 gold=SelectObject(gdc,gbmp);ReleaseDC(NULL,sd);
 {HBRUSH b=CreateSolidBrush(RGB(255,255,255));RECT f={0,0,gw,gh};FillRect(gdc,&f,b);DeleteObject(b);}
 ShowWindow(gwnd,SW_SHOW);UpdateWindow(gwnd);gopen=1;CreateThread(NULL,0,mt,NULL,0,NULL);return 1;
}
candle_int candle_gui_isOpen(void){return gopen&&gwnd;}
candle_int candle_gui_close(void){if(gwnd){gopen=0;PostMessage(gwnd,WM_CLOSE,0,0);}return 0;}
candle_int candle_gui_setColor(candle_int r,candle_int g,candle_int b){gcol=RGB((int)(r&0xFF),(int)(g&0xFF),(int)(b&0xFF));return 0;}
candle_int candle_gui_setPenWidth(candle_int w){gpw=(int)w;if(gpw<1)gpw=1;if(gpw>20)gpw=20;return 0;}
candle_int candle_gui_clear(candle_int r,candle_int g,candle_int b){
 EnterCriticalSection(&glk);if(gdc){HBRUSH br=CreateSolidBrush(RGB((int)(r&0xFF),(int)(g&0xFF),(int)(b&0xFF)));
 RECT f={0,0,gw,gh};FillRect(gdc,&f,br);DeleteObject(br);}LeaveCriticalSection(&glk);return 0;}
candle_int candle_gui_fillRect(candle_int x,candle_int y,candle_int w,candle_int h){
 EnterCriticalSection(&glk);if(gdc){HBRUSH br=CreateSolidBrush(gcol);
 RECT f={(int)x,(int)y,(int)(x+w),(int)(y+h)};FillRect(gdc,&f,br);DeleteObject(br);}LeaveCriticalSection(&glk);return 0;}
candle_int candle_gui_drawLine(candle_int x1,candle_int y1,candle_int x2,candle_int y2){
 EnterCriticalSection(&glk);if(gdc){HPEN p=CreatePen(PS_SOLID,gpw,gcol);HPEN old=SelectObject(gdc,p);
 MoveToEx(gdc,(int)x1,(int)y1,NULL);LineTo(gdc,(int)x2,(int)y2);SelectObject(gdc,old);DeleteObject(p);}
 LeaveCriticalSection(&glk);return 0;}
candle_int candle_gui_drawText(candle_int x,candle_int y,candle_string t){
 if(!t)t="";EnterCriticalSection(&glk);if(gdc){SetBkMode(gdc,TRANSPARENT);SetTextColor(gdc,gcol);
 TextOutA(gdc,(int)x,(int)y,t,(int)strlen(t));}LeaveCriticalSection(&glk);return 0;}
candle_int candle_gui_refresh(void){
 POINT pt;GetCursorPos(&pt);if(gwnd)ScreenToClient(gwnd,&pt);gmx=pt.x;gmy=pt.y;
 gmdown=(GetAsyncKeyState(VK_LBUTTON)&0x8000)?1:0;if(gwnd)InvalidateRect(gwnd,NULL,FALSE);
 MSG m;while(PeekMessage(&m,NULL,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessage(&m);}return 0;
}
candle_int candle_gui_getMouseX(void){return gmx;}
candle_int candle_gui_getMouseY(void){return gmy;}
candle_int candle_gui_isMouseDown(void){return gmdown;}
candle_int candle_gui_isKeyDown(candle_int vk){return (GetAsyncKeyState((int)vk)&0x8000)?1:0;}
candle_int candle_gui_getWidth(void){return gw;}
candle_int candle_gui_getHeight(void){return gh;}
