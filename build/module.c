#define CANDLE_RUNTIME_MAIN
#include "candle_runtime.h"


#include "std/io.h"
#include "std/math.h"
#include "std/collections.h"
#include "network/http.h"
#include "data/json.h"
/* class Widget */
typedef struct Widget Widget;
struct Widget {
    candle_string id;
    candle_int width;
    candle_int height;
    candle_string color;
};

static Widget *Widget__new(candle_string id, candle_int width, candle_int height) {
    Widget *self = GC_MALLOC(sizeof(Widget));
    self->id = id;
    self->width = width;
    self->height = height;
    return self;
}

candle_string Widget__describe(Widget *self) {
    return candle_fmt("Widget(%s: %sx%s)", candle_to_str(self->id), candle_to_str(self->width), candle_to_str(self->height));
}

void Widget__setColor(Widget *self, candle_string c) {
    self->color = c;
}

candle_string Widget__getColor(Widget *self) {
    return self->color;
}

/* class StringUtil */
typedef struct StringUtil StringUtil;
struct StringUtil {
};

static StringUtil *StringUtil__new() {
    StringUtil *self = GC_MALLOC(sizeof(StringUtil));
    return self;
}

static candle_int StringUtil__length(candle_string s) {
    /* ellipsis */
    return 0;
}

static candle_string StringUtil__repeat(candle_string s, candle_int n) {
    candle_string result = candle_str("");
    for (candle_int i = 0; i < n; i += 1) {
        result = candle_str_concat(result, s);
    }
    return result;
}

static void candle_init(void) {
    __auto_type response = http_get(candle_str("https://example.com"));
    __auto_type data = json_parse(response->body);
    Widget* w = Widget__new(candle_str("btn1"), 100, 50);
    Widget__setColor(w, candle_str("red"));
    print(Widget__getColor(w));
    print(Widget__describe(w));
    print(StringUtil__repeat(candle_str("ha"), 3));
}

int main(void) {
    GC_INIT();
    candle_init();
    return 0;
}
