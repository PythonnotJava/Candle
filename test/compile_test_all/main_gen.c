#define CANDLE_RUNTIME_MAIN
#include "candle_runtime.h"

/* == module: math_utils ============================================ */
static candle_int math_utils_add(candle_int a, candle_int b);
static candle_int math_utils_multiply(candle_int a, candle_int b);
static candle_int math_utils_factorial(candle_int n);
static candle_int math_utils_isPrime(candle_int n);

static candle_int math_utils_add(candle_int a, candle_int b) {
    return (a + b);
}

static candle_int math_utils_multiply(candle_int a, candle_int b) {
    return (a * b);
}

static candle_int math_utils_factorial(candle_int n) {
    if ((n <= ((candle_int)1))) {
        return ((candle_int)1);
    }
    return (n * math_utils_factorial((n - ((candle_int)1))));
}

static candle_int math_utils_isPrime(candle_int n) {
    if ((n < ((candle_int)2))) {
        return ((candle_int)0);
    }
    candle_int limit = ((n / ((candle_int)2)) + ((candle_int)1));
    for (candle_int d = ((candle_int)2); d < limit; d += 1) {
        if (((n % d) == ((candle_int)0))) {
            return ((candle_int)0);
        }
    }
    return ((candle_int)1);
}

/* == module: control_flow ============================================ */
static candle_int control_flow_testIf(candle_int x);
static candle_int control_flow_testFor(candle_int n);
static candle_int control_flow_testWhen(candle_int x);
static candle_int control_flow_testTry(candle_int denom);
static candle_int control_flow_testAssert(candle_int x);
static candle_int control_flow_testFib(candle_int n);

static candle_int control_flow_testIf(candle_int x) {
    if ((x > ((candle_int)10))) {
        return ((candle_int)1);
    }
    if ((x < ((candle_int)0))) {
        return -((candle_int)1);
    }
    return ((candle_int)0);
}

static candle_int control_flow_testFor(candle_int n) {
    candle_int sum = ((candle_int)0);
    for (candle_int i = ((candle_int)1); i < n; i += 1) {
        sum = (sum + i);
    }
    return sum;
}

static candle_int control_flow_testWhen(candle_int x) {
    candle_int count = ((candle_int)0);
    while ((x > ((candle_int)0))) {
        count = (count + ((candle_int)1));
        while ((x > ((candle_int)10))) {
            count = (count + ((candle_int)1));
            break;
        }
    }
    return count;
}

static candle_int control_flow_testTry(candle_int denom) {
    candle_try {
        candle_int result = (((candle_int)10) / denom);
        return result;
    } else {
        if (strcmp(_cj.exc.type, "DivByZero") == 0 || strcmp("DivByZero", "Exception") == 0) {
            CandleExc e = _cj.exc;
            (void)e;
            return ((candle_int)0);
        }
    } candle_catch_end;
}

static candle_int control_flow_testAssert(candle_int x) {
    candle_assert((x > ((candle_int)0)));
    return x;
}

static candle_int control_flow_testFib(candle_int n) {
    if ((n <= ((candle_int)1))) {
        return n;
    }
    return (control_flow_testFib((n - ((candle_int)1))) + control_flow_testFib((n - ((candle_int)2))));
}

/* == module: types_demo ============================================ */
static candle_int types_demo_testInt();
static candle_double types_demo_testDouble();
static candle_string types_demo_testString();
static candle_bool types_demo_testBool();
static candle_int types_demo_testList();
static candle_int types_demo_testMap();
static candle_int types_demo_testConst();
static int|double types_demo_testUnion(candle_int flag);

static candle_int types_demo_testInt() {
    candle_int a = ((candle_int)42);
    candle_int b = (a + ((candle_int)8));
    return b;
}

static candle_double types_demo_testDouble() {
    candle_double x = 3.14;
    candle_double y = (x * 2);
    return y;
}

static candle_string types_demo_testString() {
    candle_string s = candle_str("hello");
    candle_string t = candle_str(" world");
    return (s + t);
}

static candle_bool types_demo_testBool() {
    candle_bool a = ((candle_bool)1);
    candle_bool b = ((candle_bool)0);
    if (a) {
        return !b;
    }
    return ((candle_bool)0);
}

static candle_int types_demo_testList() {
    CandleList* xs = candle_list_new(5, (candle_int)(((candle_int)1)), (candle_int)(((candle_int)2)), (candle_int)(((candle_int)3)), (candle_int)(((candle_int)4)), (candle_int)(((candle_int)5)));
    return (candle_index(xs, ((candle_int)0)) + candle_index(xs, ((candle_int)4)));
}

static candle_int types_demo_testMap() {
    CandleMap* m = candle_map_new(2, candle_str("a"), ((candle_int)1), candle_str("b"), ((candle_int)2));
    return (candle_index(m, candle_str("a")) + candle_index(m, candle_str("b")));
}

static candle_int types_demo_testConst() {
    const candle_int PI = ((candle_int)3);
    candle_int r = ((candle_int)5);
    return (PI * r);
}

static int|double types_demo_testUnion(candle_int flag) {
    if ((flag == ((candle_int)1))) {
        return ((candle_int)42);
    }
    return 3.14;
}

/* == module: lambda_demo ============================================ */
static candle_int lambda_demo_testSimpleLambda();
static candle_int lambda_demo_testMultiParamLambda();
static candle_int lambda_demo_testClosure();

static candle_int lambda_demo_testSimpleLambda() {
    __auto_type doubleIt = __lambda_0;
    return doubleIt(((candle_int)21));
}

static candle_int lambda_demo_testMultiParamLambda() {
    __auto_type add = __lambda_0;
    return add(((candle_int)10), ((candle_int)20));
}

static candle_int lambda_demo_testClosure() {
    candle_int base = ((candle_int)100);
    __auto_type addBase = __lambda_0;
    return addBase(((candle_int)50));
}

/* == module: parallel_demo ============================================ */
static candle_int parallel_demo_testParallel();

static candle_int parallel_demo_testParallel() {
    candle_int a = ((candle_int)0);
    candle_int b = ((candle_int)0);
    candle_int c = ((candle_int)0);
    candle_int d = ((candle_int)0);
    _Pragma("omp parallel sections")
    {
        _Pragma("omp section")
        {
            for (candle_int i = 0; i < ((candle_int)1000); i += 1) {
                a = (a + ((candle_int)1));
            }
        }
        _Pragma("omp section")
        {
            for (candle_int i = 0; i < ((candle_int)1000); i += 1) {
                b = (b + ((candle_int)1));
            }
        }
        _Pragma("omp section")
        {
            for (candle_int i = 0; i < ((candle_int)1000); i += 1) {
                c = (c + ((candle_int)1));
            }
        }
        _Pragma("omp section")
        {
            for (candle_int i = 0; i < ((candle_int)1000); i += 1) {
                d = (d + ((candle_int)1));
            }
        }
    }
    return (((a + b) + c) + d);
}

int candle_user_main();

int candle_user_main() {
    print(candle_str("=== Multi-File Module Test ==="));
    print(candle_str_concat(candle_str("add="), String(math_utils_add(((candle_int)3), ((candle_int)4)))));
    print(candle_str_concat(candle_str("fact="), String(math_utils_factorial(((candle_int)5)))));
    print(candle_str_concat(candle_str("if="), String(control_flow_testIf(((candle_int)15)))));
    print(candle_str_concat(candle_str("for="), String(control_flow_testFor(((candle_int)5)))));
    print(candle_str_concat(candle_str("fib="), String(control_flow_testFib(((candle_int)6)))));
    print(candle_str_concat(candle_str("int="), String(types_demo_testInt())));
    print((candle_str("str=") + types_demo_testString()));
    print(candle_str_concat(candle_str("bool="), String(types_demo_testBool())));
    print(candle_str_concat(candle_str("list="), String(types_demo_testList())));
    print(candle_str_concat(candle_str("lam="), String(lambda_demo_testSimpleLambda())));
    print(candle_str_concat(candle_str("closure="), String(lambda_demo_testClosure())));
    print(candle_str_concat(candle_str("par="), String(parallel_demo_testParallel())));
    print(candle_str("=== ALL MULTI-FILE TESTS PASSED ==="));
    return ((candle_int)0);
}

int main(void) {
    GC_INIT();
    return candle_user_main();
}
