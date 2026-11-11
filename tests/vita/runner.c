#include <psp2/kernel/clib.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>

#include "../test_watchdog.h"

#ifndef OPENSLES_TEST_NAME
#error "OPENSLES_TEST_NAME is required"
#endif

int _opensles_user_freq = 44100;
unsigned int _newlib_heap_size_user = 32 * 1024 * 1024;

static atomic_uint deadline;
static atomic_int finished;

void opensles_test_watchdog(unsigned seconds)
{
    unsigned limit = seconds == 0 ? 0 : sceKernelGetProcessTimeLow() + seconds * 1000000U;
    atomic_store(&deadline, limit);
}

static int watchdog(SceSize size, void *arg)
{
    (void)size;
    (void)arg;
    while (!atomic_load(&finished)) {
        unsigned limit = atomic_load(&deadline);
        if (limit != 0 && (int32_t)(sceKernelGetProcessTimeLow() - limit) >= 0) {
            sceClibPrintf("OPENSL_TEST_TIMEOUT name=%s\n", OPENSLES_TEST_NAME);
            sceKernelExitProcess(124);
        }
        sceKernelDelayThread(10000);
    }
    return 0;
}

#ifdef OPENSLES_TEST_HAS_ARGS
extern int opensles_test_main(int argc, char **argv);
#else
extern int opensles_test_main(void);
#endif

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
#ifdef HAVE_SCE_THREADS
    const char *backend = "sce";
#else
    const char *backend = "pthread";
#endif
    SceKernelSystemSwVersion version = {0};
    version.size = sizeof(version);
    int version_result = sceKernelGetSystemSwVersion(&version);
    printf("OPENSL_TEST_BEGIN name=%s backend=%s reported_fw=%.*s version=0x%08x query=%d\n",
        OPENSLES_TEST_NAME, backend, (int)sizeof(version.versionString),
        version.versionString, version.version, version_result);
    SceUID timer = sceKernelCreateThread("OpenSLES test watchdog", watchdog,
        0x10000100, 0x10000, 0, 0, NULL);
    if (timer < 0) {
        printf("OPENSL_TEST_FAIL name=%s watchdog_create=0x%08x\n", OPENSLES_TEST_NAME, timer);
        return 1;
    }
    opensles_test_watchdog(90);
    int started = sceKernelStartThread(timer, 0, NULL);
    if (started < 0) {
        sceKernelDeleteThread(timer);
        printf("OPENSL_TEST_FAIL name=%s watchdog_start=0x%08x\n", OPENSLES_TEST_NAME, started);
        return 1;
    }
#ifdef OPENSLES_TEST_HAS_ARGS
    char name[] = OPENSLES_TEST_NAME;
    char option[] = "stop";
    char *args[] = {name, option, NULL};
#ifdef OPENSLES_TEST_STOP_CALLBACK
    int result = opensles_test_main(2, args);
#else
    int result = opensles_test_main(1, args);
#endif
#else
    int result = opensles_test_main();
#endif
    atomic_store(&finished, 1);
    int joined = sceKernelWaitThreadEnd(timer, NULL, NULL);
    int deleted = joined < 0 ? joined : sceKernelDeleteThread(timer);
    if (joined < 0 || deleted < 0) result = 1;
    printf("OPENSL_TEST_%s name=%s backend=%s result=%d\n",
        result == 0 ? "PASS" : "FAIL", OPENSLES_TEST_NAME, backend, result);
    return result;
}
