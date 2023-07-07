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

GStreamerWebAudioPlayerClient::GStreamerWebAudioPlayerClient(WebAudioSinkCallbacks callbacks)
    : m_isOpen(false), m_pushSamplesTimer(notifyPushSamplesCallback, this, "notifyPushSamplesCallback"), m_isEos(false),
      m_config({}), m_callbacks(callbacks)
{
    m_backendQueue.start();
    m_clientBackend = std::make_unique<firebolt::rialto::client::WebAudioClientBackend>();
}

GStreamerWebAudioPlayerClient::~GStreamerWebAudioPlayerClient()
{
    m_backendQueue.stop();
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

    m_backendQueue.callInEventLoop(
        [&]()
        {
            firebolt::rialto::WebAudioConfig config{pcm};

            // Only recreate player if the config has changed
            if (!m_isOpen || isNewConfig(audioMimeType, config))
            {
                if (m_isOpen)
                {
                    // Destroy the previously created player
                    m_clientBackend->destroyWebAudioBackend();
                }

                uint32_t priority = 1;
                if (m_clientBackend->createWebAudioBackend(shared_from_this(), audioMimeType, priority, &config))
                {
                    if (!m_clientBackend->getDeviceInfo(m_preferredFrames, m_maximumFrames, m_supportDeferredPlay))
                    {
                        GST_ERROR("GetDeviceInfo failed, could not process samples");
                    }
                    m_frameSize = (pcm.sampleSize * pcm.channels) / CHAR_BIT;
                    m_isOpen = true;

                    // Store config
                    m_config.pcm = pcm;
                    m_mimeType = audioMimeType;
                }
                else
                {
                    GST_ERROR("Could not create web audio backend");
                    m_isOpen = false;
                }
                result = m_isOpen;
            }
        });

    return result;
}

bool GStreamerWebAudioPlayerClient::close()
{
    GST_DEBUG("entry:");

    m_backendQueue.callInEventLoop(
        [&]()
        {
            m_clientBackend->destroyWebAudioBackend();
            m_pushSamplesTimer.cancel();
            m_isOpen = false;
        });

    return true;
}

bool GStreamerWebAudioPlayerClient::play()
{
    GST_DEBUG("entry:");

    bool result = false;
    m_backendQueue.callInEventLoop(
        [&]()
        {
            if (m_isOpen)
            {
                result = m_clientBackend->play();
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
    m_backendQueue.callInEventLoop(
        [&]()
        {
            if (m_isOpen)
            {
                result = m_clientBackend->pause();
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
    m_backendQueue.callInEventLoop(
        [&]()
        {
            if (m_isOpen && !m_isEos)
            {
                m_isEos = true;
                if (m_dataBuffers.empty())
                {
                    result = m_clientBackend->setEos();
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

bool GStreamerWebAudioPlayerClient::isOpen()
{
    GST_DEBUG("entry:");

    bool result = false;
    m_backendQueue.callInEventLoop([&]() { result = m_isOpen; });

    return result;
}

void GStreamerWebAudioPlayerClient::notifyPushSamplesTimerExpired()
{
    m_backendQueue.callInEventLoop([&]() { pushSamples(); });
}

bool GStreamerWebAudioPlayerClient::notifyNewSample(GstBuffer *buf)
{
    GST_DEBUG("entry:");

    bool result = false;
    m_backendQueue.callInEventLoop(
        [&]()
        {
            if (buf)
            {
                m_pushSamplesTimer.cancel();
                m_dataBuffers.push(buf);
                pushSamples();
                result = true;
            }
        });

    return result;
}

void GStreamerWebAudioPlayerClient::pushSamples()
{
    GST_DEBUG("entry:");
    if (!m_isOpen || m_dataBuffers.empty())
    {
        return;
    }

    uint32_t availableFrames = 0u;
    do
    {
        if (!m_clientBackend->getBufferAvailable(availableFrames))
        {
            GST_ERROR("getBufferAvailable failed, could not process the samples");
            // clear the queue if getBufferAvailable failed
            std::queue<GstBuffer *> empty;
            std::swap(m_dataBuffers, empty);
        }
        else if (0 != availableFrames)
        {
            bool writeFailure = false;
            GstBuffer *buffer = m_dataBuffers.front();
            gsize bufferSize = gst_buffer_get_size(buffer);
            auto framesToWrite = std::min(availableFrames, static_cast<uint32_t>(bufferSize / m_frameSize));
            if (framesToWrite > 0)
            {
                GstMapInfo bufferMap;
                if (!gst_buffer_map(buffer, &bufferMap, GST_MAP_READ))
                {
                    GST_ERROR("Could not map audio buffer, discarding buffer!");
                    writeFailure = true;
                }
                else
                {
                    if (!m_clientBackend->writeBuffer(framesToWrite, bufferMap.data))
                    {
                        GST_ERROR("Could not map audio buffer, discarding buffer!");
                        writeFailure = true;
                    }
                    gst_buffer_unmap(buffer, &bufferMap);
                }
            }

            if ((!writeFailure) && (framesToWrite * m_frameSize < bufferSize))
            {
                // Handle any leftover data
                uint32_t leftoverData = bufferSize - (availableFrames * m_frameSize);
                gst_buffer_resize(buffer, framesToWrite * m_frameSize, leftoverData);
                if ((leftoverData / m_frameSize == 0) && (m_dataBuffers.size() > 1))
                {
                    // If the leftover data is smaller than a frame, it must be processed with the next buffer
                    m_dataBuffers.pop();
                    m_dataBuffers.front() = gst_buffer_append(buffer, m_dataBuffers.front());
                    gst_buffer_unref(buffer);
                }
            }
            else
            {
                m_dataBuffers.pop();
                gst_buffer_unref(buffer);
            }
        }
    } while (!m_dataBuffers.empty() && availableFrames != 0);

    // If we still have samples stored that could not be pushed
    // This avoids any stoppages in the pushing of samples to the server if the consumption of
    // samples is slow.
    if (m_dataBuffers.size())
    {
        m_pushSamplesTimer.arm(100);
    }
    else if (m_isEos)
    {
        m_clientBackend->setEos();
    }
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
        if (m_callbacks.eosCallback)
        {
            m_callbacks.eosCallback();
        }
        m_isEos = false;
        break;
    }
    case firebolt::rialto::WebAudioPlayerState::FAILURE:
    {
        std::string errMessage = "Rialto server webaudio playback failed";
        GST_ERROR("%s", errMessage.c_str());
        if (m_callbacks.errorCallback)
        {
            m_callbacks.errorCallback(errMessage.c_str());
        }
        break;
    }
    case firebolt::rialto::WebAudioPlayerState::IDLE:
    case firebolt::rialto::WebAudioPlayerState::PLAYING:
    case firebolt::rialto::WebAudioPlayerState::PAUSED:
    {
        if (m_callbacks.stateChangedCallback)
        {
            m_callbacks.stateChangedCallback(state);
        }
        break;
    }
    case firebolt::rialto::WebAudioPlayerState::UNKNOWN:
    default:
    {
        GST_WARNING("Web audio player sent unknown state");
        break;
    }
    }
}
