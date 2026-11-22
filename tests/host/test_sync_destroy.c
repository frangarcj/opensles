#include <SLES/OpenSLES.h>
#include "threads.h"
#include "../test_watchdog.h"

#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

static sles_mutex_t gate_mutex;
static sles_cond_t gate_cond;
static atomic_uintptr_t target;
static atomic_int destroying;
static atomic_int destroyed;
static int entered;
static int released;

extern void __real_audioPlayerTransportUpdate(void *player);

void __wrap_audioPlayerTransportUpdate(void *player)
{
    if ((uintptr_t)player != atomic_load(&target)) {
        __real_audioPlayerTransportUpdate(player);
        return;
    }
    assert(sles_mutex_lock(&gate_mutex) == 0);
    entered = 1;
    assert(sles_cond_broadcast(&gate_cond) == 0);
    while (!released) assert(sles_cond_wait(&gate_cond, &gate_mutex) == 0);
    assert(sles_mutex_unlock(&gate_mutex) == 0);
}

static void *destroy_player(void *arg)
{
    SLObjectItf player = arg;
    atomic_store(&destroying, 1);
    (*player)->Destroy(player);
    atomic_store(&destroyed, 1);
    return NULL;
}

int main(void)
{
    opensles_test_watchdog(15);
    assert(sles_mutex_init(&gate_mutex) == 0);
    assert(sles_cond_init(&gate_cond, &gate_mutex) == 0);
    SLObjectItf engine_object, mix, player;
    SLEngineItf engine;
    SLPlayItf play;
    SLBufferQueueItf queue;
    assert(slCreateEngine(&engine_object, 0, NULL, 0, NULL, NULL) == SL_RESULT_SUCCESS);
    assert((*engine_object)->Realize(engine_object, SL_BOOLEAN_FALSE) == SL_RESULT_SUCCESS);
    assert((*engine_object)->GetInterface(engine_object, SL_IID_ENGINE, &engine) == SL_RESULT_SUCCESS);
    assert((*engine)->CreateOutputMix(engine, &mix, 0, NULL, NULL) == SL_RESULT_SUCCESS);
    assert((*mix)->Realize(mix, SL_BOOLEAN_FALSE) == SL_RESULT_SUCCESS);
    SLDataLocator_BufferQueue locator = {SL_DATALOCATOR_BUFFERQUEUE, 2};
    SLDataFormat_PCM format = {SL_DATAFORMAT_PCM, 2, SL_SAMPLINGRATE_44_1,
        SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT, SL_BYTEORDER_LITTLEENDIAN};
    SLDataSource source = {&locator, &format};
    SLDataLocator_OutputMix output = {SL_DATALOCATOR_OUTPUTMIX, mix};
    SLDataSink sink = {&output, NULL};
    SLInterfaceID interfaces[] = {SL_IID_BUFFERQUEUE};
    SLboolean required[] = {SL_BOOLEAN_TRUE};
    assert((*engine)->CreateAudioPlayer(engine, &player, &source, &sink,
        1, interfaces, required) == SL_RESULT_SUCCESS);
    assert((*player)->Realize(player, SL_BOOLEAN_FALSE) == SL_RESULT_SUCCESS);
    assert((*player)->GetInterface(player, SL_IID_PLAY, &play) == SL_RESULT_SUCCESS);
    assert((*player)->GetInterface(player, SL_IID_BUFFERQUEUE, &queue) == SL_RESULT_SUCCESS);
    static short pcm[512];
    assert((*queue)->Enqueue(queue, pcm, sizeof(pcm)) == SL_RESULT_SUCCESS);
    atomic_store(&target, (uintptr_t)player);
    assert((*play)->SetPlayState(play, SL_PLAYSTATE_PLAYING) == SL_RESULT_SUCCESS);
    assert(sles_mutex_lock(&gate_mutex) == 0);
    while (!entered) assert(sles_cond_wait(&gate_cond, &gate_mutex) == 0);
    assert(sles_mutex_unlock(&gate_mutex) == 0);
    sles_thread_t destroyer;
    assert(sles_thread_create(&destroyer, destroy_player, (void *)player) == 0);
    while (!atomic_load(&destroying)) usleep(1000);
    usleep(300000);
    assert(!atomic_load(&destroyed));
    assert(sles_mutex_lock(&gate_mutex) == 0);
    released = 1;
    assert(sles_cond_broadcast(&gate_cond) == 0);
    assert(sles_mutex_unlock(&gate_mutex) == 0);
    assert(sles_thread_join(destroyer) == 0);
    assert(atomic_load(&destroyed));
    (*mix)->Destroy(mix);
    (*engine_object)->Destroy(engine_object);
    assert(sles_cond_destroy(&gate_cond) == 0);
    assert(sles_mutex_destroy(&gate_mutex) == 0);
    opensles_test_watchdog(0);
    puts("Destroy waits for the sync worker's in-flight player reference");
    return 0;
}
