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
    MessageQueue mQueue;
    GstElement *mRialtoSink;
    std::shared_ptr<BufferParser> mBufferParser;
};

class HaveDataMessage : public Message
{
public:
    HaveDataMessage(firebolt::rialto::MediaSourceStatus status, int sourceId, unsigned int needDataRequestId,
                    GStreamerMSEMediaPlayerClient *player);
    void handle();

private:
    firebolt::rialto::MediaSourceStatus mStatus;
    int mSourceId;
    unsigned int mNeedDataRequestId;
    GStreamerMSEMediaPlayerClient *mPlayer;
};

class PullBufferMessage : public Message
{
public:
    PullBufferMessage(int sourceId, size_t frameCount, unsigned int needDataRequestId, GstElement *rialtoSink,
                      const std::shared_ptr<BufferParser> &bufferParser, MessageQueue &pullerQueue,
                      GStreamerMSEMediaPlayerClient *player);
    void handle();

private:
    int mSourceId;
    size_t mFrameCount;
    unsigned int mNeedDataRequestId;
    GstElement *mRialtoSink;
    std::shared_ptr<BufferParser> mBufferParser;
    MessageQueue &mPullerQueue;
    GStreamerMSEMediaPlayerClient *mPlayer;
};

class NeedDataMessage : public Message
{
public:
    NeedDataMessage(int sourceId, size_t frameCount, unsigned int needDataRequestId,
                    GStreamerMSEMediaPlayerClient *player);
    void handle();

private:
    int mSourceId;
    size_t mFrameCount;
    unsigned int mNeedDataRequestId;
    GStreamerMSEMediaPlayerClient *mPlayer;
};

class PlaybackStateMessage : public Message
{
public:
    PlaybackStateMessage(firebolt::rialto::PlaybackState state, GStreamerMSEMediaPlayerClient *player);
    void handle();

private:
    firebolt::rialto::PlaybackState mState;
    GStreamerMSEMediaPlayerClient *mPlayer;
};

class QosMessage : public Message
{
public:
    QosMessage(int sourceId, firebolt::rialto::QosInfo qosInfo, GStreamerMSEMediaPlayerClient *player);
    void handle();

private:
    int mSourceId;
    firebolt::rialto::QosInfo mQosInfo;
    GStreamerMSEMediaPlayerClient *mPlayer;
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
    AttachedSource(RialtoMSEBaseSink *rialtoSink, std::shared_ptr<BufferPuller> puller)
        : mRialtoSink(rialtoSink), mBufferPuller(puller)
    {
    }

private:
    RialtoMSEBaseSink *mRialtoSink;
    std::shared_ptr<BufferPuller> mBufferPuller;
    SeekingState mSeekingState = SeekingState::IDLE;
    std::unordered_set<uint32_t> mOngoingNeedDataRequests;
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
                             const std::shared_ptr<firebolt::rialto::ShmInfo> &shmInfo) override;
    void notifyCancelNeedMediaData(int32_t sourceId) override;
    void notifyQos(int32_t sourceId, const firebolt::rialto::QosInfo &qosInfo) override;

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

    bool attachSource(firebolt::rialto::IMediaPipeline::MediaSource &source, RialtoMSEBaseSink *rialtoSink);
    void removeSource(int32_t sourceId);
    void handlePlaybackStateChange(firebolt::rialto::PlaybackState state);

    void setVideoRectangle(const std::string &rectangleString);
    std::string getVideoRectangle();

    bool isConnectedToServer();

    bool requestPullBuffer(int streamId, size_t frameCount, unsigned int needDataRequestId);
    bool handleQos(int sourceId, firebolt::rialto::QosInfo qosInfo);
    void notifySourceStartedSeeking(int32_t sourceId);
    void startPullingDataIfSeekFinished();
    void stopStreaming();
    void destroyClientBackend();

private:
    MessageQueue mBackendQueue;
    std::shared_ptr<firebolt::rialto::client::MediaPlayerClientBackendInterface> mClientBackend;
    int64_t mPosition;
    int64_t mDuration;
    std::atomic<bool> mIsConnected;
    std::mutex mPlayerMutex;
    std::unordered_map<int32_t, AttachedSource> mAttachedSources;
    SeekingState mServerSeekingState = SeekingState::IDLE;

    struct Rectangle
    {
        unsigned int x, y, width, height;
    } mVideoRectangle;

    // To check if the backend message queue and pulling of data to serve backend is stopped or not
    bool mStreamingStopped;

    const uint32_t mMaxWidth;
    const uint32_t mMaxHeight;
};
