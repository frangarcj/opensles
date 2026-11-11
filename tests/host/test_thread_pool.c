#include <SLES/OpenSLES.h>
#include "threads.h"
#include "ThreadPool.h"

#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <unistd.h>
#ifdef HAVE_SCE_THREADS
#include "mock_control.h"
#endif

static atomic_int entered;
static atomic_int released;
static atomic_int executed;

static void handler(void *context, int parameter)
{
    (void)context;
    if (parameter == 1) {
        atomic_store(&entered, 1);
        while (!atomic_load(&released)) usleep(1000);
    } else {
        atomic_fetch_add(&executed, 1);
    }
}

static void *shutdown_pool(void *arg)
{
    ThreadPool_deinit(arg);
    return NULL;
}

int main(void)
{
    alarm(15);
    for (int round = 0; round < 8; ++round) {
        ThreadPool pool;
        atomic_store(&entered, 0);
        atomic_store(&released, 0);
        assert(ThreadPool_init(&pool, 3, 1) == SL_RESULT_SUCCESS);
        for (int i = 0; i < 8; ++i) {
            assert(sles_mutex_lock(&pool.mMutex) == 0);
            assert(sles_cond_broadcast(&pool.mCondNotEmpty) == 0);
            assert(sles_mutex_unlock(&pool.mMutex) == 0);
            usleep(2000);
        }
        assert(ThreadPool_add(&pool, handler, NULL, 1) == SL_RESULT_SUCCESS);
        while (!atomic_load(&entered)) usleep(1000);
        assert(ThreadPool_add(&pool, handler, NULL, 2) == SL_RESULT_SUCCESS);
        assert(ThreadPool_add(&pool, handler, NULL, 3) == SL_RESULT_SUCCESS);
        sles_thread_t shutdown_thread;
        assert(sles_thread_create(&shutdown_thread, shutdown_pool, &pool) == 0);
        for (;;) {
            assert(sles_mutex_lock(&pool.mMutex) == 0);
            int shutdown = pool.mShutdown;
            assert(sles_mutex_unlock(&pool.mMutex) == 0);
            if (shutdown) break;
            usleep(1000);
        }
        atomic_store(&released, 1);
        assert(sles_thread_join(shutdown_thread) == 0);
        assert(atomic_load(&executed) == 0);
#ifdef HAVE_SCE_THREADS
        assert(mock_live_resources() == 0);
#endif
    }
    alarm(0);
    puts("Thread pool predicate wakeups and pending-work shutdown cleanup passed");
    return 0;
}
