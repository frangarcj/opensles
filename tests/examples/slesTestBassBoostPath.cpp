/*
 * Copyright (C) 2010 The Android Open Source Project
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

#ifdef ANDROID
#define LOG_NDEBUG 0
#define LOG_TAG "slesTest_bassboost"

#include <utils/Log.h>
#else
#define LOGV printf
#endif

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>

#include "SLES/OpenSLES.h"
#ifdef ANDROID
#include "SLES/OpenSLES_Android.h"
#endif


#define MAX_NUMBER_INTERFACES 3

#define TIME_S_BETWEEN_BB_ON_OFF 3

//-----------------------------------------------------------------
/* Exits the application if an error is encountered */
#define ExitOnError(x) ExitOnErrorFunc(x,__LINE__)

void ExitOnErrorFunc( SLresult result , int line)
{
    if (SL_RESULT_SUCCESS != result) {
        fprintf(stderr, "%lu error code encountered at line %d, exiting\n", result, line);
        exit(1);
    }
}


//-----------------------------------------------------------------

/* Play an audio path by opening a file descriptor on that path  */
void TestBassBoostPathFromFD( SLObjectItf sl, const char* path, int16_t boostStrength)
{
    SLresult  result;
    SLEngineItf EngineItf;

    /* Objects this application uses: one player and an ouput mix */
    SLObjectItf  player, outputMix;

    /* Source of audio data to play */
    SLDataSource            audioSource;
#ifdef ANDROID
    SLDataLocator_AndroidFD locatorFd;
#else
    SLDataLocator_URI       locatorUri;
#endif
    SLDataFormat_MIME       mime;

    /* Data sinks for the audio player */
    SLDataSink               audioSink;
    SLDataLocator_OutputMix  locator_outputmix;

    /* Interfaces for the audio player */
    SLPlayItf              playItf;
    SLPrefetchStatusItf    prefetchItf;
    SLBassBoostItf         bbItf;

    SLboolean required[MAX_NUMBER_INTERFACES];
    SLInterfaceID iidArray[MAX_NUMBER_INTERFACES];

    /* Get the SL Engine Interface which is implicit */
    result = (*sl)->GetInterface(sl, SL_IID_ENGINE, (void*)&EngineItf);
    ExitOnError(result);

    /* Initialize arrays required[] and iidArray[] */
    for (int i=0 ; i < MAX_NUMBER_INTERFACES ; i++) {
        required[i] = SL_BOOLEAN_FALSE;
        iidArray[i] = SL_IID_NULL;
    }

    /* ------------------------------------------------------ */
    /* Configuration of the output mix  */

    /* Create Output Mix object to be used by the player */
     result = (*EngineItf)->CreateOutputMix(EngineItf, &outputMix, 1, iidArray, required);
     ExitOnError(result);

    /* Realize the Output Mix object in synchronous mode */
    result = (*outputMix)->Realize(outputMix, SL_BOOLEAN_FALSE);
    ExitOnError(result);

    /* Setup the data sink structure */
    locator_outputmix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
    locator_outputmix.outputMix   = outputMix;
    audioSink.pLocator            = (void*)&locator_outputmix;
    audioSink.pFormat             = NULL;

    /* ------------------------------------------------------ */
    /* Configuration of the player  */

    /* Set arrays required[] and iidArray[] for required interfaces */
    /*  (SLPlayItf is implicit) */
    required[0] = SL_BOOLEAN_TRUE;
    iidArray[0] = SL_IID_PREFETCHSTATUS;
    required[1] = SL_BOOLEAN_TRUE;
    iidArray[1] = SL_IID_BASSBOOST;

#ifdef ANDROID
    /* Setup the data source structure for the URI */
    locatorFd.locatorType = SL_DATALOCATOR_ANDROIDFD;
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        ExitOnError(SL_RESULT_RESOURCE_ERROR);
    }
    locatorFd.fd = (SLint32) fd;
    locatorFd.length = SL_DATALOCATOR_ANDROIDFD_USE_FILE_SIZE;
    locatorFd.offset = 0;
#else
    locatorUri.locatorType = SL_DATALOCATOR_URI;
    locatorUri.URI = (SLchar *) path;
#endif

    mime.formatType = SL_DATAFORMAT_MIME;
    /*     this is how ignored mime information is specified, according to OpenSL ES spec
     *     in 9.1.6 SLDataFormat_MIME and 8.23 SLMetadataTraversalItf GetChildInfo */
    mime.mimeType      = (SLchar*)NULL;
    mime.containerType = SL_CONTAINERTYPE_UNSPECIFIED;

    audioSource.pFormat  = (void*)&mime;
#ifdef ANDROID
    audioSource.pLocator = (void*)&locatorFd;
#else
    audioSource.pLocator = (void*)&locatorUri;
#endif

    /* Create the audio player */
    result = (*EngineItf)->CreateAudioPlayer(EngineItf, &player, &audioSource, &audioSink, 2,
            iidArray, required);
    ExitOnError(result);

    /* Realize the player in synchronous mode. */
    result = (*player)->Realize(player, SL_BOOLEAN_FALSE); ExitOnError(result);
    fprintf(stdout, "URI example: after Realize\n");

    /* Get the SLPlayItf, SLPrefetchStatusItf and SLBassBoostItf interfaces for the player */
    result = (*player)->GetInterface(player, SL_IID_PLAY, (void*)&playItf);
    ExitOnError(result);

    result = (*player)->GetInterface(player, SL_IID_PREFETCHSTATUS, (void*)&prefetchItf);
    ExitOnError(result);

    result = (*player)->GetInterface(player, SL_IID_BASSBOOST, (void*)&bbItf);
    ExitOnError(result);

    fprintf(stdout, "Player configured\n");

    /* ------------------------------------------------------ */
    /* Playback and test */

    /* Start the data prefetching by setting the player to the paused state */
    result = (*playItf)->SetPlayState( playItf, SL_PLAYSTATE_PAUSED );
    ExitOnError(result);

    /* Wait until there's data to play */
    SLuint32 prefetchStatus = SL_PREFETCHSTATUS_UNDERFLOW;
    while (prefetchStatus != SL_PREFETCHSTATUS_SUFFICIENTDATA) {
        usleep(100 * 1000);
        (*prefetchItf)->GetPrefetchStatus(prefetchItf, &prefetchStatus);
        ExitOnError(result);
    }

    /* Get duration */
    SLmillisecond durationInMsec = SL_TIME_UNKNOWN;
    result = (*playItf)->GetDuration(playItf, &durationInMsec);
    ExitOnError(result);
    if (durationInMsec == SL_TIME_UNKNOWN) {
        durationInMsec = 5000;
    }

    /* Start playback */
    fprintf(stdout, "Starting to play\n");
    result = (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PLAYING );
    ExitOnError(result);

    /* Configure BassBoost */
    SLboolean strengthSupported = SL_BOOLEAN_FALSE;
    result = (*bbItf)->IsStrengthSupported(bbItf, &strengthSupported);
    ExitOnError(result);
    if (SL_BOOLEAN_FALSE == strengthSupported) {
        fprintf(stdout, "BassBoost strength is not supported on this platform. Too bad!\n");
    } else {
        fprintf(stdout, "BassBoost strength is supported, setting strength to %d\n", boostStrength);
        result = (*bbItf)->SetStrength(bbItf, boostStrength);
        ExitOnError(result);
    }

    SLpermille strength = 0;
    result = (*bbItf)->GetRoundedStrength(bbItf, &strength);
    ExitOnError(result);
    fprintf(stdout, "Rounded strength of boost = %d\n", strength);


    /* Switch BassBoost on/off every TIME_S_BETWEEN_BB_ON_OFF seconds */
    SLboolean enabled = SL_BOOLEAN_TRUE;
    result = (*bbItf)->SetEnabled(bbItf, enabled);
    ExitOnError(result);
    for(unsigned int j=0 ; j<(durationInMsec/(1000*TIME_S_BETWEEN_BB_ON_OFF)) ; j++) {
        usleep(TIME_S_BETWEEN_BB_ON_OFF * 1000 * 1000);
        result = (*bbItf)->IsEnabled(bbItf, &enabled);
        ExitOnError(result);
        enabled = enabled == SL_BOOLEAN_TRUE ? SL_BOOLEAN_FALSE : SL_BOOLEAN_TRUE;
        result = (*bbItf)->SetEnabled(bbItf, enabled);
        if (SL_BOOLEAN_TRUE == enabled) {
            fprintf(stdout, "BassBoost on\n");
        } else {
            fprintf(stdout, "BassBoost off\n");
        }
        ExitOnError(result);
    }

    /* Make sure player is stopped */
    fprintf(stdout, "Stopping playback\n");
    result = (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_STOPPED);
    ExitOnError(result);

    /* Destroy the player */
    (*player)->Destroy(player);

    /* Destroy Output Mix object */
    (*outputMix)->Destroy(outputMix);

#ifdef ANDROID
    close(fd);
#endif
}

//-----------------------------------------------------------------
int main(int argc, char* const argv[])
{
    LOGV("Starting %s\n", argv[0]);

    SLresult    result;
    SLObjectItf sl;

    fprintf(stdout, "OpenSL ES test %s: exercises SLBassBoostItf ", argv[0]);
    fprintf(stdout, "and AudioPlayer with SLDataLocator_AndroidFD source / OutputMix sink\n");
    fprintf(stdout, "Plays the sound file designated by the given path, ");
    fprintf(stdout, "and applies a bass boost effect of the specified strength,\n");
    fprintf(stdout, "where strength is a integer value between 0 and 1000.\n");
    fprintf(stdout, "Every %d seconds, the BassBoost will be turned on and off.\n",
            TIME_S_BETWEEN_BB_ON_OFF);

    if (argc < 3) {
        fprintf(stdout, "Usage: \t%s path bass_boost_strength\n", argv[0]);
        fprintf(stdout, "Example: \"%s /sdcard/my.mp3 1000\" \n", argv[0]);
        exit(1);
    }

    SLEngineOption EngineOption[] = {
            {(SLuint32) SL_ENGINEOPTION_THREADSAFE, (SLuint32) SL_BOOLEAN_TRUE}
    };

    result = slCreateEngine( &sl, 1, EngineOption, 0, NULL, NULL);
    ExitOnError(result);

    /* Realizing the SL Engine in synchronous mode. */
    result = (*sl)->Realize(sl, SL_BOOLEAN_FALSE);
    ExitOnError(result);

    // intentionally not checking that argv[2], the bassboost strength, is between 0 and 1000
    TestBassBoostPathFromFD(sl, argv[1], (int16_t)atoi(argv[2]));

    /* Shutdown OpenSL ES */
    (*sl)->Destroy(sl);
    exit(0);

    return 0;
}
