/*
 * Copyright (C) 2026
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <SLES/OpenSLES.h>

#define MAX_NUMBER_INTERFACES 2
#define MAX_NUMBER_PLAYERS 40
#define DEFAULT_ROUNDS 40
#define DEFAULT_SETTLE_MS 10000
#define PREFETCH_WAIT_STEPS 10

static SLObjectItf slEngine;
static SLEngineItf engineItf;
static SLObjectItf outputMix;

static SLboolean required[MAX_NUMBER_INTERFACES];
static SLInterfaceID iidArray[MAX_NUMBER_INTERFACES];

static SLObjectItf audioPlayer[MAX_NUMBER_PLAYERS];
static SLPlayItf playItfs[MAX_NUMBER_PLAYERS];
static SLPrefetchStatusItf prefetchItfs[MAX_NUMBER_PLAYERS];
static bool validPlayer[MAX_NUMBER_PLAYERS];

static SLDataSource audioSource;
static SLDataLocator_URI uri;
static SLDataFormat_MIME mime;

static SLDataSink audioSink;
static SLDataLocator_OutputMix locator_outputmix;

static void exitOnError(SLresult result, int line)
{
    if (SL_RESULT_SUCCESS != result) {
        fprintf(stderr, "Error %lu at line %d\n", result, line);
        exit(EXIT_FAILURE);
    }
}

#define CHECK(x) exitOnError((x), __LINE__)

static void setup(const char *path)
{
    SLresult res;

    SLEngineOption engineOption[] = {
        {(SLuint32) SL_ENGINEOPTION_THREADSAFE, (SLuint32) SL_BOOLEAN_TRUE}
    };

    res = slCreateEngine(&slEngine, 1, engineOption, 0, NULL, NULL);
    CHECK(res);
    res = (*slEngine)->Realize(slEngine, SL_BOOLEAN_FALSE);
    CHECK(res);
    res = (*slEngine)->GetInterface(slEngine, SL_IID_ENGINE, (void *) &engineItf);
    CHECK(res);

    res = (*engineItf)->CreateOutputMix(engineItf, &outputMix, 0, iidArray, required);
    CHECK(res);
    res = (*outputMix)->Realize(outputMix, SL_BOOLEAN_FALSE);
    CHECK(res);

    uri.locatorType = SL_DATALOCATOR_URI;
    uri.URI = (SLchar *) path;
    mime.formatType = SL_DATAFORMAT_MIME;
    mime.mimeType = (SLchar *) NULL;
    mime.containerType = SL_CONTAINERTYPE_UNSPECIFIED;

    audioSource.pFormat = (void *) &mime;
    audioSource.pLocator = (void *) &uri;

    locator_outputmix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
    locator_outputmix.outputMix = outputMix;
    audioSink.pLocator = (void *) &locator_outputmix;
    audioSink.pFormat = NULL;

    for (int i = 0; i < MAX_NUMBER_INTERFACES; i++) {
        required[i] = SL_BOOLEAN_FALSE;
        iidArray[i] = SL_IID_NULL;
    }
    required[0] = SL_BOOLEAN_TRUE;
    iidArray[0] = SL_IID_PREFETCHSTATUS;
    required[1] = SL_BOOLEAN_TRUE;
    iidArray[1] = SL_IID_VOLUME;
}

static void teardown()
{
    (*outputMix)->Destroy(outputMix);
    (*slEngine)->Destroy(slEngine);
}

static int createPlayer(int playerId)
{
    SLresult res;

    validPlayer[playerId] = false;
    audioPlayer[playerId] = NULL;
    playItfs[playerId] = NULL;
    prefetchItfs[playerId] = NULL;

    res = (*engineItf)->CreateAudioPlayer(engineItf, &audioPlayer[playerId],
            &audioSource, &audioSink, MAX_NUMBER_INTERFACES, iidArray, required);
    if (SL_RESULT_SUCCESS != res) {
        return 0;
    }

    res = (*audioPlayer[playerId])->Realize(audioPlayer[playerId], SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != res) {
        (*audioPlayer[playerId])->Destroy(audioPlayer[playerId]);
        audioPlayer[playerId] = NULL;
        return 0;
    }

    res = (*audioPlayer[playerId])->GetInterface(audioPlayer[playerId], SL_IID_PLAY,
            (void *) &playItfs[playerId]);
    CHECK(res);

    res = (*audioPlayer[playerId])->GetInterface(audioPlayer[playerId], SL_IID_PREFETCHSTATUS,
            (void *) &prefetchItfs[playerId]);
    CHECK(res);

    res = (*playItfs[playerId])->SetPlayState(playItfs[playerId], SL_PLAYSTATE_PAUSED);
    CHECK(res);

    SLuint32 prefetchStatus = SL_PREFETCHSTATUS_UNDERFLOW;
    int waitSteps = PREFETCH_WAIT_STEPS;
    while ((prefetchStatus != SL_PREFETCHSTATUS_SUFFICIENTDATA) && (waitSteps-- > 0)) {
        usleep(100 * 1000);
        res = (*prefetchItfs[playerId])->GetPrefetchStatus(prefetchItfs[playerId], &prefetchStatus);
        CHECK(res);
    }
    if (prefetchStatus != SL_PREFETCHSTATUS_SUFFICIENTDATA) {
        fprintf(stderr, "Prefetch timeout in round player=%d\n", playerId);
        (*audioPlayer[playerId])->Destroy(audioPlayer[playerId]);
        audioPlayer[playerId] = NULL;
        return -1;
    }

    res = (*playItfs[playerId])->SetPlayState(playItfs[playerId], SL_PLAYSTATE_PLAYING);
    CHECK(res);
    validPlayer[playerId] = true;
    return 1;
}

static void destroyValidPlayers()
{
    for (int i = 0; i < MAX_NUMBER_PLAYERS; i++) {
        if (validPlayer[i] && audioPlayer[i] != NULL) {
            if (playItfs[i] != NULL) {
                (void) (*playItfs[i])->SetPlayState(playItfs[i], SL_PLAYSTATE_STOPPED);
            }
            (*audioPlayer[i])->Destroy(audioPlayer[i]);
            audioPlayer[i] = NULL;
            playItfs[i] = NULL;
            prefetchItfs[i] = NULL;
            validPlayer[i] = false;
        }
    }
}

int main(int argc, char *const argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <path> [rounds] [settle_ms]\n", argv[0]);
        return EXIT_FAILURE;
    }

    int rounds = DEFAULT_ROUNDS;
    int settleMs = DEFAULT_SETTLE_MS;
    if (argc >= 3) {
        rounds = atoi(argv[2]);
        if (rounds <= 0) {
            rounds = DEFAULT_ROUNDS;
        }
    }
    if (argc >= 4) {
        settleMs = atoi(argv[3]);
        if (settleMs < 0) {
            settleMs = DEFAULT_SETTLE_MS;
        }
    }

    fprintf(stdout, "OpenSL ES churn test: rounds=%d settleMs=%d maxPlayers=%d\n",
            rounds, settleMs, MAX_NUMBER_PLAYERS);

    setup(argv[1]);

    int expectedValidCount = -1;
    for (int round = 0; round < rounds; round++) {
        int validCount = 0;
        for (int playerId = 0; playerId < MAX_NUMBER_PLAYERS; playerId++) {
            int state = createPlayer(playerId);
            if (state < 0) {
                destroyValidPlayers();
                teardown();
                return EXIT_FAILURE;
            }
            if (state > 0) {
                validCount++;
            }
        }

        if (expectedValidCount < 0) {
            expectedValidCount = validCount;
        } else if (validCount != expectedValidCount) {
            fprintf(stderr, "Churn drift at round %d: expected %d got %d\n",
                    round, expectedValidCount, validCount);
            destroyValidPlayers();
            teardown();
            return EXIT_FAILURE;
        }

        fprintf(stdout, "Round %d/%d: validPlayers=%d\n", round + 1, rounds, validCount);
        usleep(settleMs * 1000);
        destroyValidPlayers();
        usleep(50 * 1000);
    }

    teardown();
    fprintf(stdout, "Churn test completed successfully\n");
    return EXIT_SUCCESS;
}
