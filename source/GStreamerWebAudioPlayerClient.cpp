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
#include <cstdlib>
#include <string.h>
#include <thread>

namespace
{
/**
 * @brief The callback called when push sampels timer expires
 *
 * @param[in] vSelf : private user data
 */
void notifyPushSamplesCallback(void *vSelf)
{
    GStreamerWebAudioPlayerClient *self = static_cast<GStreamerWebAudioPlayerClient *>(vSelf);
    self->notifyPushSamplesTimerExpired();
}

bool parseGstStructureFormat(const std::string &format, uint32_t &sampleSize, bool &isBigEndian, bool &isSigned,
                             bool &isFloat)
{
    if (format.size() != 5)
    {
        return false;
    }
    std::string sampleSizeStr = format.substr(1, 2);
    char *pEnd = NULL;
    errno = 0;
    sampleSize = strtoul(sampleSizeStr.c_str(), &pEnd, 10);
    if (errno == ERANGE)
    {
        return false;
    }

    isBigEndian = format.substr(3) == "BE";

    switch (format[0])
    {
    case 'S':
        isSigned = true;
        isFloat = false;
        break;
    case 'U':
        isSigned = false;
        isFloat = false;
        break;
    case 'F':
        isSigned = false;
        isFloat = true;
        break;
    default:
        return false;
        break;
    }
    return true;
}

bool operator!=(const firebolt::rialto::WebAudioPcmConfig &lac, const firebolt::rialto::WebAudioPcmConfig &rac)
{
    return lac.rate != rac.rate || lac.channels != rac.channels || lac.sampleSize != rac.sampleSize ||
           lac.isBigEndian != rac.isBigEndian || lac.isSigned != rac.isSigned || lac.isFloat != rac.isFloat;
}
} // namespace

GStreamerWebAudioPlayerClient::GStreamerWebAudioPlayerClient(GstElement *appSink)
    : mIsOpen(false), mAppSink(appSink),
      m_pushSamplesTimer(notifyPushSamplesCallback, this, "notifyPushSamplesCallback"), m_isEos(false), m_config({})
{
    mBackendQueue.start();
    mClientBackend = std::make_unique<firebolt::rialto::client::WebAudioClientBackend>();
}

GStreamerWebAudioPlayerClient::~GStreamerWebAudioPlayerClient()
{
    mBackendQueue.stop();
}

bool GStreamerWebAudioPlayerClient::open(GstCaps *caps)
{
    GST_DEBUG("entry:");

    bool result = false;
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    std::string audioMimeType = gst_structure_get_name(structure);
    audioMimeType = audioMimeType.substr(0, audioMimeType.find(' '));
    std::string format = gst_structure_get_string(structure, "format");
    firebolt::rialto::WebAudioPcmConfig pcm;
    gint tmp;

    if (format.empty())
    {
        GST_ERROR("Format not found in caps");
        return result;
    }

    if (!gst_structure_get_int(structure, "rate", &tmp))
    {
        GST_ERROR("Rate not found in caps");
        return result;
    }
    pcm.rate = tmp;

    if (!gst_structure_get_int(structure, "channels", &tmp))
    {
        GST_ERROR("Rate not found in caps");
        return result;
    }
    pcm.channels = tmp;

    if (!parseGstStructureFormat(format, pcm.sampleSize, pcm.isBigEndian, pcm.isSigned, pcm.isFloat))
    {
        GST_ERROR("Can't parse format or it is not supported: %s", format.c_str());
        return result;
    }

    mBackendQueue.callInEventLoop(
        [&]()
        {
            firebolt::rialto::WebAudioConfig config{pcm};

            // Only recreate player if the config has changed
            if (!mIsOpen || isNewConfig(audioMimeType, config))
            {
                if (mIsOpen)
                {
                    // Destroy the previously created player
                    mClientBackend->destroyWebAudioBackend();
                }

                uint32_t priority = 1;
                if (mClientBackend->createWebAudioBackend(shared_from_this(), audioMimeType, priority, &config))
                {
                    if (!mClientBackend->getDeviceInfo(m_preferredFrames, m_maximumFrames, m_supportDeferredPlay))
                    {
                        GST_ERROR("GetDeviceInfo failed, could not process samples");
                    }
                    m_frameSize = (pcm.sampleSize * pcm.channels) / CHAR_BIT;
                    mIsOpen = true;

                    // Store config
                    m_config.pcm = pcm;
                    m_mimeType = audioMimeType;
                }
                else
                {
                    GST_ERROR("Could not create web audio backend");
                    mIsOpen = false;
                }
                result = mIsOpen;
            }
        });

    return result;
}

bool GStreamerWebAudioPlayerClient::close()
{
    GST_DEBUG("entry:");

    mBackendQueue.callInEventLoop(
        [&]()
        {
            mClientBackend->destroyWebAudioBackend();
            m_pushSamplesTimer.cancel();
            mIsOpen = false;
        });

    return true;
}

bool GStreamerWebAudioPlayerClient::play()
{
    GST_DEBUG("entry:");

    bool result = false;
    mBackendQueue.callInEventLoop(
        [&]()
        {
            if (mIsOpen)
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

bool GStreamerWebAudioPlayerClient::pause()
{
    GST_DEBUG("entry:");

    bool result = false;
    mBackendQueue.callInEventLoop(
        [&]()
        {
            if (mIsOpen)
            {
                result = mClientBackend->pause();
            }
            else
            {
                GST_ERROR("No web audio backend");
            }
        });

    return result;
}

bool GStreamerWebAudioPlayerClient::setEos()
{
    GST_DEBUG("entry:");

    bool result = false;
    mBackendQueue.callInEventLoop(
        [&]()
        {
            if (mIsOpen && !m_isEos)
            {
                m_isEos = true;
                if (mSampleDataBuffer.empty())
                {
                    result = mClientBackend->setEos();
                }
                else
                {
                    pushSamples();
                    result = true;
                }
            }
            else
            {
                GST_DEBUG("No web audio backend, valid scenario");
            }
        });

    return result;
}

void GStreamerWebAudioPlayerClient::notifyPushSamplesTimerExpired()
{
    mBackendQueue.callInEventLoop([&]() { pushSamples(); });
}

bool GStreamerWebAudioPlayerClient::notifyNewSample()
{
    GST_DEBUG("entry:");

    mBackendQueue.callInEventLoop(
        [&]()
        {
            m_pushSamplesTimer.cancel();
            getNextBufferData();
            pushSamples();
        });

    return true;
}

void GStreamerWebAudioPlayerClient::pushSamples()
{
    GST_DEBUG("entry:");
    if (!mIsOpen || mSampleDataBuffer.empty())
    {
        return;
    }
    uint32_t availableFrames = 0u;
    if (mClientBackend->getBufferAvailable(availableFrames))
    {
        auto dataToPush = std::min(static_cast<std::size_t>(availableFrames * m_frameSize), mSampleDataBuffer.size());
        if ((dataToPush / m_frameSize > 0))
        {
            if (mClientBackend->writeBuffer(dataToPush / m_frameSize, mSampleDataBuffer.data()))
            {
                // remove pushed data from mSampleDataBuffer
                if (dataToPush < mSampleDataBuffer.size())
                {
                    // to compact the memory copy everything to a new vector and swap.
                    std::vector<uint8_t>(mSampleDataBuffer.begin() + dataToPush, mSampleDataBuffer.end())
                        .swap(mSampleDataBuffer);
                }
                else
                {
                    mSampleDataBuffer.clear();
                }
            }
            else
            {
                GST_ERROR("writeBuffer failed, could not process samples");
                // clear the buffer if writeBuffer failed
                mSampleDataBuffer.clear();
            }
        }
    }
    else
    {
        GST_ERROR("getBufferAvailable failed, could not process samples");
        // clear the buffer if getBufferAvailable failed
        mSampleDataBuffer.clear();
    }

    // If we still have samples stored that could not be pushed
    // This avoids any stoppages in the pushing of samples to the server if the consumption of
    // samples is slow.
    if (mSampleDataBuffer.size())
    {
        m_pushSamplesTimer.arm(100);
    }
    else if (m_isEos)
    {
        mClientBackend->setEos();
    }
}

void GStreamerWebAudioPlayerClient::getNextBufferData()
{
    GstSample *sample = gst_app_sink_try_pull_sample(GST_APP_SINK(mAppSink), 0);
    if (!sample)
    {
        return;
    }

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    uint32_t bufferSize = gst_buffer_get_size(buffer);
    GstMapInfo bufferMap;

    if (!gst_buffer_map(buffer, &bufferMap, GST_MAP_READ))
    {
        GST_ERROR("Could not map audio buffer");
        gst_sample_unref(sample);
        return;
    }

    mSampleDataBuffer.insert(mSampleDataBuffer.end(), bufferMap.data, bufferMap.data + bufferSize);
    gst_buffer_unmap(buffer, &bufferMap);
    gst_sample_unref(sample);
}

bool GStreamerWebAudioPlayerClient::isNewConfig(const std::string &audioMimeType,
                                                const firebolt::rialto::WebAudioConfig &config)
{
    if (audioMimeType != m_mimeType)
    {
        return true;
    }

    if (audioMimeType != "audio/x-raw")
    {
        GST_ERROR("Cannot compare none pcm config");
        return true;
    }

    if (config.pcm != m_config.pcm)
    {
        return true;
    }

    return false;
}

void GStreamerWebAudioPlayerClient::notifyState(firebolt::rialto::WebAudioPlayerState state)
{
    switch (state)
    {
    case firebolt::rialto::WebAudioPlayerState::END_OF_STREAM:
    {
        GST_INFO("Notify end of stream.");
        gst_element_post_message(mAppSink, gst_message_new_eos(GST_OBJECT_CAST(mAppSink)));
        m_isEos = false;
        break;
    }
    case firebolt::rialto::WebAudioPlayerState::FAILURE:
    {
        std::string errMessage = "Rialto server webaudio playback failed";
        GST_ERROR("%s", errMessage.c_str());
        gst_element_post_message(mAppSink,
                                 gst_message_new_error(GST_OBJECT_CAST(mAppSink),
                                                       g_error_new_literal(GST_STREAM_ERROR, 0, errMessage.c_str()),
                                                       errMessage.c_str()));
        break;
    }
    default:
    {
    }
    }
}
