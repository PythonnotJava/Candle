#include "modules.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "../vendor/cjson/cJSON.h"

// ── 辅助：取参数为 double / int ───────────────────────────────────────────────

static double arg_d(int argc, Value *argv, int i) {
    if (i >= argc) return 0.0;
    Value v = argv[i];
    if (v.type == V_DOUBLE) return v.as.d;
    if (v.type == V_INT) return (double)v.as.i;
    return 0.0;
}

static int64_t arg_i(int argc, Value *argv, int i) {
    if (i >= argc) return 0;
    Value v = argv[i];
    if (v.type == V_INT) return v.as.i;
    if (v.type == V_DOUBLE) return (int64_t)v.as.d;
    return 0;
}

// ── std.math ─────────────────────────────────────────────────────────────────

static Value m_sqrt(int argc, Value *argv) { return v_double(sqrt(arg_d(argc, argv, 0))); }
static Value m_cbrt(int argc, Value *argv) { return v_double(cbrt(arg_d(argc, argv, 0))); }
static Value m_pow(int argc, Value *argv)  { return v_double(pow(arg_d(argc, argv, 0), arg_d(argc, argv, 1))); }
static Value m_exp(int argc, Value *argv)  { return v_double(exp(arg_d(argc, argv, 0))); }
static Value m_hypot(int argc, Value *argv){ return v_double(hypot(arg_d(argc, argv, 0), arg_d(argc, argv, 1))); }
static Value m_log(int argc, Value *argv)  { return v_double(log(arg_d(argc, argv, 0))); }
static Value m_log2(int argc, Value *argv) { return v_double(log2(arg_d(argc, argv, 0))); }
static Value m_log10(int argc, Value *argv){ return v_double(log10(arg_d(argc, argv, 0))); }
static Value m_sin(int argc, Value *argv)  { return v_double(sin(arg_d(argc, argv, 0))); }
static Value m_cos(int argc, Value *argv)  { return v_double(cos(arg_d(argc, argv, 0))); }
static Value m_tan(int argc, Value *argv)  { return v_double(tan(arg_d(argc, argv, 0))); }
static Value m_asin(int argc, Value *argv) { return v_double(asin(arg_d(argc, argv, 0))); }
static Value m_acos(int argc, Value *argv) { return v_double(acos(arg_d(argc, argv, 0))); }
static Value m_atan(int argc, Value *argv) { return v_double(atan(arg_d(argc, argv, 0))); }
static Value m_atan2(int argc, Value *argv){ return v_double(atan2(arg_d(argc, argv, 0), arg_d(argc, argv, 1))); }
static Value m_sinh(int argc, Value *argv) { return v_double(sinh(arg_d(argc, argv, 0))); }
static Value m_cosh(int argc, Value *argv) { return v_double(cosh(arg_d(argc, argv, 0))); }
static Value m_tanh(int argc, Value *argv) { return v_double(tanh(arg_d(argc, argv, 0))); }
static Value m_floor(int argc, Value *argv){ return v_double(floor(arg_d(argc, argv, 0))); }
static Value m_ceil(int argc, Value *argv) { return v_double(ceil(arg_d(argc, argv, 0))); }
static Value m_round(int argc, Value *argv){ return v_double(round(arg_d(argc, argv, 0))); }
static Value m_trunc(int argc, Value *argv){ return v_double(trunc(arg_d(argc, argv, 0))); }

static Value m_abs(int argc, Value *argv) {
    if (argc > 0 && argv[0].type == V_DOUBLE) return v_double(fabs(argv[0].as.d));
    int64_t x = arg_i(argc, argv, 0);
    return v_int(x < 0 ? -x : x);
}
static Value m_sign(int argc, Value *argv) {
    double x = arg_d(argc, argv, 0);
    if (x > 0) return v_int(1); if (x < 0) return v_int(-1); return v_int(0);
}
static Value m_min(int argc, Value *argv) {
    if (argc < 2) return argc ? argv[0] : v_null();
    double a = arg_d(argc, argv, 0), b = arg_d(argc, argv, 1);
    return (a < b) ? argv[0] : argv[1];
}
static Value m_max(int argc, Value *argv) {
    if (argc < 2) return argc ? argv[0] : v_null();
    double a = arg_d(argc, argv, 0), b = arg_d(argc, argv, 1);
    return (a > b) ? argv[0] : argv[1];
}
static Value m_radians(int argc, Value *argv){ return v_double(arg_d(argc, argv, 0) * 3.14159265358979323846 / 180.0); }
static Value m_degrees(int argc, Value *argv){ return v_double(arg_d(argc, argv, 0) * 180.0 / 3.14159265358979323846); }

static int g_rand_seeded = 0;
static void ensure_seed(void) {
    if (!g_rand_seeded) { srand((unsigned)time(NULL)); g_rand_seeded = 1; }
}
static Value m_random(int argc, Value *argv) {
    ensure_seed();
    double r = (double)rand() / ((double)RAND_MAX + 1.0);
    if (argc >= 1) { int64_t n = arg_i(argc, argv, 0); if (n <= 0) return v_int(0); return v_int((int64_t)(r * (double)n)); }
    return v_double(r);
}
static Value m_random_range(int argc, Value *argv) {
    ensure_seed(); int64_t lo = arg_i(argc, argv, 0), hi = arg_i(argc, argv, 1);
    if (hi <= lo) return v_int(lo);
    double r = (double)rand() / ((double)RAND_MAX + 1.0);
    return v_int(lo + (int64_t)(r * (double)(hi - lo)));
}
static Value m_seed(int argc, Value *argv) { srand((unsigned)arg_i(argc, argv, 0)); g_rand_seeded = 1; return v_null(); }

static Value build_math(void) {
    Value mod = v_map(); VMap *m = mod.as.map;
    v_map_set(m, "sqrt", v_native(m_sqrt)); v_map_set(m, "cbrt", v_native(m_cbrt));
    v_map_set(m, "pow", v_native(m_pow)); v_map_set(m, "exp", v_native(m_exp)); v_map_set(m, "hypot", v_native(m_hypot));
    v_map_set(m, "log", v_native(m_log)); v_map_set(m, "log2", v_native(m_log2)); v_map_set(m, "log10", v_native(m_log10));
    v_map_set(m, "sin", v_native(m_sin)); v_map_set(m, "cos", v_native(m_cos)); v_map_set(m, "tan", v_native(m_tan));
    v_map_set(m, "asin", v_native(m_asin)); v_map_set(m, "acos", v_native(m_acos)); v_map_set(m, "atan", v_native(m_atan)); v_map_set(m, "atan2", v_native(m_atan2));
    v_map_set(m, "sinh", v_native(m_sinh)); v_map_set(m, "cosh", v_native(m_cosh)); v_map_set(m, "tanh", v_native(m_tanh));
    v_map_set(m, "floor", v_native(m_floor)); v_map_set(m, "ceil", v_native(m_ceil));
    v_map_set(m, "round", v_native(m_round)); v_map_set(m, "trunc", v_native(m_trunc));
    v_map_set(m, "abs", v_native(m_abs)); v_map_set(m, "sign", v_native(m_sign));
    v_map_set(m, "min", v_native(m_min)); v_map_set(m, "max", v_native(m_max));
    v_map_set(m, "radians", v_native(m_radians)); v_map_set(m, "degrees", v_native(m_degrees));
    v_map_set(m, "random", v_native(m_random)); v_map_set(m, "randomRange", v_native(m_random_range)); v_map_set(m, "seed", v_native(m_seed));
    v_map_set(m, "PI", v_double(3.14159265358979323846)); v_map_set(m, "E", v_double(2.71828182845904523536));
    v_map_set(m, "SQRT2", v_double(1.41421356237309504880)); v_map_set(m, "SQRT1_2", v_double(0.70710678118654752440));
    v_map_set(m, "LN2", v_double(0.69314718055994530942)); v_map_set(m, "LN10", v_double(2.30258509299404568402));
    return mod;
}

// ── std.io ───────────────────────────────────────────────────────────────────

static Value io_readline(int argc, Value *argv) {
    (void)argc; (void)argv; char buf[4096];
    if (!fgets(buf, sizeof(buf), stdin)) return v_string("");
    size_t n = strlen(buf); while (n && (buf[n-1]=='\n'||buf[n-1]=='\r')) buf[--n]=0;
    return v_string(buf);
}
static Value io_write(int argc, Value *argv) {
    for (int i=0;i<argc;i++){char*s=v_to_string(argv[i]);fputs(s,stdout);free(s);} return v_null();
}
static Value io_writeln(int argc, Value *argv) {
    for (int i=0;i<argc;i++){char*s=v_to_string(argv[i]);fputs(s,stdout);free(s);} fputc('\n',stdout); return v_null();
}
static Value io_readFile(int argc, Value *argv) {
    if (argc<1||argv[0].type!=V_STRING) return v_null();
    FILE*f=fopen(argv[0].as.s,"rb"); if(!f)return v_null();
    fseek(f,0,SEEK_END);long n=ftell(f);fseek(f,0,SEEK_SET);
    char*buf=malloc(n+1);size_t rd=fread(buf,1,n,f);buf[rd]=0;fclose(f);
    Value r=v_string(buf);free(buf);return r;
}
static Value io_writeFile(int argc, Value *argv) {
    if (argc<2||argv[0].type!=V_STRING) return v_bool(0);
    FILE*f=fopen(argv[0].as.s,"wb");if(!f)return v_bool(0);
    char*content=v_to_string(argv[1]);fputs(content,f);free(content);fclose(f);return v_bool(1);
}
static Value build_io(void) {
    Value mod=v_map();VMap*m=mod.as.map;
    v_map_set(m,"readline",v_native(io_readline)); v_map_set(m,"write",v_native(io_write));
    v_map_set(m,"writeln",v_native(io_writeln)); v_map_set(m,"readFile",v_native(io_readFile));
    v_map_set(m,"writeFile",v_native(io_writeFile)); return mod;
}

// ── std.json (cJSON backend) ──────────────────────────────────────────────────

static Value json_to_value(cJSON*item){
    if(!item)return v_null();
    switch(item->type){
        case cJSON_False:return v_bool(0); case cJSON_True:return v_bool(1); case cJSON_NULL:return v_null();
        case cJSON_Number:
            if(item->valuedouble==(double)(int64_t)item->valuedouble && item->valuedouble>=INT64_MIN && item->valuedouble<=INT64_MAX)
                return v_int((int64_t)item->valuedouble);
            return v_double(item->valuedouble);
        case cJSON_String:return v_string(item->valuestring?item->valuestring:"");
        case cJSON_Array:{Value out=v_list();cJSON*child=item->child;while(child){v_list_push(out.as.list,json_to_value(child));child=child->next;}return out;}
        case cJSON_Object:{Value out=v_map();cJSON*child=item->child;while(child){v_map_set(out.as.map,child->string,json_to_value(child));child=child->next;}return out;}
        default:return v_null();
    }
}
static cJSON*value_to_json(Value v){
    switch(v.type){
        case V_NULL:return cJSON_CreateNull(); case V_BOOL:return cJSON_CreateBool(v.as.b);
        case V_INT:return cJSON_CreateNumber((double)v.as.i); case V_DOUBLE:return cJSON_CreateNumber(v.as.d);
        case V_STRING:return cJSON_CreateString(v.as.s?v.as.s:"");
        case V_LIST:{cJSON*arr=cJSON_CreateArray();if(v.as.list){for(int i=0;i<v.as.list->len;i++)cJSON_AddItemToArray(arr,value_to_json(v.as.list->items[i]));}return arr;}
        case V_MAP:{cJSON*obj=cJSON_CreateObject();if(v.as.map){for(int i=0;i<v.as.map->len;i++)cJSON_AddItemToObject(obj,v.as.map->keys[i],value_to_json(v.as.map->vals[i]));}return obj;}
        default:return cJSON_CreateNull();
    }
}
static Value json_parse(int argc,Value*argv){
    if(argc<1||argv[0].type!=V_STRING||!argv[0].as.s)return v_null();
    cJSON*root=cJSON_Parse(argv[0].as.s); if(!root){const char*err=cJSON_GetErrorPtr();if(err)fprintf(stderr,"json.parse error: %s\n",err);return v_null();}
    Value r=json_to_value(root);cJSON_Delete(root);return r;
}
static Value json_stringify(int argc,Value*argv){
    if(argc<1)return v_string("null");cJSON*root=value_to_json(argv[0]);
    char*s=cJSON_PrintUnformatted(root);Value r=v_string(s?s:"null");if(s)free(s);cJSON_Delete(root);return r;
}
static Value json_pretty(int argc,Value*argv){
    if(argc<1)return v_string("null");cJSON*root=value_to_json(argv[0]);
    char*s=cJSON_Print(root);Value r=v_string(s?s:"null");if(s)free(s);cJSON_Delete(root);return r;
}
static Value build_json(void) {
    Value mod=v_map();VMap*m=mod.as.map;
    v_map_set(m,"parse",v_native(json_parse));v_map_set(m,"stringify",v_native(json_stringify));
    v_map_set(m,"pretty",v_native(json_pretty));return mod;
}

// ── extern modules (separate .c files, avoid TokenType/winnt.h conflicts) ─────

extern Value build_http(void);
extern Value build_time(void);
extern Value build_fs(void);
extern Value build_file_map(void);
extern Value build_random(void);
extern Value build_process(void);
extern Value build_path(void);
extern Value build_encoding(void);

// ── 模块注册表 ───────────────────────────────────────────────────────────────

typedef struct { const char *path; Value (*build)(void); } ModuleDef;

static const ModuleDef g_modules[] = {
    { "std.math",     build_math     },
    { "std.io",       build_io       },
    { "std.http",     build_http     },
    { "std.time",     build_time     },
    { "std.fs",       build_fs       },
    { "std.json",     build_json     },
    { "std.file",     build_file_map },
    { "std.random",   build_random   },
    { "std.process",  build_process  },
    { "std.path",     build_path     },
    { "std.encoding", build_encoding },
    { NULL, NULL }
};

int module_exists(const char *path) {
    if (!path) return 0;
    for (int i = 0; g_modules[i].path; i++)
        if (strcmp(g_modules[i].path, path) == 0) return 1;
    return 0;
}

Value module_load(const char *path) {
    if (!path) return v_null();
    for (int i = 0; g_modules[i].path; i++)
        if (strcmp(g_modules[i].path, path) == 0) return g_modules[i].build();
    return v_null();
}
