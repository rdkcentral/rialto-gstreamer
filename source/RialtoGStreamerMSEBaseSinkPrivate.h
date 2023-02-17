/*
 * Copyright (C) 2022 Sky UK
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include <gst/gst.h>

#include <string>

#include "MediaPlayerManager.h"
#include "RialtoControlClientBackendInterface.h"
#include "RialtoGStreamerMSEBaseSinkCallbacks.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <queue>

G_BEGIN_DECLS

struct _RialtoMSEBaseSinkPrivate
{
    _RialtoMSEBaseSinkPrivate() : mSourceId(-1), mIsFlushOngoing(false), mIsStateCommitNeeded(false) {}
    ~_RialtoMSEBaseSinkPrivate()
    {
        if (mCaps)
            gst_caps_unref(mCaps);
        clearBuffersUnlocked();
    }

    void clearBuffersUnlocked()
    {
        mIsFlushOngoing = true;
        mNeedDataCondVariable.notify_all();
        while (!mSamples.empty())
        {
            GstSample *sample = mSamples.front();
            mSamples.pop();
            gst_sample_unref(sample);
        }
    }

    GstPad *mSinkPad = nullptr;
    GstSegment mLastSegment;
    GstCaps *mCaps = nullptr;

    std::atomic<int32_t> mSourceId;
    std::queue<GstSample *> mSamples;
    bool mIsEos = false;
    std::atomic<bool> mIsFlushOngoing;
    std::atomic<bool> mIsStateCommitNeeded;
    std::mutex mSinkMutex;

    std::condition_variable mNeedDataCondVariable;
    std::condition_variable mSeekCondVariable;
    std::mutex mSeekMutex;

    std::string mUri;
    RialtoGStreamerMSEBaseSinkCallbacks mCallbacks;

    MediaPlayerManager m_mediaPlayerManager;
    std::unique_ptr<firebolt::rialto::client::RialtoControlClientBackendInterface> m_rialtoControlClient;
    bool mHandleResetTimeMessage = false;
    bool mSourceAttached = false;
};
}
