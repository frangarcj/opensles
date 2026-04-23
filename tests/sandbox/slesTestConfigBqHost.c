#include <SLES/OpenSLES.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
    SLuint8 numChannels;
    SLuint32 milliHz;
    SLuint8 bitsPerSample;
} Pcm;

static Pcm formats[] = {
    {1, SL_SAMPLINGRATE_8, 8}, {2, SL_SAMPLINGRATE_8, 16},
    {1, SL_SAMPLINGRATE_16, 16}, {2, SL_SAMPLINGRATE_22_05, 16},
    {1, SL_SAMPLINGRATE_44_1, 16}, {2, SL_SAMPLINGRATE_48, 16},
    {0, 0, 0}
};

int main(void)
{
    SLresult result;
    SLObjectItf engineObject;

    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    assert(SL_RESULT_SUCCESS == result);

    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);

    SLEngineItf engineItf;
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineItf);
    assert(SL_RESULT_SUCCESS == result);

    SLObjectItf outputMixObject;
    result = (*engineItf)->CreateOutputMix(engineItf, &outputMixObject, 0, NULL, NULL);
    assert(SL_RESULT_SUCCESS == result);
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);

    for (Pcm *fmt = formats; fmt->numChannels != 0; ++fmt) {
        SLDataLocator_BufferQueue locBufq = {SL_DATALOCATOR_BUFFERQUEUE, 1};
        SLDataFormat_PCM formatPcm = {
            SL_DATAFORMAT_PCM,
            fmt->numChannels,
            fmt->milliHz,
            fmt->bitsPerSample,
            fmt->bitsPerSample,
            0,
            SL_BYTEORDER_LITTLEENDIAN
        };
        SLDataSource src = {&locBufq, &formatPcm};

        SLDataLocator_OutputMix locOut = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
        SLDataSink sink = {&locOut, NULL};

        SLInterfaceID ids[1] = {SL_IID_BUFFERQUEUE};
        SLboolean req[1] = {SL_BOOLEAN_TRUE};

        SLObjectItf playerObject = NULL;
        result = (*engineItf)->CreateAudioPlayer(engineItf, &playerObject, &src, &sink,
                1, ids, req);
        if (result != SL_RESULT_SUCCESS) {
            continue;
        }

        result = (*playerObject)->Realize(playerObject, SL_BOOLEAN_FALSE);
        assert(SL_RESULT_SUCCESS == result);

        SLBufferQueueItf bqItf;
        result = (*playerObject)->GetInterface(playerObject, SL_IID_BUFFERQUEUE, &bqItf);
        assert(SL_RESULT_SUCCESS == result);

        SLPlayItf playItf;
        result = (*playerObject)->GetInterface(playerObject, SL_IID_PLAY, &playItf);
        assert(SL_RESULT_SUCCESS == result);

        unsigned char buffer[4096] = {0};
        result = (*bqItf)->Enqueue(bqItf, buffer, sizeof(buffer));
        assert(SL_RESULT_SUCCESS == result);

        result = (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PLAYING);
        assert(SL_RESULT_SUCCESS == result);

        usleep(20 * 1000);
        (void) (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_STOPPED);

        (*playerObject)->Destroy(playerObject);
    }

    (*outputMixObject)->Destroy(outputMixObject);
    (*engineObject)->Destroy(engineObject);

    fprintf(stdout, "slesTestConfigBqHost completed\n");
    return EXIT_SUCCESS;
}
