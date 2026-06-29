// collections.c — std.collections: Queue, Stack, PriorityQueue, Counter
#include <stdlib.h>
#include <string.h>

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
static Value v_bool(int x)        { Value v; v.type = V_BOOL; v.as.b = x ? 1 : 0; return v; }
static Value v_string(const char *s) { Value v; v.type = V_STRING; v.as.s = s ? _strdup(s) : NULL; return v; }
static Value v_native(NativeFn fn){ Value v; v.type = V_NATIVE; v.as.native = fn; return v; }
static Value v_map_new(void) { Value v; v.type = V_MAP; VMap *m = calloc(1,sizeof(VMap)); v.as.map = m; return v; }
static Value v_list_new(void) { Value v; v.type = V_LIST; VList *l = calloc(1,sizeof(VList)); l->cap=8; l->items=calloc(l->cap,sizeof(Value)); v.as.list=l; return v; }
static void vm_set(VMap *m, const char *k, Value v) {
    int i; for (i = 0; i < m->len; i++) if (strcmp(m->keys[i],k)==0) { m->vals[i]=v; return; }
    if (m->len >= m->cap) { m->cap=m->cap?m->cap*2:8; m->keys=realloc(m->keys,sizeof(char*)*m->cap); m->vals=realloc(m->vals,sizeof(Value)*m->cap); }
    m->keys[m->len]=_strdup(k); m->vals[m->len]=v; m->len++;
}
static Value vm_get(VMap *m, const char *k) {
    int i; for (i = 0; i < m->len; i++) if (strcmp(m->keys[i],k)==0) return m->vals[i];
    return v_null();
}
static void vl_push(VList *l, Value v) {
    if (l->len >= l->cap) { l->cap*=2; l->items=realloc(l->items,l->cap*sizeof(Value)); }
    l->items[l->len++] = v;
}
static Value vl_pop(VList *l) { if (l->len <= 0) return v_null(); return l->items[--l->len]; }

/* Queue — FIFO */
static Value q_new(int argc, Value *argv) {
    Value q; VMap *m; (void)argc; (void)argv;
    q = v_map_new(); m = q.as.map;
    vm_set(m, "data",  v_list_new());
    vm_set(m, "head",  v_int(0));
    vm_set(m, "tail",  v_int(0));
    vm_set(m, "count", v_int(0));
    return q;
}
static Value q_push(int argc, Value *argv) {
    VMap *q; VList *data; long long count;
    if (argc < 2 || argv[0].type != V_MAP) return v_null();
    q = argv[0].as.map;
    data = vm_get(q, "data").as.list;
    vl_push(data, argv[1]);
    count = vm_get(q, "count").as.i + 1;
    vm_set(q, "count", v_int(count));
    return v_null();
}
static Value q_pop(int argc, Value *argv) {
    VMap *q; long long count, head; VList *data; Value result;
    if (argc < 1 || argv[0].type != V_MAP) return v_null();
    q = argv[0].as.map;
    count = vm_get(q, "count").as.i;
    if (count <= 0) return v_null();
    head = vm_get(q, "head").as.i;
    data = vm_get(q, "data").as.list;
    result = data->items[head];
    head = (head + 1) % data->cap;
    vm_set(q, "head", v_int(head));
    vm_set(q, "count", v_int(count - 1));
    return result;
}
static Value q_peek(int argc, Value *argv) {
    VMap *q; long long head; VList *data;
    if (argc < 1 || argv[0].type != V_MAP) return v_null();
    q = argv[0].as.map;
    if (vm_get(q, "count").as.i <= 0) return v_null();
    head = vm_get(q, "head").as.i;
    data = vm_get(q, "data").as.list;
    return data->items[head];
}
static Value q_size(int argc, Value *argv) {
    if (argc < 1 || argv[0].type != V_MAP) return v_int(0);
    return vm_get(argv[0].as.map, "count");
}
static Value q_isEmpty(int argc, Value *argv) { return v_bool(q_size(argc, argv).as.i == 0); }

/* Stack — LIFO */
static Value s_new(int argc, Value *argv) {
    Value s; VMap *m; (void)argc; (void)argv;
    s = v_map_new(); m = s.as.map;
    vm_set(m, "data", v_list_new());
    return s;
}
static Value s_push(int argc, Value *argv) {
    VMap *s; VList *data;
    if (argc < 2 || argv[0].type != V_MAP) return v_null();
    s = argv[0].as.map;
    data = vm_get(s, "data").as.list;
    vl_push(data, argv[1]);
    return v_null();
}
static Value s_pop(int argc, Value *argv) {
    VList *data;
    if (argc < 1 || argv[0].type != V_MAP) return v_null();
    data = vm_get(argv[0].as.map, "data").as.list;
    return vl_pop(data);
}
static Value s_peek(int argc, Value *argv) {
    VList *data;
    if (argc < 1 || argv[0].type != V_MAP) return v_null();
    data = vm_get(argv[0].as.map, "data").as.list;
    if (data->len <= 0) return v_null();
    return data->items[data->len - 1];
}
static Value s_size(int argc, Value *argv) {
    VList *data;
    if (argc < 1 || argv[0].type != V_MAP) return v_int(0);
    data = vm_get(argv[0].as.map, "data").as.list;
    return v_int(data->len);
}
static Value s_isEmpty(int argc, Value *argv) { return v_bool(s_size(argc, argv).as.i == 0); }

/* PriorityQueue — min-heap */
static void pq_sift_up(VList *data, int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (data->items[idx].as.i >= data->items[parent].as.i) break;
        { Value tmp = data->items[idx]; data->items[idx] = data->items[parent]; data->items[parent] = tmp; }
        idx = parent;
    }
}
static void pq_sift_down(VList *data, int idx, int len) {
    while (1) {
        int left = idx * 2 + 1, right = idx * 2 + 2, smallest = idx;
        if (left < len && data->items[left].as.i < data->items[smallest].as.i) smallest = left;
        if (right < len && data->items[right].as.i < data->items[smallest].as.i) smallest = right;
        if (smallest == idx) break;
        { Value tmp = data->items[idx]; data->items[idx] = data->items[smallest]; data->items[smallest] = tmp; }
        idx = smallest;
    }
}
static Value pq_new(int argc, Value *argv) {
    Value pq; VMap *m; (void)argc; (void)argv;
    pq = v_map_new(); m = pq.as.map;
    vm_set(m, "data", v_list_new());
    return pq;
}
static Value pq_push(int argc, Value *argv) {
    VMap *pq; VList *data; long long priority; Value item; VMap *im;
    if (argc < 2 || argv[0].type != V_MAP) return v_null();
    pq = argv[0].as.map;
    data = vm_get(pq, "data").as.list;
    priority = (argc > 2) ? argv[2].as.i : argv[1].as.i;
    item = v_map_new(); im = item.as.map;
    vm_set(im, "priority", v_int(priority));
    vm_set(im, "value", argv[1]);
    vl_push(data, item);
    pq_sift_up(data, data->len - 1);
    return v_null();
}
static Value pq_pop(int argc, Value *argv) {
    VList *data; Value result;
    if (argc < 1 || argv[0].type != V_MAP) return v_null();
    data = vm_get(argv[0].as.map, "data").as.list;
    if (data->len <= 0) return v_null();
    result = data->items[0];
    data->items[0] = data->items[data->len - 1];
    data->len--;
    if (data->len > 0) pq_sift_down(data, 0, data->len);
    return vm_get(result.as.map, "value");
}
static Value pq_peek(int argc, Value *argv) {
    VList *data;
    if (argc < 1 || argv[0].type != V_MAP) return v_null();
    data = vm_get(argv[0].as.map, "data").as.list;
    if (data->len <= 0) return v_null();
    return vm_get(data->items[0].as.map, "value");
}
static Value pq_size(int argc, Value *argv) {
    VList *data;
    if (argc < 1 || argv[0].type != V_MAP) return v_int(0);
    data = vm_get(argv[0].as.map, "data").as.list;
    return v_int(data->len);
}
static Value pq_isEmpty(int argc, Value *argv) { return v_bool(pq_size(argc, argv).as.i == 0); }

/* Counter */
static Value cnt_new(int argc, Value *argv) { (void)argc; (void)argv; return v_map_new(); }
static Value cnt_add(int argc, Value *argv) {
    VMap *cnt; const char *key; long long old; int i;
    if (argc < 2 || argv[0].type != V_MAP) return v_null();
    cnt = argv[0].as.map;
    key = (argv[1].type == V_STRING) ? argv[1].as.s : NULL;
    if (!key) return v_null();
    old = 0;
    for (i = 0; i < cnt->len; i++) if (strcmp(cnt->keys[i], key) == 0) old = cnt->vals[i].as.i;
    vm_set(cnt, key, v_int(old + 1));
    return v_null();
}
static Value cnt_get(int argc, Value *argv) {
    if (argc < 2 || argv[0].type != V_MAP) return v_int(0);
    return vm_get(argv[0].as.map, argv[1].as.s);
}
static Value cnt_mostCommon(int argc, Value *argv) {
    VMap *cnt; long long n; int i, j; Value result; VList *rl;
    typedef struct { char *k; long long v; } Entry;
    Entry *entries;
    if (argc < 1 || argv[0].type != V_MAP) return v_list_new();
    cnt = argv[0].as.map;
    n = (argc > 1) ? argv[1].as.i : cnt->len;
    if (n > cnt->len) n = cnt->len;
    result = v_list_new(); rl = result.as.list;
    entries = calloc(cnt->len, sizeof(Entry));
    for (i = 0; i < cnt->len; i++) { entries[i].k = cnt->keys[i]; entries[i].v = cnt->vals[i].as.i; }
    for (i = 0; i < cnt->len - 1; i++)
        for (j = 0; j < cnt->len - i - 1; j++)
            if (entries[j].v < entries[j+1].v) { Entry t = entries[j]; entries[j] = entries[j+1]; entries[j+1] = t; }
    for (i = 0; i < (int)n; i++) {
        Value pair; VList *pl;
        pair = v_list_new(); pl = pair.as.list;
        vl_push(pl, v_string(entries[i].k));
        vl_push(pl, v_int(entries[i].v));
        vl_push(rl, pair);
    }
    free(entries);
    return result;
}

/* Module */
Value build_collections(void) {
    Value mod; VMap *m;
    mod = v_map_new(); m = mod.as.map;
    vm_set(m, "Queue_new",     v_native(q_new));
    vm_set(m, "Queue_push",    v_native(q_push));
    vm_set(m, "Queue_pop",     v_native(q_pop));
    vm_set(m, "Queue_peek",    v_native(q_peek));
    vm_set(m, "Queue_size",    v_native(q_size));
    vm_set(m, "Queue_isEmpty", v_native(q_isEmpty));
    vm_set(m, "Stack_new",     v_native(s_new));
    vm_set(m, "Stack_push",    v_native(s_push));
    vm_set(m, "Stack_pop",     v_native(s_pop));
    vm_set(m, "Stack_peek",    v_native(s_peek));
    vm_set(m, "Stack_size",    v_native(s_size));
    vm_set(m, "Stack_isEmpty", v_native(s_isEmpty));
    vm_set(m, "PriorityQueue_new",     v_native(pq_new));
    vm_set(m, "PriorityQueue_push",    v_native(pq_push));
    vm_set(m, "PriorityQueue_pop",     v_native(pq_pop));
    vm_set(m, "PriorityQueue_peek",    v_native(pq_peek));
    vm_set(m, "PriorityQueue_size",    v_native(pq_size));
    vm_set(m, "PriorityQueue_isEmpty", v_native(pq_isEmpty));
    vm_set(m, "Counter_new",        v_native(cnt_new));
    vm_set(m, "Counter_add",        v_native(cnt_add));
    vm_set(m, "Counter_get",        v_native(cnt_get));
    vm_set(m, "Counter_mostCommon", v_native(cnt_mostCommon));
    return mod;
}
