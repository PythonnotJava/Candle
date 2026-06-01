#ifndef CANDLE_STD_IO_H
#define CANDLE_STD_IO_H

#include "candle_runtime.h"

static inline candle_string io_readline(void) {
    char *buf = GC_MALLOC(1024);
    if (!fgets(buf, 1024, stdin)) return "";
    size_t n = strlen(buf);
    if (n && buf[n-1] == '\n') buf[n-1] = 0;
    return buf;
}

static inline void io_write(candle_string s) { fputs(s ? s : "", stdout); }
static inline void io_writeln(candle_string s) { puts(s ? s : ""); }

static inline candle_string io_readFile(candle_string path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = GC_MALLOC(n + 1);
    fread(buf, 1, n, f);
    buf[n] = 0;
    fclose(f);
    return buf;
}

static inline candle_bool io_writeFile(candle_string path, candle_string content) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fputs(content ? content : "", f);
    fclose(f);
    return 1;
}

#endif
