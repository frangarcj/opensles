#include "SLES/OpenSLES.h"
#include "SLES/OpenSLESUT.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

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

    for (SLuint32 objectID = 0x1000; objectID <= 0x100B; ++objectID) {
        SLuint32 count = 0;
        result = (*engineItf)->QueryNumSupportedInterfaces(engineItf, objectID, &count);
        if (result == SL_RESULT_FEATURE_UNSUPPORTED) {
            continue;
        }
        assert(SL_RESULT_SUCCESS == result);
        for (SLuint32 i = 0; i < count; ++i) {
            SLInterfaceID iid;
            result = (*engineItf)->QuerySupportedInterfaces(engineItf, objectID, i, &iid);
            assert(SL_RESULT_SUCCESS == result);
            (void) iid;
        }
    }

    (*engineObject)->Destroy(engineObject);
    fprintf(stdout, "slesTestObjectHost completed\n");
    return EXIT_SUCCESS;
}
