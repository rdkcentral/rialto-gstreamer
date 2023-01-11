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

#include "GStreamerWebAudioPlayerClient.h"
#include "WebAudioClientBackend.h"
#include <algorithm>
#include <chrono>
#include <string.h>
#include <thread>

/**
 * @brief The callback called when push sampels timer expires
 *
 * @param[in] vSelf : private user data
 */
static void notifyPushSamplesCallback(void *vSelf)
{
    GStreamerWebAudioPlayerClient *self = static_cast<GStreamerWebAudioPlayerClient *>(vSelf);
    self->notifyPushSamplesTimerExpired();
}

GStreamerWebAudioPlayerClient::GStreamerWebAudioPlayerClient(GstElement *appSink)
    : mIsOpen(false), mSampleDataFrameCount(0u), mIsResetInProgress(false), mAppSink(appSink),
      m_pushSamplesTimer(notifyPushSamplesCallback, this, "notifyPushSamplesCallback"), m_resetTimeout(5000)
{
    mBackendQueue.start();
    if (!createBackend("", 0, nullptr))
    {
        GST_ERROR("Cannot create backend");
    }
}

GStreamerWebAudioPlayerClient::~GStreamerWebAudioPlayerClient()
{
    mBackendQueue.stop();
}

bool GStreamerWebAudioPlayerClient::createBackend(const std::string &audioMimeType, const uint32_t priority,
                                                  const firebolt::rialto::WebAudioConfig *config)
{
    GST_DEBUG("entry:");

    bool result = true;
    mBackendQueue.callInEventLoop(
        [&]()
        {
            mClientBackend = std::make_unique<firebolt::rialto::client::WebAudioClientBackend>();
            mClientBackend->createWebAudioBackend(shared_from_this(), audioMimeType, priority, config);

            if (!mClientBackend->isWebAudioBackendCreated())
            {
                GST_ERROR("Could not create web audio backend");
                result = false;
            }
        });
    return result;
}

bool GStreamerWebAudioPlayerClient::open(GstCaps *caps)
{
    GST_DEBUG("entry:");

    bool result = false;
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    std::string format = gst_structure_get_string(structure, "format");
    uint32_t sampleSize;
    gint rate;
    gint channels;
    bool isBigEndian;
    bool isSigned;
    bool isFloat;

    // Required capabilities
    if (format.empty())
    {
        GST_ERROR("Format not found in caps");
        return result;
    }

    if (!gst_structure_get_int(structure, "rate", &rate))
    {
        GST_ERROR("Rate not found in caps");
        return result;
    }

    if (!gst_structure_get_int(structure, "channels", &channels))
    {
        GST_ERROR("Rate not found in caps");
        return result;
    }

    // Only format S16LE & F32LE supported
    if (format == std::string("S16LE"))
    {
        sampleSize = 16;
        isBigEndian = false;
        isSigned = true;
        isFloat = false;
    }
    else if (format == std::string("F32LE"))
    {
        sampleSize = 32;
        isBigEndian = false;
        isSigned = false;
        isFloat = true;
    }
    else
    {
        GST_ERROR("Format not supported");
        return result;
    }

    mBackendQueue.callInEventLoop(
        [&]()
        {
            if (mClientBackend)
            {
                result = mClientBackend->open(rate, channels, sampleSize, isBigEndian, isSigned, isFloat);
                mIsOpen = true;
            }
            else
            {
                GST_ERROR("No web audio backend");
            }
        });

    return result;
}

bool GStreamerWebAudioPlayerClient::play()
{
    GST_DEBUG("entry:");

    bool result = false;
    mBackendQueue.callInEventLoop(
        [&]()
        {
            if (mClientBackend)
            {
                result = mClientBackend->play();
            }
            else
            {
                GST_ERROR("No web audio backend");
            }
        });

    return result;
}

bool GStreamerWebAudioPlayerClient::reset()
{
    GST_DEBUG("entry:");

    mBackendQueue.callInEventLoop(
        [&]()
        {
            if (mClientBackend)
            {
                {
                    std::unique_lock<std::mutex> resetlock(m_resetLock);
                    mIsResetInProgress = true;
                }

                // Push any leftover samples
                pushSamples();
            }
            else
            {
                GST_ERROR("No web audio backend");
            }
        });

    std::unique_lock<std::mutex> resetlock(m_resetLock);
    if (mIsResetInProgress)
    {
        const auto deadline = std::chrono::steady_clock::now() + m_resetTimeout;

        // Wait until reset has been finished out before returning to client
        m_resetCond.wait_until(resetlock, deadline);
    }

    return true;
}

bool GStreamerWebAudioPlayerClient::close()
{
    GST_DEBUG("entry:");

    bool result = false;
    mBackendQueue.callInEventLoop(
        [&]()
        {
            if (mClientBackend)
            {
                result = mClientBackend->close();
                mIsOpen = false;
            }
            else
            {
                GST_ERROR("No web audio backend");
            }
        });

    return result;
}

void GStreamerWebAudioPlayerClient::notifyPushSamplesTimerExpired()
{
    mBackendQueue.callInEventLoop(
        [&]()
        {
            if (mClientBackend)
            {
                pushSamples();
            }
            else
            {
                GST_ERROR("No web audio backend");
            }
        });
}

bool GStreamerWebAudioPlayerClient::notifyNewSample()
{
    GST_DEBUG("entry:");

    mBackendQueue.callInEventLoop(
        [&]()
        {
            if (mClientBackend)
            {
                m_pushSamplesTimer.cancel();
                pushSamples();
            }
            else
            {
                GST_ERROR("No web audio backend");
            }
        });

    return true;
}

void GStreamerWebAudioPlayerClient::pushSamples()
{
    uint32_t preferredFrames = 0u;
    uint32_t maximumFrames = 0u;
    bool supportDeferredPlay = false;
    uint32_t availableFrames = 0u;
    int16_t *sharedBuffer = nullptr;

    if (!mClientBackend->getBufferAvailable(availableFrames))
    {
        GST_ERROR("GetBufferAvailable failed, could not process samples");
    }
    else if (!mClientBackend->getDeviceInfo(preferredFrames, maximumFrames, supportDeferredPlay))
    {
        GST_ERROR("GetDeviceInfo failed, could not process samples");
    }
    else
    {
        if ((availableFrames >= preferredFrames) || (mIsResetInProgress))
        {
            // Store any buffes from gstreamer, up to the preferred frame count
            while (mSampleDataFrameCount < preferredFrames)
            {
                std::vector<uint8_t> bufferData = getNextBufferData();
                if (bufferData.empty())
                {
                    break;
                }
                else if (bufferData.size() / 4 > maximumFrames)
                {
                    GST_ERROR("Pulled buffer with frames %u too large, maximum frames %u", bufferData.size() / 4,
                              maximumFrames);
                    break;
                }
                else
                {
                    mSampleDataBuffers.push_back(bufferData);
                    mSampleDataFrameCount += bufferData.size() / 4;
                }
            }

            // Write data to the shared buffer and commit
            if ((mSampleDataFrameCount >= preferredFrames) || (mIsResetInProgress))
            {
                uint32_t framesMax = std::min(maximumFrames, availableFrames);
                uint32_t bytesCopied = 0u;
                std::vector<uint8_t> tmpBufferData;

                for (auto it = mSampleDataBuffers.begin(); it != mSampleDataBuffers.end(); it++)
                {
                    if ((it->size() + bytesCopied) / 4 > framesMax)
                    {
                        // Cannot fit in buffer, store the data until we have committed
                        tmpBufferData = *it;
                        break;
                    }
                    else
                    {
                        // Get the buffer here as getBuffer must be paired with a commit
                        if (nullptr == sharedBuffer)
                        {
                            if (!mClientBackend->getBuffer(&sharedBuffer, framesMax))
                            {
                                GST_ERROR("GetBuffer could not get the buffer");
                                break;
                            }
                        }
                        memcpy(sharedBuffer + bytesCopied, &(*it)[0], it->size());
                        bytesCopied += it->size();
                    }
                }

                // Commit buffer if we have written data
                if (bytesCopied != 0)
                {
                    if (!mClientBackend->commitBuffer(bytesCopied / 4))
                    {
                        // Keep the stored samples if we fail to push the data
                        GST_ERROR("CommitBuffer failed");
                    }
                    else
                    {
                        mSampleDataFrameCount = tmpBufferData.size() / 4;
                        mSampleDataBuffers.clear();
                        if (!tmpBufferData.empty())
                        {
                            mSampleDataBuffers.push_back(tmpBufferData);
                        }
                    }
                }
            }
        }
    }

    if (mSampleDataBuffers.empty())
    {
        // Check if there are anymore samples to push //
        std::vector<uint8_t> bufferData = getNextBufferData();
        if (!bufferData.empty())
        {
            mSampleDataBuffers.push_back(bufferData);
            mSampleDataFrameCount += bufferData.size() / 4;
        }
    }

    // If we still have samples stored that could not be pushed, start a timer.
    // This avoids any stoppages in the pushing of samples to the server if the consumption of
    // samples is slow.
    if (!mSampleDataBuffers.empty())
    {
        m_pushSamplesTimer.arm(100);
    }
    else
    {
        if ((mIsResetInProgress) && (!mClientBackend->reset()))
        {
            GST_ERROR("Reset failed, try again when sample timer expires");
            m_pushSamplesTimer.arm(100);
        }
        else
        {
            // Cancel any timers, no more samples to push
            m_pushSamplesTimer.cancel();
            {
                {
                    std::unique_lock<std::mutex> resetlock(m_resetLock);
                    mIsResetInProgress = false;
                }
                m_resetCond.notify_all();
            }
        }
    }
}

std::vector<uint8_t> GStreamerWebAudioPlayerClient::getNextBufferData()
{
    GstSample *sample = gst_app_sink_try_pull_sample(GST_APP_SINK(mAppSink), 0);
    if (!sample)
    {
        return {};
    }

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    uint32_t bufferSize = gst_buffer_get_size(buffer);
    GstMapInfo bufferMap;

    if (!gst_buffer_map(buffer, &bufferMap, GST_MAP_READ))
    {
        GST_ERROR("Could not map audio buffer");
        gst_sample_unref(sample);
        return {};
    }

    std::vector<uint8_t> bufferData(bufferMap.data, bufferMap.data + bufferSize);
    gst_buffer_unmap(buffer, &bufferMap);
    gst_sample_unref(sample);

    return bufferData;
}

bool GStreamerWebAudioPlayerClient::isOpen()
{
    return mIsOpen;
}

void GStreamerWebAudioPlayerClient::notifyState(firebolt::rialto::WebAudioPlayerState state) {}
