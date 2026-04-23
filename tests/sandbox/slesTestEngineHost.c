#include "SLES/OpenSLES.h"
#include "SLES/OpenSLESUT.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    SLresult result;
    SLuint32 numSupportedInterfaces = 0;

    result = slQueryNumSupportedEngineInterfaces(&numSupportedInterfaces);
    assert(SL_RESULT_SUCCESS == result);

    SLInterfaceID *engineIds = (SLInterfaceID *) calloc(numSupportedInterfaces, sizeof(SLInterfaceID));
    SLboolean *engineReq = (SLboolean *) calloc(numSupportedInterfaces, sizeof(SLboolean));
    assert(engineIds != NULL);
    assert(engineReq != NULL);

    for (SLuint32 i = 0; i < numSupportedInterfaces; ++i) {
        SLInterfaceID interfaceID;
        memset(&interfaceID, 0, sizeof(interfaceID));
        result = slQuerySupportedEngineInterfaces(i, &interfaceID);
        assert(SL_RESULT_SUCCESS == result);
        engineIds[i] = interfaceID;
        engineReq[i] = SL_BOOLEAN_TRUE;
    }

    SLObjectItf engineObject;
    result = slCreateEngine(&engineObject, 0, NULL, numSupportedInterfaces, engineIds, engineReq);
    assert(SL_RESULT_SUCCESS == result);

    for (SLuint32 i = 0; i < numSupportedInterfaces; ++i) {
        void *iface = NULL;
        result = (*engineObject)->GetInterface(engineObject, engineIds[i], &iface);
        assert(SL_RESULT_PRECONDITIONS_VIOLATED == result);
    }

    (*engineObject)->Destroy(engineObject);

    result = slCreateEngine(&engineObject, 0, NULL, numSupportedInterfaces, engineIds, engineReq);
    assert(SL_RESULT_SUCCESS == result);
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);

    for (SLuint32 i = 0; i < numSupportedInterfaces; ++i) {
        void *iface = NULL;
        result = (*engineObject)->GetInterface(engineObject, engineIds[i], &iface);
        assert(SL_RESULT_SUCCESS == result);
        assert(iface != NULL);
    }

    (*engineObject)->Destroy(engineObject);
    free(engineIds);
    free(engineReq);

    fprintf(stdout, "slesTestEngineHost completed\n");
    return EXIT_SUCCESS;
}
