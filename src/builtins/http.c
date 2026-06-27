// http.c — std.http 模块 (完全自包含，避免 TokenType 冲突)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #define _WINSOCK_DEPRECATED_NO_WARNINGS
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netdb.h>
  #include <unistd.h>
  #define closesocket close
#endif

// ── 最小 Value 类型定义 (与 value.h 一致) ────────────────────────────────────
typedef enum { V_NULL, V_INT, V_DOUBLE, V_BOOL, V_STRING, V_LIST, V_TUPLE,
               V_MAP, V_FN, V_LAMBDA, V_CLASS, V_OBJECT, V_NATIVE } ValueType;

typedef struct Value { ValueType type; union { long long i; double d; int b;
    char *s; void *list; void *map; void *fn; void *cls; void *obj;
    struct Value (*native)(int, struct Value*); } as; } Value;

typedef struct { char **keys; Value *vals; int len, cap; } VMap;

static Value v_string_ptr(char *s) { Value v; v.type = V_STRING; v.as.s = s; return v; }
static Value v_null(void) { Value v; v.type = V_NULL; return v; }

static void v_map_set(VMap *m, const char *k, Value v) {
    for (int i = 0; i < m->len; i++)
        if (strcmp(m->keys[i], k) == 0) { m->vals[i] = v; return; }
    if (m->len >= m->cap) {
        m->cap = m->cap ? m->cap * 2 : 4;
        m->keys = realloc(m->keys, sizeof(char*) * m->cap);
        m->vals = realloc(m->vals, sizeof(Value) * m->cap);
    }
    m->keys[m->len] = _strdup(k);
    m->vals[m->len] = v;
    m->len++;
}

static Value v_map_new(void) {
    Value v; v.type = V_MAP;
    VMap *m = calloc(1, sizeof(VMap));
    v.as.map = m;
    return v;
}
// ──────────────────────────────────────────────────────────────────────────────

static int g_ws_init = 0;
static void ensure_winsock(void) {
#ifdef _WIN32
    if (!g_ws_init) { WSADATA w; WSAStartup(MAKEWORD(2,2), &w); g_ws_init = 1; }
#endif
}

static Value http_get(int argc, Value *argv) {
    if (argc < 1 || argv[0].type != V_STRING || !argv[0].as.s)
        return v_string_ptr(_strdup(""));
    ensure_winsock();

    const char *url = argv[0].as.s;
    const char *hs = (*url == 'h' && url[1] == 't') ? url + 7 : url;

    const char *ps = strchr(hs, '/');
    const char *cs = strchr(hs, ':');
    char host[256] = {0}, path[512] = "/";
    int port = 80;

    if (cs && (!ps || cs < ps)) { size_t n = cs-hs; if(n>=256)n=255; memcpy(host,hs,n); port=atoi(cs+1); }
    else if (ps) { size_t n = ps-hs; if(n>=256)n=255; memcpy(host,hs,n); }
    else { size_t n = strlen(hs); if(n>=256)n=255; memcpy(host,hs,n); }
    if (ps) { size_t n = strlen(ps); if(n>=512)n=511; memcpy(path,ps,n); }

    struct hostent *he = gethostbyname(host);
    if (!he) return v_string_ptr(_strdup(""));

    int s = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return v_string_ptr(_strdup(""));

    struct sockaddr_in a; memset(&a,0,sizeof(a));
    a.sin_family = AF_INET; a.sin_port = htons((u_short)port);
    memcpy(&a.sin_addr, he->h_addr, he->h_length);

    if (connect(s, (struct sockaddr*)&a, sizeof(a)) < 0) { closesocket(s); return v_string_ptr(_strdup("")); }

    char req[1024];
    snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
    send(s, req, (int)strlen(req), 0);

    size_t cap=8192, total=0;
    char *body = malloc(cap); body[0]=0;
    char chunk[4096];
    int in_body=0;

    while (1) {
        int n = recv(s, chunk, sizeof(chunk)-1, 0);
        if (n <= 0) break; chunk[n]=0;
        if (total+n+1 > cap) { cap*=2; body=realloc(body,cap); }
        memcpy(body+total, chunk, n); total+=n; body[total]=0;
        if (!in_body) {
            char *s2 = strstr(body, "\r\n\r\n");
            if (s2) { in_body=1; size_t off = (s2+4)-body; memmove(body,body+off,total-off); total-=off; body[total]=0; }
        }
    }
    closesocket(s);
    Value r = v_string_ptr(body);
    return r;
}

Value build_http(void) {
    Value m = v_map_new();
    Value fn; fn.type = V_NATIVE; fn.as.native = http_get;
    v_map_set(m.as.map, "get", fn);
    return m;
}
