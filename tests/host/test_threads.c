#include "threads.h"
#include "../test_watchdog.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#if defined(HAVE_SCE_THREADS) && !defined(OPENSLES_TEST_VITA_NATIVE)
#include <psp2/kernel/error.h>
#include "mock_control.h"
#endif

static sles_mutex_t mutex;
static sles_cond_t ready_cond;
static sles_cond_t go_cond;
static unsigned ready;
static int go;
static unsigned counter;

static void *worker(void *arg)
{
    assert(arg == &counter);
    assert(sles_thread_equal(sles_thread_self(), sles_thread_self()));
    assert(sles_thread_join(sles_thread_self()) == EDEADLK);
    assert(sles_mutex_lock(&mutex) == 0);
    ++ready;
    assert(sles_cond_signal(&ready_cond) == 0);
    while (!go) assert(sles_cond_wait(&go_cond, &mutex) == 0);
    assert(sles_mutex_unlock(&mutex) == 0);
    for (unsigned i = 0; i < 1000; ++i) {
        assert(sles_mutex_lock(&mutex) == 0);
        ++counter;
        assert(sles_mutex_unlock(&mutex) == 0);
    }
    return NULL;
}

static void *try_locked(void *arg)
{
    assert(sles_mutex_trylock(arg) == EBUSY);
    return NULL;
}

static void assert_owned(void)
{
    sles_thread_t probe;
    assert(sles_thread_create(&probe, try_locked, &mutex) == 0);
    assert(sles_thread_join(probe) == 0);
}

int main(void)
{
    opensles_test_watchdog(30);
    assert(sles_thread_create(NULL, worker, NULL) == EINVAL);
    sles_thread_t thread;
    assert(sles_thread_create(&thread, NULL, NULL) == EINVAL);
    assert(sles_mutex_init(&mutex) == 0);
    sles_mutex_t other;
    assert(sles_mutex_init(&other) == 0);
    assert(sles_cond_init(&ready_cond, &mutex) == 0);
    assert(sles_cond_init(&go_cond, &mutex) == 0);
    assert(sles_cond_signal(&go_cond) == 0);
    assert(sles_cond_broadcast(&go_cond) == 0);
    assert(sles_mutex_lock(&mutex) == 0);
    assert(sles_mutex_trylock(&mutex) == EBUSY);
    assert(sles_cond_wait(&go_cond, &other) == EINVAL);
    assert(sles_cond_timedwait(&go_cond, &other, 1000) == EINVAL);
    assert_owned();
    assert(sles_cond_timedwait(&go_cond, &mutex, 0) == ETIMEDOUT);
    assert_owned();
    assert(sles_cond_timedwait(&go_cond, &mutex, 1000) == ETIMEDOUT);
    assert_owned();
    assert(sles_mutex_unlock(&mutex) == 0);

    for (unsigned round = 0; round < 32; ++round) {
        sles_thread_t workers[4];
        assert(sles_mutex_lock(&mutex) == 0);
        ready = 0;
        go = 0;
        counter = 0;
        for (unsigned i = 0; i < 4; ++i) {
            assert(sles_thread_create(&workers[i], worker, &counter) == 0);
        }
        while (ready != 4) {
            assert(sles_cond_timedwait(&ready_cond, &mutex, 5000000) == 0);
        }
        go = 1;
        assert(sles_cond_broadcast(&go_cond) == 0);
        assert(sles_mutex_unlock(&mutex) == 0);
        for (unsigned i = 0; i < 4; ++i) assert(sles_thread_join(workers[i]) == 0);
        assert(counter == 4000);
    }
    assert(sles_cond_destroy(&go_cond) == 0);
    assert(sles_cond_destroy(&ready_cond) == 0);
    assert(sles_mutex_destroy(&other) == 0);
    assert(sles_mutex_destroy(&mutex) == 0);

#if defined(HAVE_SCE_THREADS) && !defined(OPENSLES_TEST_VITA_NATIVE)
    assert(mock_live_resources() == 0);
    mock_fail_at(MOCK_MUTEX_CREATE, 1, SCE_KERNEL_ERROR_NO_MEMORY);
    assert(sles_mutex_init(&mutex) == ENOMEM);
    assert(mock_live_resources() == 0);
    assert(sles_mutex_init(&mutex) == 0);
    mock_fail_at(MOCK_COND_CREATE, 1, SCE_KERNEL_ERROR_NO_MEMORY);
    assert(sles_cond_init(&go_cond, &mutex) == ENOMEM);
    assert(mock_live_resources() == 1);
    assert(sles_mutex_destroy(&mutex) == 0);
    thread = 12345;
    mock_fail_at(MOCK_THREAD_CREATE, 1, SCE_KERNEL_ERROR_NO_MEMORY);
    assert(sles_thread_create(&thread, worker, &counter) == ENOMEM);
    assert(thread == 12345 && mock_live_resources() == 0);
    mock_fail_at(MOCK_THREAD_START, 1, SCE_KERNEL_ERROR_NO_MEMORY);
    assert(sles_thread_create(&thread, worker, &counter) == ENOMEM);
    assert(thread == 12345 && mock_live_resources() == 0);
    mock_fail_at(MOCK_THREAD_CREATE, 1, (int)0x8002ffff);
    assert(sles_thread_create(&thread, worker, &counter) == EIO);
    assert(thread == 12345 && mock_live_resources() == 0);
    puts("SCE-adapter mock: synchronization, ownership, stress and failure cleanup passed");
#elif defined(HAVE_SCE_THREADS)
    puts("Native SCE adapter: synchronization, ownership and stress passed");
#else
    puts("POSIX adapter: synchronization, ownership and stress passed");
#endif
    opensles_test_watchdog(0);
    return 0;
}
