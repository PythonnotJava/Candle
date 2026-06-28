#ifndef CANDLE_THREADING_H
#define CANDLE_THREADING_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── Platform detection ─────────────────────────────────────────────────── */

#if defined(_WIN32) || defined(_WIN64)
  #define CANDLE_PLATFORM_WINDOWS
#else
  #define CANDLE_PLATFORM_POSIX
  #include <pthread.h>
#endif

/* ── Opaque types ───────────────────────────────────────────────────────── */

typedef struct thread_s thread_t;
typedef struct mutex_s mutex_t;
typedef struct cond_s cond_t;

#ifdef CANDLE_PLATFORM_POSIX
/* POSIX: define structs inline (pthread.h already included) */
struct thread_s {
    pthread_t handle;
    int joined;
    void (*fn)(void*);
    void *arg;
};
struct mutex_s { pthread_mutex_t m; };
struct cond_s  { pthread_cond_t c; };
#endif
/* Windows: structs defined in threading.c to avoid <windows.h> TokenType clash */

/* ── Thread ─────────────────────────────────────────────────────────────── */

thread_t *thread_create(void (*fn)(void*), void *arg);
void thread_join(thread_t *t);
void thread_destroy(thread_t *t);

/* ── Mutex ──────────────────────────────────────────────────────────────── */

mutex_t *mutex_create(void);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);
void mutex_destroy(mutex_t *m);

/* ── Condition variable ─────────────────────────────────────────────────── */

cond_t *cond_create(void);
void cond_wait(cond_t *c, mutex_t *m);
void cond_signal(cond_t *c);
void cond_broadcast(cond_t *c);
void cond_destroy(cond_t *c);

/* ── GC integration ─────────────────────────────────────────────────────── */

void thread_gc_register(void);
void thread_gc_unregister(void);

#ifdef __cplusplus
}
#endif

#endif /* CANDLE_THREADING_H */
