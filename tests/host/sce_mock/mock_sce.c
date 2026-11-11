#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/error.h>
#include "mock_control.h"

#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static atomic_int fail_operation;
static atomic_int fail_ordinal;
static atomic_int fail_error;
static atomic_int live_resources;
static atomic_int next_id = 100;
static _Thread_local SceUID current_id;

void mock_fail_at(enum MockOperation operation, int ordinal, int error)
{
    atomic_store(&fail_ordinal, ordinal);
    atomic_store(&fail_error, error);
    atomic_store(&fail_operation, operation);
}

int mock_live_resources(void)
{
    return atomic_load(&live_resources);
}

void mock_resource_change(int delta)
{
    atomic_fetch_add(&live_resources, delta);
}

int mock_injected(enum MockOperation operation)
{
    if (atomic_load(&fail_operation) == (int)operation &&
            atomic_fetch_sub(&fail_ordinal, 1) == 1) {
        atomic_store(&fail_operation, MOCK_NONE);
        return atomic_load(&fail_error);
    }
    return 0;
}

static int sce_error(int error)
{
    switch (error) {
    case 0: return 0;
    case ETIMEDOUT: return SCE_KERNEL_ERROR_WAIT_TIMEOUT;
    case EBUSY: return SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN;
    case EDEADLK: return SCE_KERNEL_ERROR_LW_MUTEX_RECURSIVE;
    case EPERM: return SCE_KERNEL_ERROR_LW_MUTEX_NOT_OWNED;
    default: return SCE_KERNEL_ERROR_NO_MEMORY;
    }
}

int sceKernelCreateLwMutex(SceKernelLwMutexWork *work, const char *name,
    unsigned int attr, int count, const void *opt)
{
    (void)name;
    assert(attr == 0 && count == 0 && opt == NULL);
    int error = mock_injected(MOCK_MUTEX_CREATE);
    if (error != 0) return error;
    pthread_mutexattr_t attributes;
    assert(pthread_mutexattr_init(&attributes) == 0);
    assert(pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_ERRORCHECK) == 0);
    error = pthread_mutex_init(&work->native, &attributes);
    assert(pthread_mutexattr_destroy(&attributes) == 0);
    if (error == 0) atomic_fetch_add(&live_resources, 1);
    return sce_error(error);
}

int sceKernelDeleteLwMutex(SceKernelLwMutexWork *work)
{
    int error = pthread_mutex_destroy(&work->native);
    if (error == 0) atomic_fetch_sub(&live_resources, 1);
    return sce_error(error);
}

int sceKernelLockLwMutex(SceKernelLwMutexWork *work, int count, unsigned int *timeout)
{
    assert(count == 1 && timeout == NULL);
    return sce_error(pthread_mutex_lock(&work->native));
}

int sceKernelTryLockLwMutex(SceKernelLwMutexWork *work, int count)
{
    assert(count == 1);
    return sce_error(pthread_mutex_trylock(&work->native));
}

int sceKernelUnlockLwMutex(SceKernelLwMutexWork *work, int count)
{
    assert(count == 1);
    return sce_error(pthread_mutex_unlock(&work->native));
}

int sceKernelCreateLwCond(SceKernelLwCondWork *work, const char *name, unsigned int attr,
    SceKernelLwMutexWork *mutex, const void *opt)
{
    (void)name;
    assert(attr == 0 && opt == NULL);
    int error = mock_injected(MOCK_COND_CREATE);
    if (error != 0) return error;
    error = pthread_cond_init(&work->native, NULL);
    if (error == 0) {
        work->mutex = mutex;
        atomic_fetch_add(&live_resources, 1);
    }
    return sce_error(error);
}

int sceKernelDeleteLwCond(SceKernelLwCondWork *work)
{
    int error = pthread_cond_destroy(&work->native);
    if (error == 0) atomic_fetch_sub(&live_resources, 1);
    return sce_error(error);
}

int sceKernelWaitLwCond(SceKernelLwCondWork *work, unsigned int *timeout)
{
    if (timeout == NULL) {
        return sce_error(pthread_cond_wait(&work->native, &work->mutex->native));
    }
    struct timespec deadline;
    assert(clock_gettime(CLOCK_REALTIME, &deadline) == 0);
    deadline.tv_sec += *timeout / 1000000;
    deadline.tv_nsec += (long)(*timeout % 1000000) * 1000;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_nsec -= 1000000000L;
        ++deadline.tv_sec;
    }
    int error = pthread_cond_timedwait(&work->native, &work->mutex->native, &deadline);
    if (error == ETIMEDOUT) *timeout = 0;
    return sce_error(error);
}

int sceKernelSignalLwCond(SceKernelLwCondWork *work)
{
    return sce_error(pthread_cond_signal(&work->native));
}

int sceKernelSignalLwCondAll(SceKernelLwCondWork *work)
{
    return sce_error(pthread_cond_broadcast(&work->native));
}

typedef struct MockThread {
    struct MockThread *next;
    SceUID id;
    SceKernelThreadEntry entry;
    pthread_t native;
    void *args;
    SceSize size;
    int started;
    int joined;
    int status;
} MockThread;

static pthread_mutex_t registry_mutex = PTHREAD_MUTEX_INITIALIZER;
static MockThread *threads;

static MockThread *lookup(SceUID id)
{
    pthread_mutex_lock(&registry_mutex);
    MockThread *thread = threads;
    while (thread != NULL && thread->id != id) thread = thread->next;
    pthread_mutex_unlock(&registry_mutex);
    assert(thread != NULL);
    return thread;
}

SceUID sceKernelCreateThread(const char *name, SceKernelThreadEntry entry, int priority,
    SceSize stack_size, unsigned int attr, int affinity, const void *opt)
{
    (void)name;
    assert(priority > 0 && stack_size > 0 && attr == 0 && affinity == 0 && opt == NULL);
    int error = mock_injected(MOCK_THREAD_CREATE);
    if (error != 0) return error;
    MockThread *thread = calloc(1, sizeof(*thread));
    if (thread == NULL) return SCE_KERNEL_ERROR_NO_MEMORY;
    thread->id = atomic_fetch_add(&next_id, 1);
    thread->entry = entry;
    pthread_mutex_lock(&registry_mutex);
    thread->next = threads;
    threads = thread;
    pthread_mutex_unlock(&registry_mutex);
    atomic_fetch_add(&live_resources, 1);
    return thread->id;
}

static void *mock_entry(void *arg)
{
    MockThread *thread = arg;
    current_id = thread->id;
    thread->status = thread->entry(thread->size, thread->args);
    free(thread->args);
    thread->args = NULL;
    return NULL;
}

int sceKernelStartThread(SceUID id, SceSize size, const void *args)
{
    int error = mock_injected(MOCK_THREAD_START);
    if (error != 0) return error;
    MockThread *thread = lookup(id);
    assert(!thread->started);
    thread->args = malloc(size);
    if (thread->args == NULL) return SCE_KERNEL_ERROR_NO_MEMORY;
    memcpy(thread->args, args, size);
    thread->size = size;
    error = pthread_create(&thread->native, NULL, mock_entry, thread);
    if (error != 0) {
        free(thread->args);
        thread->args = NULL;
        return sce_error(error);
    }
    thread->started = 1;
    return 0;
}

int sceKernelWaitThreadEnd(SceUID id, int *status, unsigned int *timeout)
{
    assert(timeout == NULL);
    MockThread *thread = lookup(id);
    assert(thread->started && !thread->joined);
    int error = pthread_join(thread->native, NULL);
    if (error == 0) {
        thread->joined = 1;
        if (status != NULL) *status = thread->status;
    }
    return sce_error(error);
}

int sceKernelDeleteThread(SceUID id)
{
    MockThread *thread = lookup(id);
    assert(!thread->started || thread->joined);
    pthread_mutex_lock(&registry_mutex);
    MockThread **link = &threads;
    while (*link != thread) link = &(*link)->next;
    *link = thread->next;
    pthread_mutex_unlock(&registry_mutex);
    free(thread);
    atomic_fetch_sub(&live_resources, 1);
    return 0;
}

SceUID sceKernelGetThreadId(void)
{
    if (current_id == 0) current_id = atomic_fetch_add(&next_id, 1);
    return current_id;
}

int sceKernelDelayThread(unsigned int usec)
{
    return usleep(usec);
}
