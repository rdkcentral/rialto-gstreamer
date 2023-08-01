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
#include <queue>
#include <sys/syscall.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

struct WebAudioSinkCallbacks
{
    std::function<void(const char *message)> errorCallback;
    std::function<void(void)> eosCallback;
    std::function<void(firebolt::rialto::WebAudioPlayerState)> stateChangedCallback;
};

class GStreamerWebAudioPlayerClient : public firebolt::rialto::IWebAudioPlayerClient,
                                      public std::enable_shared_from_this<GStreamerWebAudioPlayerClient>
{
public:
    /**
     * @brief Constructor.
     *
     * @param[in] callbacks : The callbacks for the sink.
     */
    explicit GStreamerWebAudioPlayerClient(WebAudioSinkCallbacks callbacks);

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
     * @brief Closes the web audio player.
     *
     * @retval true on success.
     */
    bool close();

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
     * @brief Whether the backend has been opened or not.
     *
     * @retval true if open.
     */
    bool isOpen();

    /**
     * @brief Notifies that there is a new sample in gstreamer.
     *
     * @param[in] buf : The new sample buffer.
     *
     * @retval true on success.
     */
    bool notifyNewSample(GstBuffer *buf);

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
     * @brief Checks the config against that previously stored in the object.
     *
     * @retval true if this is a new config.
     */
    bool isNewConfig(const std::string &audioMimeType, const firebolt::rialto::WebAudioConfig &config);

    /**
     * @brief Backend message queue.
     */
    MessageQueue m_backendQueue;

    /**
     * @brief The web audio client backend interface.
     */
    std::unique_ptr<firebolt::rialto::client::WebAudioClientBackendInterface> m_clientBackend;

    /**
     * @brief Whether the web audio backend is currently open.
     */
    std::atomic<bool> m_isOpen;

    /**
     * @brief Vector to store the gst sample buffers.
     */
    std::queue<GstBuffer *> m_dataBuffers;

    /**
     * @brief The push samples timer.
     */
    Timer m_pushSamplesTimer;

    /**
     * @brief The preferred number of frames to be written.
     */
    uint32_t m_preferredFrames;

    /**
     * @brief The maximum number of frames that can be written.
     */
    uint32_t m_maximumFrames;

    /**
     * @brief Whether defered play is supported.
     */
    bool m_supportDeferredPlay;

    /**
     * @brief Whether the sink element has received EOS.
     */
    bool m_isEos;

    /**
     * @brief The number of bytes in the frame.
     */
    uint32_t m_frameSize;

    /**
     * @brief The current web audio player mime type.
     */
    std::string m_mimeType;

    /**
     * @brief The current web audio player config.
     */
    firebolt::rialto::WebAudioConfig m_config;

    /**
     * @brief The sink callbacks.
     */
    WebAudioSinkCallbacks m_callbacks;
};
