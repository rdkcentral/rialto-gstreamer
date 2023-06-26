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

#include "ControlBackendInterface.h"
#include "MediaPlayerManager.h"
#include "RialtoGStreamerMSEBaseSinkCallbacks.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <queue>

G_BEGIN_DECLS

struct _RialtoMSEBaseSinkPrivate
{
    _RialtoMSEBaseSinkPrivate() : m_sourceId(-1), m_isFlushOngoing(false), m_isStateCommitNeeded(false), m_hasDrm(true)
    {
    }
    ~_RialtoMSEBaseSinkPrivate()
    {
        if (m_caps)
            gst_caps_unref(m_caps);
        clearBuffersUnlocked();
    }

    void clearBuffersUnlocked()
    {
        m_isFlushOngoing = true;
        m_needDataCondVariable.notify_all();
        while (!m_samples.empty())
        {
            GstSample *sample = m_samples.front();
            m_samples.pop();
            gst_sample_unref(sample);
        }
    }

    GstPad *m_sinkPad = nullptr;
    GstSegment m_lastSegment;
    GstCaps *m_caps = nullptr;

    std::atomic<int32_t> m_sourceId;
    std::queue<GstSample *> m_samples;
    bool m_isEos = false;
    std::atomic<bool> m_isFlushOngoing;
    std::atomic<bool> m_isStateCommitNeeded;
    std::mutex m_sinkMutex;

    std::condition_variable m_needDataCondVariable;
    std::condition_variable m_seekCondVariable;
    std::mutex m_seekMutex;

    std::string m_uri;
    RialtoGStreamerMSEBaseSinkCallbacks m_callbacks;

    MediaPlayerManager m_mediaPlayerManager;
    std::unique_ptr<firebolt::rialto::client::ControlBackendInterface> m_rialtoControlClient;
    bool m_handleResetTimeMessage = false;
    bool m_sourceAttached = false;
    bool m_isSinglePathStream = false;
    int32_t m_numOfStreams = 1;
    std::atomic<bool> m_hasDrm;
};
G_END_DECLS
