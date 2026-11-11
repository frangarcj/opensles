#include <SLES/OpenSLES.h>
#include <psp2/kernel/error.h>
#include "mock_control.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    alarm(30);
    unsigned injected_failures = 0;
    int last_operation = MOCK_THREAD_START;
#ifdef OPENSLES_TEST_VITA_AUDIO
    last_operation = MOCK_AUDIO_VOLUME;
#endif
    for (int operation = MOCK_MUTEX_CREATE; operation <= last_operation; ++operation) {
        for (int ordinal = 1; ordinal <= 8; ++ordinal) {
            fprintf(stderr, "failure case: operation=%d ordinal=%d\n", operation, ordinal);
            assert(mock_live_resources() == 0);
            mock_fail_at(operation, ordinal, SCE_KERNEL_ERROR_NO_MEMORY);
            SLObjectItf engine = NULL;
            SLresult result = slCreateEngine(&engine, 0, NULL, 0, NULL, NULL);
            if (result != SL_RESULT_SUCCESS) {
                ++injected_failures;
                assert(engine == NULL && mock_live_resources() == 0);
                mock_fail_at(MOCK_NONE, 0, 0);
                assert(slCreateEngine(&engine, 0, NULL, 0, NULL, NULL) == SL_RESULT_SUCCESS);
            }
            result = (*engine)->Realize(engine, SL_BOOLEAN_FALSE);
            mock_fail_at(MOCK_NONE, 0, 0);
            if (result != SL_RESULT_SUCCESS) {
                ++injected_failures;
                assert((*engine)->Realize(engine, SL_BOOLEAN_FALSE) == SL_RESULT_SUCCESS);
            }
            (*engine)->Destroy(engine);
            assert(mock_live_resources() == 0);
        }
    }
    assert(injected_failures >= 10);
    printf("Engine allocation/start failure cleanup and retry passed (%u injected failures)\n",
        injected_failures);
    alarm(0);
    return 0;
}
