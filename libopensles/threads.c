#include "threads.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>

#ifdef HAVE_SCE_THREADS
#include <psp2/kernel/error.h>

#ifndef OPENSLES_THREAD_STACK_SIZE
#define OPENSLES_THREAD_STACK_SIZE (512 * 1024)
#endif
#ifndef OPENSLES_THREAD_PRIORITY
#define OPENSLES_THREAD_PRIORITY 0x10000100
#endif

static int sce_result(int result)
{
    if (result >= 0) {
        return 0;
    }
    switch ((uint32_t)result) {
    case SCE_KERNEL_ERROR_NO_MEMORY:
        return ENOMEM;
    case SCE_KERNEL_ERROR_WAIT_TIMEOUT:
        return ETIMEDOUT;
    case SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN:
        return EBUSY;
    case SCE_KERNEL_ERROR_LW_MUTEX_RECURSIVE:
        return EDEADLK;
    case SCE_KERNEL_ERROR_LW_MUTEX_NOT_OWNED:
    case SCE_KERNEL_ERROR_LW_MUTEX_UNLOCK_UDF:
        return EPERM;
    default:
        return EIO;
    }
}

int sles_mutex_init(sles_mutex_t *mutex)
{
    return sce_result(sceKernelCreateLwMutex(mutex, "OpenSLES mutex", 0, 0, NULL));
}

int sles_mutex_destroy(sles_mutex_t *mutex)
{
    return sce_result(sceKernelDeleteLwMutex(mutex));
}

int sles_mutex_lock(sles_mutex_t *mutex)
{
    return sce_result(sceKernelLockLwMutex(mutex, 1, NULL));
}

int sles_mutex_trylock(sles_mutex_t *mutex)
{
    int result = sce_result(sceKernelTryLockLwMutex(mutex, 1));
    return result == EDEADLK ? EBUSY : result;
}

int sles_mutex_unlock(sles_mutex_t *mutex)
{
    return sce_result(sceKernelUnlockLwMutex(mutex, 1));
}

int sles_cond_init(sles_cond_t *cond, sles_mutex_t *mutex)
{
    if (cond == NULL || mutex == NULL) {
        return EINVAL;
    }
    int result = sce_result(sceKernelCreateLwCond(&cond->native,
        "OpenSLES condition", 0, mutex, NULL));
    cond->mutex = result == 0 ? mutex : NULL;
    return result;
}

int sles_cond_destroy(sles_cond_t *cond)
{
    return sce_result(sceKernelDeleteLwCond(&cond->native));
}

int sles_cond_wait(sles_cond_t *cond, sles_mutex_t *mutex)
{
    if (cond->mutex != mutex) {
        return EINVAL;
    }
    return sce_result(sceKernelWaitLwCond(&cond->native, NULL));
}

int sles_cond_timedwait(sles_cond_t *cond, sles_mutex_t *mutex, uint32_t timeout_us)
{
    if (cond->mutex != mutex) {
        return EINVAL;
    }
    unsigned int remaining = timeout_us;
    return sce_result(sceKernelWaitLwCond(&cond->native, &remaining));
}

int sles_cond_signal(sles_cond_t *cond)
{
    return sce_result(sceKernelSignalLwCond(&cond->native));
}

int sles_cond_broadcast(sles_cond_t *cond)
{
    return sce_result(sceKernelSignalLwCondAll(&cond->native));
}

typedef struct {
    void *(*entry)(void *);
    void *arg;
} ThreadStart;

static int thread_start(SceSize size, void *arg)
{
    if (size != sizeof(ThreadStart) || arg == NULL) {
        return EINVAL;
    }
    ThreadStart start;
    memcpy(&start, arg, sizeof(start));
    start.entry(start.arg);
    return 0;
}

int sles_thread_create(sles_thread_t *thread, void *(*entry)(void *), void *arg)
{
    if (thread == NULL || entry == NULL) {
        return EINVAL;
    }
    SceUID id = sceKernelCreateThread("OpenSLES worker", thread_start,
        OPENSLES_THREAD_PRIORITY, OPENSLES_THREAD_STACK_SIZE, 0, 0, NULL);
    if (id < 0) {
        return sce_result(id);
    }
    ThreadStart start = {entry, arg};
    int result = sceKernelStartThread(id, sizeof(start), &start);
    if (result < 0) {
        sceKernelDeleteThread(id);
        return sce_result(result);
    }
    *thread = id;
    return 0;
}

int sles_thread_join(sles_thread_t thread)
{
    if (thread == sceKernelGetThreadId()) {
        return EDEADLK;
    }
    int result = sceKernelWaitThreadEnd(thread, NULL, NULL);
    if (result < 0) {
        return sce_result(result);
    }
    return sce_result(sceKernelDeleteThread(thread));
}

sles_thread_t sles_thread_self(void)
{
    return sceKernelGetThreadId();
}

int sles_thread_equal(sles_thread_t a, sles_thread_t b)
{
    return a == b;
}

#else
#include <time.h>

int sles_mutex_init(sles_mutex_t *mutex)
{
    return pthread_mutex_init(mutex, NULL);
}

int sles_mutex_destroy(sles_mutex_t *mutex)
{
    return pthread_mutex_destroy(mutex);
}

int sles_mutex_lock(sles_mutex_t *mutex)
{
    return pthread_mutex_lock(mutex);
}

int sles_mutex_trylock(sles_mutex_t *mutex)
{
    return pthread_mutex_trylock(mutex);
}

int sles_mutex_unlock(sles_mutex_t *mutex)
{
    return pthread_mutex_unlock(mutex);
}

int sles_cond_init(sles_cond_t *cond, sles_mutex_t *mutex)
{
    if (cond == NULL || mutex == NULL) {
        return EINVAL;
    }
    int result = pthread_cond_init(&cond->native, NULL);
    cond->mutex = result == 0 ? mutex : NULL;
    return result;
}

int sles_cond_destroy(sles_cond_t *cond)
{
    return pthread_cond_destroy(&cond->native);
}

int sles_cond_wait(sles_cond_t *cond, sles_mutex_t *mutex)
{
    if (cond->mutex != mutex) {
        return EINVAL;
    }
    return pthread_cond_wait(&cond->native, mutex);
}

int sles_cond_timedwait(sles_cond_t *cond, sles_mutex_t *mutex, uint32_t timeout_us)
{
    if (cond->mutex != mutex) {
        return EINVAL;
    }
    struct timespec deadline;
    if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
        return errno != 0 ? errno : EIO;
    }
    int64_t seconds = (int64_t)deadline.tv_sec + timeout_us / 1000000;
    deadline.tv_nsec += (long)(timeout_us % 1000000) * 1000;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_nsec -= 1000000000L;
        ++seconds;
    }
    deadline.tv_sec = (time_t)seconds;
    if ((int64_t)deadline.tv_sec != seconds) {
        return EOVERFLOW;
    }
    return pthread_cond_timedwait(&cond->native, mutex, &deadline);
}

int sles_cond_signal(sles_cond_t *cond)
{
    return pthread_cond_signal(&cond->native);
}

int sles_cond_broadcast(sles_cond_t *cond)
{
    return pthread_cond_broadcast(&cond->native);
}

int sles_thread_create(sles_thread_t *thread, void *(*entry)(void *), void *arg)
{
    if (thread == NULL || entry == NULL) {
        return EINVAL;
    }
    return pthread_create(thread, NULL, entry, arg);
}

int sles_thread_join(sles_thread_t thread)
{
    return pthread_join(thread, NULL);
}

sles_thread_t sles_thread_self(void)
{
    return pthread_self();
}

int sles_thread_equal(sles_thread_t a, sles_thread_t b)
{
    return pthread_equal(a, b);
}
#endif
