#ifndef CANDLE_NETWORK_HTTP_H
#define CANDLE_NETWORK_HTTP_H

#include "../candle_runtime.h"
#include "../vendor/mongoose/mongoose.h"

typedef struct {
    candle_string body;
    candle_int status;
    candle_string headers;
} HttpResponse;

typedef struct {
    HttpResponse *resp;
    int done;
} CandleHttpCtx;

static void candle_http_ev(struct mg_connection *c, int ev, void *ev_data) {
    CandleHttpCtx *ctx = (CandleHttpCtx *)c->fn_data;
    if (ev == MG_EV_CONNECT) {
        struct mg_str host = mg_url_host(*(const char **)c->data);
        (void)host;
    } else if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        char *body = GC_MALLOC(hm->body.len + 1);
        memcpy(body, hm->body.buf, hm->body.len);
        body[hm->body.len] = 0;
        ctx->resp->body = body;
        ctx->resp->status = mg_http_status(hm);
        ctx->done = 1;
        c->is_draining = 1;
    } else if (ev == MG_EV_ERROR) {
        ctx->resp->status = -1;
        ctx->done = 1;
    } else if (ev == MG_EV_CLOSE) {
        ctx->done = 1;
    }
}

static inline HttpResponse *http_get(candle_string url) {
    HttpResponse *r = GC_MALLOC(sizeof(HttpResponse));
    r->body = "";
    r->status = 0;
    r->headers = "";

    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    CandleHttpCtx ctx = { r, 0 };
    struct mg_connection *c = mg_http_connect(&mgr, url, candle_http_ev, &ctx);
    if (c) {
        struct mg_str host = mg_url_host(url);
        mg_printf(c, "GET %s HTTP/1.0\r\nHost: %.*s\r\n\r\n",
                  mg_url_uri(url), (int)host.len, host.buf);
    } else {
        r->status = -1;
        ctx.done = 1;
    }
    int timeout = 100;
    while (!ctx.done && timeout-- > 0) mg_mgr_poll(&mgr, 100);
    mg_mgr_free(&mgr);
    return r;
}

#endif
