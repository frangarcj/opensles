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

/* sync */

#include "sles_allinclusive.h"

#include <errno.h>

static void sync_wait_for_work(CEngine *this)
{
    if (!this->mEngine.mShutdown && !this->m3DCommit.mWaiting && !this->mEngine.mChangedMask) {
        int ok = object_cond_timedwait(&this->mObject, 20000);
        if (ok != 0 && ok != ETIMEDOUT) {
            SL_LOGE("OpenSLES synchronization wait failed: %d", ok);
            abort();
        }
    }
}


/** \brief Sync thread.
 *  The sync thread runs periodically to synchronize audio state between
 *  the application and platform-specific device driver; for best results
 *  it should run about every graphics frame (e.g. 20 Hz to 50 Hz).
 */

void *sync_start(void *arg)
{
    CEngine *this = (CEngine *) arg;
    for (;;) {
        object_lock_exclusive(&this->mObject);
        sync_wait_for_work(this);
        if (this->mEngine.mShutdown) {
            this->mEngine.mShutdownAck = SL_BOOLEAN_TRUE;
            // broadcast not signal, because this condition is also used for other purposes
            object_cond_broadcast(&this->mObject);
            object_unlock_exclusive(&this->mObject);
            break;
        }
        if (this->m3DCommit.mWaiting) {
            this->m3DCommit.mWaiting = 0;
            ++this->m3DCommit.mGeneration;
            // There might be more than one thread blocked in Commit, so wake them all
            object_cond_broadcast(&this->mObject);
            // here is where we would process the enqueued 3D commands
        }
        unsigned instanceMask = this->mEngine.mInstanceMask;
        unsigned changedMask = this->mEngine.mChangedMask;
        this->mEngine.mChangedMask = 0;
        object_unlock_exclusive(&this->mObject);

        // now we know which objects exist, and which of those have changes

        unsigned combinedMask = changedMask | instanceMask;
        while (combinedMask) {
            unsigned i = ctz(combinedMask);
            assert(MAX_INSTANCE > i);
            combinedMask &= ~(1 << i);
            object_lock_exclusive(&this->mObject);
            IObject *instance = (IObject *) this->mEngine.mInstances[i];
            if (instance != NULL) {
                // Publication and lifetime acquisition must be one atomic decision.
                __atomic_fetch_add(&instance->mSyncRefCount, 1, __ATOMIC_RELAXED);
            }
            object_unlock_exclusive(&this->mObject);
            // Could be NULL during construct or destroy
            if (NULL == instance) {
                continue;
            }

            object_lock_exclusive(instance);
            if (instance->mState == SL_OBJECT_STATE_DESTROYING) {
                goto release_instance;
            }
            unsigned attributesMask = instance->mAttributesMask;
            instance->mAttributesMask = 0;

            switch (IObjectToObjectID(instance)) {
            case SL_OBJECTID_AUDIOPLAYER:
                // do something here
                object_unlock_exclusive(instance);
#ifdef USE_SNDFILE
                if (attributesMask & (ATTR_POSITION | ATTR_PLAYSTATE)) {
                    CAudioPlayer *audioPlayer = (CAudioPlayer *) instance;
                    audioPlayerTransportUpdate(audioPlayer);
                }
                if (attributesMask & ATTR_SNDREFILL) {
                    CAudioPlayer *audioPlayer = (CAudioPlayer *) instance;
                    audioPlayerRefillBuffers(audioPlayer);
                }
#endif
                break;

            default:
                object_unlock_exclusive(instance);
                break;
            }
            object_lock_exclusive(instance);
release_instance:
            if (__atomic_sub_fetch(&instance->mSyncRefCount, 1, __ATOMIC_ACQ_REL) == 0) {
                object_cond_broadcast(instance);
            }
            object_unlock_exclusive(instance);
        }
    }
    return NULL;
}
