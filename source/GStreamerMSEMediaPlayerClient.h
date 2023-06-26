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

#include "MediaPlayerClientBackendInterface.h"
#include "MessageQueue.h"
#include <IMediaPipeline.h>
#include <MediaCommon.h>
#include <condition_variable>
#include <gst/gst.h>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include <functional>
#include <memory>
#include <sys/syscall.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

#include "BufferParser.h"
#include "RialtoGStreamerMSEBaseSink.h"
#include "RialtoGStreamerMSEBaseSinkCallbacks.h"
#include <atomic>
#include <unordered_set>

#define DEFAULT_MAX_VIDEO_WIDTH 3840
#define DEFAULT_MAX_VIDEO_HEIGHT 2160

class GStreamerMSEMediaPlayerClient;

class BufferPuller
{
public:
    BufferPuller(GstElement *rialtoSink, const std::shared_ptr<BufferParser> &bufferParser);

    void start();
    void stop();
    bool requestPullBuffer(int sourceId, size_t frameCount, unsigned int needDataRequestId,
                           GStreamerMSEMediaPlayerClient *player);
    void clearQueue();

private:
    MessageQueue m_queue;
    GstElement *m_rialtoSink;
    std::shared_ptr<BufferParser> m_bufferParser;
};

class HaveDataMessage : public Message
{
public:
    HaveDataMessage(firebolt::rialto::MediaSourceStatus status, int sourceId, unsigned int needDataRequestId,
                    GStreamerMSEMediaPlayerClient *player);
    void handle();

private:
    firebolt::rialto::MediaSourceStatus m_status;
    int m_sourceId;
    unsigned int m_needDataRequestId;
    GStreamerMSEMediaPlayerClient *m_player;
};

class PullBufferMessage : public Message
{
public:
    PullBufferMessage(int sourceId, size_t frameCount, unsigned int needDataRequestId, GstElement *rialtoSink,
                      const std::shared_ptr<BufferParser> &bufferParser, MessageQueue &pullerQueue,
                      GStreamerMSEMediaPlayerClient *player);
    void handle();

private:
    int m_sourceId;
    size_t m_frameCount;
    unsigned int m_needDataRequestId;
    GstElement *m_rialtoSink;
    std::shared_ptr<BufferParser> m_bufferParser;
    MessageQueue &m_pullerQueue;
    GStreamerMSEMediaPlayerClient *m_player;
};

class NeedDataMessage : public Message
{
public:
    NeedDataMessage(int sourceId, size_t frameCount, unsigned int needDataRequestId,
                    GStreamerMSEMediaPlayerClient *player);
    void handle();

private:
    int m_sourceId;
    size_t m_frameCount;
    unsigned int m_needDataRequestId;
    GStreamerMSEMediaPlayerClient *m_player;
};

class PlaybackStateMessage : public Message
{
public:
    PlaybackStateMessage(firebolt::rialto::PlaybackState state, GStreamerMSEMediaPlayerClient *player);
    void handle();

private:
    firebolt::rialto::PlaybackState m_state;
    GStreamerMSEMediaPlayerClient *m_player;
};

class QosMessage : public Message
{
public:
    QosMessage(int sourceId, firebolt::rialto::QosInfo qosInfo, GStreamerMSEMediaPlayerClient *player);
    void handle();

private:
    int m_sourceId;
    firebolt::rialto::QosInfo m_qosInfo;
    GStreamerMSEMediaPlayerClient *m_player;
};

class BufferUnderflowMessage : public Message
{
public:
    BufferUnderflowMessage(int sourceId, GStreamerMSEMediaPlayerClient *player);
    void handle();

private:
    int m_sourceId;
    GStreamerMSEMediaPlayerClient *m_player;
};

enum class SeekingState
{
    IDLE,
    SEEKING,
    SEEK_DONE
};

class AttachedSource
{
    friend class GStreamerMSEMediaPlayerClient;

public:
    AttachedSource(RialtoMSEBaseSink *rialtoSink, std::shared_ptr<BufferPuller> puller,
                   firebolt::rialto::MediaSourceType type)
        : m_rialtoSink(rialtoSink), m_bufferPuller(puller), m_type(type)
    {
    }

    firebolt::rialto::MediaSourceType getType() { return m_type; }

private:
    RialtoMSEBaseSink *m_rialtoSink;
    std::shared_ptr<BufferPuller> m_bufferPuller;
    SeekingState m_seekingState = SeekingState::IDLE;
    std::unordered_set<uint32_t> m_ongoingNeedDataRequests;
    firebolt::rialto::MediaSourceType m_type = firebolt::rialto::MediaSourceType::UNKNOWN;
};

class GStreamerMSEMediaPlayerClient : public firebolt::rialto::IMediaPipelineClient,
                                      public std::enable_shared_from_this<GStreamerMSEMediaPlayerClient>
{
    friend class NeedDataMessage;
    friend class PullBufferMessage;
    friend class HaveDataMessage;
    friend class QosMessage;

public:
    GStreamerMSEMediaPlayerClient(
        const std::shared_ptr<firebolt::rialto::client::MediaPlayerClientBackendInterface> &MediaPlayerClientBackend,
        const uint32_t maxVideoWidth, const uint32_t maxVideoHeight);
    virtual ~GStreamerMSEMediaPlayerClient();

    void notifyDuration(int64_t duration) override;
    void notifyPosition(int64_t position) override;
    void notifyNativeSize(uint32_t width, uint32_t height, double aspect) override;
    void notifyNetworkState(firebolt::rialto::NetworkState state) override;
    void notifyPlaybackState(firebolt::rialto::PlaybackState state) override;
    void notifyVideoData(bool hasData) override;
    void notifyAudioData(bool hasData) override;
    void notifyNeedMediaData(int32_t sourceId, size_t frameCount, uint32_t needDataRequestId,
                             const std::shared_ptr<firebolt::rialto::MediaPlayerShmInfo> &shmInfo) override;
    void notifyCancelNeedMediaData(int32_t sourceId) override;
    void notifyQos(int32_t sourceId, const firebolt::rialto::QosInfo &qosInfo) override;
    void notifyBufferUnderflow(int32_t sourceId) override;

    void getPositionDo(int64_t *position);
    int64_t getPosition();
    void getDurationDo(int64_t *duration);
    int64_t getDuration();
    firebolt::rialto::AddSegmentStatus
    addSegment(unsigned int needDataRequestId,
               const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &mediaSegment);

    bool createBackend();
    void play();
    void pause();
    void stop();
    void seek(int64_t seekPosition);
    void setPlaybackRate(double rate);

    bool attachSource(std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> &source,
                      RialtoMSEBaseSink *rialtoSink);
    void removeSource(int32_t sourceId);
    void handlePlaybackStateChange(firebolt::rialto::PlaybackState state);

    void setVideoRectangle(const std::string &rectangleString);
    std::string getVideoRectangle();

    bool requestPullBuffer(int streamId, size_t frameCount, unsigned int needDataRequestId);
    bool handleQos(int sourceId, firebolt::rialto::QosInfo qosInfo);
    bool handleBufferUnderflow(int sourceId);
    void notifySourceStartedSeeking(int32_t sourceId);
    void startPullingDataIfSeekFinished();
    void stopStreaming();
    void destroyClientBackend();
    bool renderFrame(RialtoMSEBaseSink *sink);
    void setVolume(double volume);
    double getVolume();
    void setMute(bool mute);
    bool getMute();
    void setAudioStreamsInfo(int32_t audioStreams, bool isAudioOnly);
    void setVideoStreamsInfo(int32_t videoStreams, bool isVideoOnly);

private:
    bool areAllStreamsAttached();

    MessageQueue m_backendQueue;
    std::shared_ptr<firebolt::rialto::client::MediaPlayerClientBackendInterface> m_clientBackend;
    int64_t m_position;
    int64_t m_duration;
    double m_volume = 1.0;
    bool m_mute = false;
    std::mutex m_playerMutex;
    std::unordered_map<int32_t, AttachedSource> m_attachedSources;
    bool m_wasAllSourcesAttachedSent = false;
    int32_t m_audioStreams;
    int32_t m_videoStreams;
    SeekingState m_serverSeekingState = SeekingState::IDLE;

    struct Rectangle
    {
        unsigned int x, y, width, height;
    } m_videoRectangle;

    // To check if the backend message queue and pulling of data to serve backend is stopped or not
    bool m_streamingStopped;

    const uint32_t m_maxWidth;
    const uint32_t m_maxHeight;
};
