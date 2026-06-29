// _builtins_aot_runtime.c — AOT runtime for all C builtin modules
// Linked into every transpiled C program
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <direct.h>
  #define MKDIR(p) _mkdir(p)
  #define RMDIR(p)  _rmdir(p)
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFDIR) != 0)
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m) & _S_IFREG) != 0)
#endif
#else
  #include <unistd.h>
  #include <dirent.h>
  #define MKDIR(p) mkdir(p, 0755)
  #define RMDIR(p)  rmdir(p)
#endif
#include "candle_runtime.h"
#include "std/_builtins_aot.h"

/* ==== std.math ==== */
const candle_double candle_math_PI = 3.14159265358979323846;

candle_double candle_math_sqrt(candle_double x) { return sqrt(x); }
candle_double candle_math_sin(candle_double x)  { return sin(x); }
candle_double candle_math_cos(candle_double x)  { return cos(x); }
candle_double candle_math_tan(candle_double x)  { return tan(x); }
candle_double candle_math_pow(candle_double x, candle_double y) { return pow(x,y); }
candle_double candle_math_log(candle_double x)   { return log(x); }
candle_double candle_math_log10(candle_double x) { return log10(x); }
candle_double candle_math_exp(candle_double x)   { return exp(x); }
candle_double candle_math_floor(candle_double x) { return floor(x); }
candle_double candle_math_ceil(candle_double x)  { return ceil(x); }
candle_double candle_math_round(candle_double x) { return round(x); }
candle_double candle_math_abs(candle_double x)   { return fabs(x); }
candle_double candle_math_asin(candle_double x)  { return asin(x); }
candle_double candle_math_acos(candle_double x)  { return acos(x); }
candle_double candle_math_atan(candle_double x)  { return atan(x); }
candle_double candle_math_atan2(candle_double y, candle_double x) { return atan2(y,x); }
candle_double candle_math_sinh(candle_double x)  { return sinh(x); }
candle_double candle_math_cosh(candle_double x)  { return cosh(x); }
candle_double candle_math_tanh(candle_double x)  { return tanh(x); }
candle_double candle_math_radians(candle_double d) { return d * 3.14159265358979323846 / 180.0; }
candle_double candle_math_degrees(candle_double r) { return r * 180.0 / 3.14159265358979323846; }
candle_double candle_math_cbrt(candle_double x)    { return cbrt(x); }
candle_double candle_math_hypot(candle_double x, candle_double y) { return hypot(x,y); }
candle_double candle_math_log2(candle_double x)   { return log2(x); }
candle_double candle_math_trunc(candle_double x)  { return trunc(x); }
candle_int   candle_math_min(candle_double a, candle_double b) { return (candle_int)(a < b ? a : b); }
candle_int   candle_math_max(candle_double a, candle_double b) { return (candle_int)(a > b ? a : b); }
candle_int   candle_math_sign(candle_double x)    { return x > 0 ? 1 : (x < 0 ? -1 : 0); }

static int g_seeded = 0;
candle_int candle_math_random(candle_int n) {
    if (!g_seeded) { srand((unsigned)time(NULL)); g_seeded = 1; }
    if (n <= 0) return 0;
    return (candle_int)((double)rand() / ((double)RAND_MAX + 1.0) * (double)n);
}
candle_int candle_math_randomRange(candle_int lo, candle_int hi) {
    if (!g_seeded) { srand((unsigned)time(NULL)); g_seeded = 1; }
    if (hi <= lo) return lo;
    return lo + (candle_int)((double)rand() / ((double)RAND_MAX + 1.0) * (double)(hi - lo));
}
candle_int candle_math_seed(candle_int s) { srand((unsigned)s); g_seeded = 1; return 0; }

/* ==== std.io ==== */
candle_int candle_io_writeln(candle_string s) {
    if (s) { fputs(s, stdout); fputc('\n', stdout); }
    return 0;
}

/* ==== std.http (HTTPS: WinHTTP on Windows, libcurl on POSIX) ==== */
#ifdef _WIN32
  #include <winhttp.h>

static candle_string candle_http_request_impl(
    const char *method, const char *url,
    const char *body, const char *extra_headers)
{
    if (!url || !url[0]) return strdup("");

    wchar_t wurl[4096];
    MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 4096);

    URL_COMPONENTS uc;
    memset(&uc, 0, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256], path[2048];
    uc.lpszHostName = host; uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(wurl, 0, 0, &uc)) return strdup("");

    HINTERNET hSession = WinHttpOpen(L"Candle/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return strdup("");

    HINTERNET hConnect = WinHttpConnect(hSession, host, uc.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return strdup(""); }

    wchar_t wmethod[16];
    MultiByteToWideChar(CP_UTF8, 0, method, -1, wmethod, 16);

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, wmethod, path,
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return strdup("");
    }

    // Add custom headers (convert from UTF-8 multi-line to wide)
    if (extra_headers && extra_headers[0]) {
        wchar_t wheaders[8192];
        MultiByteToWideChar(CP_UTF8, 0, extra_headers, -1, wheaders, 8192);
        WinHttpAddRequestHeaders(hRequest, wheaders, (DWORD)-1,
            WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    }

    DWORD bodylen = body ? (DWORD)strlen(body) : 0;
    BOOL ok = WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        body ? (LPVOID)body : WINHTTP_NO_REQUEST_DATA,
        bodylen, bodylen, 0);
    if (!ok) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession); return strdup("");
    }
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession); return strdup("");
    }

    // Read response body
    size_t cap = 16384, total = 0;
    char *result = malloc(cap);
    result[0] = 0;
    DWORD avail, rd;
    while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
        if (total + avail + 1 > cap) {
            while (total + avail + 1 > cap) cap *= 2;
            result = realloc(result, cap);
        }
        WinHttpReadData(hRequest, result + total, avail, &rd);
        total += rd;
        result[total] = 0;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

#else /* POSIX: use libcurl or fallback */
  #include <curl/curl.h>
  struct _curl_buf { char *data; size_t len, cap; };
  static size_t _curl_wfn(void *p, size_t sz, size_t nm, void *ud) {
      struct _curl_buf *b = ud; size_t n = sz * nm;
      if (b->len + n + 1 > b->cap) { b->cap = (b->len + n + 1) * 2; b->data = realloc(b->data, b->cap); }
      memcpy(b->data + b->len, p, n); b->len += n; b->data[b->len] = 0;
      return n;
  }
  static candle_string candle_http_request_impl(
      const char *method, const char *url,
      const char *body, const char *extra_headers)
  {
      CURL *c = curl_easy_init();
      if (!c) return strdup("");
      struct _curl_buf buf = { malloc(4096), 0, 4096 }; buf.data[0] = 0;
      curl_easy_setopt(c, CURLOPT_URL, url);
      curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, _curl_wfn);
      curl_easy_setopt(c, CURLOPT_WRITEDATA, &buf);
      curl_easy_setopt(c, CURLOPT_TIMEOUT, 30L);
      if (strcmp(method, "POST") == 0) { curl_easy_setopt(c, CURLOPT_POST, 1L); curl_easy_setopt(c, CURLOPT_POSTFIELDS, body ? body : ""); }
      else if (strcmp(method, "PUT") == 0) { curl_easy_setopt(c, CURLOPT_CUSTOMREQUEST, "PUT"); curl_easy_setopt(c, CURLOPT_POSTFIELDS, body ? body : ""); }
      else if (strcmp(method, "DELETE") == 0) { curl_easy_setopt(c, CURLOPT_CUSTOMREQUEST, "DELETE"); }
      struct curl_slist *hlist = NULL;
      if (extra_headers && extra_headers[0]) {
          char *hcopy = strdup(extra_headers), *line = strtok(hcopy, "\r\n");
          while (line) { if (line[0]) hlist = curl_slist_append(hlist, line); line = strtok(NULL, "\r\n"); }
          free(hcopy);
          curl_easy_setopt(c, CURLOPT_HTTPHEADER, hlist);
      }
      curl_easy_perform(c);
      if (hlist) curl_slist_free_all(hlist);
      curl_easy_cleanup(c);
      return buf.data;
  }
#endif

candle_string candle_http_get(candle_string url) {
    return candle_http_request_impl("GET", url, NULL, NULL);
}

candle_string candle_http_post(candle_string url, candle_string body) {
    return candle_http_request_impl("POST", url, body,
        "Content-Type: application/json\r\n");
}

candle_string candle_http_postJson(candle_string url, candle_string json_body, candle_string auth_token) {
    char headers[4096];
    if (auth_token && auth_token[0]) {
        snprintf(headers, sizeof(headers),
            "Content-Type: application/json\r\n"
            "Authorization: Bearer %s\r\n", auth_token);
    } else {
        snprintf(headers, sizeof(headers), "Content-Type: application/json\r\n");
    }
    return candle_http_request_impl("POST", url, json_body, headers);
}

candle_string candle_http_put(candle_string url, candle_string body, candle_string headers) {
    return candle_http_request_impl("PUT", url, body, headers);
}

candle_string candle_http_delete(candle_string url, candle_string headers) {
    return candle_http_request_impl("DELETE", url, NULL, headers);
}

candle_string candle_http_request(candle_string method, candle_string url,
                                  candle_string body, candle_string headers) {
    return candle_http_request_impl(method ? method : "GET", url, body, headers);
}

// ── JSON helpers (minimal, for API interaction) ──

candle_string candle_json_escape(candle_string s) {
    if (!s) return strdup("");
    size_t slen = strlen(s);
    size_t cap = slen * 2 + 1;
    char *out = malloc(cap);
    size_t j = 0;
    for (size_t i = 0; i < slen; i++) {
        if (j + 6 > cap) { cap *= 2; out = realloc(out, cap); }
        char c = s[i];
        switch (c) {
            case '"':  out[j++] = '\\'; out[j++] = '"'; break;
            case '\\': out[j++] = '\\'; out[j++] = '\\'; break;
            case '\n': out[j++] = '\\'; out[j++] = 'n'; break;
            case '\r': out[j++] = '\\'; out[j++] = 'r'; break;
            case '\t': out[j++] = '\\'; out[j++] = 't'; break;
            default:   out[j++] = c; break;
        }
    }
    out[j] = 0;
    return out;
}

candle_string candle_json_get(candle_string json, candle_string key) {
    if (!json || !key) return strdup("");
    size_t klen = strlen(key);
    char pattern[512];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *pos = strstr(json, pattern);
    if (!pos) return strdup("");
    pos += klen + 2; // skip past "key"
    // skip whitespace and colon
    while (*pos == ' ' || *pos == ':' || *pos == '\t' || *pos == '\n' || *pos == '\r') pos++;
    if (*pos == '"') {
        // String value
        pos++;
        const char *end = pos;
        while (*end && !(*end == '"' && *(end - 1) != '\\')) end++;
        size_t vlen = (size_t)(end - pos);
        char *val = malloc(vlen + 1);
        memcpy(val, pos, vlen);
        val[vlen] = 0;
        // Unescape
        char *w = val, *r = val;
        while (*r) {
            if (*r == '\\' && *(r+1)) {
                r++;
                switch (*r) {
                    case 'n': *w++ = '\n'; break;
                    case 'r': *w++ = '\r'; break;
                    case 't': *w++ = '\t'; break;
                    case '"': *w++ = '"'; break;
                    case '\\': *w++ = '\\'; break;
                    default: *w++ = '\\'; *w++ = *r; break;
                }
                r++;
            } else { *w++ = *r++; }
        }
        *w = 0;
        return val;
    } else {
        // Number/bool/null
        const char *end = pos;
        while (*end && *end != ',' && *end != '}' && *end != ']' && *end != '\n') end++;
        size_t vlen = (size_t)(end - pos);
        while (vlen > 0 && (pos[vlen-1] == ' ' || pos[vlen-1] == '\r')) vlen--;
        char *val = malloc(vlen + 1);
        memcpy(val, pos, vlen);
        val[vlen] = 0;
        return val;
    }
}

/* ==== std.time ==== */
#ifdef _WIN32
candle_double candle_time_now(void) {
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER c;
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&c);
    return (candle_double)((double)c.QuadPart * 1000.0 / (double)freq.QuadPart);
}
candle_int candle_time_sleep(candle_int ms) { if (ms > 0) Sleep((DWORD)ms); return 0; }
#else
candle_double candle_time_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (candle_double)((double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0);
}
candle_int candle_time_sleep(candle_int ms) {
    if (ms > 0) { struct timespec ts = {(time_t)(ms/1000), (long)((ms%1000)*1000000)}; nanosleep(&ts, NULL); }
    return 0;
}
#endif

/* ==== std.fs ==== */
candle_int candle_fs_exists(candle_string path) {
    struct stat st;
    return path ? (stat(path, &st) == 0 ? 1 : 0) : 0;
}
candle_int candle_fs_isDir(candle_string path) {
    struct stat st;
    return (path && stat(path, &st) == 0) ? (S_ISDIR(st.st_mode) ? 1 : 0) : 0;
}
candle_int candle_fs_isFile(candle_string path) {
    struct stat st;
    return (path && stat(path, &st) == 0) ? (S_ISREG(st.st_mode) ? 1 : 0) : 0;
}
candle_int candle_fs_fileSize(candle_string path) {
    struct stat st;
    return (path && stat(path, &st) == 0) ? (candle_int)st.st_size : 0;
}
candle_string candle_fs_readFile(candle_string path) {
    if (!path) return "";
    FILE *f = fopen(path, "rb");
    if (!f) return "";
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(n + 1);
    long rd = (long)fread(buf, 1, n, f);
    buf[rd] = 0;
    fclose(f);
    char *ret = strdup(buf);
    free(buf);
    return ret;
}
candle_int candle_fs_writeFile(candle_string path, candle_string content) {
    if (!path || !content) return 0;
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fputs(content, f);
    fclose(f);
    return 1;
}
candle_int candle_fs_mkdir(candle_string path) {
    return path ? (MKDIR(path) == 0 ? 1 : 0) : 0;
}
candle_int candle_fs_delete(candle_string path) {
    if (!path) return 0;
    if (remove(path) == 0) return 1;
    return RMDIR(path) == 0 ? 1 : 0;
}
candle_int candle_fs_listDir(candle_string path) { (void)path; return 0; }

/* ==== std.json (stub) ==== */
candle_int    candle_json_parse(candle_string s)       { (void)s; return 0; }
candle_string candle_json_stringify(candle_int v)      { (void)v; return ""; }
candle_string candle_json_pretty(candle_int v)         { (void)v; return ""; }

/* ==== std.file ==== */
candle_int candle_file_open(candle_string path, candle_string mode) {
    if (!path || !mode) return 0;
    return (candle_int)(intptr_t)fopen(path, mode);
}
candle_int candle_file_close(candle_int handle) {
    if (handle) fclose((FILE*)(intptr_t)handle);
    return 0;
}
candle_string candle_file_read(candle_int handle) {
    if (!handle) return "";
    FILE *f = (FILE*)(intptr_t)handle;
    long pos = ftell(f);
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, pos, SEEK_SET);
    char *buf = malloc(n + 1);
    long rd = (long)fread(buf, 1, n, f);
    buf[rd] = 0;
    char *ret = strdup(buf);
    free(buf);
    return ret;
}
candle_string candle_file_readLine(candle_int handle) {
    if (!handle) return "";
    FILE *f = (FILE*)(intptr_t)handle;
    char buf[8192];
    if (!fgets(buf, sizeof(buf), f)) return "";
    size_t n = strlen(buf);
    while (n && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = 0;
    return strdup(buf);
}
candle_int candle_file_readBytes(candle_int handle) {
    if (!handle) return 0;
    FILE *f = (FILE*)(intptr_t)handle;
    long pos = ftell(f);
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, pos, SEEK_SET);
    unsigned char *buf = malloc(n);
    fread(buf, 1, n, f);
    return (candle_int)(intptr_t)buf;
}
candle_int candle_file_write(candle_int handle, candle_string s) {
    if (!handle || !s) return 0;
    fputs(s, (FILE*)(intptr_t)handle);
    return 1;
}
candle_int candle_file_writeLine(candle_int handle, candle_string s) {
    if (!handle || !s) return 0;
    FILE *f = (FILE*)(intptr_t)handle;
    fputs(s, f);
    fputc('\n', f);
    return 1;
}
candle_int candle_file_writeBytes(candle_int handle, candle_int bytes_ptr) {
    (void)handle; (void)bytes_ptr; return 0;
}
candle_int candle_file_seek(candle_int handle, candle_int pos) {
    if (!handle) return 0;
    fseek((FILE*)(intptr_t)handle, (long)pos, SEEK_SET);
    return 1;
}
candle_int candle_file_tell(candle_int handle) {
    if (!handle) return 0;
    return (candle_int)ftell((FILE*)(intptr_t)handle);
}

/* ==== std.random ==== */
candle_int candle_random_randomInt(candle_int lo, candle_int hi) {
    return candle_math_randomRange(lo, hi);
}
candle_double candle_random_randomDouble(void) {
    if (!g_seeded) { srand((unsigned)time(NULL)); g_seeded = 1; }
    return (candle_double)rand() / ((candle_double)RAND_MAX + 1.0);
}
candle_int candle_random_seed(candle_int s) { return candle_math_seed(s); }

/* ==== std.process (stub) ==== */
candle_int    candle_process_exec(candle_string cmd)         { (void)cmd; return 0; }
candle_string candle_process_execCapture(candle_string cmd)  { (void)cmd; return ""; }

/* ==== std.path ==== */
candle_string candle_path_basename(candle_string p) {
    if (!p) return "";
    const char *s = strrchr(p, '/');
    if (!s) s = strrchr(p, '\\');
    return s ? strdup(s + 1) : strdup(p);
}
candle_string candle_path_dirname(candle_string p) {
    if (!p) return ".";
    const char *s = strrchr(p, '/');
    if (!s) s = strrchr(p, '\\');
    if (!s) return ".";
    size_t n = (size_t)(s - p);
    char *buf = malloc(n + 1);
    memcpy(buf, p, n); buf[n] = 0;
    char *ret = strdup(buf);
    free(buf);
    return ret;
}
candle_string candle_path_extension(candle_string p) {
    if (!p) return "";
    const char *s = strrchr(p, '.');
    return s ? strdup(s) : "";
}
candle_string candle_path_join(candle_string a, candle_string b) {
    if (!a || !b) return a ? strdup(a) : (b ? strdup(b) : "");
    size_t la = strlen(a), lb = strlen(b);
    int need = (la > 0 && a[la-1] != '/' && a[la-1] != '\\');
    char *buf = malloc(la + lb + 2);
    memcpy(buf, a, la);
    if (need) buf[la++] = '/';
    memcpy(buf + la, b, lb + 1);
    char *ret = strdup(buf);
    free(buf);
    return ret;
}
candle_int candle_path_isAbsolute(candle_string p) {
    if (!p) return 0;
#ifdef _WIN32
    return (p[0] && p[1] == ':' && (p[2] == '/' || p[2] == '\\')) ? 1 : 0;
#else
    return (p[0] == '/') ? 1 : 0;
#endif
}

/* ==== std.encoding (stub) ==== */
candle_int    candle_encoding_encode(candle_string s, candle_string enc) { (void)s; (void)enc; return 0; }
candle_string candle_encoding_decode(candle_int b, candle_string enc)   { (void)b; (void)enc; return ""; }
candle_int    candle_encoding_hexEncode(candle_int b)                   { (void)b; return 0; }
candle_int    candle_encoding_hexDecode(candle_string s)                { (void)s; return 0; }
candle_string candle_encoding_base64Encode(candle_string s)             { (void)s; return ""; }
candle_int    candle_encoding_base64Decode(candle_string s)             { (void)s; return 0; }
