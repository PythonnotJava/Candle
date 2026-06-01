#define CANDLE_RUNTIME_MAIN
#include "candle_runtime.h"


/* class Point */
typedef struct Point Point;
struct Point {
    candle_int x;
    candle_int y;
};

static Point *Point__new(candle_int x, candle_int y) {
    Point *self = GC_MALLOC(sizeof(Point));
    self->x = x;
    self->y = y;
    return self;
}

candle_int Point__getX(Point *self) {
    return self->x;
}

candle_int Point__getY(Point *self) {
    return self->y;
}

candle_string Point__toString(Point *self) {
    return candle_fmt("(%s, %s)", candle_to_str(self->x), candle_to_str(self->y));
}

/* class Shape */
typedef struct Shape Shape;
struct Shape {
    candle_string name;
};

static Shape *Shape__new(candle_string name) {
    Shape *self = GC_MALLOC(sizeof(Shape));
    self->name = name;
    return self;
}

candle_double Shape__area(Shape *self) {
    return 0;
}

/* class Circle */
typedef struct Circle Circle;
struct Circle {
    candle_string name;
    candle_double radius;
};

static Circle *Circle__new(candle_string name, candle_double radius) {
    Circle *self = GC_MALLOC(sizeof(Circle));
    self->name = name;
    self->radius = radius;
    return self;
}

candle_double Circle__area(Circle *self) {
    return ((3.14159 * self->radius) * self->radius);
}

/* class Config */
typedef struct Config Config;
struct Config {
    candle_string host;
    candle_int port;
};

static Config *Config__new(candle_string host, candle_int port) {
    Config *self = GC_MALLOC(sizeof(Config));
    self->host = host;
    self->port = port;
    return self;
}

candle_string Config__getUrl(Config *self) {
    return candle_fmt("%s:%s", candle_to_str(self->host), candle_to_str(self->port));
}

/* class Account */
typedef struct Account Account;
struct Account {
    candle_string owner;
    candle_double balance;
};

static Account *Account__new(candle_string owner, candle_double balance) {
    Account *self = GC_MALLOC(sizeof(Account));
    self->owner = owner;
    self->balance = balance;
    return self;
}

candle_double Account__getBalance(Account *self) {
    return self->balance;
}

void Account__validate(Account *self, candle_double amount) {
    candle_assert((amount > 0));
}

void Account__deposit(Account *self, candle_double amount) {
    Account__validate(self, amount);
    self->balance += amount;
}

/* class MathUtil */
typedef struct MathUtil MathUtil;
struct MathUtil {
};

static MathUtil *MathUtil__new() {
    MathUtil *self = GC_MALLOC(sizeof(MathUtil));
    return self;
}

static candle_int MathUtil__max(candle_int a, candle_int b) {
    if ((a > b)) {
        return a;
    }
    return b;
}

static candle_double MathUtil__abs(candle_double x) {
    if ((x < 0)) {
        return (0 - x);
    }
    return x;
}

/* class Logger */
typedef struct Logger Logger;
struct Logger {
};

static Logger *Logger__new() {
    Logger *self = GC_MALLOC(sizeof(Logger));
    return self;
}

static void Logger__info(candle_string msg) {
    print(candle_fmt("[INFO] %s", candle_to_str(msg)));
}

static void Logger__error(candle_string msg) {
    print(candle_fmt("[ERROR] %s", candle_to_str(msg)));
}

/* class Color */
typedef struct Color Color;
struct Color {
    candle_int r;
    candle_int g;
    candle_int b;
};

static Color *Color__new(candle_int r, candle_int g, candle_int b) {
    Color *self = GC_MALLOC(sizeof(Color));
    self->r = r;
    self->g = g;
    self->b = b;
    return self;
}

static Color *Color__red() {
    return Color__new(255, 0, 0);
}

static Color *Color__green() {
    return Color__new(0, 255, 0);
}

static Color *Color__blue() {
    return Color__new(0, 0, 255);
}

/* class Serializable */
typedef struct Serializable Serializable;
struct Serializable {
};

static Serializable *Serializable__new() {
    Serializable *self = GC_MALLOC(sizeof(Serializable));
    return self;
}

candle_string Serializable__serialize(Serializable *self);
void Serializable__deserialize(Serializable *self, candle_string data);
/* class Comparable */
typedef struct Comparable Comparable;
struct Comparable {
};

static Comparable *Comparable__new() {
    Comparable *self = GC_MALLOC(sizeof(Comparable));
    return self;
}

candle_int Comparable__compareTo(Comparable *self, Serializable* other);
/* class Person */
typedef struct Person Person;
struct Person {
    candle_string name;
    candle_int age;
};

static Person *Person__new(candle_string name, candle_int age) {
    Person *self = GC_MALLOC(sizeof(Person));
    self->name = name;
    self->age = age;
    return self;
}

candle_string Person__serialize(Person *self) {
    return candle_fmt("%s,%s", candle_to_str(self->name), candle_to_str(self->age));
}

void Person__deserialize(Person *self, candle_string data) {
    /* ellipsis */
}

/* class Base */
typedef struct Base Base;
struct Base {
};

static Base *Base__new() {
    Base *self = GC_MALLOC(sizeof(Base));
    return self;
}

candle_int Base__id(Base *self) {
    return 42;
}

candle_string Base__label(Base *self) {
    return candle_str("base");
}

/* class Derived */
typedef struct Derived Derived;
struct Derived {
};

static Derived *Derived__new() {
    Derived *self = GC_MALLOC(sizeof(Derived));
    return self;
}

candle_string Derived__label(Derived *self) {
    return candle_str("derived");
}

static void candle_init(void) {
    Point* p = Point__new(3, 4);
    print(Point__toString(p));
    Circle* c = Circle__new(candle_str("circle"), 5);
    print(Circle__area(c));
    __auto_type red = Color__red();
    Logger__info(candle_str("Application started"));
}

int main(void) {
    GC_INIT();
    candle_init();
    return 0;
}
