#define CANDLE_RUNTIME_MAIN
#include "candle_runtime.h"

candle_int heavyCompute(candle_int n);
candle_int loadData();
candle_int processData(candle_int d);
void doCleanup();
candle_int compute1();
candle_int compute2(candle_int x);
candle_int compute3(candle_int x);
void doExtraWork();

candle_int heavyCompute(candle_int n) {
    candle_int sum = 0;
    for (candle_int i = n; i < (n + 10000); i += 1) {
        sum += i;
    }
    return sum;
}

candle_int loadData() {
    /* ellipsis */
    return 100;
}

candle_int processData(candle_int d) {
    return (d * 2);
}

void doCleanup() {
    /* ellipsis */
}

candle_int compute1() {
    return 10;
}

candle_int compute2(candle_int x) {
    return (x * 2);
}

candle_int compute3(candle_int x) {
    return (x + 1);
}

void doExtraWork() {
    /* ellipsis */
}

static void candle_init(void) {
    candle_int sum1 = 0;
    candle_int sum2 = 0;
    candle_int sum3 = 0;
    _Pragma("omp parallel sections")
    {
        _Pragma("omp section")
        {
            for (candle_int i = 0; i < 10000; i += 1) {
                sum1 += i;
            }
        }
        _Pragma("omp section")
        {
            for (candle_int i = 10000; i < 20000; i += 1) {
                sum2 += i;
            }
        }
        _Pragma("omp section")
        {
            for (candle_int i = 20000; i < 30000; i += 1) {
                sum3 += i;
            }
        }
    }
    candle_int total = ((sum1 + sum2) + sum3);
    print(candle_fmt("Total: %s", candle_to_str(total)));
    candle_int results = 0;
    _Pragma("omp parallel for")
    for (candle_int i = 0; i < 8; i++) {
        results += heavyCompute(i);
    }
    candle_int data = 0;
    candle_int processed = 0;
    _Pragma("omp parallel sections")
    {
        _Pragma("omp section")
        {
            data = loadData();
            /* signal(dataReady) */
            print(candle_str("Data loaded"));
            doCleanup();
        }
        _Pragma("omp section")
        /* delay(dataReady) */
        {
            processed = processData(data);
            print(candle_fmt("Processed: %s", candle_to_str(processed)));
        }
    }
    candle_int step1Result = 0;
    candle_int step2Result = 0;
    candle_int finalResult = 0;
    _Pragma("omp parallel sections")
    {
        _Pragma("omp section")
        {
            step1Result = compute1();
            /* signal(step1Done) */
            print(candle_str("Step 1 done"));
            doExtraWork();
        }
        _Pragma("omp section")
        /* delay(step1Done) */
        {
            step2Result = compute2(step1Result);
            /* signal(step2Done) */
            print(candle_str("Step 2 done"));
        }
        _Pragma("omp section")
        /* delay(step2Done) */
        {
            finalResult = compute3(step2Result);
            print(candle_fmt("Final: %s", candle_to_str(finalResult)));
        }
    }
    _Pragma("omp parallel sections")
    {
        _Pragma("omp section")
        {
            _Pragma("omp parallel sections")
            {
                _Pragma("omp section")
                {
                    print(candle_str("nested-1a"));
                }
                _Pragma("omp section")
                {
                    print(candle_str("nested-1b"));
                }
            }
        }
        _Pragma("omp section")
        {
            print(candle_str("task-2"));
        }
    }
}

int main(void) {
    GC_INIT();
    candle_init();
    return 0;
}
