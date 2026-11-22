#include <psp2/audioout.h>
#include "mock_control.h"

#include <assert.h>
#include <stdatomic.h>
#include <stddef.h>
#include <unistd.h>

static atomic_int open_ports[8];

int sceAudioOutOpenPort(int type, int length, int frequency, int mode)
{
    assert(type == SCE_AUDIO_OUT_PORT_TYPE_BGM && length > 0 && frequency > 0);
    assert(mode == SCE_AUDIO_OUT_MODE_STEREO);
    int error = mock_injected(MOCK_AUDIO_OPEN);
    if (error != 0) return error;
    for (int i = 0; i < 8; ++i) {
        int expected = 0;
        if (atomic_compare_exchange_strong(&open_ports[i], &expected, 1)) {
            mock_resource_change(1);
            return i;
        }
    }
    return -1;
}

int sceAudioOutSetVolume(int port, int channels, int *volume)
{
    assert(port >= 0 && port < 8 && atomic_load(&open_ports[port]));
    assert(channels == (SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH));
    assert(volume[0] == 32767 && volume[1] == 32767);
    return mock_injected(MOCK_AUDIO_VOLUME);
}

int sceAudioOutOutput(int port, const void *buffer)
{
    assert(port >= 0 && port < 8 && atomic_load(&open_ports[port]));
    assert(buffer != NULL);
    int error = mock_injected(MOCK_AUDIO_OUTPUT);
    if (error != 0) return error;
    usleep(2000);
    return 0;
}

int sceAudioOutReleasePort(int port)
{
    assert(port >= 0 && port < 8);
    assert(atomic_exchange(&open_ports[port], 0) == 1);
    mock_resource_change(-1);
    return 0;
}
