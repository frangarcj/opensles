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

#define LOG_NDEBUG 0
#define LOG_TAG "slesTest_virtualizer"

#include <utils/Log.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>

#include "SLES/OpenSLES.h"
#include "SLES/OpenSLES_Android.h"


#define MAX_NUMBER_INTERFACES 3

#define GUID_DISPLAY_LENGTH 35
#define FX_NAME_LENGTH 64

static int testMode;
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
void guidToString(const SLInterfaceID guid, char *str) {
    if ((NULL == guid) || (NULL == str)) {
        return;
    }
    snprintf(str, GUID_DISPLAY_LENGTH, "%08lx-%04x-%04x-%04x-%02x%02x%02x%02x%02x%02x",
            guid->time_low,
            guid->time_mid,
            guid->time_hi_and_version,
            guid->clock_seq,
            guid->node[0],
            guid->node[1],
            guid->node[2],
            guid->node[3],
            guid->node[4],
            guid->node[5]);
}

//-----------------------------------------------------------------

/* Query available effects on Android  */
void TestGenericFxCapabilities(  )
{

    SLresult    result;
    SLObjectItf sl;

    /* ------------------------------------------------------ */
    /* Engine configuration and creation */

    SLEngineOption EngineOption[] = {
            {(SLuint32) SL_ENGINEOPTION_THREADSAFE, (SLuint32) SL_BOOLEAN_TRUE}
    };

    SLboolean required[MAX_NUMBER_INTERFACES];
    SLInterfaceID iidArray[MAX_NUMBER_INTERFACES];

    /* Initialize arrays required[] and iidArray[] */
    for (int i=0 ; i < MAX_NUMBER_INTERFACES ; i++) {
        required[i] = SL_BOOLEAN_FALSE;
        iidArray[i] = SL_IID_NULL;
    }

    iidArray[0] = SL_IID_ANDROIDEFFECTCAPABILITIES;
    required[0] = SL_BOOLEAN_TRUE;


    result = slCreateEngine( &sl, 1, EngineOption, 1, iidArray, required);
    ExitOnError(result);

    /* Realizing the SL Engine in synchronous mode. */
    result = (*sl)->Realize(sl, SL_BOOLEAN_FALSE);
    ExitOnError(result);


    SLEngineItf EngineItf;
    SLAndroidEffectCapabilitiesItf EffectLibItf;

    /* Get the SL Engine interface which is implicit */
    result = (*sl)->GetInterface(sl, SL_IID_ENGINE, (void*)&EngineItf);
    ExitOnError(result);

    /* Get the Android Effect Capabilities interface */
    result = (*sl)->GetInterface(sl, SL_IID_ANDROIDEFFECTCAPABILITIES, (void*)&EffectLibItf);
    ExitOnError(result);

    /* ------------------------------------------------------ */
    /* Query the effect library */

    SLuint32 nbEffects = 0;
    result = (*EffectLibItf)->QueryNumEffects(EffectLibItf, &nbEffects);
    ExitOnError(result);
    fprintf(stdout, "Effect library contains %ld effects:\n", nbEffects);

    SLchar effectName[FX_NAME_LENGTH];
    SLuint16 effectNameLength = FX_NAME_LENGTH;
    char typeString[GUID_DISPLAY_LENGTH];
    char implString[GUID_DISPLAY_LENGTH];

    SLInterfaceID effectType, effectImplementation;
    for (SLuint32 i = 0 ; i < nbEffects ; i++ ) {
        fprintf(stdout,"- effect %ld: ", i);
        result = (*EffectLibItf)->QueryEffect(EffectLibItf, i,
                &effectType, &effectImplementation, effectName, &effectNameLength);
        ExitOnError(result);
        guidToString(effectType, typeString);
        guidToString(effectImplementation, implString);
        fprintf(stdout, "type = %s impl = %s name = %s \n", typeString, implString, effectName);
    }

    /* Shutdown OpenSL ES */
     (*sl)->Destroy(sl);
}

//-----------------------------------------------------------------
int main(int argc, char* const argv[])
{
    LOGV("Starting %s\n", argv[0]);

    SLresult    result;
    SLObjectItf sl;

    fprintf(stdout, "OpenSL ES test %s: exercises SLAndroidEffectCapabilitiesItf.\n", argv[0]);

    TestGenericFxCapabilities();

    return 0;
}
