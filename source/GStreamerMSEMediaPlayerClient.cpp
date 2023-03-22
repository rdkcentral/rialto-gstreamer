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
#include "RialtoGStreamerMSEBaseSink.h"
#include "RialtoGStreamerMSEBaseSinkPrivate.h"
#include "RialtoGStreamerMSEVideoSink.h"
#include <chrono>
#include <thread>

namespace
{
// The start time of segment might differ from the first sample which is injected.
// That difference should not be bigger than 1 video / audio frame.
// 1 second is probably erring on the side of caution, but should not have side effect.
const int64_t segmentStartMaximumDiff = 1000000000;
const int32_t UNKNOWN_STREAMS_NUMBER = -1;
} // namespace

GStreamerMSEMediaPlayerClient::GStreamerMSEMediaPlayerClient(
    const std::shared_ptr<firebolt::rialto::client::MediaPlayerClientBackendInterface> &MediaPlayerClientBackend,
    const uint32_t maxVideoWidth, const uint32_t maxVideoHeight)
    : mClientBackend(MediaPlayerClientBackend), mPosition(0),
      mDuration(0), mAudioStreams{UNKNOWN_STREAMS_NUMBER}, mVideoStreams{UNKNOWN_STREAMS_NUMBER}, mVideoRectangle{0, 0,
                                                                                                                  1920,
                                                                                                                  1080},
      mStreamingStopped(false), mMaxWidth(maxVideoWidth == 0 ? DEFAULT_MAX_VIDEO_WIDTH : maxVideoWidth),
      mMaxHeight(maxVideoHeight == 0 ? DEFAULT_MAX_VIDEO_HEIGHT : maxVideoHeight)
{
    mBackendQueue.start();
}

GStreamerMSEMediaPlayerClient::~GStreamerMSEMediaPlayerClient()
{
    stopStreaming();
}

void GStreamerMSEMediaPlayerClient::stopStreaming()
{
    if (!mStreamingStopped)
    {
        mBackendQueue.stop();

        for (auto &source : mAttachedSources)
        {
            source.second.mBufferPuller->stop();
        }

        mStreamingStopped = true;
    }
}

// Deletes client backend -> this deletes mediapipeline object
void GStreamerMSEMediaPlayerClient::destroyClientBackend()
{
    mClientBackend.reset();
}

void GStreamerMSEMediaPlayerClient::notifyDuration(int64_t duration)
{
    mBackendQueue.postMessage(std::make_shared<SetDurationMessage>(duration, mDuration));
}

void GStreamerMSEMediaPlayerClient::notifyPosition(int64_t position)
{
    mBackendQueue.postMessage(std::make_shared<SetPositionMessage>(position, mPosition));
}

void GStreamerMSEMediaPlayerClient::notifyNativeSize(uint32_t width, uint32_t height, double aspect) {}

void GStreamerMSEMediaPlayerClient::notifyNetworkState(firebolt::rialto::NetworkState state) {}

void GStreamerMSEMediaPlayerClient::notifyPlaybackState(firebolt::rialto::PlaybackState state)
{
    mBackendQueue.postMessage(std::make_shared<PlaybackStateMessage>(state, this));
}

void GStreamerMSEMediaPlayerClient::notifyVideoData(bool hasData) {}

void GStreamerMSEMediaPlayerClient::notifyAudioData(bool hasData) {}

void GStreamerMSEMediaPlayerClient::notifyNeedMediaData(
    int32_t sourceId, size_t frameCount, uint32_t needDataRequestId,
    const std::shared_ptr<firebolt::rialto::MediaPlayerShmInfo> & /*shmInfo*/)
{
    mBackendQueue.postMessage(std::make_shared<NeedDataMessage>(sourceId, frameCount, needDataRequestId, this));

    return;
}

void GStreamerMSEMediaPlayerClient::notifyCancelNeedMediaData(int sourceId) {}

void GStreamerMSEMediaPlayerClient::notifyQos(int32_t sourceId, const firebolt::rialto::QosInfo &qosInfo)
{
    mBackendQueue.postMessage(std::make_shared<QosMessage>(sourceId, qosInfo, this));
}

void GStreamerMSEMediaPlayerClient::getPositionDo(int64_t *position)
{
    if (mClientBackend->getPosition(*position))
    {
        mPosition = *position;
    }
    else
    {
        *position = mPosition;
    }
}

int64_t GStreamerMSEMediaPlayerClient::getPosition()
{
    int64_t position;
    mBackendQueue.callInEventLoop(&GStreamerMSEMediaPlayerClient::getPositionDo, this, &position);
    return position;
}

void GStreamerMSEMediaPlayerClient::getDurationDo(int64_t *duration)
{
    *duration = mDuration;
}

int64_t GStreamerMSEMediaPlayerClient::getDuration()
{
    int64_t duration;
    mBackendQueue.callInEventLoop(&GStreamerMSEMediaPlayerClient::getDurationDo, this, &duration);
    return duration;
}

bool GStreamerMSEMediaPlayerClient::createBackend()
{
    bool result = false;
    mBackendQueue.callInEventLoop(
        [&]()
        {
            mClientBackend->createMediaPlayerBackend(shared_from_this(), mMaxWidth, mMaxHeight);

            if (mClientBackend->isMediaPlayerBackendCreated())
            {
                std::string utf8url = "mse://1";
                firebolt::rialto::MediaType mediaType = firebolt::rialto::MediaType::MSE;
                if (!mClientBackend->load(mediaType, "", utf8url))
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

void GStreamerMSEMediaPlayerClient::play()
{
    mBackendQueue.callInEventLoop([&]() { mClientBackend->play(); });
}

void GStreamerMSEMediaPlayerClient::pause()
{
    mBackendQueue.callInEventLoop([&]() { mClientBackend->pause(); });
}

void GStreamerMSEMediaPlayerClient::stop()
{
    mBackendQueue.callInEventLoop([&]() { mClientBackend->stop(); });
}

void GStreamerMSEMediaPlayerClient::notifySourceStartedSeeking(int32_t sourceId)
{
    mBackendQueue.callInEventLoop(
        [&]()
        {
            auto sourceIt = mAttachedSources.find(sourceId);
            if (sourceIt == mAttachedSources.end())
            {
                return;
            }

            sourceIt->second.mSeekingState = SeekingState::SEEKING;
            sourceIt->second.mBufferPuller->stop();

            startPullingDataIfSeekFinished();
        });
}

void GStreamerMSEMediaPlayerClient::seek(int64_t seekPosition)
{
    mBackendQueue.callInEventLoop(
        [&]()
        {
            mServerSeekingState = SeekingState::SEEKING;
            mClientBackend->seek(seekPosition);
            mPosition = seekPosition;
        });
}

void GStreamerMSEMediaPlayerClient::setPlaybackRate(double rate)
{
    mBackendQueue.callInEventLoop([&]() { mClientBackend->setPlaybackRate(rate); });
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
    mBackendQueue.callInEventLoop(
        [&]()
        {
            result = mClientBackend->attachSource(source);

            if (result)
            {
                std::shared_ptr<BufferPuller> bufferPuller;
                if (source->getType() == firebolt::rialto::MediaSourceType::AUDIO)
                {
                    std::shared_ptr<AudioBufferParser> audioBufferParser = std::make_shared<AudioBufferParser>();
                    bufferPuller = std::make_shared<BufferPuller>(GST_ELEMENT_CAST(rialtoSink), audioBufferParser);
                }
                else if (source->getType() == firebolt::rialto::MediaSourceType::VIDEO)
                {
                    std::shared_ptr<VideoBufferParser> videoBufferParser = std::make_shared<VideoBufferParser>();
                    bufferPuller = std::make_shared<BufferPuller>(GST_ELEMENT_CAST(rialtoSink), videoBufferParser);
                }

                if (mAttachedSources.find(source->getId()) == mAttachedSources.end())
                {
                    mAttachedSources.emplace(source->getId(),
                                             AttachedSource(rialtoSink, bufferPuller, source->getType()));
                    rialtoSink->priv->mSourceId = source->getId();
                    bufferPuller->start();
                }
            }

            if (!mWasAllSourcesAttachedSent && areAllStreamsAttached())
            {
                // RialtoServer doesn't support dynamic source attachment.
                // It means that when we notify that all sources were attached, we cannot add any more sources in the current session
                GST_INFO("All sources attached");
                mClientBackend->allSourcesAttached();
                mWasAllSourcesAttachedSent = true;
            }
        });

    return result;
}

void GStreamerMSEMediaPlayerClient::removeSource(int32_t sourceId)
{
    mBackendQueue.callInEventLoop(
        [&]()
        {
            if (!mClientBackend->removeSource(sourceId))
            {
                GST_WARNING("Remove source %d failed", sourceId);
            }
            mAttachedSources.erase(sourceId);
        });
}

void GStreamerMSEMediaPlayerClient::startPullingDataIfSeekFinished()
{
    mBackendQueue.callInEventLoop(
        [&]()
        {
            if (mServerSeekingState != SeekingState::SEEK_DONE)
            {
                return;
            }

            for (const auto &source : mAttachedSources)
            {
                if (source.second.mSeekingState != SeekingState::SEEKING)
                {
                    return;
                }
            }

            GST_INFO("Server and all attached sourced finished seek");

            mServerSeekingState = SeekingState::IDLE;
            for (auto &source : mAttachedSources)
            {
                source.second.mBufferPuller->start();
                source.second.mSeekingState = SeekingState::IDLE;
            }
        });
}

void GStreamerMSEMediaPlayerClient::handlePlaybackStateChange(firebolt::rialto::PlaybackState state)
{
    GST_DEBUG("Received state change to state %u", static_cast<uint32_t>(state));
    mBackendQueue.callInEventLoop(
        [&]()
        {
            switch (state)
            {
            case firebolt::rialto::PlaybackState::PAUSED:
            case firebolt::rialto::PlaybackState::PLAYING:
            {
                for (const auto &source : mAttachedSources)
                {
                    rialto_mse_base_handle_rialto_server_state_changed(source.second.mRialtoSink, state);
                }
                break;
            }
            case firebolt::rialto::PlaybackState::END_OF_STREAM:
            {
                for (const auto &source : mAttachedSources)
                {
                    rialto_mse_base_handle_rialto_server_eos(source.second.mRialtoSink);
                }
            }
            break;
            case firebolt::rialto::PlaybackState::FLUSHED:
            {
                if (mServerSeekingState == SeekingState::SEEKING)
                {
                    mServerSeekingState = SeekingState::SEEK_DONE;
                    startPullingDataIfSeekFinished();

                    for (auto &source : mAttachedSources)
                    {
                        rialto_mse_base_handle_rialto_server_completed_seek(source.second.mRialtoSink);
                    }
                }
                else
                {
                    GST_WARNING("Received unexpected FLUSHED state change");
                }
                break;
            }
            case firebolt::rialto::PlaybackState::FAILURE:
            {
                for (auto &source : mAttachedSources)
                {
                    if (mServerSeekingState == SeekingState::SEEKING)
                    {
                        rialto_mse_base_handle_rialto_server_completed_seek(source.second.mRialtoSink);
                    }
                    rialto_mse_base_handle_rialto_server_error(source.second.mRialtoSink);
                }
                mServerSeekingState = SeekingState::IDLE;
                mPosition = 0;

                break;
            }
            break;
            default:
                break;
            }
        });
}

void GStreamerMSEMediaPlayerClient::setVideoRectangle(const std::string &rectangleString)
{
    mBackendQueue.callInEventLoop(
        [&]()
        {
            if (!mClientBackend || !mClientBackend->isMediaPlayerBackendCreated())
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

            mClientBackend->setVideoWindow(rect.x, rect.y, rect.width, rect.height);
            mVideoRectangle = rect;
        });
}

std::string GStreamerMSEMediaPlayerClient::getVideoRectangle()
{
    char rectangle[64];
    mBackendQueue.callInEventLoop(
        [&]()
        {
            sprintf(rectangle, "%u,%u,%u,%u", mVideoRectangle.x, mVideoRectangle.y, mVideoRectangle.width,
                    mVideoRectangle.height);
        });

    return std::string(rectangle);
}

bool GStreamerMSEMediaPlayerClient::renderFrame(RialtoMSEBaseSink *sink)
{
    bool result = false;
    mBackendQueue.callInEventLoop(
        [&]()
        {
            result = mClientBackend->renderFrame();
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
    mBackendQueue.callInEventLoop([&]() { mClientBackend->setVolume(volume); });
}

double GStreamerMSEMediaPlayerClient::getVolume()
{
    double volume;
    mBackendQueue.callInEventLoop(
        [&]()
        {
            if (mClientBackend->getVolume(volume))
            {
                mVolume = volume;
            }
            else
            {
                volume = mVolume;
            }
        });
    return volume;
}

void GStreamerMSEMediaPlayerClient::setAudioStreamsInfo(int32_t audioStreams, bool isAudioOnly)
{
    mBackendQueue.callInEventLoop(
        [&]()
        {
            if (mAudioStreams == UNKNOWN_STREAMS_NUMBER)
            {
                mAudioStreams = audioStreams;
                GST_INFO("Set audio streams number to %d", mAudioStreams);
            }

            if (mVideoStreams == UNKNOWN_STREAMS_NUMBER && isAudioOnly)
            {
                mVideoStreams = 0;
                GST_INFO("Set audio only session");
            }
        });
}

void GStreamerMSEMediaPlayerClient::setVideoStreamsInfo(int32_t videoStreams, bool isVideoOnly)
{
    mBackendQueue.callInEventLoop(
        [&]()
        {
            if (mVideoStreams == UNKNOWN_STREAMS_NUMBER)
            {
                mVideoStreams = videoStreams;
                GST_INFO("Set video streams number to %d", mVideoStreams);
            }

            if (mAudioStreams == UNKNOWN_STREAMS_NUMBER && isVideoOnly)
            {
                mAudioStreams = 0;
                GST_INFO("Set video only session");
            }
        });
}

bool GStreamerMSEMediaPlayerClient::areAllStreamsAttached()
{
    int32_t attachedVideoSources = 0;
    int32_t attachedAudioSources = 0;
    for (auto &source : mAttachedSources)
    {
        if (source.second.getType() == firebolt::rialto::MediaSourceType::VIDEO)
        {
            attachedVideoSources++;
        }
        else if (source.second.getType() == firebolt::rialto::MediaSourceType::AUDIO)
        {
            attachedAudioSources++;
        }
    }

    return attachedVideoSources == mVideoStreams && attachedAudioSources == mAudioStreams;
}

bool GStreamerMSEMediaPlayerClient::requestPullBuffer(int streamId, size_t frameCount, unsigned int needDataRequestId)
{
    bool result = false;
    mBackendQueue.callInEventLoop(
        [&]()
        {
            auto sourceIt = mAttachedSources.find(streamId);
            if (sourceIt == mAttachedSources.end() || mServerSeekingState != SeekingState::IDLE)
            {
                GST_ERROR("There's no attached source with id %d or seek is not finished %u", streamId,
                          static_cast<uint32_t>(mServerSeekingState));

                result = false;
                return;
            }
            result = sourceIt->second.mBufferPuller->requestPullBuffer(streamId, frameCount, needDataRequestId, this);
        });

    return result;
}

bool GStreamerMSEMediaPlayerClient::handleQos(int sourceId, firebolt::rialto::QosInfo qosInfo)
{
    bool result = false;
    mBackendQueue.callInEventLoop(
        [&]()
        {
            auto sourceIt = mAttachedSources.find(sourceId);
            if (sourceIt == mAttachedSources.end())
            {
                result = false;
                return;
            }

            rialto_mse_base_handle_rialto_server_sent_qos(sourceIt->second.mRialtoSink, qosInfo.processed,
                                                          qosInfo.dropped);

            result = true;
        });

    return result;
}

firebolt::rialto::AddSegmentStatus GStreamerMSEMediaPlayerClient::addSegment(
    unsigned int needDataRequestId, const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &mediaSegment)
{
    // rialto client's addSegment call is MT safe, so it's ok to call it from the Puller's thread
    return mClientBackend->addSegment(needDataRequestId, mediaSegment);
}

BufferPuller::BufferPuller(GstElement *rialtoSink, const std::shared_ptr<BufferParser> &bufferParser)
    : mRialtoSink(rialtoSink), mBufferParser(bufferParser)
{
}

void BufferPuller::start()
{
    mQueue.start();
}

void BufferPuller::stop()
{
    mQueue.stop();
}

bool BufferPuller::requestPullBuffer(int sourceId, size_t frameCount, unsigned int needDataRequestId,
                                     GStreamerMSEMediaPlayerClient *player)
{
    return mQueue.postMessage(std::make_shared<PullBufferMessage>(sourceId, frameCount, needDataRequestId, mRialtoSink,
                                                                  mBufferParser, mQueue, player));
}

void BufferPuller::clearQueue()
{
    mQueue.clear();
}

HaveDataMessage::HaveDataMessage(firebolt::rialto::MediaSourceStatus status, int sourceId,
                                 unsigned int needDataRequestId, GStreamerMSEMediaPlayerClient *player)
    : mStatus(status), mSourceId(sourceId), mNeedDataRequestId(needDataRequestId), mPlayer(player)
{
}

void HaveDataMessage::handle()
{
    if (mPlayer->mAttachedSources.find(mSourceId) == mPlayer->mAttachedSources.end())
    {
        GST_WARNING("Source id %d is invalid", mSourceId);
        return;
    }

    mPlayer->mClientBackend->haveData(mStatus, mNeedDataRequestId);
}

PullBufferMessage::PullBufferMessage(int sourceId, size_t frameCount, unsigned int needDataRequestId,
                                     GstElement *rialtoSink, const std::shared_ptr<BufferParser> &bufferParser,
                                     MessageQueue &pullerQueue, GStreamerMSEMediaPlayerClient *player)
    : mSourceId(sourceId), mFrameCount(frameCount), mNeedDataRequestId(needDataRequestId), mRialtoSink(rialtoSink),
      mBufferParser(bufferParser), mPullerQueue(pullerQueue), mPlayer(player)
{
}

void PullBufferMessage::handle()
{
    bool isEos = false;
    unsigned int addedSegments = 0;

    for (unsigned int frame = 0; frame < mFrameCount; ++frame)
    {
        GstSample *sample = rialto_mse_base_sink_get_front_sample(RIALTO_MSE_BASE_SINK(mRialtoSink));
        if (!sample)
        {
            if (rialto_mse_base_sink_is_eos(RIALTO_MSE_BASE_SINK(mRialtoSink)))
            {
                isEos = true;
            }
            else
            {
                // it's not a critical issue. It might be caused by receiving too many need data requests.
                GST_INFO_OBJECT(mRialtoSink, "Could not get a sample");
            }
            break;
        }

        // we pass GstMapInfo's pointers on data buffers to RialtoClient
        // so we need to hold it until RialtoClient copies them to shm
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GstMapInfo map;
        if (!gst_buffer_map(buffer, &map, GST_MAP_READ))
        {
            GST_ERROR_OBJECT(mRialtoSink, "Could not map audio buffer");
            rialto_mse_base_sink_pop_sample(RIALTO_MSE_BASE_SINK(mRialtoSink));
            continue;
        }

        std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> mseData =
            mBufferParser->parseBuffer(sample, buffer, map, mSourceId);
        if (!mseData)
        {
            GST_ERROR_OBJECT(mRialtoSink, "No data returned from the parser");
            gst_buffer_unmap(buffer, &map);
            rialto_mse_base_sink_pop_sample(RIALTO_MSE_BASE_SINK(mRialtoSink));
            continue;
        }

        firebolt::rialto::AddSegmentStatus addSegmentStatus = mPlayer->addSegment(mNeedDataRequestId, mseData);
        if (addSegmentStatus == firebolt::rialto::AddSegmentStatus::NO_SPACE)
        {
            gst_buffer_unmap(buffer, &map);
            GST_INFO_OBJECT(mRialtoSink, "There's no space to add sample");
            break;
        }

        gst_buffer_unmap(buffer, &map);
        rialto_mse_base_sink_pop_sample(RIALTO_MSE_BASE_SINK(mRialtoSink));
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

    mPlayer->mBackendQueue.postMessage(std::make_shared<HaveDataMessage>(status, mSourceId, mNeedDataRequestId, mPlayer));
}

NeedDataMessage::NeedDataMessage(int sourceId, size_t frameCount, unsigned int needDataRequestId,
                                 GStreamerMSEMediaPlayerClient *player)
    : mSourceId(sourceId), mFrameCount(frameCount), mNeedDataRequestId(needDataRequestId), mPlayer(player)
{
}

void NeedDataMessage::handle()
{
    if (!mPlayer->requestPullBuffer(mSourceId, mFrameCount, mNeedDataRequestId))
    {
        GST_ERROR("Failed to pull buffer for sourceId=%d and NeedDataRequestId %u", mSourceId, mNeedDataRequestId);
        mPlayer->mBackendQueue.postMessage(std::make_shared<HaveDataMessage>(firebolt::rialto::MediaSourceStatus::ERROR,
                                                                             mSourceId, mNeedDataRequestId, mPlayer));
    }
}

PlaybackStateMessage::PlaybackStateMessage(firebolt::rialto::PlaybackState state, GStreamerMSEMediaPlayerClient *player)
    : mState(state), mPlayer(player)
{
}

void PlaybackStateMessage::handle()
{
    mPlayer->handlePlaybackStateChange(mState);
}

QosMessage::QosMessage(int sourceId, firebolt::rialto::QosInfo qosInfo, GStreamerMSEMediaPlayerClient *player)
    : mSourceId(sourceId), mQosInfo(qosInfo), mPlayer(player)
{
}

void QosMessage::handle()
{
    if (!mPlayer->handleQos(mSourceId, mQosInfo))
    {
        GST_ERROR("Failed to handle qos for sourceId=%d", mSourceId);
    }
}
