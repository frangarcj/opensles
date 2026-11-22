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


/** \brief Exclusively lock an object */

#ifdef USE_DEBUG
void object_lock_exclusive_(IObject *this, const char *file, int line)
{
    int ok;
    ok = sles_mutex_trylock(&this->mMutex);
    if (0 != ok) {
        // pthread_mutex_timedlock_np is not available, but wait up to 100 ms
        static const useconds_t backoffs[] = {1, 10000, 20000, 30000, 40000};
        unsigned i = 0;
        for (;;) {
            usleep(backoffs[i]);
            ok = sles_mutex_trylock(&this->mMutex);
            if (0 == ok)
                break;
            if (++i >= (sizeof(backoffs) / sizeof(backoffs[0]))) {
                SL_LOGE("%s:%d: object %p remained contended for 100 ms\n",
                    file, line, this);
                // attempt one more time; maybe this time we will be successful
                ok = sles_mutex_lock(&this->mMutex);
                assert(0 == ok);
                break;
            }
        }
    }
    sles_thread_t zero;
    memset(&zero, 0, sizeof(sles_thread_t));
    if (0 != memcmp(&zero, &this->mOwner, sizeof(sles_thread_t))) {
        if (sles_thread_equal(sles_thread_self(), this->mOwner)) {
            SL_LOGE("%s:%d: recursive object lock %p; previous lock %s:%d\n",
                file, line, this, this->mFile, this->mLine);
        } else {
            SL_LOGE("%s:%d: object %p had stale owner metadata from %s:%d\n",
                file, line, this, this->mFile, this->mLine);
        }
        assert(false);
    }
    this->mOwner = sles_thread_self();
    this->mFile = file;
    this->mLine = line;
}

SLboolean object_trylock_exclusive_(IObject *this, const char *file, int line)
{
    int ok = sles_mutex_trylock(&this->mMutex);
    if (EBUSY == ok) {
        return SL_BOOLEAN_FALSE;
    }
    assert(0 == ok);

    sles_thread_t zero;
    memset(&zero, 0, sizeof(sles_thread_t));
    if (0 != memcmp(&zero, &this->mOwner, sizeof(sles_thread_t))) {
        if (sles_thread_equal(sles_thread_self(), this->mOwner)) {
            SL_LOGE("%s:%d: recursive object trylock %p; previous lock %s:%d\n",
                file, line, this, this->mFile, this->mLine);
        } else {
            SL_LOGE("%s:%d: object %p had stale owner metadata from %s:%d\n",
                file, line, this, this->mFile, this->mLine);
        }
        assert(false);
    }
    this->mOwner = sles_thread_self();
    this->mFile = file;
    this->mLine = line;
    return SL_BOOLEAN_TRUE;
}
#else
void object_lock_exclusive(IObject *this)
{
    int ok;
    ok = sles_mutex_lock(&this->mMutex);
    assert(0 == ok);
}

SLboolean object_trylock_exclusive(IObject *this)
{
    int ok = sles_mutex_trylock(&this->mMutex);
    if (EBUSY == ok) {
        return SL_BOOLEAN_FALSE;
    }
    assert(0 == ok);
    return SL_BOOLEAN_TRUE;
}
#endif


/** \brief Exclusively unlock an object and do not report any updates */

#ifdef USE_DEBUG
void object_unlock_exclusive_(IObject *this, const char *file, int line)
{
    assert(sles_thread_equal(sles_thread_self(), this->mOwner));
    assert(NULL != this->mFile);
    assert(0 != this->mLine);
    memset(&this->mOwner, 0, sizeof(sles_thread_t));
    this->mFile = file;
    this->mLine = line;
    int ok;
    ok = sles_mutex_unlock(&this->mMutex);
    assert(0 == ok);
}
#else
void object_unlock_exclusive(IObject *this)
{
    int ok;
    ok = sles_mutex_unlock(&this->mMutex);
    assert(0 == ok);
}
#endif


/** \brief Exclusively unlock an object and report updates to the specified bit-mask of
 *  attributes
 */

#ifdef USE_DEBUG
void object_unlock_exclusive_attributes_(IObject *this, unsigned attributes,
    const char *file, int line)
#else
void object_unlock_exclusive_attributes(IObject *this, unsigned attributes)
#endif
{

#ifdef USE_DEBUG
    assert(sles_thread_equal(sles_thread_self(), this->mOwner));
    assert(NULL != this->mFile);
    assert(0 != this->mLine);
#endif

    int ok;
    SLuint32 objectID = IObjectToObjectID(this);
    CAudioPlayer *ap;
    static const unsigned synchronousAttributes[] = {
        ATTR_GAIN,
        ATTR_POSITION,
        ATTR_TRANSPORT,
        ATTR_PLAYSTATE,
        ATTR_ENQUEUE
    };
    SLuint32 i;

    // Android likes to see certain updates synchronously

    for (i = 0; i < sizeof(synchronousAttributes) / sizeof(synchronousAttributes[0]); ++i) {
        unsigned attribute = synchronousAttributes[i];
        if (!(attributes & attribute)) {
            continue;
        }
        switch (attribute) {
        case ATTR_GAIN:
            switch (objectID) {
            case SL_OBJECTID_AUDIOPLAYER:
                attributes &= ~ATTR_GAIN;   // no need to process asynchronously also
                ap = (CAudioPlayer *) this;
#ifdef ANDROID
                android_audioPlayer_volumeUpdate(ap);
#else
                audioPlayerGainUpdate(ap);
#endif
                break;
            case SL_OBJECTID_OUTPUTMIX:
            case SL_OBJECTID_MIDIPLAYER:
                attributes &= ~ATTR_GAIN;
                break;
            default:
                break;
            }
            break;
        case ATTR_POSITION:
            switch (objectID) {
            case SL_OBJECTID_AUDIOPLAYER:
#ifdef ANDROID
                ap = (CAudioPlayer *) this;
                attributes &= ~ATTR_POSITION;   // no need to process asynchronously also
                android_audioPlayer_seek(ap, ap->mSeek.mPos);
#endif
                break;
            case SL_OBJECTID_MIDIPLAYER:
                attributes &= ~ATTR_POSITION;
                break;
            default:
                break;
            }
            break;
        case ATTR_TRANSPORT:
            if (SL_OBJECTID_AUDIOPLAYER == objectID) {
#ifdef ANDROID
                attributes &= ~ATTR_TRANSPORT;   // no need to process asynchronously also
                ap = (CAudioPlayer *) this;
                android_audioPlayer_useEventMask(ap);
#endif
            } else if (SL_OBJECTID_AUDIORECORDER == objectID) {
#ifdef ANDROID
                attributes &= ~ATTR_TRANSPORT;   // no need to process asynchronously also
                CAudioRecorder *ar = (CAudioRecorder *) this;
                android_audioRecorder_useEventMask(ar);
#endif
            }
            break;
        case ATTR_PLAYSTATE:
            if (SL_OBJECTID_AUDIOPLAYER == objectID) {
#ifdef ANDROID
                attributes &= ~ATTR_PLAYSTATE;   // no need to process asynchronously also
                ap = (CAudioPlayer *) this;
                android_audioPlayer_setPlayState(ap, false /*lockAP*/);
#endif
            }
            break;
        case ATTR_ENQUEUE:
            if (SL_OBJECTID_AUDIOPLAYER == objectID) {
                attributes &= ~ATTR_ENQUEUE;
                ap = (CAudioPlayer *) this;
                if (SL_PLAYSTATE_PLAYING == ap->mPlay.mState) {
#ifdef ANDROID
                    android_audioPlayer_bufferQueue_onRefilled(ap);
#endif
                }
            }
            break;
        default:
            break;
        }
    }

    if (attributes) {
        unsigned oldAttributesMask = this->mAttributesMask;
        this->mAttributesMask = oldAttributesMask | attributes;
        if (oldAttributesMask)
            attributes = ATTR_NONE;
    }
#ifdef USE_DEBUG
    memset(&this->mOwner, 0, sizeof(sles_thread_t));
    this->mFile = file;
    this->mLine = line;
#endif
    ok = sles_mutex_unlock(&this->mMutex);
    assert(0 == ok);
    // first update to this interface since previous sync
    if (attributes) {
        unsigned id = this->mInstanceID;
        if (0 != id) {
            --id;
            assert(MAX_INSTANCE > id);
            IEngine *thisEngine = this->mEngine;
            interface_lock_exclusive(thisEngine);
            thisEngine->mChangedMask |= 1 << id;
            object_cond_broadcast(thisEngine->mThis);
            interface_unlock_exclusive(thisEngine);
        }
    }
}


/** \brief Wait on the condition variable associated with the object; see sles_cond_wait */

#ifdef USE_DEBUG
void object_cond_wait_(IObject *this, const char *file, int line)
{
    // note that this will unlock the mutex, so we have to clear the owner
    assert(sles_thread_equal(sles_thread_self(), this->mOwner));
    assert(NULL != this->mFile);
    assert(0 != this->mLine);
    memset(&this->mOwner, 0, sizeof(sles_thread_t));
    this->mFile = file;
    this->mLine = line;
    // alas we don't know the new owner's identity
    int ok;
    ok = sles_cond_wait(&this->mCond, &this->mMutex);
    assert(0 == ok);
    // restore my ownership
    this->mOwner = sles_thread_self();
    this->mFile = file;
    this->mLine = line;
}
#else
void object_cond_wait(IObject *this)
{
    int ok;
    ok = sles_cond_wait(&this->mCond, &this->mMutex);
    assert(0 == ok);
}
#endif


/** \brief Signal the condition variable associated with the object; see sles_cond_signal */

int object_cond_timedwait(IObject *this, uint32_t timeout_us)
{
#ifdef USE_DEBUG
    assert(sles_thread_equal(sles_thread_self(), this->mOwner));
    const char *file = this->mFile;
    int line = this->mLine;
    memset(&this->mOwner, 0, sizeof(this->mOwner));
#endif
    int result = sles_cond_timedwait(&this->mCond, &this->mMutex, timeout_us);
#ifdef USE_DEBUG
    this->mOwner = sles_thread_self();
    this->mFile = file;
    this->mLine = line;
#endif
    return result;
}

void object_cond_signal(IObject *this)
{
    int ok;
    ok = sles_cond_signal(&this->mCond);
    assert(0 == ok);
}


/** \brief Broadcast the condition variable associated with the object;
 *  see sles_cond_broadcast
 */

void object_cond_broadcast(IObject *this)
{
    int ok;
    ok = sles_cond_broadcast(&this->mCond);
    assert(0 == ok);
}
