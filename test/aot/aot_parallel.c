#define CANDLE_RUNTIME_MAIN
#include "candle_runtime.h"

int candle_user_main();

int candle_user_main() {
    print(candle_str("AOT Parallel Test"));
    _Pragma("omp parallel sections")
    {
        _Pragma("omp section")
        {
            print(candle_str("  A"));
        }
        _Pragma("omp section")
        {
            print(candle_str("  B"));
        }
        _Pragma("omp section")
        {
            print(candle_str("  C"));
        }
        _Pragma("omp section")
        {
            print(candle_str("  D"));
        }
    }
    print(candle_str("End"));
    return ((candle_int)0);
}

int main(void) {
    GC_INIT();
    return candle_user_main();
}
