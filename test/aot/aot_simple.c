#define CANDLE_RUNTIME_MAIN
#include "candle_runtime.h"

candle_int add(candle_int a, candle_int b);
int candle_user_main();

candle_int add(candle_int a, candle_int b) {
    return (a + b);
}

int candle_user_main() {
    candle_int x = add(((candle_int)3), ((candle_int)4));
    print(candle_str("x ="));
    print(x);
    print(candle_str("AOT COMPILE OK"));
    return ((candle_int)0);
}

int main(void) {
    GC_INIT();
    return candle_user_main();
}
