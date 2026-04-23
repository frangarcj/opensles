#include <SLES/OpenSLES.h>

#include <stdio.h>
#include <stdlib.h>

#include "opensles_host_adapter.h"

static void exit_on_error(SLresult result, int line)
{
    if (result != SL_RESULT_SUCCESS) {
        fprintf(stderr, "SL error %lu at line %d\n", (unsigned long) result, line);
        exit(EXIT_FAILURE);
    }
}

#define CHECK(x) exit_on_error((x), __LINE__)

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <audio-path-or-file-uri>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char uri[2048];
    if (host_path_to_file_uri(argv[1], uri, sizeof(uri)) != 0) {
        fprintf(stderr, "Failed to convert path to URI\n");
        return EXIT_FAILURE;
    }

    SLObjectItf engineObject = NULL;
    SLEngineItf engineItf = NULL;
    SLObjectItf outputMix = NULL;
    SLObjectItf playerObject = NULL;
    SLPlayItf playerPlay = NULL;
    SLBufferQueueItf bqItf = NULL;

    CHECK(slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL));
    CHECK((*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE));
    CHECK((*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineItf));

    CHECK((*engineItf)->CreateOutputMix(engineItf, &outputMix, 0, NULL, NULL));
    CHECK((*outputMix)->Realize(outputMix, SL_BOOLEAN_FALSE));

    SLDataLocator_URI loc_uri = {SL_DATALOCATOR_URI, (SLchar *) uri};
    SLDataFormat_MIME format_mime = {SL_DATAFORMAT_MIME, NULL, SL_CONTAINERTYPE_UNSPECIFIED};
    SLDataSource audioSrc = {&loc_uri, &format_mime};

    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMix};
    SLDataSink audioSnk = {&loc_outmix, NULL};

    CHECK((*engineItf)->CreateAudioPlayer(engineItf, &playerObject, &audioSrc, &audioSnk,
            0, NULL, NULL));
    CHECK((*playerObject)->Realize(playerObject, SL_BOOLEAN_FALSE));
    CHECK((*playerObject)->GetInterface(playerObject, SL_IID_PLAY, &playerPlay));

    SLInterfaceID ids[1] = {SL_IID_BUFFERQUEUE};
    SLboolean req[1] = {SL_BOOLEAN_TRUE};
    SLObjectItf playerWithBq = NULL;
    SLresult withBq = (*engineItf)->CreateAudioPlayer(engineItf, &playerWithBq, &audioSrc, &audioSnk,
            1, ids, req);
    if (withBq == SL_RESULT_SUCCESS && playerWithBq != NULL) {
        (*playerWithBq)->Destroy(playerWithBq);
    } else if (withBq != SL_RESULT_FEATURE_UNSUPPORTED) {
        fprintf(stderr, "Unexpected result for URI+BUFFERQUEUE create: %lu\n",
                (unsigned long) withBq);
        (*playerObject)->Destroy(playerObject);
        (*outputMix)->Destroy(outputMix);
        (*engineObject)->Destroy(engineObject);
        return EXIT_FAILURE;
    }

    SLresult bqRes = (*playerObject)->GetInterface(playerObject, SL_IID_BUFFERQUEUE, &bqItf);
    if (bqRes != SL_RESULT_FEATURE_UNSUPPORTED && bqRes != SL_RESULT_SUCCESS) {
        fprintf(stderr, "Unexpected BUFFERQUEUE interface result on URI player: %lu\n",
                (unsigned long) bqRes);
        (*playerObject)->Destroy(playerObject);
        (*outputMix)->Destroy(outputMix);
        (*engineObject)->Destroy(engineObject);
        return EXIT_FAILURE;
    }

    (*playerObject)->Destroy(playerObject);
    (*outputMix)->Destroy(outputMix);
    (*engineObject)->Destroy(engineObject);

    fprintf(stdout, "slesTestUriMimeHost completed\n");
    return EXIT_SUCCESS;
}
