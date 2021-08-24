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

/** \file SDL.c SDL platform implementation */

#include "sles_allinclusive.h"
#include <vitasdk.h>

/** \brief Called by SDL to fill the next audio output buffer */
IEngine *slEngine;

uint8_t audio_buffers[SndFile_NUMBUFS][SndFile_BUFSIZE];

static int audioThread(unsigned int args, void* arg) {
	int ch = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, SndFile_BUFSIZE / 4, &_opensles_user_freq != NULL ? _opensles_user_freq : 44100, SCE_AUDIO_OUT_MODE_STEREO);
	sceAudioOutSetConfig(ch, -1, -1, (SceAudioOutMode)-1);
	
	int vol_stereo[] = {32767, 32767};
	sceAudioOutSetVolume(ch, (SceAudioOutChannelFlag)(SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH), vol_stereo);
	
	int buf_idx = 0;
	
	for (;;) {
		uint8_t *stream = audio_buffers[buf_idx];
		memset(stream, 0, (size_t)SndFile_BUFSIZE);
		buf_idx = (buf_idx + 1) % SndFile_NUMBUFS;
		
		// A peek lock would be risky if output mixes are dynamic, so we use SDL_PauseAudio to
		// temporarily disable callbacks during any change to the current output mix, and use a
		// shared lock here
		interface_lock_shared(slEngine);
		COutputMix *outputMix = slEngine->mOutputMix;
		interface_unlock_shared(slEngine);
		if (NULL != outputMix) {
			SLOutputMixExtItf OutputMixExt = &outputMix->mOutputMixExt.mItf;
			IOutputMixExt_FillBuffer(OutputMixExt, stream, (SLuint32)SndFile_BUFSIZE);
		}
		
		sceAudioOutOutput(ch, stream);
	}
	
	return sceKernelExitDeleteThread(0);
}

/** \brief Called during slCreateEngine */

void SDL_open(IEngine *thisEngine)
{
	slEngine = thisEngine;
	SceUID thd = sceKernelCreateThread("OpenSLES Playback", &audioThread, 0x10000100, 0x10000, 0, 0, NULL);
	sceKernelStartThread(thd, 0, NULL);
}


/** \brief Called during Object::Destroy */

void SDL_close(void)
{
}
