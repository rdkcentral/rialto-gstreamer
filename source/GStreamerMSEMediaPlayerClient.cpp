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

#include "GStreamerMSEMediaPlayerClient.h"
#include "GstreamerCatLog.h"
#include "RialtoGStreamerMSEBaseSink.h"
#include "RialtoGStreamerMSEBaseSinkPrivate.h"
#include "RialtoGStreamerMSEVideoSink.h"
#include <algorithm>
#include <chrono>
#include <thread>

namespace
{
// The start time of segment might differ from the first sample which is injected.
// That difference should not be bigger than 1 video / audio frame.
// 1 second is probably erring on the side of caution, but should not have side effect.
const int64_t segmentStartMaximumDiff = 1000000000;
const int32_t UNKNOWN_STREAMS_NUMBER = -1;

const char *toString(const firebolt::rialto::PlaybackError &error)
{
    switch (error)
    {
    case firebolt::rialto::PlaybackError::DECRYPTION:
        return "DECRYPTION";
    case firebolt::rialto::PlaybackError::UNKNOWN:
        return "UNKNOWN";
    }
    return "UNKNOWN";
}
const char *toString(const firebolt::rialto::MediaSourceType &src)
{
    switch (src)
    {
    case firebolt::rialto::MediaSourceType::AUDIO:
        return "AUDIO";
    case firebolt::rialto::MediaSourceType::VIDEO:
        return "VIDEO";
    case firebolt::rialto::MediaSourceType::SUBTITLE:
        return "SUBTITLE";
    case firebolt::rialto::MediaSourceType::UNKNOWN:
        return "UNKNOWN";
    }
    return "UNKNOWN";
}
} // namespace
#define GST_CAT_DEFAULT rialtoGStreamerCat
GStreamerMSEMediaPlayerClient::GStreamerMSEMediaPlayerClient(
    const std::shared_ptr<IMessageQueueFactory> &messageQueueFactory,
    const std::shared_ptr<firebolt::rialto::client::MediaPlayerClientBackendInterface> &MediaPlayerClientBackend,
    const uint32_t maxVideoWidth, const uint32_t maxVideoHeight)
    : m_backendQueue{messageQueueFactory->createMessageQueue()}, m_messageQueueFactory{messageQueueFactory},
      m_clientBackend(MediaPlayerClientBackend),
      m_duration(0), m_audioStreams{UNKNOWN_STREAMS_NUMBER}, m_videoStreams{UNKNOWN_STREAMS_NUMBER},
      m_subtitleStreams{UNKNOWN_STREAMS_NUMBER}, m_videoRectangle{0, 0, 1920, 1080}, m_streamingStopped(false),
      m_maxWidth(maxVideoWidth == 0 ? DEFAULT_MAX_VIDEO_WIDTH : maxVideoWidth),
      m_maxHeight(maxVideoHeight == 0 ? DEFAULT_MAX_VIDEO_HEIGHT : maxVideoHeight)
{
    m_backendQueue->start();
}

GStreamerMSEMediaPlayerClient::~GStreamerMSEMediaPlayerClient()
{
    stopStreaming();
}

void GStreamerMSEMediaPlayerClient::stopStreaming()
{
    if (!m_streamingStopped)
    {
        m_backendQueue->stop();

        for (auto &source : m_attachedSources)
        {
            source.second.m_bufferPuller->stop();
        }

        m_streamingStopped = true;
    }
}

// Deletes client backend -> this deletes mediapipeline object
void GStreamerMSEMediaPlayerClient::destroyClientBackend()
{
    m_clientBackend.reset();
}

void GStreamerMSEMediaPlayerClient::notifyDuration(int64_t duration)
{
    m_backendQueue->postMessage(std::make_shared<SetDurationMessage>(duration, m_duration));
}

void GStreamerMSEMediaPlayerClient::notifyPosition(int64_t position)
{
    m_backendQueue->postMessage(std::make_shared<SetPositionMessage>(position, m_attachedSources));
}

void GStreamerMSEMediaPlayerClient::notifyNativeSize(uint32_t width, uint32_t height, double aspect) {}

void GStreamerMSEMediaPlayerClient::notifyNetworkState(firebolt::rialto::NetworkState state) {}

void GStreamerMSEMediaPlayerClient::notifyPlaybackState(firebolt::rialto::PlaybackState state)
{
    m_backendQueue->postMessage(std::make_shared<PlaybackStateMessage>(state, this));
}

void GStreamerMSEMediaPlayerClient::notifyVideoData(bool hasData) {}

void GStreamerMSEMediaPlayerClient::notifyAudioData(bool hasData) {}

void GStreamerMSEMediaPlayerClient::notifyNeedMediaData(
    int32_t sourceId, size_t frameCount, uint32_t needDataRequestId,
    const std::shared_ptr<firebolt::rialto::MediaPlayerShmInfo> & /*shmInfo*/)
{
    m_backendQueue->postMessage(std::make_shared<NeedDataMessage>(sourceId, frameCount, needDataRequestId, this));

    return;
}

void GStreamerMSEMediaPlayerClient::notifyCancelNeedMediaData(int sourceId) {}

void GStreamerMSEMediaPlayerClient::notifyQos(int32_t sourceId, const firebolt::rialto::QosInfo &qosInfo)
{
    m_backendQueue->postMessage(std::make_shared<QosMessage>(sourceId, qosInfo, this));
}

void GStreamerMSEMediaPlayerClient::notifyBufferUnderflow(int32_t sourceId)
{
    m_backendQueue->postMessage(std::make_shared<BufferUnderflowMessage>(sourceId, this));
}

void GStreamerMSEMediaPlayerClient::notifyPlaybackError(int32_t sourceId, firebolt::rialto::PlaybackError error)
{
    m_backendQueue->postMessage(std::make_shared<PlaybackErrorMessage>(sourceId, error, this));
}

void GStreamerMSEMediaPlayerClient::notifySourceFlushed(int32_t sourceId)
{
    m_backendQueue->postMessage(std::make_shared<SourceFlushedMessage>(sourceId, this));
}

void GStreamerMSEMediaPlayerClient::getPositionDo(int64_t *position, int32_t sourceId)
{
    auto sourceIt = m_attachedSources.find(sourceId);
    if (sourceIt == m_attachedSources.end())
    {
        *position = -1;
        return;
    }

    if (m_clientBackend && m_clientBackend->getPosition(*position))
    {
        sourceIt->second.m_position = *position;
    }
    else
    {
        *position = sourceIt->second.m_position;
    }
}

int64_t GStreamerMSEMediaPlayerClient::getPosition(int32_t sourceId)
{
    int64_t position;
    m_backendQueue->callInEventLoop([&]() { getPositionDo(&position, sourceId); });
    return position;
}

bool GStreamerMSEMediaPlayerClient::createBackend()
{
    bool result = false;
    m_backendQueue->callInEventLoop(
        [&]()
        {
            if (!m_clientBackend)
            {
                GST_ERROR("Client backend is NULL");
                result = false;
                return;
            }
            m_clientBackend->createMediaPlayerBackend(shared_from_this(), m_maxWidth, m_maxHeight);

            if (m_clientBackend->isMediaPlayerBackendCreated())
            {
                std::string utf8url = "mse://1";
                firebolt::rialto::MediaType mediaType = firebolt::rialto::MediaType::MSE;
                if (!m_clientBackend->load(mediaType, "", utf8url))
                {
                    GST_ERROR("Could not load RialtoClient");
                    return;
                }
                result = true;
            }
            else
            {
                GST_ERROR("Media player backend could not be created");
            }
        });

    return result;
}

StateChangeResult GStreamerMSEMediaPlayerClient::play(int32_t sourceId)
{
    StateChangeResult result = StateChangeResult::NOT_ATTACHED;
    m_backendQueue->callInEventLoop(
        [&]()
        {
            auto sourceIt = m_attachedSources.find(sourceId);
            if (sourceIt == m_attachedSources.end())
            {
                GST_ERROR("Cannot play - there's no attached source with id %d", sourceId);
                result = StateChangeResult::NOT_ATTACHED;
                return;
            }

            if (m_serverPlaybackState == firebolt::rialto::PlaybackState::PLAYING)
            {
                GST_INFO("Server is already playing");
                sourceIt->second.m_state = ClientState::PLAYING;

                if (checkIfAllAttachedSourcesInState(ClientState::PLAYING))
                {
                    m_clientState = ClientState::PLAYING;
                }

                result = StateChangeResult::SUCCESS_SYNC;
                return;
            }

            sourceIt->second.m_state = ClientState::AWAITING_PLAYING;

            if (m_clientState == ClientState::PAUSED)
            {
                if (checkIfAllAttachedSourcesInState(ClientState::AWAITING_PLAYING))
                {
                    GST_INFO("Sending play command");
                    m_clientBackend->play();
                    m_clientState = ClientState::AWAITING_PLAYING;
                }
                else
                {
                    GST_DEBUG("Not all sources are ready to play");
                }
            }
            else
            {
                GST_WARNING("Not in PAUSED state in %u state", static_cast<uint32_t>(m_clientState));
            }

            result = StateChangeResult::SUCCESS_ASYNC;
        });

    return result;
}

StateChangeResult GStreamerMSEMediaPlayerClient::pause(int32_t sourceId)
{
    StateChangeResult result = StateChangeResult::NOT_ATTACHED;
    m_backendQueue->callInEventLoop(
        [&]()
        {
            auto sourceIt = m_attachedSources.find(sourceId);
            if (sourceIt == m_attachedSources.end())
            {
                GST_WARNING("Cannot pause - there's no attached source with id %d", sourceId);

                result = StateChangeResult::NOT_ATTACHED;
                return;
            }

            if (m_serverPlaybackState == firebolt::rialto::PlaybackState::PAUSED &&
                m_clientState != ClientState::AWAITING_PLAYING && m_clientState != ClientState::AWAITING_PAUSED)
            {
                // if the server is already paused and we are not in async, we don't need to send pause command
                GST_INFO("Server is already paused");
                sourceIt->second.m_state = ClientState::PAUSED;

                if (checkIfAllAttachedSourcesInState(ClientState::PAUSED))
                {
                    m_clientState = ClientState::PAUSED;
                }

                result = StateChangeResult::SUCCESS_SYNC;
            }
            else
            {
                sourceIt->second.m_state = ClientState::AWAITING_PAUSED;

                bool shouldPause = false;
                if (m_clientState == ClientState::READY)
                {
                    if (checkIfAllAttachedSourcesInState(ClientState::AWAITING_PAUSED))
                    {
                        shouldPause = true;
                    }
                    else
                    {
                        GST_DEBUG("Not all attached sources are ready to pause");
                    }
                }
                else if (m_clientState == ClientState::AWAITING_PLAYING || m_clientState == ClientState::PLAYING)
                {
                    shouldPause = true;
                }
                else
                {
                    GST_DEBUG("Cannot pause in %u state", static_cast<uint32_t>(m_clientState));
                }

                if (shouldPause)
                {
                    GST_INFO("Sending pause command in %u state", static_cast<uint32_t>(m_clientState));
                    m_clientBackend->pause();
                    m_clientState = ClientState::AWAITING_PAUSED;
                }

                result = StateChangeResult::SUCCESS_ASYNC;
            }
        });

    return result;
}

void GStreamerMSEMediaPlayerClient::notifyLostState(int32_t sourceId)
{
    m_backendQueue->callInEventLoop(
        [&]()
        {
            auto sourceIt = m_attachedSources.find(sourceId);
            if (sourceIt == m_attachedSources.end())
            {
                GST_ERROR("There's no attached source with id %d", sourceId);
                return;
            }

            rialto_mse_base_sink_lost_state(sourceIt->second.m_rialtoSink);

            sourceIt->second.m_state = ClientState::AWAITING_PAUSED;
            if (m_clientState == ClientState::AWAITING_PLAYING || m_clientState == ClientState::PLAYING)
            {
                GST_INFO("Sending pause command in %u state", static_cast<uint32_t>(m_clientState));
                m_clientBackend->pause();
                m_clientState = ClientState::AWAITING_PAUSED;
            }
            else if (m_clientState == ClientState::PAUSED)
            {
                m_clientState = ClientState::AWAITING_PAUSED;
            }
        });
}

void GStreamerMSEMediaPlayerClient::stop()
{
    m_backendQueue->callInEventLoop([&]() { m_clientBackend->stop(); });
}

void GStreamerMSEMediaPlayerClient::setPlaybackRate(double rate)
{
    m_backendQueue->callInEventLoop([&]() { m_clientBackend->setPlaybackRate(rate); });
}

void GStreamerMSEMediaPlayerClient::flush(int32_t sourceId, bool resetTime)
{
    m_backendQueue->callInEventLoop(
        [&]()
        {
            auto sourceIt = m_attachedSources.find(sourceId);
            if (sourceIt == m_attachedSources.end())
            {
                GST_ERROR("Cannot flush - there's no attached source with id %d", sourceId);
                return;
            }
            if (!m_clientBackend->flush(sourceId, resetTime))
            {
                GST_ERROR("Flush operation failed for source with id %d", sourceId);
                return;
            }
            sourceIt->second.m_isFlushing = true;
            sourceIt->second.m_bufferPuller->stop();
        });
}

void GStreamerMSEMediaPlayerClient::setSourcePosition(int32_t sourceId, int64_t position)
{
    m_backendQueue->callInEventLoop(
        [&]()
        {
            auto sourceIt = m_attachedSources.find(sourceId);
            if (sourceIt == m_attachedSources.end())
            {
                GST_ERROR("Cannot Set Source Position - there's no attached source with id %d", sourceId);
                return;
            }
            if (!m_clientBackend->setSourcePosition(sourceId, position))
            {
                GST_ERROR("Set Source Position operation failed for source with id %d", sourceId);
                return;
            }
            sourceIt->second.m_position = position;
        });
}

bool GStreamerMSEMediaPlayerClient::attachSource(std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> &source,
                                                 RialtoMSEBaseSink *rialtoSink)
{
    if (source->getType() != firebolt::rialto::MediaSourceType::AUDIO &&
        source->getType() != firebolt::rialto::MediaSourceType::VIDEO)
    {
        GST_WARNING_OBJECT(rialtoSink, "Invalid source type %u", static_cast<uint32_t>(source->getType()));
        return false;
    }

    bool result = false;
    m_backendQueue->callInEventLoop(
        [&]()
        {
            result = m_clientBackend->attachSource(source);

            if (result)
            {
                std::shared_ptr<BufferPuller> bufferPuller;
                if (source->getType() == firebolt::rialto::MediaSourceType::AUDIO)
                {
                    std::shared_ptr<AudioBufferParser> audioBufferParser = std::make_shared<AudioBufferParser>();
                    bufferPuller = std::make_shared<BufferPuller>(m_messageQueueFactory, GST_ELEMENT_CAST(rialtoSink),
                                                                  audioBufferParser);
                }
                else if (source->getType() == firebolt::rialto::MediaSourceType::VIDEO)
                {
                    std::shared_ptr<VideoBufferParser> videoBufferParser = std::make_shared<VideoBufferParser>();
                    bufferPuller = std::make_shared<BufferPuller>(m_messageQueueFactory, GST_ELEMENT_CAST(rialtoSink),
                                                                  videoBufferParser);
                }

                if (m_attachedSources.find(source->getId()) == m_attachedSources.end())
                {
                    m_attachedSources.emplace(source->getId(),
                                              AttachedSource(rialtoSink, bufferPuller, source->getType()));

                    rialtoSink->priv->m_sourceId = source->getId();
                    bufferPuller->start();
                }
            }

            sendAllSourcesAttachedIfPossibleInternal();
        });

    return result;
}

void GStreamerMSEMediaPlayerClient::sendAllSourcesAttachedIfPossible()
{
    m_backendQueue->callInEventLoop([&]() { sendAllSourcesAttachedIfPossibleInternal(); });
}

void GStreamerMSEMediaPlayerClient::sendAllSourcesAttachedIfPossibleInternal()
{
    if (!m_wasAllSourcesAttachedSent && areAllStreamsAttached())
    {
        // RialtoServer doesn't support dynamic source attachment.
        // It means that when we notify that all sources were attached, we cannot add any more sources in the current session
        GST_INFO("All sources attached");
        m_clientBackend->allSourcesAttached();
        m_wasAllSourcesAttachedSent = true;
        m_clientState = ClientState::READY;

        // In playbin3 streams, confirmation about number of available sources comes after attaching the source,
        // so we need to check if all sources are ready to pause
        if (checkIfAllAttachedSourcesInState(ClientState::AWAITING_PAUSED))
        {
            GST_INFO("Sending pause command, because all attached sources are ready to pause");
            m_clientBackend->pause();
            m_clientState = ClientState::AWAITING_PAUSED;
        }
    }
}

void GStreamerMSEMediaPlayerClient::removeSource(int32_t sourceId)
{
    m_backendQueue->callInEventLoop(
        [&]()
        {
            if (!m_clientBackend->removeSource(sourceId))
            {
                GST_WARNING("Remove source %d failed", sourceId);
            }
            m_attachedSources.erase(sourceId);
        });
}

void GStreamerMSEMediaPlayerClient::handlePlaybackStateChange(firebolt::rialto::PlaybackState state)
{
    GST_DEBUG("Received state change to state %u", static_cast<uint32_t>(state));
    m_backendQueue->callInEventLoop(
        [&]()
        {
            m_serverPlaybackState = state;
            switch (state)
            {
            case firebolt::rialto::PlaybackState::PAUSED:
            case firebolt::rialto::PlaybackState::PLAYING:
            {
                for (auto &source : m_attachedSources)
                {
                    if (state == firebolt::rialto::PlaybackState::PAUSED &&
                        source.second.m_state == ClientState::AWAITING_PAUSED)
                    {
                        source.second.m_state = ClientState::PAUSED;
                    }
                    else if (state == firebolt::rialto::PlaybackState::PLAYING &&
                             source.second.m_state == ClientState::AWAITING_PLAYING)
                    {
                        source.second.m_state = ClientState::PLAYING;
                    }

                    rialto_mse_base_handle_rialto_server_state_changed(source.second.m_rialtoSink, state);
                }

                if (state == firebolt::rialto::PlaybackState::PAUSED && m_clientState == ClientState::AWAITING_PAUSED)
                {
                    m_clientState = ClientState::PAUSED;
                }
                else if (state == firebolt::rialto::PlaybackState::PLAYING &&
                         m_clientState == ClientState::AWAITING_PLAYING)
                {
                    m_clientState = ClientState::PLAYING;
                }

                break;
            }
            case firebolt::rialto::PlaybackState::END_OF_STREAM:
            {
                for (const auto &source : m_attachedSources)
                {
                    rialto_mse_base_handle_rialto_server_eos(source.second.m_rialtoSink);
                }
            }
            break;
            case firebolt::rialto::PlaybackState::SEEK_DONE:
            {
                GST_WARNING("firebolt::rialto::PlaybackState::SEEK_DONE notification not supported");
                break;
            }
            case firebolt::rialto::PlaybackState::FAILURE:
            {
                for (auto &source : m_attachedSources)
                {
                    rialto_mse_base_handle_rialto_server_error(source.second.m_rialtoSink,
                                                               firebolt::rialto::PlaybackError::UNKNOWN);
                }
                for (auto &source : m_attachedSources)
                {
                    source.second.m_position = 0;
                }

                break;
            }
            break;
            default:
                break;
            }
        });
}

void GStreamerMSEMediaPlayerClient::handleSourceFlushed(int32_t sourceId)
{
    m_backendQueue->callInEventLoop(
        [&]()
        {
            auto sourceIt = m_attachedSources.find(sourceId);
            if (sourceIt == m_attachedSources.end())
            {
                GST_ERROR("Cannot finish flush - there's no attached source with id %d", sourceId);
                return;
            }
            if (!sourceIt->second.m_isFlushing)
            {
                GST_ERROR("Cannot finish flush - source with id %d is not flushing!", sourceId);
                return;
            }
            sourceIt->second.m_isFlushing = false;
            sourceIt->second.m_bufferPuller->start();
            rialto_mse_base_handle_rialto_server_completed_flush(sourceIt->second.m_rialtoSink);
        });
}

void GStreamerMSEMediaPlayerClient::setVideoRectangle(const std::string &rectangleString)
{
    m_backendQueue->callInEventLoop(
        [&]()
        {
            if (!m_clientBackend || !m_clientBackend->isMediaPlayerBackendCreated())
            {
                GST_WARNING("Missing RialtoClient backend - can't set video window now");
                return;
            }

            if (rectangleString.empty())
            {
                GST_WARNING("Empty video rectangle string");
                return;
            }

            Rectangle rect = {0, 0, 0, 0};
            if (sscanf(rectangleString.c_str(), "%u,%u,%u,%u", &rect.x, &rect.y, &rect.width, &rect.height) != 4)
            {
                GST_WARNING("Invalid video rectangle values");
                return;
            }

            m_clientBackend->setVideoWindow(rect.x, rect.y, rect.width, rect.height);
            m_videoRectangle = rect;
        });
}

std::string GStreamerMSEMediaPlayerClient::getVideoRectangle()
{
    char rectangle[64];
    m_backendQueue->callInEventLoop(
        [&]()
        {
            sprintf(rectangle, "%u,%u,%u,%u", m_videoRectangle.x, m_videoRectangle.y, m_videoRectangle.width,
                    m_videoRectangle.height);
        });

    return std::string(rectangle);
}

bool GStreamerMSEMediaPlayerClient::renderFrame(RialtoMSEBaseSink *sink)
{
    bool result = false;
    m_backendQueue->callInEventLoop(
        [&]()
        {
            result = m_clientBackend->renderFrame();
            if (result)
            {
                // RialtoServer's video sink should drop PAUSED state due to skipping prerolled buffer in PAUSED state
                rialto_mse_base_sink_lost_state(sink);
            }
        });
    return result;
}

void GStreamerMSEMediaPlayerClient::setVolume(double volume)
{
    m_backendQueue->callInEventLoop([&]() { m_clientBackend->setVolume(volume); });
}

double GStreamerMSEMediaPlayerClient::getVolume()
{
    double volume{0.0};
    m_backendQueue->callInEventLoop(
        [&]()
        {
            if (m_clientBackend->getVolume(volume))
            {
                m_volume = volume;
            }
            else
            {
                volume = m_volume;
            }
        });
    return volume;
}

void GStreamerMSEMediaPlayerClient::setMute(bool mute)
{
    m_backendQueue->callInEventLoop([&]() { m_clientBackend->setMute(mute); });
}

bool GStreamerMSEMediaPlayerClient::getMute()
{
    bool mute{false};
    m_backendQueue->callInEventLoop(
        [&]()
        {
            if (m_clientBackend->getMute(mute))
            {
                m_mute = mute;
            }
            else
            {
                mute = m_mute;
            }
        });
    return mute;
}

ClientState GStreamerMSEMediaPlayerClient::getClientState()
{
    ClientState state{ClientState::IDLE};
    m_backendQueue->callInEventLoop([&]() { state = m_clientState; });
    return state;
}

void GStreamerMSEMediaPlayerClient::handleStreamCollection(int32_t audioStreams, int32_t videoStreams,
                                                           int32_t subtitleStreams)
{
    m_backendQueue->callInEventLoop(
        [&]()
        {
            if (m_audioStreams == UNKNOWN_STREAMS_NUMBER)
                m_audioStreams = audioStreams;
            if (m_videoStreams == UNKNOWN_STREAMS_NUMBER)
                m_videoStreams = videoStreams;
            if (m_subtitleStreams == UNKNOWN_STREAMS_NUMBER)
                m_subtitleStreams = subtitleStreams;

            GST_INFO("Updated number of streams. New stream values; video=%d, audio=%d, text=%d", m_videoStreams,
                     m_audioStreams, m_subtitleStreams);

            // TODO: remove below log after subtitle sink is implemented
            if (m_subtitleStreams > 0)
            {
                GST_WARNING("Subtitle streams are not supported yet");
            }
        });
}

bool GStreamerMSEMediaPlayerClient::checkIfAllAttachedSourcesInState(ClientState state)
{
    return std::all_of(m_attachedSources.begin(), m_attachedSources.end(),
                       [state](const auto &source) { return source.second.m_state == state; });
}

bool GStreamerMSEMediaPlayerClient::areAllStreamsAttached()
{
    int32_t attachedVideoSources = 0;
    int32_t attachedAudioSources = 0;
    int32_t attachedSubtitleSources = 0;
    for (auto &source : m_attachedSources)
    {
        if (source.second.getType() == firebolt::rialto::MediaSourceType::VIDEO)
        {
            attachedVideoSources++;
        }
        else if (source.second.getType() == firebolt::rialto::MediaSourceType::AUDIO)
        {
            attachedAudioSources++;
        }
        else if (source.second.getType() == firebolt::rialto::MediaSourceType::SUBTITLE)
        {
            attachedSubtitleSources++;
        }
    }

    GST_WARNING("attachedVideoSources %d, m_videoStreams %d, attachedAudioSources %d, m_audioStreams %d, attachedSubtitleSources %d, m_subtitleStreams %d", 
    attachedVideoSources, m_videoStreams, attachedAudioSources, m_audioStreams, attachedSubtitleSources, m_subtitleStreams);
    return attachedVideoSources == m_videoStreams && attachedAudioSources == m_audioStreams &&
           attachedSubtitleSources == m_subtitleStreams;
}

bool GStreamerMSEMediaPlayerClient::requestPullBuffer(int streamId, size_t frameCount, unsigned int needDataRequestId)
{
    bool result = false;
    m_backendQueue->callInEventLoop(
        [&]()
        {
            auto sourceIt = m_attachedSources.find(streamId);
            if (sourceIt == m_attachedSources.end())
            {
                GST_ERROR("There's no attached source with id %d", streamId);

                result = false;
                return;
            }
            result = sourceIt->second.m_bufferPuller->requestPullBuffer(streamId, frameCount, needDataRequestId, this);
        });

    return result;
}

bool GStreamerMSEMediaPlayerClient::handleQos(int sourceId, firebolt::rialto::QosInfo qosInfo)
{
    bool result = false;
    m_backendQueue->callInEventLoop(
        [&]()
        {
            auto sourceIt = m_attachedSources.find(sourceId);
            if (sourceIt == m_attachedSources.end())
            {
                result = false;
                return;
            }

            rialto_mse_base_handle_rialto_server_sent_qos(sourceIt->second.m_rialtoSink, qosInfo.processed,
                                                          qosInfo.dropped);

            result = true;
        });

    return result;
}

bool GStreamerMSEMediaPlayerClient::handleBufferUnderflow(int sourceId)
{
    bool result = false;
    m_backendQueue->callInEventLoop(
        [&]()
        {
            auto sourceIt = m_attachedSources.find(sourceId);
            if (sourceIt == m_attachedSources.end())
            {
                result = false;
                return;
            }

            rialto_mse_base_handle_rialto_server_sent_buffer_underflow(sourceIt->second.m_rialtoSink);

            result = true;
        });

    return result;
}

bool GStreamerMSEMediaPlayerClient::handlePlaybackError(int sourceId, firebolt::rialto::PlaybackError error)
{
    bool result = false;
    m_backendQueue->callInEventLoop(
        [&]()
        {
            auto sourceIt = m_attachedSources.find(sourceId);
            if (sourceIt == m_attachedSources.end())
            {
                result = false;
                return;
            }

            // Even though rialto has only reported a non-fatal error, still fail the pipeline from rialto-gstreamer
            GST_ERROR("Received Playback error '%s', posting error on %s sink", toString(error),
                      toString(sourceIt->second.getType()));
            rialto_mse_base_handle_rialto_server_error(sourceIt->second.m_rialtoSink, error);

            result = true;
        });

    return result;
}

firebolt::rialto::AddSegmentStatus GStreamerMSEMediaPlayerClient::addSegment(
    unsigned int needDataRequestId, const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &mediaSegment)
{
    // rialto client's addSegment call is MT safe, so it's ok to call it from the Puller's thread
    return m_clientBackend->addSegment(needDataRequestId, mediaSegment);
}

BufferPuller::BufferPuller(const std::shared_ptr<IMessageQueueFactory> &messageQueueFactory, GstElement *rialtoSink,
                           const std::shared_ptr<BufferParser> &bufferParser)
    : m_queue{messageQueueFactory->createMessageQueue()}, m_rialtoSink(rialtoSink), m_bufferParser(bufferParser)
{
}

void BufferPuller::start()
{
    m_queue->start();
}

void BufferPuller::stop()
{
    m_queue->stop();
}

bool BufferPuller::requestPullBuffer(int sourceId, size_t frameCount, unsigned int needDataRequestId,
                                     GStreamerMSEMediaPlayerClient *player)
{
    return m_queue->postMessage(std::make_shared<PullBufferMessage>(sourceId, frameCount, needDataRequestId,
                                                                    m_rialtoSink, m_bufferParser, *m_queue, player));
}

HaveDataMessage::HaveDataMessage(firebolt::rialto::MediaSourceStatus status, int sourceId,
                                 unsigned int needDataRequestId, GStreamerMSEMediaPlayerClient *player)
    : m_status(status), m_sourceId(sourceId), m_needDataRequestId(needDataRequestId), m_player(player)
{
}

void HaveDataMessage::handle()
{
    if (m_player->m_attachedSources.find(m_sourceId) == m_player->m_attachedSources.end())
    {
        GST_WARNING("Source id %d is invalid", m_sourceId);
        return;
    }

    m_player->m_clientBackend->haveData(m_status, m_needDataRequestId);
}

PullBufferMessage::PullBufferMessage(int sourceId, size_t frameCount, unsigned int needDataRequestId,
                                     GstElement *rialtoSink, const std::shared_ptr<BufferParser> &bufferParser,
                                     IMessageQueue &pullerQueue, GStreamerMSEMediaPlayerClient *player)
    : m_sourceId(sourceId), m_frameCount(frameCount), m_needDataRequestId(needDataRequestId), m_rialtoSink(rialtoSink),
      m_bufferParser(bufferParser), m_pullerQueue(pullerQueue), m_player(player)
{
}

void PullBufferMessage::handle()
{
    bool isEos = false;
    unsigned int addedSegments = 0;

    for (unsigned int frame = 0; frame < m_frameCount; ++frame)
    {
        GstRefSample sample = rialto_mse_base_sink_get_front_sample(RIALTO_MSE_BASE_SINK(m_rialtoSink));
        if (!sample)
        {
            if (rialto_mse_base_sink_is_eos(RIALTO_MSE_BASE_SINK(m_rialtoSink)))
            {
                isEos = true;
            }
            else
            {
                // it's not a critical issue. It might be caused by receiving too many need data requests.
                GST_INFO_OBJECT(m_rialtoSink, "Could not get a sample");
            }
            break;
        }

        // we pass GstMapInfo's pointers on data buffers to RialtoClient
        // so we need to hold it until RialtoClient copies them to shm
        GstBuffer *buffer = sample.getBuffer();
        GstMapInfo map;
        if (!gst_buffer_map(buffer, &map, GST_MAP_READ))
        {
            GST_ERROR_OBJECT(m_rialtoSink, "Could not map audio buffer");
            rialto_mse_base_sink_pop_sample(RIALTO_MSE_BASE_SINK(m_rialtoSink));
            continue;
        }

        std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> mseData =
            m_bufferParser->parseBuffer(sample, buffer, map, m_sourceId);
        if (!mseData)
        {
            GST_ERROR_OBJECT(m_rialtoSink, "No data returned from the parser");
            gst_buffer_unmap(buffer, &map);
            rialto_mse_base_sink_pop_sample(RIALTO_MSE_BASE_SINK(m_rialtoSink));
            continue;
        }

        firebolt::rialto::AddSegmentStatus addSegmentStatus = m_player->addSegment(m_needDataRequestId, mseData);
        if (addSegmentStatus == firebolt::rialto::AddSegmentStatus::NO_SPACE)
        {
            gst_buffer_unmap(buffer, &map);
            GST_INFO_OBJECT(m_rialtoSink, "There's no space to add sample");
            break;
        }

        gst_buffer_unmap(buffer, &map);
        rialto_mse_base_sink_pop_sample(RIALTO_MSE_BASE_SINK(m_rialtoSink));
        addedSegments++;
    }

    firebolt::rialto::MediaSourceStatus status = firebolt::rialto::MediaSourceStatus::OK;
    if (isEos)
    {
        status = firebolt::rialto::MediaSourceStatus::EOS;
    }
    else if (addedSegments == 0)
    {
        status = firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES;
    }

    m_player->m_backendQueue->postMessage(
        std::make_shared<HaveDataMessage>(status, m_sourceId, m_needDataRequestId, m_player));
}

NeedDataMessage::NeedDataMessage(int sourceId, size_t frameCount, unsigned int needDataRequestId,
                                 GStreamerMSEMediaPlayerClient *player)
    : m_sourceId(sourceId), m_frameCount(frameCount), m_needDataRequestId(needDataRequestId), m_player(player)
{
}

void NeedDataMessage::handle()
{
    if (!m_player->requestPullBuffer(m_sourceId, m_frameCount, m_needDataRequestId))
    {
        GST_ERROR("Failed to pull buffer for sourceId=%d and NeedDataRequestId %u", m_sourceId, m_needDataRequestId);
        m_player->m_backendQueue->postMessage(
            std::make_shared<HaveDataMessage>(firebolt::rialto::MediaSourceStatus::ERROR, m_sourceId,
                                              m_needDataRequestId, m_player));
    }
}

PlaybackStateMessage::PlaybackStateMessage(firebolt::rialto::PlaybackState state, GStreamerMSEMediaPlayerClient *player)
    : m_state(state), m_player(player)
{
}

void PlaybackStateMessage::handle()
{
    m_player->handlePlaybackStateChange(m_state);
}

QosMessage::QosMessage(int sourceId, firebolt::rialto::QosInfo qosInfo, GStreamerMSEMediaPlayerClient *player)
    : m_sourceId(sourceId), m_qosInfo(qosInfo), m_player(player)
{
}

void QosMessage::handle()
{
    if (!m_player->handleQos(m_sourceId, m_qosInfo))
    {
        GST_ERROR("Failed to handle qos for sourceId=%d", m_sourceId);
    }
}

BufferUnderflowMessage::BufferUnderflowMessage(int sourceId, GStreamerMSEMediaPlayerClient *player)
    : m_sourceId(sourceId), m_player(player)
{
}

void BufferUnderflowMessage::handle()
{
    if (!m_player->handleBufferUnderflow(m_sourceId))
    {
        GST_ERROR("Failed to handle buffer underflow for sourceId=%d", m_sourceId);
    }
}

PlaybackErrorMessage::PlaybackErrorMessage(int sourceId, firebolt::rialto::PlaybackError error,
                                           GStreamerMSEMediaPlayerClient *player)
    : m_sourceId(sourceId), m_error(error), m_player(player)
{
}

void PlaybackErrorMessage::handle()
{
    if (!m_player->handlePlaybackError(m_sourceId, m_error))
    {
        GST_ERROR("Failed to handle playback error for sourceId=%d, error %s", m_sourceId, toString(m_error));
    }
}

SetPositionMessage::SetPositionMessage(int64_t newPosition, std::unordered_map<int32_t, AttachedSource> &attachedSources)
    : m_newPosition(newPosition), m_attachedSources(attachedSources)
{
}

void SetPositionMessage::handle()
{
    for (auto &source : m_attachedSources)
    {
        source.second.setPosition(m_newPosition);
    }
}

SetDurationMessage::SetDurationMessage(int64_t newDuration, int64_t &targetDuration)
    : m_newDuration(newDuration), m_targetDuration(targetDuration)
{
}

void SetDurationMessage::handle()
{
    m_targetDuration = m_newDuration;
}

SourceFlushedMessage::SourceFlushedMessage(int32_t sourceId, GStreamerMSEMediaPlayerClient *player)
    : m_sourceId{sourceId}, m_player{player}
{
}

void SourceFlushedMessage::handle()
{
    m_player->handleSourceFlushed(m_sourceId);
}
