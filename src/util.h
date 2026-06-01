#ifndef CANDLE_UTIL_H
#define CANDLE_UTIL_H

#include <stddef.h>
#include <stdarg.h>

char *read_file(const char *path);
void error_at(const char *filename, int line, int column, const char *fmt, ...);

#endif
