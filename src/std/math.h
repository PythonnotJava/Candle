#ifndef CANDLE_STD_MATH_H
#define CANDLE_STD_MATH_H

#include "candle_runtime.h"
#include <math.h>

static inline candle_double math_sqrt(candle_double x) { return sqrt(x); }
static inline candle_double math_pow(candle_double x, candle_double y) { return pow(x, y); }
static inline candle_double math_sin(candle_double x) { return sin(x); }
static inline candle_double math_cos(candle_double x) { return cos(x); }
static inline candle_double math_log(candle_double x) { return log(x); }
static inline candle_int math_abs(candle_int x) { return x < 0 ? -x : x; }
static inline candle_int math_min(candle_int a, candle_int b) { return a < b ? a : b; }
static inline candle_int math_max(candle_int a, candle_int b) { return a > b ? a : b; }

#define MATH_PI 3.14159265358979323846
#define MATH_E  2.71828182845904523536

#endif
