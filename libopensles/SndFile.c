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

/** \brief libsndfile integration */

#include "sles_allinclusive.h"


static SLpermille sndfile_compute_fill_level(SLuint32 queuedBufferCount, SLuint16 totalBuffers)
{
    if ((0 == totalBuffers) || (0 == queuedBufferCount)) {
        return 0;
    }
    if (queuedBufferCount >= totalBuffers) {
        return 1000;
    }
    return (SLpermille) ((queuedBufferCount * 1000) / totalBuffers);
}


static void sndfile_update_prefetch_status(CAudioPlayer *audioPlayer, SLuint32 status)
{
    slPrefetchCallback callback = NULL;
    void *context = NULL;

    interface_lock_exclusive(&audioPlayer->mPrefetchStatus);
    if (audioPlayer->mPrefetchStatus.mStatus != status) {
        audioPlayer->mPrefetchStatus.mStatus = status;
        if (audioPlayer->mPrefetchStatus.mCallbackEventsMask & SL_PREFETCHEVENT_STATUSCHANGE) {
            callback = audioPlayer->mPrefetchStatus.mCallback;
            context = audioPlayer->mPrefetchStatus.mContext;
        }
    }
    interface_unlock_exclusive(&audioPlayer->mPrefetchStatus);

    if (NULL != callback) {
        (*callback)(&audioPlayer->mPrefetchStatus.mItf, context, SL_PREFETCHEVENT_STATUSCHANGE);
    }
}


static void sndfile_update_prefetch_fill_level(CAudioPlayer *audioPlayer, SLpermille level)
{
    slPrefetchCallback callback = NULL;
    void *context = NULL;

    interface_lock_exclusive(&audioPlayer->mPrefetchStatus);
    SLpermille oldLevel = audioPlayer->mPrefetchStatus.mLevel;
    SLpermille updatePeriod = audioPlayer->mPrefetchStatus.mFillUpdatePeriod;
    audioPlayer->mPrefetchStatus.mLevel = level;
    if ((oldLevel != level) &&
            (audioPlayer->mPrefetchStatus.mCallbackEventsMask & SL_PREFETCHEVENT_FILLLEVELCHANGE)) {
        SLpermille delta = (oldLevel > level) ? (oldLevel - level) : (level - oldLevel);
        if ((delta >= updatePeriod) || (0 == oldLevel) || (0 == level) ||
                (1000 == oldLevel) || (1000 == level)) {
            callback = audioPlayer->mPrefetchStatus.mCallback;
            context = audioPlayer->mPrefetchStatus.mContext;
        }
    }
    interface_unlock_exclusive(&audioPlayer->mPrefetchStatus);

    if (NULL != callback) {
        (*callback)(&audioPlayer->mPrefetchStatus.mItf, context,
            SL_PREFETCHEVENT_FILLLEVELCHANGE);
    }
}


void audioPlayerRefillBuffers(CAudioPlayer *audioPlayer)
{
    struct SndFile *this = &audioPlayer->mSndFile;

    if (NULL == this->mSNDFILE) {
        return;
    }

    for (;;) {
        object_lock_exclusive(&audioPlayer->mObject);
        SLuint32 queuedBufferCount = audioPlayer->mBufferQueue.mState.count;
        SLuint16 totalBuffers = audioPlayer->mBufferQueue.mNumBuffers;
        SLuint32 state = audioPlayer->mPlay.mState;
        object_unlock_exclusive(&audioPlayer->mObject);

        sndfile_update_prefetch_fill_level(audioPlayer,
            sndfile_compute_fill_level(queuedBufferCount, totalBuffers));

        if (((SL_PLAYSTATE_PLAYING != state) && (SL_PLAYSTATE_PAUSED != state)) ||
            (queuedBufferCount >= totalBuffers)) {
            return;
        }

        pthread_mutex_lock(&this->mMutex);
        if (this->mEOF) {
            pthread_mutex_unlock(&this->mMutex);
            return;
        }

        short *pBuffer = &this->mBuffer[this->mWhich * SndFile_BUFSIZE];
        if (++this->mWhich >= SndFile_NUMBUFS) {
            this->mWhich = 0;
        }

        sf_count_t count = sf_read_short(this->mSNDFILE, pBuffer, (sf_count_t) SndFile_BUFSIZE);
        pthread_mutex_unlock(&this->mMutex);

        if (0 < count) {
            SLresult result = IBufferQueue_Enqueue(&audioPlayer->mBufferQueue.mItf, pBuffer,
                (SLuint32) (count * sizeof(short)));
            if (SL_RESULT_SUCCESS != result) {
                SL_LOGE("enqueue failed 0x%lx", result);
                return;
            }
            sndfile_update_prefetch_status(audioPlayer, SL_PREFETCHSTATUS_SUFFICIENTDATA);
            continue;
        }

        object_lock_exclusive(&audioPlayer->mObject);
        slPlayCallback callback = NULL;
        void *context = NULL;
        queuedBufferCount = audioPlayer->mBufferQueue.mState.count;
        totalBuffers = audioPlayer->mBufferQueue.mNumBuffers;
        audioPlayer->mPlay.mState = SL_PLAYSTATE_PAUSED;
        if (!audioPlayer->mPlay.mHeadAtEnd) {
            audioPlayer->mPlay.mHeadAtEnd = SL_BOOLEAN_TRUE;
            if (audioPlayer->mPlay.mEventFlags & SL_PLAYEVENT_HEADATEND) {
                callback = audioPlayer->mPlay.mCallback;
                context = audioPlayer->mPlay.mContext;
            }
        }
        this->mEOF = SL_BOOLEAN_TRUE;
        object_unlock_exclusive_attributes(&audioPlayer->mObject, ATTR_PLAYSTATE);
        sndfile_update_prefetch_fill_level(audioPlayer,
            sndfile_compute_fill_level(queuedBufferCount, totalBuffers));
        if (NULL != callback) {
            (*callback)(&audioPlayer->mPlay.mItf, context, SL_PLAYEVENT_HEADATEND);
        }
        return;
    }
}


/** \brief Called by SndFile.c:audioPlayerTransportUpdate after a play state change or seek,
 *  and by IOutputMixExt::FillBuffer after each buffer is consumed.
 */

void SndFile_Callback(SLBufferQueueItf caller, void *pContext)
{
    CAudioPlayer *thisAP = (CAudioPlayer *) pContext;
    SLboolean headAtMarker = SL_BOOLEAN_FALSE;
    SLboolean headAtNewPos = SL_BOOLEAN_FALSE;
    object_lock_exclusive(&thisAP->mObject);
    if (SL_PLAYSTATE_PLAYING != thisAP->mPlay.mState) {
        object_unlock_exclusive(&thisAP->mObject);
        return;
    }
    slPlayCallback callback = thisAP->mPlay.mCallback;
    void *context = thisAP->mPlay.mContext;
    SLuint32 queuedBufferCount = thisAP->mBufferQueue.mState.count;
    SLuint16 totalBuffers = thisAP->mBufferQueue.mNumBuffers;
    audioPlayerHandlePositionUpdate(thisAP, &headAtMarker, &headAtNewPos);
    object_unlock_exclusive_attributes(&thisAP->mObject, ATTR_SNDREFILL);
    sndfile_update_prefetch_fill_level(thisAP,
        sndfile_compute_fill_level(queuedBufferCount, totalBuffers));
    // callbacks are called with mutex unlocked
    if (NULL != callback) {
        if (headAtMarker) {
            (*callback)(&thisAP->mPlay.mItf, context, SL_PLAYEVENT_HEADATMARKER);
        }
        if (headAtNewPos) {
            (*callback)(&thisAP->mPlay.mItf, context, SL_PLAYEVENT_HEADATNEWPOS);
        }
    }
}


/** \brief Check whether the supplied libsndfile format is supported by us */

SLboolean SndFile_IsSupported(const SF_INFO *sfinfo)
{
    switch (sfinfo->format & SF_FORMAT_TYPEMASK) {
    case SF_FORMAT_WAV:
        break;
    default:
        return SL_BOOLEAN_FALSE;
    }
    switch (sfinfo->format & SF_FORMAT_SUBMASK) {
    case SF_FORMAT_PCM_U8:
    case SF_FORMAT_PCM_16:
        break;
    default:
        return SL_BOOLEAN_FALSE;
    }
    switch (sfinfo->samplerate) {
    case 11025:
    case 22050:
    case 44100:
        break;
    default:
        return SL_BOOLEAN_FALSE;
    }
    switch (sfinfo->channels) {
    case 1:
    case 2:
        break;
    default:
        return SL_BOOLEAN_FALSE;
    }
    return SL_BOOLEAN_TRUE;
}


/** \brief Check whether the partially-constructed AudioPlayer is compatible with libsndfile */

SLresult SndFile_checkAudioPlayerSourceSink(CAudioPlayer *this)
{
    const SLDataSource *pAudioSrc = &this->mDataSource.u.mSource;
    SLuint32 locatorType = *(SLuint32 *)pAudioSrc->pLocator;
    SLuint32 formatType = *(SLuint32 *)pAudioSrc->pFormat;
    switch (locatorType) {
    case SL_DATALOCATOR_BUFFERQUEUE:
    case SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE:
        break;
    case SL_DATALOCATOR_URI:
        {
        SLDataLocator_URI *dl_uri = (SLDataLocator_URI *) pAudioSrc->pLocator;
        SLchar *uri = dl_uri->URI;
        if (NULL == uri) {
            return SL_RESULT_PARAMETER_INVALID;
        }
        if (!strncmp((const char *) uri, "file:///", 8)) {
            uri += 8;
        }
        switch (formatType) {
        case SL_DATAFORMAT_MIME:    // MIME is validated centrally; only the URI matters here
            break;
        default:
            return SL_RESULT_CONTENT_UNSUPPORTED;
        }
        this->mSndFile.mPathname = uri;
        this->mBufferQueue.mNumBuffers = SndFile_NUMBUFS;
        }
        break;
    default:
        return SL_RESULT_CONTENT_UNSUPPORTED;
    }
    this->mSndFile.mWhich = 0;
    this->mSndFile.mSNDFILE = NULL;
    // this->mSndFile.mMutex is initialized only when there is a valid mSNDFILE
    this->mSndFile.mEOF = SL_BOOLEAN_FALSE;

    return SL_RESULT_SUCCESS;
}


/** \brief Called with mutex unlocked for marker and position updates, and play state change */

void audioPlayerTransportUpdate(CAudioPlayer *audioPlayer)
{

    if (NULL != audioPlayer->mSndFile.mSNDFILE) {

        object_lock_exclusive(&audioPlayer->mObject);
        SLboolean empty = 0 == audioPlayer->mBufferQueue.mState.count;
        SLmillisecond pos = audioPlayer->mSeek.mPos;
        if (SL_TIME_UNKNOWN != pos) {
            audioPlayer->mSeek.mPos = SL_TIME_UNKNOWN;
            // trim seek position to the current known duration
            if (pos > audioPlayer->mPlay.mDuration) {
                pos = audioPlayer->mPlay.mDuration;
            }
            audioPlayer->mPlay.mPosition = pos;
            audioPlayer->mPlay.mLastSeekPosition = pos;
            audioPlayer->mPlay.mFramesSinceLastSeek = 0;
            // seek postpones the next head at new position callback
            audioPlayer->mPlay.mFramesSincePositionUpdate = 0;
            audioPlayer->mPlay.mHeadAtEnd = SL_BOOLEAN_FALSE;
            audioPlayer->mPlay.mHeadStalled = SL_BOOLEAN_FALSE;
            audioPlayer->mPlay.mMarkerReached = SL_BOOLEAN_FALSE;
        }
        object_unlock_exclusive(&audioPlayer->mObject);

        if (SL_TIME_UNKNOWN != pos) {
            SLboolean seekSucceeded = SL_BOOLEAN_FALSE;

            // discard any enqueued buffers for the old position
            object_lock_exclusive(&audioPlayer->mObject);
            IBufferQueue_ReleaseArrayBuffers(&audioPlayer->mBufferQueue);
            audioPlayer->mBufferQueue.mFront = &audioPlayer->mBufferQueue.mArray[0];
            audioPlayer->mBufferQueue.mRear = &audioPlayer->mBufferQueue.mArray[0];
            audioPlayer->mBufferQueue.mState.count = 0;
            audioPlayer->mBufferQueue.mState.playIndex = 0;
            audioPlayer->mBufferQueue.mClearRequested = SL_BOOLEAN_FALSE;
            object_unlock_exclusive(&audioPlayer->mObject);
            empty = SL_BOOLEAN_TRUE;
            sndfile_update_prefetch_fill_level(audioPlayer, 0);

            pthread_mutex_lock(&audioPlayer->mSndFile.mMutex);
            sf_count_t seekResult = sf_seek(audioPlayer->mSndFile.mSNDFILE,
                (sf_count_t) (((long long) pos * audioPlayer->mSndFile.mSfInfo.samplerate) / 1000LL),
                SEEK_SET);
            if (seekResult >= 0) {
                audioPlayer->mSndFile.mEOF = SL_BOOLEAN_FALSE;
                audioPlayer->mSndFile.mWhich = 0;
                seekSucceeded = SL_BOOLEAN_TRUE;
            }
            pthread_mutex_unlock(&audioPlayer->mSndFile.mMutex);

            if (!seekSucceeded) {
                SL_LOGE("sndfile seek failed for position %u ms", (unsigned) pos);
                sndfile_update_prefetch_status(audioPlayer, SL_PREFETCHSTATUS_UNDERFLOW);
                return;
            }

        }

        if (empty && ((SL_TIME_UNKNOWN != pos) ||
            (SL_PLAYSTATE_PLAYING == audioPlayer->mPlay.mState) ||
            (SL_PLAYSTATE_PAUSED == audioPlayer->mPlay.mState))) {
            audioPlayerRefillBuffers(audioPlayer);
        }

    }

}


/** \brief Called by CAudioPlayer_Realize */

SLresult SndFile_Realize(CAudioPlayer *this)
{
    SLresult result = SL_RESULT_SUCCESS;
    if (NULL != this->mSndFile.mPathname) {
        this->mSndFile.mSfInfo.format = 0;
        this->mSndFile.mSNDFILE = sf_open(
            (const char *) this->mSndFile.mPathname, SFM_READ, &this->mSndFile.mSfInfo);
        if (NULL == this->mSndFile.mSNDFILE) {
            result = SL_RESULT_CONTENT_NOT_FOUND;
        } else if (!SndFile_IsSupported(&this->mSndFile.mSfInfo)) {
            sf_close(this->mSndFile.mSNDFILE);
            this->mSndFile.mSNDFILE = NULL;
            result = SL_RESULT_CONTENT_UNSUPPORTED;
        } else {
            int ok;
            ok = pthread_mutex_init(&this->mSndFile.mMutex, (const pthread_mutexattr_t *) NULL);
            assert(0 == ok);
            // URI players need internal refill/position callbacks even when the public
            // BufferQueue interface is not exposed.
            this->mBufferQueue.mThis = this;
            this->mBufferQueue.mCallback = SndFile_Callback;
            this->mBufferQueue.mContext = this;
            this->mBufferQueue.samplerate = this->mSndFile.mSfInfo.samplerate * 1000;
            this->mBufferQueue.channels = this->mSndFile.mSfInfo.channels;
            this->mBufferQueue.bps = ((this->mSndFile.mSfInfo.format & SF_FORMAT_SUBMASK) ==
                    SF_FORMAT_PCM_U8) ? 8 : 16;
            this->mPrefetchStatus.mStatus = SL_PREFETCHSTATUS_UNDERFLOW;
            this->mPrefetchStatus.mLevel = 0;
            // this is the initial duration; will update when a new maximum position is detected
            this->mPlay.mDuration = (SLmillisecond) (((long long) this->mSndFile.mSfInfo.frames *
                1000LL) / this->mSndFile.mSfInfo.samplerate);
            this->mNumChannels = this->mSndFile.mSfInfo.channels;
            this->mSampleRateMilliHz = this->mSndFile.mSfInfo.samplerate * 1000;
#ifdef USE_OUTPUTMIXEXT
            this->mPlay.mFrameUpdatePeriod = ((long long) this->mPlay.mPositionUpdatePeriod *
                (long long) this->mSampleRateMilliHz) / 1000000LL;
#endif
        }
    }
    return result;
}


/** \brief Called by CAudioPlayer_Destroy */

void SndFile_Destroy(CAudioPlayer *this)
{
    if (NULL != this->mSndFile.mSNDFILE) {
        sf_close(this->mSndFile.mSNDFILE);
        this->mSndFile.mSNDFILE = NULL;
        int ok;
        ok = pthread_mutex_destroy(&this->mSndFile.mMutex);
        assert(0 == ok);
    }
}
