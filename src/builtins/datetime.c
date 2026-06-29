// datetime.c — std.datetime: now, utc, epoch, Date, Time, format, timestamp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/timeb.h>

typedef enum { V_NULL, V_INT, V_DOUBLE, V_BOOL, V_STRING, V_LIST, V_TUPLE,
               V_MAP, V_FN, V_LAMBDA, V_CLASS, V_OBJECT, V_NATIVE } ValueType;
typedef struct Value Value;
typedef Value (*NativeFn)(int argc, Value *argv);
struct Value { ValueType type; union { long long i; double d; int b;
    char *s; void *list; void *map; void *fn; void *cls; void *obj; NativeFn native; } as; };
typedef struct { char **keys; Value *vals; int len, cap; } VMap;
typedef struct { Value *items; int len, cap; } VList;

static Value v_int(long long x)   { Value v; v.type = V_INT; v.as.i = x; return v; }
static Value v_double(double x)   { Value v; v.type = V_DOUBLE; v.as.d = x; return v; }
static Value v_null(void)         { Value v; v.type = V_NULL; return v; }
static Value v_string(const char *s) { Value v; v.type = V_STRING; v.as.s = s ? _strdup(s) : NULL; return v; }
static Value v_bool(int x)        { Value v; v.type = V_BOOL; v.as.b = x ? 1 : 0; return v; }
static Value v_native(NativeFn fn){ Value v; v.type = V_NATIVE; v.as.native = fn; return v; }
static Value v_map_new(void) { Value v; v.type = V_MAP; VMap *m = calloc(1,sizeof(VMap)); v.as.map = m; return v; }
static Value v_list_new(void) { Value v; v.type = V_LIST; VList *l = calloc(1,sizeof(VList)); l->cap=8; l->items=calloc(l->cap,sizeof(Value)); v.as.list=l; return v; }
static void vm_set(VMap *m, const char *k, Value v) {
    int i; for (i = 0; i < m->len; i++) if (strcmp(m->keys[i],k)==0) { m->vals[i]=v; return; }
    if (m->len >= m->cap) { m->cap=m->cap?m->cap*2:8; m->keys=realloc(m->keys,sizeof(char*)*m->cap); m->vals=realloc(m->vals,sizeof(Value)*m->cap); }
    m->keys[m->len]=_strdup(k); m->vals[m->len]=v; m->len++;
}

/* now() — local datetime map */
static Value dt_now(int argc, Value *argv) {
    SYSTEMTIME st; Value m; VMap *vm; int i, yd;
    static int dim[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    (void)argc; (void)argv;
    GetLocalTime(&st);
    m = v_map_new(); vm = m.as.map;
    vm_set(vm, "year",    v_int(st.wYear));
    vm_set(vm, "month",   v_int(st.wMonth));
    vm_set(vm, "day",     v_int(st.wDay));
    vm_set(vm, "hour",    v_int(st.wHour));
    vm_set(vm, "minute",  v_int(st.wMinute));
    vm_set(vm, "second",  v_int(st.wSecond));
    vm_set(vm, "ms",      v_int(st.wMilliseconds));
    vm_set(vm, "weekday", v_int(st.wDayOfWeek));
    yd = 0; for (i = 1; i < (int)st.wMonth; i++) yd += dim[i];
    yd += st.wDay;
    if (st.wMonth > 2 && (st.wYear % 4 == 0 && (st.wYear % 100 != 0 || st.wYear % 400 == 0))) yd++;
    vm_set(vm, "yearday", v_int(yd));
    return m;
}

/* utc() — UTC datetime map */
static Value dt_utc(int argc, Value *argv) {
    SYSTEMTIME st; Value m; VMap *vm;
    (void)argc; (void)argv;
    GetSystemTime(&st);
    m = v_map_new(); vm = m.as.map;
    vm_set(vm, "year",    v_int(st.wYear));
    vm_set(vm, "month",   v_int(st.wMonth));
    vm_set(vm, "day",     v_int(st.wDay));
    vm_set(vm, "hour",    v_int(st.wHour));
    vm_set(vm, "minute",  v_int(st.wMinute));
    vm_set(vm, "second",  v_int(st.wSecond));
    vm_set(vm, "ms",      v_int(st.wMilliseconds));
    vm_set(vm, "weekday", v_int(st.wDayOfWeek));
    return m;
}

static Value dt_epoch(int argc, Value *argv) {
    struct _timeb tb;
    (void)argc; (void)argv;
    _ftime(&tb);
    return v_double((double)tb.time + (double)tb.millitm / 1000.0);
}

static Value dt_epoch_ms(int argc, Value *argv) {
    struct _timeb tb;
    (void)argc; (void)argv;
    _ftime(&tb);
    return v_int((long long)tb.time * 1000 + tb.millitm);
}

static Value dt_sleep(int argc, Value *argv) {
    long long ms = (argc > 0 && argv[0].type == V_INT) ? argv[0].as.i : 0;
    if (ms > 0) Sleep((DWORD)ms);
    return v_null();
}

/* Date(year, month, day) */
static Value dt_date(int argc, Value *argv) {
    long long y, m, d, k, j, h, wd; Value r; VMap *vm;
    y = (argc > 0) ? argv[0].as.i : 2000;
    m = (argc > 1) ? argv[1].as.i : 1;
    d = (argc > 2) ? argv[2].as.i : 1;
    r = v_map_new(); vm = r.as.map;
    vm_set(vm, "year",  v_int(y));
    vm_set(vm, "month", v_int(m));
    vm_set(vm, "day",   v_int(d));
    if (m < 3) { m += 12; y--; }
    k = y % 100; j = y / 100;
    h = (d + (13*(m+1))/5 + k + k/4 + j/4 + 5*j) % 7;
    wd = (h + 6) % 7;
    vm_set(vm, "weekday", v_int(wd));
    return r;
}

static Value dt_time(int argc, Value *argv) {
    Value r; VMap *vm;
    r = v_map_new(); vm = r.as.map;
    vm_set(vm, "hour",   v_int((argc > 0) ? argv[0].as.i : 0));
    vm_set(vm, "minute", v_int((argc > 1) ? argv[1].as.i : 0));
    vm_set(vm, "second", v_int((argc > 2) ? argv[2].as.i : 0));
    vm_set(vm, "ms",     v_int((argc > 3) ? argv[3].as.i : 0));
    return r;
}

static Value dt_days_in_month(int argc, Value *argv) {
    int y, m, d;
    static int days[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    y = (int)((argc > 0) ? argv[0].as.i : 2024);
    m = (int)((argc > 1) ? argv[1].as.i : 1);
    d = (m >= 1 && m <= 12) ? days[m] : 0;
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) d = 29;
    return v_int(d);
}

static Value dt_is_leap(int argc, Value *argv) {
    int y = (int)((argc > 0) ? argv[0].as.i : 2024);
    return v_bool((y % 4 == 0 && y % 100 != 0) || y % 400 == 0);
}

/* format(dt_map, fmt_string) */
static Value dt_format(int argc, Value *argv) {
    VMap *dt; const char *fmt; char buf[256], *out;
    long long yy, mm, dd, hh, min, ss, ms, wd;
    int i;
    static const char *weekdays[] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
    static const char *months[] = {"","January","February","March","April","May","June","July","August","September","October","November","December"};
    static const char *wd_short[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    static const char *mo_short[] = {"","Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

    if (argc < 2 || argv[0].type != V_MAP || argv[1].type != V_STRING) return v_string("");
    dt = argv[0].as.map; fmt = argv[1].as.s; if (!fmt) return v_string("");
    yy=0;mm=0;dd=0;hh=0;min=0;ss=0;ms=0;wd=0;
    for (i = 0; i < dt->len; i++) {
        if (strcmp(dt->keys[i], "year") == 0)    yy = dt->vals[i].as.i;
        if (strcmp(dt->keys[i], "month") == 0)   mm = dt->vals[i].as.i;
        if (strcmp(dt->keys[i], "day") == 0)     dd = dt->vals[i].as.i;
        if (strcmp(dt->keys[i], "hour") == 0)    hh = dt->vals[i].as.i;
        if (strcmp(dt->keys[i], "minute") == 0)  min = dt->vals[i].as.i;
        if (strcmp(dt->keys[i], "second") == 0)  ss = dt->vals[i].as.i;
        if (strcmp(dt->keys[i], "ms") == 0)      ms = dt->vals[i].as.i;
        if (strcmp(dt->keys[i], "weekday") == 0) wd = dt->vals[i].as.i;
    }

    out = buf;
    while (*fmt && out - buf < 250) {
        if (*fmt == '%' && *(fmt+1)) {
            fmt++;
            switch (*fmt) {
            case 'Y': out += sprintf(out, "%04lld", yy); break;
            case 'y': out += sprintf(out, "%02lld", yy % 100); break;
            case 'm': out += sprintf(out, "%02lld", mm); break;
            case 'd': out += sprintf(out, "%02lld", dd); break;
            case 'H': out += sprintf(out, "%02lld", hh); break;
            case 'M': out += sprintf(out, "%02lld", min); break;
            case 'S': out += sprintf(out, "%02lld", ss); break;
            case 'f': out += sprintf(out, "%03lld", ms); break;
            case 'A': out += sprintf(out, "%s", weekdays[wd % 7]); break;
            case 'a': out += sprintf(out, "%s", wd_short[wd % 7]); break;
            case 'B': out += sprintf(out, "%s", months[mm % 13]); break;
            case 'b': out += sprintf(out, "%s", mo_short[mm % 13]); break;
            case '%': *out++ = '%'; break;
            default:  *out++ = '%'; *out++ = *fmt; break;
            }
        } else { *out++ = *fmt; }
        fmt++;
    }
    *out = 0;
    return v_string(buf);
}

static Value dt_weekday_name(int argc, Value *argv) {
    long long wd; static const char *n[] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
    wd = (argc > 0) ? argv[0].as.i : 0;
    return v_string(n[((int)wd % 7 + 7) % 7]);
}

static Value dt_month_name(int argc, Value *argv) {
    long long mo; static const char *n[] = {"","January","February","March","April","May","June","July","August","September","October","November","December"};
    mo = (argc > 0) ? argv[0].as.i : 1;
    if (mo < 1 || mo > 12) mo = 1;
    return v_string(n[mo]);
}

static Value dt_timestamp(int argc, Value *argv) {
    SYSTEMTIME st; char buf[30];
    (void)argc; (void)argv;
    GetLocalTime(&st);
    sprintf(buf, "%04d-%02d-%02dT%02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return v_string(buf);
}

static Value dt_bench(int argc, Value *argv) {
    static LARGE_INTEGER freq = {0}; LARGE_INTEGER c;
    (void)argc; (void)argv;
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&c);
    return v_double((double)c.QuadPart * 1e6 / (double)freq.QuadPart);
}

Value build_datetime(void) {
    Value mod; VMap *m;
    mod = v_map_new(); m = mod.as.map;
    vm_set(m, "now",         v_native(dt_now));
    vm_set(m, "utc",         v_native(dt_utc));
    vm_set(m, "epoch",       v_native(dt_epoch));
    vm_set(m, "epochMs",     v_native(dt_epoch_ms));
    vm_set(m, "sleep",       v_native(dt_sleep));
    vm_set(m, "Date",        v_native(dt_date));
    vm_set(m, "Time",        v_native(dt_time));
    vm_set(m, "daysInMonth", v_native(dt_days_in_month));
    vm_set(m, "isLeapYear",  v_native(dt_is_leap));
    vm_set(m, "format",      v_native(dt_format));
    vm_set(m, "weekdayName", v_native(dt_weekday_name));
    vm_set(m, "monthName",   v_native(dt_month_name));
    vm_set(m, "timestamp",   v_native(dt_timestamp));
    vm_set(m, "bench",       v_native(dt_bench));
    return mod;
}
