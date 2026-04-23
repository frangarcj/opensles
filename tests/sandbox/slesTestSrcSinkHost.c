#include <SLES/OpenSLES.h>

#include <stdio.h>
#include <stdlib.h>

static int expect(SLresult got, SLresult want, const char *what)
{
    if (got != want) {
        fprintf(stderr, "%s: got %lu, expected %lu\n", what,
                (unsigned long) got, (unsigned long) want);
        return 0;
    }
    return 1;
}

int main(void)
{
    SLObjectItf engineObject = NULL;
    SLEngineItf engineItf = NULL;
    SLObjectItf outputMix = NULL;
    SLObjectItf playerObject = NULL;

    SLresult r = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    if (!expect(r, SL_RESULT_SUCCESS, "slCreateEngine")) {
        return EXIT_FAILURE;
    }
    r = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    if (!expect(r, SL_RESULT_SUCCESS, "engine Realize")) {
        return EXIT_FAILURE;
    }
    r = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineItf);
    if (!expect(r, SL_RESULT_SUCCESS, "engine GetInterface")) {
        return EXIT_FAILURE;
    }

    SLDataLocator_BufferQueue loc_bufq = {SL_DATALOCATOR_BUFFERQUEUE, 1};
    SLDataFormat_PCM fmt = {
        SL_DATAFORMAT_PCM,
        2,
        SL_SAMPLINGRATE_44_1,
        16,
        16,
        SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
        SL_BYTEORDER_LITTLEENDIAN
    };
    SLDataSource src = {&loc_bufq, &fmt};

    SLDataLocator_OutputMix loc_out = {SL_DATALOCATOR_OUTPUTMIX, NULL};
    SLDataSink sink = {&loc_out, NULL};

    SLInterfaceID ids[1] = {SL_IID_BUFFERQUEUE};
    SLboolean req[1] = {SL_BOOLEAN_TRUE};

    r = (*engineItf)->CreateAudioPlayer(engineItf, &playerObject, &src, &sink, 1, ids, req);
    if (!expect(r, SL_RESULT_PARAMETER_INVALID, "NULL output mix")) {
        return EXIT_FAILURE;
    }

    loc_out.outputMix = engineObject;
    r = (*engineItf)->CreateAudioPlayer(engineItf, &playerObject, &src, &sink, 1, ids, req);
    if (!expect(r, SL_RESULT_PARAMETER_INVALID, "engine as output mix")) {
        return EXIT_FAILURE;
    }

    r = (*engineItf)->CreateOutputMix(engineItf, &outputMix, 0, NULL, NULL);
    if (!expect(r, SL_RESULT_SUCCESS, "CreateOutputMix")) {
        return EXIT_FAILURE;
    }

    loc_out.outputMix = outputMix;
    r = (*engineItf)->CreateAudioPlayer(engineItf, &playerObject, &src, &sink, 1, ids, req);
    if (!expect(r, SL_RESULT_PRECONDITIONS_VIOLATED, "unrealized output mix")) {
        return EXIT_FAILURE;
    }

    r = (*outputMix)->Realize(outputMix, SL_BOOLEAN_FALSE);
    if (!expect(r, SL_RESULT_SUCCESS, "output mix Realize")) {
        return EXIT_FAILURE;
    }

    r = (*engineItf)->CreateAudioPlayer(engineItf, &playerObject, &src, &sink, 1, ids, req);
    if (!expect(r, SL_RESULT_SUCCESS, "valid CreateAudioPlayer")) {
        return EXIT_FAILURE;
    }

    if (playerObject != NULL) {
        (*playerObject)->Destroy(playerObject);
    }
    (*outputMix)->Destroy(outputMix);
    (*engineObject)->Destroy(engineObject);

    fprintf(stdout, "slesTestSrcSinkHost completed\n");
    return EXIT_SUCCESS;
}
