#define CANDLE_RUNTIME_MAIN
#include "candle_runtime.h"

candle_int divide(candle_int a, candle_int b);
candle_int fibonacci(candle_int n);

candle_int divide(candle_int a, candle_int b) {
    if ((b == 0)) {
        candle_throw_str("DivisionError", candle_str("division by zero"));
    }
    return (a / b);
}

candle_int fibonacci(candle_int n) {
    if ((n <= 1)) {
        return n;
    }
    return (fibonacci((n - 1)) + fibonacci((n - 2)));
}

static void candle_init(void) {
    candle_int score = 85;
    if ((score >= 90)) {
        print(candle_str("A"));
    } else if ((score >= 80)) {
        print(candle_str("B"));
    } else if ((score >= 70)) {
        print(candle_str("C"));
    } else {
        print(candle_str("D"));
    }
    candle_int i = 0;
    while ((i < 10)) {
        print(i);
        i += 1;
    }
    for (candle_int i = 0; i < 10; i += 1) {
        print(i);
    }
    CandleList* items = candle_list_new(3, (candle_int)(10), (candle_int)(20), (candle_int)(30));
    candle_list_foreach(item, items) {
        print(item);
    }
    candle_str_foreach(ch, candle_str("hello")) {
        print(ch);
    }
    for (candle_int i = 5; i < 10; i += 1) {
        print(i);
    }
    for (candle_int i = 0; i < 100; i += 5) {
        print(i);
    }
    for (candle_int i = 0; i < 100; i += 1) {
        if ((i == 42)) {
            break;
        }
        print(i);
    }
    for (candle_int i = 0; i < 5; i += 1) {
        for (candle_int j = 0; j < 5; j += 1) {
            print(candle_fmt("%s,%s", candle_to_str(i), candle_to_str(j)));
        }
    }
    candle_try {
        candle_int result = divide(10, 0);
    } else {
        if (strcmp(_cj.exc.type, "DivisionError") == 0 || strcmp("DivisionError", "Exception") == 0) {
            CandleExc e = _cj.exc;
            (void)e;
            print(candle_fmt("Error: %s", candle_to_str(e.message)));
        }
        else if (strcmp(_cj.exc.type, "Exception") == 0 || strcmp("Exception", "Exception") == 0) {
            CandleExc e = _cj.exc;
            (void)e;
            print(candle_str("Unknown error"));
        }
    } candle_catch_end;
}

int main(void) {
    GC_INIT();
    candle_init();
    return 0;
}
