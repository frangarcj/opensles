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

#include "sles_allinclusive.h"
#include <psp2/audioout.h>
#include <psp2/kernel/threadmgr.h>
#include <stdatomic.h>

#if defined(HAVE_PTHREAD) || defined(HAVE_SCE_THREADS)
#define USE_THREAD_BACKEND 1
#endif

typedef struct {
    IEngine *engine;
    atomic_int shutdown;
    int port;
#ifdef USE_THREAD_BACKEND
    sles_thread_t thread;
#else
    SceUID thread;
#endif
    uint8_t buffers[SndFile_NUMBUFS][SndFile_BUFSIZE];
} VitaAudio;

static int output_frequency(void)
{
    return &_opensles_user_freq != NULL && _opensles_user_freq > 0 ?
        _opensles_user_freq : 44100;
}

static void fill_output_buffer(VitaAudio *audio, uint8_t *stream)
{
    memset(stream, 0, SndFile_BUFSIZE);
    interface_lock_exclusive(audio->engine);
    COutputMix *mix = audio->engine->mOutputMix;
    interface_unlock_exclusive(audio->engine);
    if (mix != NULL) {
        IOutputMixExt_FillBuffer(&mix->mOutputMixExt.mItf, stream, SndFile_BUFSIZE);
    }
}

static void *audio_thread(void *arg)
{
    VitaAudio *audio = (VitaAudio *)arg;
    unsigned index = 0;
    int output_failed = 0;
    while (!atomic_load_explicit(&audio->shutdown, memory_order_acquire)) {
        uint8_t *stream = audio->buffers[index];
        index = (index + 1) % SndFile_NUMBUFS;
        fill_output_buffer(audio, stream);
        if (!output_failed) {
            int result = sceAudioOutOutput(audio->port, stream);
            if (result < 0) {
                SL_LOGE("Vita audio output failed on port %d: 0x%x", audio->port, result);
                output_failed = 1;
            }
        }
        if (output_failed) {
            // Clear, Stop and Destroy still need mixer acknowledgements after device failure.
            sceKernelDelayThread(10000);
        }
    }
    return NULL;
}

#ifndef USE_THREAD_BACKEND
static int legacy_audio_thread(SceSize size, void *arg)
{
    if (size != sizeof(VitaAudio *)) {
        return EINVAL;
    }
    VitaAudio *audio;
    memcpy(&audio, arg, sizeof(audio));
    audio_thread(audio);
    return 0;
}
#endif

SLresult SDL_open(IEngine *engine)
{
    VitaAudio *audio = (VitaAudio *)calloc(1, sizeof(*audio));
    if (audio == NULL) {
        return SL_RESULT_MEMORY_FAILURE;
    }
    audio->engine = engine;
    atomic_init(&audio->shutdown, 0);
    audio->port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, SndFile_BUFSIZE / 4,
        output_frequency(), SCE_AUDIO_OUT_MODE_STEREO);
    if (audio->port < 0) {
        SL_LOGE("Unable to open Vita audio port: 0x%x", audio->port);
        free(audio);
        return SL_RESULT_RESOURCE_ERROR;
    }
    int volume[] = {32767, 32767};
    int result = sceAudioOutSetVolume(audio->port,
        SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, volume);
    if (result < 0) {
        SL_LOGE("Unable to set Vita audio volume: 0x%x", result);
        goto fail;
    }
#ifdef USE_THREAD_BACKEND
    result = sles_thread_create(&audio->thread, audio_thread, audio);
    if (result != 0) {
        SL_LOGE("Unable to create OpenSLES playback thread: %d", result);
        goto fail;
    }
#else
    audio->thread = sceKernelCreateThread("OpenSLES Playback", legacy_audio_thread,
        0x10000100, 0x10000, 0, 0, NULL);
    if (audio->thread < 0) {
        result = audio->thread;
        goto fail;
    }
    result = sceKernelStartThread(audio->thread, sizeof(audio), &audio);
    if (result < 0) {
        sceKernelDeleteThread(audio->thread);
        goto fail;
    }
#endif
    engine->mAudioBackend = audio;
    return SL_RESULT_SUCCESS;

fail:
    sceAudioOutReleasePort(audio->port);
    free(audio);
    return SL_RESULT_RESOURCE_ERROR;
}

void SDL_close(IEngine *engine)
{
    VitaAudio *audio = (VitaAudio *)engine->mAudioBackend;
    if (audio == NULL) {
        return;
    }
    atomic_store_explicit(&audio->shutdown, 1, memory_order_release);
#ifdef USE_THREAD_BACKEND
    int result = sles_thread_join(audio->thread);
#else
    int result = sceKernelWaitThreadEnd(audio->thread, NULL, NULL);
    if (result >= 0) {
        result = sceKernelDeleteThread(audio->thread);
    }
#endif
    if (result != 0) {
        SL_LOGE("Unable to join OpenSLES playback thread: %d", result);
        abort();
    }
    sceAudioOutReleasePort(audio->port);
    engine->mAudioBackend = NULL;
    free(audio);
}
