// crypto.c — std.crypto (SHA256, HMAC-SHA256, UUID4, CRC32, Base64)
// Pure C89, MSVC compatible
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

typedef enum { V_NULL, V_INT, V_DOUBLE, V_BOOL, V_STRING, V_LIST, V_TUPLE,
               V_MAP, V_FN, V_LAMBDA, V_CLASS, V_OBJECT, V_NATIVE } ValueType;
typedef struct Value Value;
typedef Value (*NativeFn)(int argc, Value *argv);
struct Value { ValueType type; union { long long i; double d; int b;
    char *s; void *list; void *map; void *fn; void *cls; void *obj; NativeFn native; } as; };
typedef struct { char **keys; Value *vals; int len, cap; } VMap;
typedef struct { Value *items; int len, cap; } VList;

static Value v_int(long long x)   { Value v; v.type = V_INT; v.as.i = x; return v; }
static Value v_null(void)         { Value v; v.type = V_NULL; return v; }
static Value v_string(const char *s) { Value v; v.type = V_STRING; v.as.s = s ? _strdup(s) : NULL; return v; }
static Value v_native(NativeFn fn){ Value v; v.type = V_NATIVE; v.as.native = fn; return v; }
static Value v_map_new(void) { Value v; v.type = V_MAP; VMap *m = calloc(1,sizeof(VMap)); v.as.map = m; return v; }
static Value v_list_new(void) { Value v; v.type = V_LIST; VList *l = calloc(1,sizeof(VList)); l->cap=8; l->items=calloc(l->cap,sizeof(Value)); v.as.list=l; return v; }
static void vm_set(VMap *m, const char *k, Value v) {
    int i; for (i = 0; i < m->len; i++) if (strcmp(m->keys[i],k)==0) { m->vals[i]=v; return; }
    if (m->len >= m->cap) { m->cap=m->cap?m->cap*2:8; m->keys=realloc(m->keys,sizeof(char*)*m->cap); m->vals=realloc(m->vals,sizeof(Value)*m->cap); }
    m->keys[m->len]=_strdup(k); m->vals[m->len]=v; m->len++;
}
static void vl_push(VList *l, Value v) {
    if (l->len >= l->cap) { l->cap*=2; l->items=realloc(l->items,l->cap*sizeof(Value)); }
    l->items[l->len++] = v;
}

/* ═══════════════════════════════════════════════════════════════
   SHA-256 (RFC 6234) — zero-dependency, MSVC-safe C89
   ═══════════════════════════════════════════════════════════════ */
typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t  buffer[64];
} SHA256_CTX;

static uint32_t sha256_ror32(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static uint32_t sha256_ch(uint32_t x, uint32_t y, uint32_t z)  { return (x & y) ^ ((~x) & z); }
static uint32_t sha256_maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
static uint32_t sha256_bsig0(uint32_t x) { return sha256_ror32(x, 2) ^ sha256_ror32(x, 13) ^ sha256_ror32(x, 22); }
static uint32_t sha256_bsig1(uint32_t x) { return sha256_ror32(x, 6) ^ sha256_ror32(x, 11) ^ sha256_ror32(x, 25); }
static uint32_t sha256_ssig0(uint32_t x) { return sha256_ror32(x, 7) ^ sha256_ror32(x, 18) ^ (x >> 3); }
static uint32_t sha256_ssig1(uint32_t x) { return sha256_ror32(x, 17) ^ sha256_ror32(x, 19) ^ (x >> 10); }

static const uint32_t sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static void sha256_init(SHA256_CTX *ctx) {
    ctx->count = 0;
    ctx->state[0]=0x6a09e667;ctx->state[1]=0xbb67ae85;ctx->state[2]=0x3c6ef372;ctx->state[3]=0xa54ff53a;
    ctx->state[4]=0x510e527f;ctx->state[5]=0x9b05688c;ctx->state[6]=0x1f83d9ab;ctx->state[7]=0x5be0cd19;
}

static void sha256_transform(uint32_t state[8], const uint8_t data[64]) {
    uint32_t a,b,c,d,e,f,g,h,t1,t2,m[64];
    int i,j;
    for (i=0,j=0;i<16;i++,j+=4)
        m[i] = ((uint32_t)data[j]<<24)|((uint32_t)data[j+1]<<16)|((uint32_t)data[j+2]<<8)|(uint32_t)data[j+3];
    for (i=16;i<64;i++)
        m[i]=sha256_ssig1(m[i-2])+m[i-7]+sha256_ssig0(m[i-15])+m[i-16];
    a=state[0];b=state[1];c=state[2];d=state[3];e=state[4];f=state[5];g=state[6];h=state[7];
    for (i=0;i<64;i++) {
        t1=h+sha256_bsig1(e)+sha256_ch(e,f,g)+sha256_k[i]+m[i];
        t2=sha256_bsig0(a)+sha256_maj(a,b,c);
        h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    state[0]+=a;state[1]+=b;state[2]+=c;state[3]+=d;state[4]+=e;state[5]+=f;state[6]+=g;state[7]+=h;
}

static void sha256_update(SHA256_CTX *ctx, const uint8_t *data, size_t len) {
    size_t i;
    for (i = 0; i < len; i++) {
        ctx->buffer[ctx->count % 64] = data[i];
        if (++ctx->count % 64 == 0) sha256_transform(ctx->state, ctx->buffer);
    }
}

static void sha256_final(uint8_t digest[32], SHA256_CTX *ctx) {
    uint64_t bits;
    int pad,i;
    uint8_t padding[128];
    uint8_t bitbuf[8];
    bits = ctx->count * 8;
    pad = (ctx->count % 64 < 56) ? (56 - (int)(ctx->count % 64)) : (120 - (int)(ctx->count % 64));
    memset(padding, 0, (size_t)pad); padding[0] = 0x80;
    sha256_update(ctx, padding, pad);
    for (i = 0; i < 8; i++) bitbuf[i] = (uint8_t)(bits >> (56 - i*8));
    sha256_update(ctx, bitbuf, 8);
    for (i = 0; i < 8; i++) {
        digest[i*4]   = (uint8_t)(ctx->state[i] >> 24);
        digest[i*4+1] = (uint8_t)(ctx->state[i] >> 16);
        digest[i*4+2] = (uint8_t)(ctx->state[i] >> 8);
        digest[i*4+3] = (uint8_t)(ctx->state[i]);
    }
}

/* ═══════════════════════════════════════════════════════════════
   API wrappers
   ═══════════════════════════════════════════════════════════════ */

static Value to_hex_list(const uint8_t *bytes, int len) {
    Value result; VList *rl; int i;
    result = v_list_new(); rl = result.as.list;
    for (i = 0; i < len; i++) vl_push(rl, v_int(bytes[i]));
    return result;
}

static Value crypto_sha256(int argc, Value *argv) {
    char hex[65]; int i; SHA256_CTX ctx; uint8_t digest[32];
    if (argc < 1 || argv[0].type != V_STRING) return v_string("");
    sha256_init(&ctx); sha256_update(&ctx, (const uint8_t*)argv[0].as.s, strlen(argv[0].as.s));
    sha256_final(digest, &ctx);
    for (i = 0; i < 32; i++) sprintf(hex + i*2, "%02x", digest[i]); hex[64]=0;
    return v_string(hex);
}

static Value crypto_sha256_bytes(int argc, Value *argv) {
    SHA256_CTX ctx; uint8_t digest[32];
    if (argc < 1 || argv[0].type != V_STRING) return v_list_new();
    sha256_init(&ctx); sha256_update(&ctx, (const uint8_t*)argv[0].as.s, strlen(argv[0].as.s));
    sha256_final(digest, &ctx);
    return to_hex_list(digest, 32);
}

static Value crypto_hmac_sha256(int argc, Value *argv) {
    const char *key_s, *msg;
    size_t key_len;
    uint8_t key[64], o_key_pad[64], i_key_pad[64], digest[32];
    SHA256_CTX ctx; char hex[65]; int i;

    if (argc < 2 || argv[0].type != V_STRING || argv[1].type != V_STRING) return v_string("");
    key_s = argv[0].as.s; msg = argv[1].as.s;
    if (!key_s || !msg) return v_string("");
    key_len = strlen(key_s);
    memset(key, 0, 64);
    if (key_len <= 64) memcpy(key, key_s, key_len);
    else { sha256_init(&ctx); sha256_update(&ctx, (const uint8_t*)key_s, key_len); sha256_final(key, &ctx); }

    for (i = 0; i < 64; i++) { o_key_pad[i] = key[i] ^ 0x5c; i_key_pad[i] = key[i] ^ 0x36; }

    sha256_init(&ctx); sha256_update(&ctx, i_key_pad, 64); sha256_update(&ctx, (const uint8_t*)msg, strlen(msg));
    sha256_final(digest, &ctx);
    sha256_init(&ctx); sha256_update(&ctx, o_key_pad, 64); sha256_update(&ctx, digest, 32);
    sha256_final(digest, &ctx);

    for (i = 0; i < 32; i++) sprintf(hex + i*2, "%02x", digest[i]); hex[64]=0;
    return v_string(hex);
}

/* ═══════════════════════════════════════════════════════════════
   UUID v4
   ═══════════════════════════════════════════════════════════════ */
static Value crypto_uuid4(int argc, Value *argv) {
    uint8_t bytes[16]; char buf[37]; int i;
    (void)argc; (void)argv;
    for (i = 0; i < 16; i++) bytes[i] = (uint8_t)(rand() & 0xFF);
    bytes[6] = (bytes[6] & 0x0F) | 0x40;
    bytes[8] = (bytes[8] & 0x3F) | 0x80;
    sprintf(buf, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7],
            bytes[8],bytes[9],bytes[10],bytes[11],bytes[12],bytes[13],bytes[14],bytes[15]);
    return v_string(buf);
}

/* ═══════════════════════════════════════════════════════════════
   CRC32
   ═══════════════════════════════════════════════════════════════ */
static Value crypto_crc32(int argc, Value *argv) {
    uint32_t crc; size_t i; int j;
    if (argc < 1 || argv[0].type != V_STRING) return v_int(0);
    crc = 0xFFFFFFFF;
    for (i = 0; argv[0].as.s[i]; i++) {
        crc ^= (unsigned char)argv[0].as.s[i];
        for (j = 0; j < 8; j++) crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
    }
    return v_int((long long)(crc ^ 0xFFFFFFFF));
}

/* ═══════════════════════════════════════════════════════════════
   Base64
   ═══════════════════════════════════════════════════════════════ */
static Value crypto_base64_encode(int argc, Value *argv) {
    const char *s, *tbl; size_t i, len, j; char *buf; Value r;
    if (argc < 1 || argv[0].type != V_STRING) return v_string("");
    s = argv[0].as.s; if (!s) return v_string("");
    len = strlen(s); tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    buf = malloc(((len + 2) / 3) * 4 + 1); j = 0;
    for (i = 0; i < len; i += 3) {
        uint32_t triple = ((uint32_t)(unsigned char)s[i]) << 16;
        if (i+1 < len) triple |= ((uint32_t)(unsigned char)s[i+1]) << 8;
        if (i+2 < len) triple |= (uint32_t)(unsigned char)s[i+2];
        buf[j++] = tbl[(triple >> 18) & 0x3F]; buf[j++] = tbl[(triple >> 12) & 0x3F];
        buf[j++] = (i+1 < len) ? tbl[(triple >> 6) & 0x3F] : '=';
        buf[j++] = (i+2 < len) ? tbl[triple & 0x3F] : '=';
    }
    buf[j]=0; r=v_string(buf); free(buf); return r;
}

static Value crypto_base64_decode(int argc, Value *argv) {
    const char *s; size_t slen, i, j; char *buf; uint32_t acc; int bits; Value r;
    if (argc < 1 || argv[0].type != V_STRING) return v_string("");
    s = argv[0].as.s; if (!s) return v_string("");
    slen = strlen(s); buf = malloc(slen + 1); j = 0; acc = 0; bits = 0;
    for (i = 0; i < slen; i++) {
        int v = -1; if (s[i] == '=') break;
        if (s[i] >= 'A' && s[i] <= 'Z') v = s[i] - 'A';
        else if (s[i] >= 'a' && s[i] <= 'z') v = s[i] - 'a' + 26;
        else if (s[i] >= '0' && s[i] <= '9') v = s[i] - '0' + 52;
        else if (s[i] == '+') v = 62;
        else if (s[i] == '/') v = 63;
        if (v < 0) continue;
        acc = (acc << 6) | (uint32_t)v; bits += 6;
        if (bits >= 8) { bits -= 8; buf[j++] = (char)((acc >> bits) & 0xFF); }
    }
    buf[j]=0; r=v_string(buf); free(buf); return r;
}

/* ═══════════════════════════════════════════════════════════════
   Module
   ═══════════════════════════════════════════════════════════════ */
Value build_crypto(void) {
    Value mod; VMap *m;
    mod = v_map_new(); m = mod.as.map;
    vm_set(m, "sha256",       v_native(crypto_sha256));
    vm_set(m, "sha256Bytes",  v_native(crypto_sha256_bytes));
    vm_set(m, "hmacSha256",   v_native(crypto_hmac_sha256));
    vm_set(m, "uuid4",        v_native(crypto_uuid4));
    vm_set(m, "crc32",        v_native(crypto_crc32));
    vm_set(m, "base64Encode", v_native(crypto_base64_encode));
    vm_set(m, "base64Decode", v_native(crypto_base64_decode));
    return mod;
}
