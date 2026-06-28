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
// ══════════════════════════════════════════════════════════════════════════════

static int g_ws_init = 0;
static void ensure_winsock(void) {
#ifdef _WIN32
    if (!g_ws_init) { WSADATA w; WSAStartup(MAKEWORD(2,2), &w); g_ws_init = 1; }
#endif
}

/* ── shared helper: parse host/port/path, connect, return socket ──────────── */
static int http_connect(const char *url, char *host_out, size_t hostsz,
                        char *path_out, size_t pathsz, int *port_out) {
    int is_https = (url[0] == 'h' && url[1] == 't' && url[2] == 't' && url[3] == 'p' && url[4] == 's');
    const char *hs = (url[0] == 'h' && url[4] == ':') ? url + 7 : url;
    if (is_https) { hs = url + 8; }  /* strip "https://" */

    const char *ps = strchr(hs, '/');
    const char *cs = strchr(hs, ':');
    int port = is_https ? 443 : 80;

    if (cs && (!ps || cs < ps)) {
        size_t n = cs - hs; if (n >= hostsz) n = hostsz-1;
        memcpy(host_out, hs, n); host_out[n] = 0;
        port = atoi(cs + 1);
    } else if (ps) {
        size_t n = ps - hs; if (n >= hostsz) n = hostsz-1;
        memcpy(host_out, hs, n); host_out[n] = 0;
    } else {
        size_t n = strlen(hs); if (n >= hostsz) n = hostsz-1;
        memcpy(host_out, hs, n); host_out[n] = 0;
    }
    if (ps) { size_t n = strlen(ps); if (n >= pathsz) n = pathsz-1; memcpy(path_out, ps, n); path_out[n] = 0; }
    else { path_out[0] = '/'; path_out[1] = 0; }

    *port_out = port;
    struct hostent *he = gethostbyname(host_out);
    if (!he) return -1;

    int s = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;

    struct sockaddr_in a; memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET; a.sin_port = htons((u_short)port);
    memcpy(&a.sin_addr, he->h_addr, he->h_length);

    if (connect(s, (struct sockaddr*)&a, sizeof(a)) < 0) { closesocket(s); return -1; }
    return s;
}

/* ── shared: read response, strip headers, return body ──────────────────── */
static Value http_read_body(int s) {
    size_t cap = 8192, total = 0;
    char *body = malloc(cap); body[0] = 0;
    char chunk[4096];
    int in_body = 0;

    while (1) {
        int n = recv(s, chunk, sizeof(chunk) - 1, 0);
        if (n <= 0) break; chunk[n] = 0;
        if (total + n + 1 > cap) { cap *= 2; body = realloc(body, cap); }
        memcpy(body + total, chunk, n); total += n; body[total] = 0;
        if (!in_body) {
            char *s2 = strstr(body, "\r\n\r\n");
            if (s2) { in_body = 1; size_t off = (s2 + 4) - body; memmove(body, body + off, total - off); total -= off; body[total] = 0; }
        }
    }
    closesocket(s);
    Value r = v_string_ptr(body);
    return r;
}

/* ── HTTP GET ─────────────────────────────────────────────────────────────── */
static Value http_get(int argc, Value *argv) {
    if (argc < 1 || argv[0].type != V_STRING || !argv[0].as.s)
        return v_string_ptr(_strdup(""));
    ensure_winsock();

    char host[256] = {0}, path[512] = "/"; int port;
    int s = http_connect(argv[0].as.s, host, sizeof(host), path, sizeof(path), &port);
    if (s < 0) return v_string_ptr(_strdup(""));

    char req[1024];
    snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
    send(s, req, (int)strlen(req), 0);

    return http_read_body(s);
}

/* ── HTTP POST ────────────────────────────────────────────────────────────── */
static Value http_post(int argc, Value *argv) {
    if (argc < 2 || argv[0].type != V_STRING || !argv[0].as.s)
        return v_string_ptr(_strdup(""));
    ensure_winsock();

    const char *body = (argv[1].type == V_STRING && argv[1].as.s) ? argv[1].as.s : "";
    const char *auth = (argc >= 3 && argv[2].type == V_STRING && argv[2].as.s) ? argv[2].as.s : NULL;

    char host[256] = {0}, path[512] = "/"; int port;
    int s = http_connect(argv[0].as.s, host, sizeof(host), path, sizeof(path), &port);
    if (s < 0) return v_string_ptr(_strdup(""));

    size_t blen = strlen(body);
    size_t reqcap = 4096 + blen;
    char *req = malloc(reqcap);
    char *wp = req;

    wp += snprintf(wp, reqcap - (wp - req),
        "POST %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n"
        "Content-Type: application/json\r\nContent-Length: %zu\r\n",
        path, host, blen);
    if (auth) wp += snprintf(wp, reqcap - (wp - req),
        "Authorization: Bearer %s\r\n", auth);
    wp += snprintf(wp, reqcap - (wp - req), "\r\n");
    memcpy(wp, body, blen);

    size_t total_req = (wp - req) + blen;
    send(s, req, (int)total_req, 0);
    free(req);

    return http_read_body(s);
}

/* ── Module entry ─────────────────────────────────────────────────────────── */
Value build_http(void) {
    Value m = v_map_new();
    Value g; g.type = V_NATIVE; g.as.native = http_get;
    Value p; p.type = V_NATIVE; p.as.native = http_post;
    v_map_set(m.as.map, "get",  g);
    v_map_set(m.as.map, "post", p);
    return m;
}
