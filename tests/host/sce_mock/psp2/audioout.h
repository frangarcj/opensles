#ifndef OPENSLES_MOCK_AUDIOOUT_H
#define OPENSLES_MOCK_AUDIOOUT_H
#define SCE_AUDIO_OUT_PORT_TYPE_BGM 1
#define SCE_AUDIO_OUT_MODE_STEREO 1
#define SCE_AUDIO_VOLUME_FLAG_L_CH 1
#define SCE_AUDIO_VOLUME_FLAG_R_CH 2
int sceAudioOutOpenPort(int type, int length, int frequency, int mode);
int sceAudioOutSetVolume(int port, int channels, int *volume);
int sceAudioOutOutput(int port, const void *buffer);
int sceAudioOutReleasePort(int port);
#endif
