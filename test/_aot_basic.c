#define CANDLE_RUNTIME_MAIN
#include "candle_runtime.h"

static candle_int __lambda_0(candle_int x) {
    return (x * ((candle_int)2));
}

static candle_int __lambda_1(candle_int a, candle_int b) {
    return (a + b);
}

static candle_string __lambda_2(candle_string name) {
    return candle_fmt("Hello, %s!", candle_to_str(name));
}

static candle_int __lambda_3(candle_int x) {
    return (x + ((candle_int)1));
}

static candle_int __lambda_4(candle_int a, candle_int b) {
    candle_int sum = (a + b);
    candle_int product = (a * b);
    return (sum + product);
}

static candle_int __lambda_5(candle_int a, candle_int b) {
    candle_int c = ((candle_int)10);
    return ((a + b) + c);
}

static candle_int __lambda_6(candle_int x) {
    return (x * ((candle_int)2));
}

static candle_bool __lambda_7(candle_int x) {
    return ((x % ((candle_int)2)) == ((candle_int)0));
}

static candle_int __lambda_8(candle_int acc, candle_int x) {
    return (acc + x);
}

static void __lambda_9(candle_int x) {
    print(candle_fmt("item: %s", candle_to_str(x)));
}

static candle_int __lambda_10(candle_int x) {
    return (x * x);
}

candle_int add(candle_int a, candle_int b);
candle_double multiply(candle_double a, candle_double b);
candle_int placeholder(candle_int n);
candle_int compute(candle_int a, candle_int b);
int candle_user_main();

const candle_double PI = 3.14159;
const candle_int MAX_SIZE = ((candle_int)1024);
const candle_int HEX_VAL = ((candle_int)255);
typedef candle_int Int32;
typedef void* Callback;
typedef void* Transformer;
candle_int add(candle_int a, candle_int b) {
    return (a + b);
}

candle_double multiply(candle_double a, candle_double b) {
    return (a * b);
}

candle_int placeholder(candle_int n) {
    /* ellipsis */
    return n;
}

candle_int compute(candle_int a, candle_int b);
int candle_user_main() {
    return ((candle_int)0);
}

static void candle_init(void) {
    candle_string name = candle_str("Candle");
    candle_int count = ((candle_int)42);
    candle_double ratio = 3.14;
    candle_bool flag = ((candle_bool)1);
    __auto_type nothing = NULL;
    candle_string greeting = candle_str("hello");
    __auto_type IntOrFloat = int|double;
    candle_int x = ((candle_int)10);
    candle_double y = 3.14;
    candle_string msg = candle_str("hello world");
    candle_bool active = ((candle_bool)1);
    candle_int* maybe = NULL;
    candle_double result = (((x + ((candle_int)10)) * ((candle_int)2)) - (y / 3));
    candle_int mod_val = (((candle_int)100) % ((candle_int)7));
    candle_bool check = (((x > ((candle_int)5)) && (y < 10)) || !flag);
    candle_int bits = (((candle_int)255) & ((candle_int)15));
    candle_int shifted = (bits | ((candle_int)240));
    x = (x + ((candle_int)1));
    x += ((candle_int)10);
    x -= ((candle_int)5);
    x *= ((candle_int)2);
    x /= ((candle_int)3);
    CandleList* numbers = candle_list_new(5, (candle_int)(((candle_int)1)), (candle_int)(((candle_int)2)), (candle_int)(((candle_int)3)), (candle_int)(((candle_int)4)), (candle_int)(((candle_int)5)));
    __auto_type first = candle_index(numbers, ((candle_int)0));
    CandleMap* config = candle_map_new(2, candle_str("host"), candle_str("localhost"), candle_str("port"), ((candle_int)8080));
    candle_string raw = candle_str("\\n is not a newline");
    candle_string fmt = candle_fmt("x = %s, y = %s", candle_to_str(x), candle_to_str(y));
    candle_string normal = candle_str("hello\\nworld");
    candle_assert((x > ((candle_int)0)));
    candle_assert((name != NULL));
    __auto_type doubler = __lambda_0;
    __auto_type adder = __lambda_1;
    __auto_type greet = __lambda_2;
    __auto_type inc = __lambda_3;
    __auto_type compute = __lambda_4;
    __auto_type addMixin = __lambda_5;
    __auto_type doubled = candle_list_mapList(numbers, __lambda_6);
    __auto_type evens = candle_list_filter(numbers, __lambda_7);
    __auto_type total = candle_list_reduce(numbers, __lambda_8);
    candle_list_forEach(numbers, __lambda_9);
    Transformer square = __lambda_10;
}

int main(void) {
    GC_INIT();
    candle_init();
    return candle_user_main();
}
