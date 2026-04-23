/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <SLES/OpenSLES.h>


#define MAX_NUMBER_INTERFACES 2
#define MAX_NUMBER_PLAYERS 40

#define PREFETCHEVENT_ERROR_CANDIDATE \
        (SL_PREFETCHEVENT_STATUSCHANGE | SL_PREFETCHEVENT_FILLLEVELCHANGE)

SLObjectItf slEngine;
SLEngineItf engineItf;
SLObjectItf outputMix;

SLboolean required[MAX_NUMBER_INTERFACES];
SLInterfaceID iidArray[MAX_NUMBER_INTERFACES];

SLObjectItf audioPlayer[MAX_NUMBER_PLAYERS];
bool validplayer[MAX_NUMBER_PLAYERS];
int playerNum[MAX_NUMBER_PLAYERS];
SLPlayItf playItfs[MAX_NUMBER_PLAYERS];
SLVolumeItf volItfs[MAX_NUMBER_PLAYERS];
SLPrefetchStatusItf prefetchItfs[MAX_NUMBER_PLAYERS];

SLDataSource audioSource;
SLDataLocator_URI uri;
SLDataFormat_MIME mime;

SLDataSink audioSink;
SLDataLocator_OutputMix locator_outputmix;


#define CheckErr(x) ExitOnErrorFunc(x, -1, __LINE__)
#define CheckErrPlyr(x, id) ExitOnErrorFunc(x, id, __LINE__)

void ExitOnErrorFunc(SLresult result, int playerId, int line)
{
    if (SL_RESULT_SUCCESS != result) {
        if (playerId == -1) {
            fprintf(stderr, "Error %u code encountered at line %d, exiting\n", result, line);
        } else {
            fprintf(stderr, "Error %u code encountered at line %d for player %d, exiting\n",
                    result, line, playerId);
        }
        exit(EXIT_FAILURE);
    }
}


void PrefetchEventCallback(SLPrefetchStatusItf caller, void *pContext, SLuint32 event)
{
    SLresult res;
    SLpermille level = 0;
    int *pPlayerId = (int *) pContext;
    res = (*caller)->GetFillLevel(caller, &level);
    CheckErrPlyr(res, *pPlayerId);
    SLuint32 status;
    res = (*caller)->GetPrefetchStatus(caller, &status);
    CheckErrPlyr(res, *pPlayerId);
    if ((PREFETCHEVENT_ERROR_CANDIDATE == (event & PREFETCHEVENT_ERROR_CANDIDATE)) &&
            (level == 0) && (status == SL_PREFETCHSTATUS_UNDERFLOW)) {
        fprintf(stdout, "PrefetchEventCallback: Error while prefetching data for player %d, "
                "exiting\n", *pPlayerId);
        exit(EXIT_FAILURE);
    }
    if (event & SL_PREFETCHEVENT_FILLLEVELCHANGE) {
        fprintf(stdout, "PrefetchEventCallback: Buffer fill level is = %d for player %d\n",
                level, *pPlayerId);
    }
    if (event & SL_PREFETCHEVENT_STATUSCHANGE) {
        fprintf(stdout, "PrefetchEventCallback: Prefetch Status is = %u for player %d\n",
                status, *pPlayerId);
    }
}


void PlayEventCallback(SLPlayItf caller, void *pContext, SLuint32 event)
{
    SLresult res;
    int *pPlayerId = (int *) pContext;
    if (SL_PLAYEVENT_HEADATEND & event) {
        fprintf(stdout, "SL_PLAYEVENT_HEADATEND reached for player %d\n", *pPlayerId);
    }

    if (SL_PLAYEVENT_HEADATNEWPOS & event) {
        SLmillisecond pMsec = 0;
        res = (*caller)->GetPosition(caller, &pMsec);
        CheckErrPlyr(res, *pPlayerId);
        fprintf(stdout, "SL_PLAYEVENT_HEADATNEWPOS current position=%ums for player %d\n",
                pMsec, *pPlayerId);
    }

    if (SL_PLAYEVENT_HEADATMARKER & event) {
        SLmillisecond pMsec = 0;
        res = (*caller)->GetPosition(caller, &pMsec);
        CheckErrPlyr(res, *pPlayerId);
        fprintf(stdout, "SL_PLAYEVENT_HEADATMARKER current position=%ums for player %d\n",
                pMsec, *pPlayerId);
    }
}


void TestSetup(const char *path)
{
    SLresult res;

    SLEngineOption EngineOption[] = {
        {(SLuint32) SL_ENGINEOPTION_THREADSAFE, (SLuint32) SL_BOOLEAN_TRUE}
    };

    res = slCreateEngine(&slEngine, 1, EngineOption, 0, NULL, NULL);
    CheckErr(res);
    res = (*slEngine)->Realize(slEngine, SL_BOOLEAN_FALSE);
    CheckErr(res);
    res = (*slEngine)->GetInterface(slEngine, SL_IID_ENGINE, (void *) &engineItf);
    CheckErr(res);

    res = (*engineItf)->CreateOutputMix(engineItf, &outputMix, 0, iidArray, required);
    CheckErr(res);
    res = (*outputMix)->Realize(outputMix, SL_BOOLEAN_FALSE);
    CheckErr(res);

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
    iidArray[0] = SL_IID_VOLUME;
    required[1] = SL_BOOLEAN_TRUE;
    iidArray[1] = SL_IID_PREFETCHSTATUS;

    fprintf(stdout, "TestSetup(%s) completed\n", path);
}


void TestTeardown()
{
    (*outputMix)->Destroy(outputMix);
    (*slEngine)->Destroy(slEngine);
}


void CreatePlayer(int playerId)
{
    SLresult res;
    playerNum[playerId] = playerId;

    res = (*engineItf)->CreateAudioPlayer(engineItf, &audioPlayer[playerId],
            &audioSource, &audioSink, MAX_NUMBER_INTERFACES, iidArray, required);
    if (SL_RESULT_SUCCESS != res) {
        fprintf(stdout, "CreateAudioPlayer for player %d failed\n", playerId);
        validplayer[playerId] = false;
        return;
    }
    validplayer[playerId] = true;

    res = (*audioPlayer[playerId])->Realize(audioPlayer[playerId], SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != res) {
        fprintf(stdout, "Realize for player %d failed\n", playerId);
        return;
    }

    res = (*audioPlayer[playerId])->GetInterface(audioPlayer[playerId], SL_IID_PLAY,
            (void *) &playItfs[playerId]);
    CheckErrPlyr(res, playerId);

    res = (*audioPlayer[playerId])->GetInterface(audioPlayer[playerId], SL_IID_VOLUME,
            (void *) &volItfs[playerId]);
    CheckErrPlyr(res, playerId);

    res = (*audioPlayer[playerId])->GetInterface(audioPlayer[playerId], SL_IID_PREFETCHSTATUS,
            (void *) &prefetchItfs[playerId]);
    CheckErrPlyr(res, playerId);
    res = (*prefetchItfs[playerId])->RegisterCallback(prefetchItfs[playerId],
            PrefetchEventCallback, &playerNum[playerId]);
    CheckErrPlyr(res, playerId);
    res = (*prefetchItfs[playerId])->SetCallbackEventsMask(prefetchItfs[playerId],
            SL_PREFETCHEVENT_FILLLEVELCHANGE | SL_PREFETCHEVENT_STATUSCHANGE);
    CheckErrPlyr(res, playerId);

    res = (*volItfs[playerId])->SetVolumeLevel(volItfs[playerId], -300);
    CheckErrPlyr(res, playerId);

    res = (*playItfs[playerId])->SetMarkerPosition(playItfs[playerId], 2000);
    CheckErrPlyr(res, playerId);
    res = (*playItfs[playerId])->SetPositionUpdatePeriod(playItfs[playerId], 500);
    CheckErrPlyr(res, playerId);
    res = (*playItfs[playerId])->SetCallbackEventsMask(playItfs[playerId],
            SL_PLAYEVENT_HEADATMARKER | SL_PLAYEVENT_HEADATNEWPOS | SL_PLAYEVENT_HEADATEND);
    CheckErrPlyr(res, playerId);
    res = (*playItfs[playerId])->RegisterCallback(playItfs[playerId], PlayEventCallback,
            &playerNum[playerId]);
    CheckErrPlyr(res, playerId);

    (*prefetchItfs[playerId])->SetFillUpdatePeriod(prefetchItfs[playerId], 50);

    fprintf(stdout, "Setting player %d  to PAUSED\n", playerId);
    res = (*playItfs[playerId])->SetPlayState(playItfs[playerId], SL_PLAYSTATE_PAUSED);
    CheckErrPlyr(res, playerId);
    SLuint32 prefetchStatus = SL_PREFETCHSTATUS_UNDERFLOW;
    SLuint32 timeOutIndex = 10;
    while ((prefetchStatus != SL_PREFETCHSTATUS_SUFFICIENTDATA) && (timeOutIndex > 0)) {
        usleep(100 * 1000);
        res = (*prefetchItfs[playerId])->GetPrefetchStatus(prefetchItfs[playerId],
                &prefetchStatus);
        CheckErrPlyr(res, playerId);
        timeOutIndex--;
    }

    if (timeOutIndex == 0) {
        fprintf(stderr, "Prefetch timed out for player %d\n", playerId);
        return;
    }
    res = (*playItfs[playerId])->SetPlayState(playItfs[playerId], SL_PLAYSTATE_PLAYING);
    CheckErrPlyr(res, playerId);

    SLmillisecond durationInMsec = SL_TIME_UNKNOWN;
    res = (*playItfs[playerId])->GetDuration(playItfs[playerId], &durationInMsec);
    CheckErrPlyr(res, playerId);
    if (durationInMsec == SL_TIME_UNKNOWN) {
        fprintf(stdout, "Content duration is unknown for player %d\n", playerId);
    } else {
        fprintf(stdout, "Content duration is %u ms for player %d\n", durationInMsec, playerId);
    }
}


void DestroyPlayer(int playerId)
{
    fprintf(stdout, "About to destroy player %d\n", playerId);
    (*audioPlayer[playerId])->Destroy(audioPlayer[playerId]);
}


int main(int argc, char *const argv[])
{
    fprintf(stdout, "OpenSL ES test %s: creates and destroys as many ", argv[0]);
    fprintf(stdout, "AudioPlayer objects as possible (max=%d)\n\n", MAX_NUMBER_PLAYERS);

    if (argc == 1) {
        fprintf(stdout, "Usage: %s path \n\t%s url\n", argv[0], argv[0]);
        fprintf(stdout, "Example: \"%s /sdcard/my.mp3\"  or \"%s file:///sdcard/my.mp3\"\n",
                argv[0], argv[0]);
        exit(EXIT_FAILURE);
    }

    TestSetup(argv[1]);

    for (int i = 0; i < MAX_NUMBER_PLAYERS; i++) {
        CreatePlayer(i);
    }
    fprintf(stdout, "After creating %d AudioPlayers\n", MAX_NUMBER_PLAYERS);

    usleep(10 * 1000 * 1000);

    for (int i = 0; i < MAX_NUMBER_PLAYERS; i++) {
        if (validplayer[i]) {
            DestroyPlayer(i);
        }
    }
    fprintf(stdout, "After destroying valid players among %d AudioPlayers\n",
            MAX_NUMBER_PLAYERS);

    TestTeardown();

    return EXIT_SUCCESS;
}