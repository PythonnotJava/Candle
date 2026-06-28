#include "threading.h"
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Windows implementation — <windows.h> is included ONLY here
 * ========================================================================= */
#ifdef CANDLE_PLATFORM_WINDOWS

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>

struct thread_s { HANDLE handle; void (*fn)(void*); void *arg; };
struct mutex_s { CRITICAL_SECTION cs; };
struct cond_s  { CONDITION_VARIABLE cv; };

static DWORD WINAPI thread_proc(LPVOID arg) {
    thread_t *t = (thread_t*)arg;
    t->fn(t->arg);
    return 0;
}

thread_t *thread_create(void (*fn)(void*), void *arg) {
    thread_t *t = (thread_t*)calloc(1, sizeof(thread_t));
    if (!t) return NULL;
    t->fn = fn; t->arg = arg;
    t->handle = CreateThread(NULL, 0, thread_proc, t, 0, NULL);
    if (!t->handle) { free(t); return NULL; }
    return t;
}

void thread_join(thread_t *t) {
    if (!t || !t->handle) return;
    WaitForSingleObject(t->handle, INFINITE);
    CloseHandle(t->handle); t->handle = NULL;
}

void thread_destroy(thread_t *t) {
    if (!t) return;
    if (t->handle) CloseHandle(t->handle);
    free(t);
}

mutex_t *mutex_create(void) {
    mutex_t *m = (mutex_t*)calloc(1, sizeof(mutex_t));
    if (!m) return NULL;
    InitializeCriticalSection(&m->cs); return m;
}
void mutex_lock(mutex_t *m)   { if (m) EnterCriticalSection(&m->cs); }
void mutex_unlock(mutex_t *m) { if (m) LeaveCriticalSection(&m->cs); }
void mutex_destroy(mutex_t *m){ if (m) { DeleteCriticalSection(&m->cs); free(m); } }

cond_t *cond_create(void) {
    cond_t *c = (cond_t*)calloc(1, sizeof(cond_t));
    if (!c) return NULL;
    InitializeConditionVariable(&c->cv); return c;
}
void cond_wait(cond_t *c, mutex_t *m) { if (c&&m) SleepConditionVariableCS(&c->cv,&m->cs,INFINITE); }
void cond_signal(cond_t *c)    { if (c) WakeConditionVariable(&c->cv); }
void cond_broadcast(cond_t *c) { if (c) WakeAllConditionVariable(&c->cv); }
void cond_destroy(cond_t *c)   { free(c); }

/* Boehm GC enable_threads_discovery=ON auto-detects Win32 threads. No manual registration. */
void thread_gc_register(void)   {}
void thread_gc_unregister(void) {}

/* =========================================================================
 * POSIX implementation (Linux / macOS / Android)
 * ========================================================================= */
#else

#ifdef CANDLE_USE_BOEHM_GC
#include <gc.h>
#else
#include <pthread.h>
#endif

static void *thread_proc(void *arg) {
    thread_t *t = (thread_t*)arg;
    t->fn(t->arg);
    return NULL;
}

thread_t *thread_create(void (*fn)(void*), void *arg) {
    thread_t *t = (thread_t*)calloc(1, sizeof(thread_t));
    if (!t) return NULL;
    t->fn = fn; t->arg = arg;
#ifdef CANDLE_USE_BOEHM_GC
    if (GC_pthread_create(&t->handle, NULL, thread_proc, t) != 0) { free(t); return NULL; }
#else
    if (pthread_create(&t->handle, NULL, thread_proc, t) != 0) { free(t); return NULL; }
#endif
    return t;
}

void thread_join(thread_t *t) {
    if (!t || t->joined) return;
    pthread_join(t->handle, NULL); t->joined = 1;
}

void thread_destroy(thread_t *t) { free(t); }

mutex_t *mutex_create(void) {
    mutex_t *m = (mutex_t*)calloc(1, sizeof(mutex_t));
    if (!m) return NULL;
    pthread_mutex_init(&m->m, NULL); return m;
}
void mutex_lock(mutex_t *m)   { if (m) pthread_mutex_lock(&m->m); }
void mutex_unlock(mutex_t *m) { if (m) pthread_mutex_unlock(&m->m); }
void mutex_destroy(mutex_t *m){ if (m) { pthread_mutex_destroy(&m->m); free(m); } }

cond_t *cond_create(void) {
    cond_t *c = (cond_t*)calloc(1, sizeof(cond_t));
    if (!c) return NULL;
    pthread_cond_init(&c->c, NULL); return c;
}
void cond_wait(cond_t *c, mutex_t *m) { if (c&&m) pthread_cond_wait(&c->c, &m->m); }
void cond_signal(cond_t *c)    { if (c) pthread_cond_signal(&c->c); }
void cond_broadcast(cond_t *c) { if (c) pthread_cond_broadcast(&c->c); }
void cond_destroy(cond_t *c)   { if (c) { pthread_cond_destroy(&c->c); free(c); } }

void thread_gc_register(void)   {}
void thread_gc_unregister(void) {}

#endif /* CANDLE_PLATFORM_WINDOWS */
