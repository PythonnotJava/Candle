#ifndef CANDLE_STD_COLLECTIONS_H
#define CANDLE_STD_COLLECTIONS_H

#include "candle_runtime.h"

static inline candle_int collections_size(CandleList *l) { return l ? l->len : 0; }
static inline void collections_push(CandleList *l, candle_int v) {
    if (l->len >= l->cap) {
        l->cap = l->cap ? l->cap * 2 : 8;
        candle_int *nd = GC_MALLOC(sizeof(candle_int) * l->cap);
        for (int i = 0; i < l->len; i++) nd[i] = l->data[i];
        l->data = nd;
    }
    l->data[l->len++] = v;
}
static inline candle_int collections_pop(CandleList *l) {
    return l->len > 0 ? l->data[--l->len] : 0;
}

#endif
