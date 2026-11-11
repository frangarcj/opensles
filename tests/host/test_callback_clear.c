#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef OPENSLES_TEST_VITA_AUDIO
#include "mock_control.h"
#include <string.h>
#endif

static short pcm[512];
static atomic_uint callbacks;
static atomic_int finished;
static SLPlayItf callback_play;
static int stop_in_callback;

static void buffer_callback(SLAndroidSimpleBufferQueueItf queue, void *context)
{
    (void)context;
    unsigned count = atomic_fetch_add(&callbacks, 1) + 1;
    if (stop_in_callback) {
        assert(SL_RESULT_SUCCESS == (*callback_play)->SetPlayState(
            callback_play, SL_PLAYSTATE_STOPPED));
    }
    assert(SL_RESULT_SUCCESS == (*queue)->Clear(queue));
    SLAndroidSimpleBufferQueueState state;
    assert(SL_RESULT_SUCCESS == (*queue)->GetState(queue, &state));
    assert(state.count == 0 && state.index == 0);
    if (count < 64) {
        assert(SL_RESULT_SUCCESS == (*queue)->Enqueue(queue, pcm, sizeof(pcm)));
        if (stop_in_callback) {
            assert(SL_RESULT_SUCCESS == (*callback_play)->SetPlayState(
                callback_play, SL_PLAYSTATE_PLAYING));
        }
    } else {
        atomic_store(&finished, 1);
    }
}

int main(int argc, char **argv)
{
    (void)argv;
    stop_in_callback = argc > 1;
#ifdef OPENSLES_TEST_VITA_AUDIO
    if (argc > 1 && strcmp(argv[1], "output-failure") == 0) {
        mock_fail_at(MOCK_AUDIO_OUTPUT, 1, (int)0x8002ffff);
    }
#endif
    alarm(15);
    SLObjectItf engine_object, mix, player;
    SLEngineItf engine;
    SLPlayItf play;
    SLAndroidSimpleBufferQueueItf queue;
    assert(SL_RESULT_SUCCESS == slCreateEngine(&engine_object, 0, NULL, 0, NULL, NULL));
    assert(SL_RESULT_SUCCESS == (*engine_object)->Realize(engine_object, SL_BOOLEAN_FALSE));
    assert(SL_RESULT_SUCCESS == (*engine_object)->GetInterface(engine_object, SL_IID_ENGINE, &engine));
    assert(SL_RESULT_SUCCESS == (*engine)->CreateOutputMix(engine, &mix, 0, NULL, NULL));
    assert(SL_RESULT_SUCCESS == (*mix)->Realize(mix, SL_BOOLEAN_FALSE));
    SLDataLocator_AndroidSimpleBufferQueue source_locator = {
        SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2
    };
    SLDataFormat_PCM format = {SL_DATAFORMAT_PCM, 2, SL_SAMPLINGRATE_44_1,
        SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT, SL_BYTEORDER_LITTLEENDIAN};
    SLDataSource source = {&source_locator, &format};
    SLDataLocator_OutputMix sink_locator = {SL_DATALOCATOR_OUTPUTMIX, mix};
    SLDataSink sink = {&sink_locator, NULL};
    SLInterfaceID interfaces[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    SLboolean required[] = {SL_BOOLEAN_TRUE};
    assert(SL_RESULT_SUCCESS == (*engine)->CreateAudioPlayer(engine, &player,
        &source, &sink, 1, interfaces, required));
    assert(SL_RESULT_SUCCESS == (*player)->Realize(player, SL_BOOLEAN_FALSE));
    assert(SL_RESULT_SUCCESS == (*player)->GetInterface(player, SL_IID_PLAY, &play));
    callback_play = play;
    assert(SL_RESULT_SUCCESS == (*player)->GetInterface(player,
        SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &queue));
    assert(SL_RESULT_SUCCESS == (*queue)->RegisterCallback(queue, buffer_callback, NULL));
    assert(SL_RESULT_SUCCESS == (*queue)->Enqueue(queue, pcm, sizeof(pcm)));
    assert(SL_RESULT_SUCCESS == (*play)->SetPlayState(play, SL_PLAYSTATE_PLAYING));
    while (!atomic_load(&finished)) {
        usleep(1000);
    }
    if (stop_in_callback) {
        usleep(50000);
        SLuint32 state;
        SLmillisecond position;
        assert(SL_RESULT_SUCCESS == (*play)->GetPlayState(play, &state));
        assert(state == SL_PLAYSTATE_STOPPED);
        assert(SL_RESULT_SUCCESS == (*play)->GetPosition(play, &position));
        assert(position == 0);
    }
    assert(SL_RESULT_SUCCESS == (*play)->SetPlayState(play, SL_PLAYSTATE_STOPPED));
    assert(SL_RESULT_SUCCESS == (*queue)->Clear(queue));
    (*player)->Destroy(player);
    (*mix)->Destroy(mix);
    (*engine_object)->Destroy(engine_object);
#ifdef OPENSLES_TEST_VITA_AUDIO
    assert(mock_live_resources() == 0);
#endif
    assert(atomic_load(&callbacks) == 64);
    alarm(0);
    printf("callback %sClear/Enqueue regression passed (64 loops)\n",
        stop_in_callback ? "Stop/" : "");
    return EXIT_SUCCESS;
}
