#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "error: cannot open file '%s'\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = (char *)malloc(size + 1);
    if (!buf) {
        fprintf(stderr, "error: out of memory\n");
        fclose(f);
        return NULL;
    }

    size_t read = fread(buf, 1, size, f);
    buf[read] = '\0';
    fclose(f);
    return buf;
}

void error_at(const char *filename, int line, int column, const char *fmt, ...) {
    fprintf(stderr, "%s:%d:%d: error: ", filename, line, column);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}
