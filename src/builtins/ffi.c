#include "ffi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <setjmp.h>

#if defined(_WIN32)
  // winnt.h 里 TOKEN_INFORMATION_CLASS 有个枚举常量也叫 TokenType，
  // 与 Candle ast.h 的 `typedef enum {...} TokenType` 类型名冲突。
  // 包含 windows.h 期间临时改名规避，包含后还原。
  #define WIN32_LEAN_AND_MEAN
  #define TokenType Win_TokenType_enum
  #include <windows.h>
  #undef TokenType
  typedef HMODULE DllHandle;
  #define DLL_OPEN(name)        LoadLibraryA(name)
  #define DLL_SYM(h, s)         ((void*)GetProcAddress((h), (s)))
#else
  #include <dlfcn.h>
  typedef void* DllHandle;
  #define DLL_OPEN(name)        dlopen((name), RTLD_NOW | RTLD_GLOBAL)
  #define DLL_SYM(h, s)         dlsym((h), (s))
#endif

// ── 平台库名候选 ──────────────────────────────────────────────────────────────
// dll 关键字按编译平台自动匹配主流动态库后缀，无需用户指定：
//   Windows → name.dll        Linux → libname.so / name.so
//   macOS   → libname.dylib   （.lib 是静态/导入库，运行时无法 dlopen，不在此列）
// 为提升跨平台健壮性，每个平台都给出多个候选，依次尝试加载。
#if defined(_WIN32)
  static const char *k_lib_prefixes[] = { "", "lib" };
  static const char *k_lib_suffixes[] = { ".dll" };
#elif defined(__APPLE__)
  static const char *k_lib_prefixes[] = { "lib", "" };
  static const char *k_lib_suffixes[] = { ".dylib", ".so" };
#else
  static const char *k_lib_prefixes[] = { "lib", "" };
  static const char *k_lib_suffixes[] = { ".so" };
#endif

// ── 已加载库登记表 ────────────────────────────────────────────────────────────

typedef struct {
    char *alias;        // candle 中的别名
    char *libname;      // 实际加载成功的库文件名（失败时为首选候选）
    DllHandle handle;   // NULL 表示加载失败（优雅降级）
} LoadedLib;

static LoadedLib g_libs[64];
static int g_nlibs = 0;

// dotted_path: "system.sqlite3" / "system.openssl.crypto" → 取最后一段做库名主体。
static const char *lib_base(const char *dotted_path) {
    const char *last = strrchr(dotted_path, '.');
    return last ? last + 1 : dotted_path;
}

// 拼出第 (pi, si) 个候选库文件名；调用方负责 free。
static char *make_candidate(const char *base, int pi, int si) {
    const char *pre = k_lib_prefixes[pi];
    const char *suf = k_lib_suffixes[si];
    size_t n = strlen(pre) + strlen(base) + strlen(suf) + 1;
    char *out = malloc(n);
    snprintf(out, n, "%s%s%s", pre, base, suf);
    return out;
}

#define N_LIB_PREFIXES ((int)(sizeof(k_lib_prefixes)/sizeof(k_lib_prefixes[0])))
#define N_LIB_SUFFIXES ((int)(sizeof(k_lib_suffixes)/sizeof(k_lib_suffixes[0])))

// 依次尝试所有 前缀×后缀 候选；成功则回传文件名(*out_name，调用方 free)。
// 全部失败时回传首选候选名，handle 为 NULL。
static DllHandle try_open_lib(const char *base, char **out_name) {
    char *first = NULL;
    for (int pi = 0; pi < N_LIB_PREFIXES; pi++) {
        for (int si = 0; si < N_LIB_SUFFIXES; si++) {
            char *cand = make_candidate(base, pi, si);
            if (!first) first = strdup(cand);
            DllHandle h = DLL_OPEN(cand);
            if (h) { free(first); *out_name = cand; return h; }
            free(cand);
        }
    }
    *out_name = first;   // 首选候选名（用于诊断/记录）
    return (DllHandle)NULL;
}

// PLACEHOLDER_FFI_BODY

// ── dll 加载 ─────────────────────────────────────────────────────────────────

Value ffi_dll_load(const char *dotted_path, const char *alias_name) {
    const char *base = lib_base(dotted_path);
    char *libname = NULL;
    DllHandle h = try_open_lib(base, &libname);   // 多候选加载，libname 为成功/首选名

    if (g_nlibs < 64) {
        g_libs[g_nlibs].alias   = strdup(alias_name ? alias_name : dotted_path);
        g_libs[g_nlibs].libname = strdup(libname ? libname : base);
        g_libs[g_nlibs].handle  = h;
        g_nlibs++;
    }

    // 返回 dll 句柄命名空间：__dll__ / __lib__ / __loaded__
    Value ns = v_map();
    VMap *m = ns.as.map;
    v_map_set(m, "__dll__",    v_string(dotted_path));
    v_map_set(m, "__lib__",    v_string(libname ? libname : base));
    v_map_set(m, "__loaded__", v_bool(h != NULL));
    free(libname);
    return ns;
}

// PLACEHOLDER_FFI_CALL

// 在所有已加载库中查找符号
static void *resolve_symbol(const char *sym) {
    for (int i = 0; i < g_nlibs; i++) {
        if (!g_libs[i].handle) continue;
        void *p = DLL_SYM(g_libs[i].handle, sym);
        if (p) return p;
    }
    return NULL;
}

int ffi_symbol_exists(const char *sym) {
    return resolve_symbol(sym) != NULL;
}

// 把 candle 值转成一个机器字参数（int 值 / 指针 / 字符串地址 / null=0）
static uintptr_t arg_to_word(Value v) {
    switch (v.type) {
        case V_INT:    return (uintptr_t)v.as.i;
        case V_BOOL:   return (uintptr_t)(v.as.b ? 1 : 0);
        case V_STRING: return (uintptr_t)v.as.s;          // const char*
        case V_NULL:   return (uintptr_t)0;
        default:       return (uintptr_t)0;               // 其余类型暂以 0 传递
    }
}

// 定长 trampoline：整型/指针参数在 x86-64（SysV 与 Win64）下均按机器字传递，
// 用统一的 uintptr_t 签名即可调用绝大多数 C ABI 函数（覆盖整型/指针/字符串签名）。
// 不支持 double 入参的浮点寄存器约定——返回 ret_type 解释为 double 时仍能取整型返回。
typedef uintptr_t (*FnW)(uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                         uintptr_t, uintptr_t, uintptr_t, uintptr_t);

// FFI 是用户逃生舱：若对着签名不兼容的库盲调（如把输出指针当 null 传给真实 C
// API），可能访问非法内存。这里用 signal(SIGSEGV/SIGILL)+setjmp 兜底，把崩溃
// 降级为一次告警 + 返回 0，避免整个解释器进程被一次坏调用带走。
static jmp_buf g_ffi_jmp;
static volatile sig_atomic_t g_ffi_faulted = 0;

static void ffi_fault_handler(int sig) {
    (void)sig;
    g_ffi_faulted = 1;
    longjmp(g_ffi_jmp, 1);
}

// 成功返回 1 并写 *out；调用过程中触发段错误/非法指令则返回 0。
static int invoke_trampoline(void *fp, uintptr_t *a, uintptr_t *out) {
    FnW fn = (FnW)fp;
    void (*old_segv)(int) = signal(SIGSEGV, ffi_fault_handler);
    void (*old_ill)(int)  = signal(SIGILL,  ffi_fault_handler);
    g_ffi_faulted = 0;
    int ok = 1;
    if (setjmp(g_ffi_jmp) == 0) {
        *out = fn(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
    } else {
        ok = 0;   // 从 fault handler 跳回
    }
    signal(SIGSEGV, old_segv);
    signal(SIGILL,  old_ill);
    return ok;
}

Value ffi_call(const char *sym, const char *ret_type, int argc, Value *argv) {
    void *fp = resolve_symbol(sym);
    if (!fp) {
        // 符号不可用（库未加载/无此符号）→ 优雅降级，不崩溃
        static int warned_once = 0;
        if (!warned_once) {
            fprintf(stderr, "[ffi] symbol '%s' unavailable; returning null (dll not loaded?)\n", sym);
            warned_once = 1;
        }
        if (ret_type && strcmp(ret_type, "int") == 0)    return v_int(0);
        if (ret_type && strcmp(ret_type, "double") == 0) return v_double(0.0);
        return v_null();
    }

    uintptr_t a[8] = {0,0,0,0,0,0,0,0};
    int n = argc < 8 ? argc : 8;
    for (int i = 0; i < n; i++) a[i] = arg_to_word(argv[i]);

    uintptr_t r = 0;
    if (!invoke_trampoline(fp, a, &r)) {
        fprintf(stderr, "[ffi] call to '%s' faulted (incompatible signature?); returning null\n", sym);
        if (ret_type && strcmp(ret_type, "int") == 0)    return v_int(0);
        if (ret_type && strcmp(ret_type, "double") == 0) return v_double(0.0);
        return v_null();
    }

    if (ret_type) {
        if (strcmp(ret_type, "void") == 0)   return v_null();
        if (strcmp(ret_type, "int") == 0)    return v_int((int64_t)r);
        if (strcmp(ret_type, "bool") == 0)   return v_bool(r != 0);
        if (strcmp(ret_type, "double") == 0) return v_double((double)(int64_t)r);
        if (strcmp(ret_type, "string") == 0)
            return r ? v_string((const char*)r) : v_null();
    }
    return v_int((int64_t)r);
}


