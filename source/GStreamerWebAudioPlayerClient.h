/*
 * Copyright (C) 2023 Sky UK
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

#include "MessageQueue.h"
#include "Timer.h"
#include "WebAudioClientBackendInterface.h"
#include <MediaCommon.h>
#include <condition_variable>
#include <gst/app/gstappsink.h>
#include <gst/base/gstbasesink.h>
#include <gst/gst.h>
#include <mutex>
#include <thread>
#include <vector>

#include <atomic>
#include <functional>
#include <memory>
#include <sys/syscall.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

class GStreamerWebAudioPlayerClient : public firebolt::rialto::IWebAudioPlayerClient,
                                      public std::enable_shared_from_this<GStreamerWebAudioPlayerClient>
{
public:
    /**
     * @brief Constructor.
     *
     * @param[in] appSink : Gstreamer appsink.
     */
    GStreamerWebAudioPlayerClient(GstElement *appSink);

    /**
     * @brief Destructor.
     */
    virtual ~GStreamerWebAudioPlayerClient();

    /**
     * @brief Extracts the sample info from the gstreamer capabilities
     *        and opens the web audio.
     *
     * @param[in] caps : Sample gstreamer capabilities.
     *
     * @retval true on success.
     */
    bool open(GstCaps *caps);

    /**
     * @brief Play the web audio.
     *
     * @retval true on success.
     */
    bool play();

    /**
     * @brief Pause the web audio.
     *
     * @retval true on success.
     */
    bool pause();

    /**
     * @brief Notify EOS.
     *
     * @retval true on success.
     */
    bool setEos();

    /**
     * @brief Send any remaining samples then reset the web audio.
     *
     * Will not return from this method until all data has been played out.
     *
     * @retval true on success.
     */
    bool reset();

    /**
     * @brief Notifies that there is a new sample in gstreamer.
     *
     * @retval true on success.
     */
    bool notifyNewSample();

    /**
     * @brief Notify push sample timer expiry.
     */
    void notifyPushSamplesTimerExpired();

    /**
     * @brief Implements the player state change notification.
     *
     * @param[in] state : The new playback state.
     */
    void notifyState(firebolt::rialto::WebAudioPlayerState state) override;

private:
    /**
     * @brief Perform the next push operation.
     *
     * The samples are pushed only when there is the available buffer
     * in RialtoServer.
     */
    void pushSamples();

    /**
     * @brief Get the next sample data buffer from gstreamer.
     *
     * @retval vector of the next buffer data.
     */
    std::vector<uint8_t> getNextBufferData();

    /**
     * @brief Backend message queue.
     */
    MessageQueue mBackendQueue;

    /**
     * @brief The web audio client backend interface.
     */
    std::unique_ptr<firebolt::rialto::client::WebAudioClientBackendInterface> mClientBackend;

    /**
     * @brief Whether the web audio backend is currently open.
     */
    std::atomic<bool> mIsOpen;

    /**
     * @brief Vector of buffers to store the sample data.
     */
    std::vector<std::vector<uint8_t>> mSampleDataBuffers;

    /**
     * @brief Vector of buffers to store the sample data.
     */
    uint32_t mSampleDataFrameCount;

    /**
     * @brief Whether reset has been called and is currently been executed.
     */
    bool mIsResetInProgress;

    /**
     * @brief Appsink from gstreamer.
     */
    GstElement *mAppSink;

    /**
     * @brief The push samples timer.
     */
    Timer m_pushSamplesTimer;

    /**
     * @brief Mutex for reset protection.
     */
    std::mutex m_resetLock;

    /**
     * @brief Reset cond for signalling reset complete.
     */
    std::condition_variable m_resetCond;

    /**
     * @brief Time to wait (ms) for reset complete.
     */
    const std::chrono::milliseconds m_resetTimeout;
};
