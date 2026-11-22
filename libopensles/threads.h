#ifndef OPENSLES_THREADS_H
#define OPENSLES_THREADS_H

#include <stdint.h>

#if defined(HAVE_SCE_THREADS) && defined(HAVE_PTHREAD)
#error "Select either HAVE_SCE_THREADS or HAVE_PTHREAD, not both"
#endif

#ifdef HAVE_SCE_THREADS
#include <psp2/kernel/threadmgr.h>
typedef SceKernelLwMutexWork sles_mutex_t;
typedef SceUID sles_thread_t;
typedef struct {
    SceKernelLwCondWork native;
    sles_mutex_t *mutex;
} sles_cond_t;
#else
#include <pthread.h>
typedef pthread_mutex_t sles_mutex_t;
typedef pthread_t sles_thread_t;
typedef struct {
    pthread_cond_t native;
    sles_mutex_t *mutex;
} sles_cond_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

int sles_mutex_init(sles_mutex_t *mutex);
int sles_mutex_destroy(sles_mutex_t *mutex);
int sles_mutex_lock(sles_mutex_t *mutex);
int sles_mutex_trylock(sles_mutex_t *mutex);
int sles_mutex_unlock(sles_mutex_t *mutex);
int sles_cond_init(sles_cond_t *cond, sles_mutex_t *mutex);
int sles_cond_destroy(sles_cond_t *cond);
int sles_cond_wait(sles_cond_t *cond, sles_mutex_t *mutex);
int sles_cond_timedwait(sles_cond_t *cond, sles_mutex_t *mutex, uint32_t timeout_us);
int sles_cond_signal(sles_cond_t *cond);
int sles_cond_broadcast(sles_cond_t *cond);
int sles_thread_create(sles_thread_t *thread, void *(*entry)(void *), void *arg);
int sles_thread_join(sles_thread_t thread);
sles_thread_t sles_thread_self(void);
int sles_thread_equal(sles_thread_t a, sles_thread_t b);

#ifdef __cplusplus
}
#endif
#endif
