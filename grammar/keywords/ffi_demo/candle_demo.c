/* candle_demo —— dll 关键字（FFI）的演示动态库
 *
 * 仅使用整型 / C 字符串签名，可被 Candle 的定长 trampoline 安全调用。
 * 编译（Windows）: gcc -shared -o candle_demo.dll candle_demo.c
 *        Linux  : gcc -shared -fPIC -o libcandle_demo.so candle_demo.c
 *        macOS  : gcc -dynamiclib -o libcandle_demo.dylib candle_demo.c
 */

#if defined(_WIN32)
  #define EXPORT __declspec(dllexport)
#else
  #define EXPORT
#endif

#include <string.h>

/* 整型相加 */
EXPORT int demo_add(int a, int b) {
    return a + b;
}

/* 整型相乘 */
EXPORT int demo_mul(int a, int b) {
    return a * b;
}

/* 返回 C 字符串字面量（静态存储，Candle 侧只读拷贝） */
EXPORT const char *demo_greeting(void) {
    return "hello from candle_demo dll";
}

/* C 字符串长度 */
EXPORT int demo_strlen(const char *s) {
    return s ? (int)strlen(s) : 0;
}
