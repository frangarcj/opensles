#ifndef OPENSLES_MOCK_THREADMGR_H
#define OPENSLES_MOCK_THREADMGR_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

typedef int SceUID;
typedef unsigned int SceSize;
typedef int (*SceKernelThreadEntry)(SceSize, void *);
typedef struct {
    pthread_mutex_t native;
} SceKernelLwMutexWork;
typedef struct {
    pthread_cond_t native;
    SceKernelLwMutexWork *mutex;
} SceKernelLwCondWork;

int sceKernelCreateLwMutex(SceKernelLwMutexWork *, const char *, unsigned int, int, const void *);
int sceKernelDeleteLwMutex(SceKernelLwMutexWork *);
int sceKernelLockLwMutex(SceKernelLwMutexWork *, int, unsigned int *);
int sceKernelTryLockLwMutex(SceKernelLwMutexWork *, int);
int sceKernelUnlockLwMutex(SceKernelLwMutexWork *, int);
int sceKernelCreateLwCond(SceKernelLwCondWork *, const char *, unsigned int,
    SceKernelLwMutexWork *, const void *);
int sceKernelDeleteLwCond(SceKernelLwCondWork *);
int sceKernelWaitLwCond(SceKernelLwCondWork *, unsigned int *);
int sceKernelSignalLwCond(SceKernelLwCondWork *);
int sceKernelSignalLwCondAll(SceKernelLwCondWork *);
SceUID sceKernelCreateThread(const char *, SceKernelThreadEntry, int, SceSize,
    unsigned int, int, const void *);
int sceKernelStartThread(SceUID, SceSize, const void *);
int sceKernelWaitThreadEnd(SceUID, int *, unsigned int *);
int sceKernelDeleteThread(SceUID);
SceUID sceKernelGetThreadId(void);
int sceKernelDelayThread(unsigned int);
#endif
