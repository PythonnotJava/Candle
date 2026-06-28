#define CANDLE_RUNTIME_MAIN
#include "candle_runtime.h"

int candle_user_main();

int candle_user_main() {
    print(candle_str("=== True Parallel Test ==="));
    print(candle_str("If threads are truly parallel, A/B/C/D should appear in random order."));
    for (candle_int round = ((candle_int)1); round < ((candle_int)6); round += 1) {
        print(((candle_str("--- Round ") + String(round)) + candle_str(" ---")));
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
            _Pragma("omp section")
            {
                print(candle_str("  E"));
            }
            _Pragma("omp section")
            {
                print(candle_str("  F"));
            }
            _Pragma("omp section")
            {
                print(candle_str("  G"));
            }
            _Pragma("omp section")
            {
                print(candle_str("  H"));
            }
        }
    }
    print(candle_str("=== Done ==="));
    print(candle_str("Check above: if A/B/C/D order varies, parallel is REAL."));
    return ((candle_int)0);
}

int main(void) {
    GC_INIT();
    return candle_user_main();
}
