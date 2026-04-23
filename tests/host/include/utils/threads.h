#pragma once

#include <pthread.h>

#ifndef __unused
#define __unused __attribute__((unused))
#endif

namespace android {

class Mutex {
public:
    Mutex() {
        pthread_mutex_init(&mMutex, nullptr);
    }

    ~Mutex() {
        pthread_mutex_destroy(&mMutex);
    }

    void lock() {
        pthread_mutex_lock(&mMutex);
    }

    void unlock() {
        pthread_mutex_unlock(&mMutex);
    }

    pthread_mutex_t *nativeHandle() {
        return &mMutex;
    }

    class Autolock {
    public:
        explicit Autolock(Mutex &mutex) : mMutex(mutex) {
            mMutex.lock();
        }

        ~Autolock() {
            mMutex.unlock();
        }

    private:
        Mutex &mMutex;
    };

private:
    pthread_mutex_t mMutex;
};

class Condition {
public:
    Condition() {
        pthread_cond_init(&mCond, nullptr);
    }

    ~Condition() {
        pthread_cond_destroy(&mCond);
    }

    void signal() {
        pthread_cond_signal(&mCond);
    }

    void wait(Mutex &mutex) {
        pthread_cond_wait(&mCond, mutex.nativeHandle());
    }

private:
    pthread_cond_t mCond;
};

} // namespace android
