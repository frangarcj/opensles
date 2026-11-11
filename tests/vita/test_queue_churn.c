#include <SLES/OpenSLES.h>

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include "../test_watchdog.h"

int main(void)
{
    opensles_test_watchdog(60);
    static short pcm[512];
    const SLuint32 capacities[] = {1, 2, 8, 255};
    unsigned created = 0;
    for (unsigned cycle = 0; cycle < 4; ++cycle) {
        SLObjectItf engine_object = NULL, mix = NULL;
        SLEngineItf engine;
        assert(slCreateEngine(&engine_object, 0, NULL, 0, NULL, NULL) == SL_RESULT_SUCCESS);
        assert((*engine_object)->Realize(engine_object, SL_BOOLEAN_FALSE) == SL_RESULT_SUCCESS);
        assert((*engine_object)->GetInterface(engine_object, SL_IID_ENGINE, &engine) == SL_RESULT_SUCCESS);
        assert((*engine)->CreateOutputMix(engine, &mix, 0, NULL, NULL) == SL_RESULT_SUCCESS);
        assert((*mix)->Realize(mix, SL_BOOLEAN_FALSE) == SL_RESULT_SUCCESS);
        for (unsigned round = 0; round < 8; ++round) {
            SLObjectItf players[12] = {0};
            SLPlayItf plays[12];
            SLBufferQueueItf queues[12];
            for (unsigned i = 0; i < 12; ++i) {
                SLDataLocator_BufferQueue locator = {SL_DATALOCATOR_BUFFERQUEUE, capacities[i % 4]};
                SLDataFormat_PCM format = {SL_DATAFORMAT_PCM, 2, SL_SAMPLINGRATE_44_1,
                    SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                    SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT, SL_BYTEORDER_LITTLEENDIAN};
                SLDataSource source = {&locator, &format};
                SLDataLocator_OutputMix output = {SL_DATALOCATOR_OUTPUTMIX, mix};
                SLDataSink sink = {&output, NULL};
                SLInterfaceID interfaces[] = {SL_IID_BUFFERQUEUE};
                SLboolean required[] = {SL_BOOLEAN_TRUE};
                assert((*engine)->CreateAudioPlayer(engine, &players[i], &source, &sink,
                    1, interfaces, required) == SL_RESULT_SUCCESS);
                assert((*players[i])->Realize(players[i], SL_BOOLEAN_FALSE) == SL_RESULT_SUCCESS);
                assert((*players[i])->GetInterface(players[i], SL_IID_PLAY, &plays[i]) == SL_RESULT_SUCCESS);
                assert((*players[i])->GetInterface(players[i], SL_IID_BUFFERQUEUE, &queues[i]) == SL_RESULT_SUCCESS);
                assert((*queues[i])->Enqueue(queues[i], pcm, sizeof(pcm)) == SL_RESULT_SUCCESS);
                SLBufferQueueState state;
                assert((*queues[i])->GetState(queues[i], &state) == SL_RESULT_SUCCESS);
                assert(state.count == 1 && state.playIndex == 0);
                assert((*plays[i])->SetPlayState(plays[i], SL_PLAYSTATE_PLAYING) == SL_RESULT_SUCCESS);
                ++created;
            }
            usleep(30000);
            for (unsigned i = 0; i < 12; ++i) {
                assert((*plays[i])->SetPlayState(plays[i], SL_PLAYSTATE_STOPPED) == SL_RESULT_SUCCESS);
                assert((*queues[i])->Clear(queues[i]) == SL_RESULT_SUCCESS);
                SLBufferQueueState state;
                assert((*queues[i])->GetState(queues[i], &state) == SL_RESULT_SUCCESS);
                assert(state.count == 0 && state.playIndex == 0);
                (*players[i])->Destroy(players[i]);
            }
        }
        (*mix)->Destroy(mix);
        (*engine_object)->Destroy(engine_object);
        printf("Queue churn cycle=%u created=%u\n", cycle + 1, created);
    }
    assert(created == 384);
    opensles_test_watchdog(0);
    puts("Queue capacity, player churn and engine restart checks passed");
    return 0;
}
