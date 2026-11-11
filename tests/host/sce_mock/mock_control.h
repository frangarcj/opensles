#ifndef OPENSLES_MOCK_CONTROL_H
#define OPENSLES_MOCK_CONTROL_H
enum MockOperation {
    MOCK_NONE,
    MOCK_MUTEX_CREATE,
    MOCK_COND_CREATE,
    MOCK_THREAD_CREATE,
    MOCK_THREAD_START,
    MOCK_AUDIO_OPEN,
    MOCK_AUDIO_VOLUME,
    MOCK_AUDIO_OUTPUT
};
void mock_fail_at(enum MockOperation operation, int ordinal, int error);
int mock_live_resources(void);
int mock_injected(enum MockOperation operation);
void mock_resource_change(int delta);
#endif
