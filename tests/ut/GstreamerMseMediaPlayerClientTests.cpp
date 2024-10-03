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

#include "GStreamerMSEMediaPlayerClient.h"
#include "MediaPlayerClientBackendMock.h"
#include "MediaSourceMock.h"
#include "MessageQueueMock.h"
#include "RialtoGStreamerMSEBaseSinkPrivate.h"
#include "RialtoGstTest.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using firebolt::rialto::MediaSourceMock;
using firebolt::rialto::client::MediaPlayerClientBackendMock;
using testing::_;
using testing::ByMove;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::SetArgReferee;
using testing::StrictMock;

namespace
{
constexpr uint32_t kMaxVideoWidth{1024};
constexpr uint32_t kMaxVideoHeight{768};
constexpr int64_t kPosition{123};
constexpr int32_t kUnknownSourceId{-1};
constexpr size_t kFrameCount{1};
constexpr uint32_t kNeedDataRequestId{2};
const std::shared_ptr<firebolt::rialto::MediaPlayerShmInfo> kShmInfo{nullptr};
const std::string kUrl{"mse://1"};
constexpr firebolt::rialto::MediaType kMediaType{firebolt::rialto::MediaType::MSE};
const std::string kMimeType{""};
constexpr double kVolume{1.0};
constexpr uint32_t kVolumeDuration{30};
constexpr firebolt::rialto::EaseType kEaseType{firebolt::rialto::EaseType::EASE_LINEAR};
constexpr bool kMute{true};
constexpr bool kResetTime{true};
constexpr double kAppliedRate{2.0};
constexpr uint32_t kDuration{30};
constexpr int64_t kDiscontinuityGap{1};
constexpr bool kAudioAac{false};
const std::string kTextTrackIdentifier{"TextTrackId"};
constexpr bool kImmediateOutput{true};
constexpr bool kLowLatency{true};
constexpr bool kSync{true};
constexpr bool kSyncOff{true};
constexpr int32_t kStreamSyncMode{1};
constexpr uint32_t kBufferingLimit{12384};
constexpr bool kUseBuffering{true};
constexpr uint64_t kRunningTime{234};

MATCHER_P(PtrMatcher, ptr, "")
{
    return ptr == arg.get();
}
class UnderflowSignalMock
{
public:
    static UnderflowSignalMock &instance()
    {
        static UnderflowSignalMock instance;
        return instance;
    }
    MOCK_METHOD(void, callbackCalled, (), (const));
};
void underflowSignalCallback(GstElement *, gpointer, guint, gpointer)
{
    UnderflowSignalMock::instance().callbackCalled();
}
class PlaybackErrorMock
{
public:
    static PlaybackErrorMock &instance()
    {
        static PlaybackErrorMock instance;
        return instance;
    }
    MOCK_METHOD(void, callbackCalled, (), (const));
};
void playbackErrorCallback(RialtoMSEBaseSink *sink, firebolt::rialto::PlaybackError error)
{
    PlaybackErrorMock::instance().callbackCalled();
}

} // namespace

class GstreamerMseMediaPlayerClientTests : public RialtoGstTest
{
public:
    GstreamerMseMediaPlayerClientTests()
    {
        EXPECT_CALL(*m_messageQueueFactoryMock, createMessageQueue()).WillOnce(Return(ByMove(std::move(m_messageQueue))));
        EXPECT_CALL(m_messageQueueMock, start());
        EXPECT_CALL(m_messageQueueMock, stop());
        m_sut = std::make_shared<GStreamerMSEMediaPlayerClient>(m_messageQueueFactoryMock, m_mediaPlayerClientBackendMock,
                                                                kMaxVideoWidth, kMaxVideoHeight);
    }

    ~GstreamerMseMediaPlayerClientTests() override = default;

    void expectPostMessage()
    {
        EXPECT_CALL(m_messageQueueMock, postMessage(_))
            .WillRepeatedly(Invoke(
                [](const auto &msg)
                {
                    msg->handle();
                    return true;
                }));
    }

    void expectCallInEventLoop()
    {
        EXPECT_CALL(m_messageQueueMock, callInEventLoop(_))
            .WillRepeatedly(Invoke(
                [](const auto &f)
                {
                    f();
                    return true;
                }));
    }

    int32_t attachSource(RialtoMSEBaseSink *sink, const firebolt::rialto::MediaSourceType &type)
    {
        static int32_t id{0};
        std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> mediaSource{
            std::make_unique<StrictMock<MediaSourceMock>>()};
        mediaSource->setId(id);
        StrictMock<MediaSourceMock> &mediaSourceMock{static_cast<StrictMock<MediaSourceMock> &>(*mediaSource)};
        EXPECT_CALL(mediaSourceMock, getType()).WillRepeatedly(Return(type));
        expectCallInEventLoop();
        EXPECT_CALL(*m_mediaPlayerClientBackendMock, attachSource(PtrMatcher(mediaSource.get()))).WillOnce(Return(true));
        EXPECT_TRUE(m_sut->attachSource(mediaSource, sink));
        return id++;
    }

    void attachAudioVideo()
    {
        constexpr int32_t kVideoStreams{1};
        constexpr int32_t kAudioStreams{1};
        constexpr int32_t kTextStreams{0};
        expectCallInEventLoop();
        m_sut->handleStreamCollection(kAudioStreams, kVideoStreams, kTextStreams);

        EXPECT_CALL(*m_mediaPlayerClientBackendMock, allSourcesAttached()).WillOnce(Return(true));

        m_audioSink = createAudioSink();
        bufferPullerWillBeCreated();
        m_audioSourceId = attachSource(m_audioSink, firebolt::rialto::MediaSourceType::AUDIO);

        m_videoSink = createVideoSink();
        bufferPullerWillBeCreated();
        m_videoSourceId = attachSource(m_videoSink, firebolt::rialto::MediaSourceType::VIDEO);
        EXPECT_EQ(m_sut->getClientState(), ClientState::READY);
    }

    void allSourcesWantToPause()
    {
        expectCallInEventLoop();
        EXPECT_CALL(*m_mediaPlayerClientBackendMock, pause()).WillOnce(Return(true));
        m_sut->pause(m_audioSourceId);
        m_sut->pause(m_videoSourceId);
        EXPECT_EQ(m_sut->getClientState(), ClientState::AWAITING_PAUSED);
    }

    void allSourcesWantToPlay()
    {
        EXPECT_CALL(*m_mediaPlayerClientBackendMock, play()).WillOnce(Return(true));
        m_sut->play(m_audioSourceId);
        m_sut->play(m_videoSourceId);
        EXPECT_EQ(m_sut->getClientState(), ClientState::AWAITING_PLAYING);
    }

    void serverTransitionedToPaused()
    {
        expectPostMessage();
        m_sut->notifyPlaybackState(firebolt::rialto::PlaybackState::PAUSED);
        EXPECT_EQ(m_sut->getClientState(), ClientState::PAUSED);
    }

    void serverTransitionedToPlaying()
    {
        expectPostMessage();
        m_sut->notifyPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
        EXPECT_EQ(m_sut->getClientState(), ClientState::PLAYING);
    }

    StrictMock<MessageQueueMock> &bufferPullerWillBeCreated()
    {
        std::unique_ptr<StrictMock<MessageQueueMock>> bufferPullerMessageQueue{
            std::make_unique<StrictMock<MessageQueueMock>>()};
        StrictMock<MessageQueueMock> &result{*bufferPullerMessageQueue};
        EXPECT_CALL(*bufferPullerMessageQueue, start());
        EXPECT_CALL(*bufferPullerMessageQueue, stop());
        EXPECT_CALL(*m_messageQueueFactoryMock, createMessageQueue())
            .WillOnce(Return(ByMove(std::move(bufferPullerMessageQueue))));
        return result;
    }

    std::shared_ptr<StrictMock<MediaPlayerClientBackendMock>> m_mediaPlayerClientBackendMock{
        std::make_shared<StrictMock<MediaPlayerClientBackendMock>>()};
    std::shared_ptr<StrictMock<MessageQueueFactoryMock>> m_messageQueueFactoryMock{
        std::make_shared<StrictMock<MessageQueueFactoryMock>>()};
    std::unique_ptr<StrictMock<MessageQueueMock>> m_messageQueue{std::make_unique<StrictMock<MessageQueueMock>>()};
    StrictMock<MessageQueueMock> &m_messageQueueMock{*m_messageQueue};
    std::shared_ptr<GStreamerMSEMediaPlayerClient> m_sut;

protected:
    int32_t m_audioSourceId{-1};
    int32_t m_videoSourceId{-1};
    RialtoMSEBaseSink *m_audioSink{nullptr};
    RialtoMSEBaseSink *m_videoSink{nullptr};
};

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldDestroyBackend)
{
    expectCallInEventLoop();
    m_sut->destroyClientBackend();
    EXPECT_FALSE(m_sut->createBackend()); // Operation should fail when client backend is null
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotifyDuration)
{
    EXPECT_CALL(m_messageQueueMock, postMessage(_))
        .WillRepeatedly(Invoke(
            [](const auto &msg)
            {
                msg->handle();
                return true;
            }));
    constexpr int64_t kDuration{1234};
    m_sut->notifyDuration(kDuration);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotifyPosition)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    bufferPullerWillBeCreated();
    const int32_t kSourceId{attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO)};
    expectPostMessage();
    expectCallInEventLoop();
    m_sut->notifyPosition(kPosition);
    m_sut->destroyClientBackend();
    EXPECT_EQ(m_sut->getPosition(kSourceId), kPosition);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotifyNativeSize)
{
    constexpr double kAspect{0.0};
    m_sut->notifyNativeSize(kMaxVideoWidth, kMaxVideoHeight, kAspect);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotifyNetworkState)
{
    m_sut->notifyNetworkState(firebolt::rialto::NetworkState::STALLED);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotifyPlaybackStateStopped)
{
    expectPostMessage();
    expectCallInEventLoop();
    m_sut->notifyPlaybackState(firebolt::rialto::PlaybackState::STOPPED);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldFailToNotifyPlaybackStatePausedWhenNextStateIsWrong)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    bufferPullerWillBeCreated();
    attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO);
    expectPostMessage();
    m_sut->notifyPlaybackState(firebolt::rialto::PlaybackState::PAUSED);

    const auto kReceivedMessages{getMessages(pipeline)};
    EXPECT_TRUE(kReceivedMessages.empty());

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldFailToNotifyPlaybackStatePlayingWhenNextStateIsWrong)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    bufferPullerWillBeCreated();
    attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO);
    expectPostMessage();
    m_sut->notifyPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

    const auto kReceivedMessages{getMessages(pipeline)};
    EXPECT_TRUE(kReceivedMessages.empty());

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotNotifyPlaybackStateEndOfStreamWhenStateIsWrong)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);
    bufferPullerWillBeCreated();
    attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO);
    expectPostMessage();
    m_sut->notifyPlaybackState(firebolt::rialto::PlaybackState::END_OF_STREAM);

    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ERROR));

    gst_object_unref(pipeline);
}
// EoS OK case tested in sink tests.

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldReceiveUnexpectedSeekDoneMessage)
{
    expectPostMessage();
    expectCallInEventLoop();
    m_sut->notifyPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldReceiveFailureMessage)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    bufferPullerWillBeCreated();
    const int32_t kSourceId{attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO)};
    expectPostMessage();
    expectCallInEventLoop();
    m_sut->notifyPlaybackState(firebolt::rialto::PlaybackState::FAILURE);
    // Position should be set to 0
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, getPosition(_)).WillOnce(Return(false));
    EXPECT_EQ(m_sut->getPosition(kSourceId), 0);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotifyVideoData)
{
    constexpr bool kHasData{true};
    m_sut->notifyVideoData(kHasData);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotifyAudioData)
{
    constexpr bool kHasData{true};
    m_sut->notifyAudioData(kHasData);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldFailToNotifyNeedMediaDataWhenSourceIsIsNotKnown)
{
    expectCallInEventLoop();
    expectPostMessage();
    m_sut->notifyNeedMediaData(kUnknownSourceId, kFrameCount, kNeedDataRequestId, kShmInfo);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldFailToNotifyNeedMediaDataWhenBufferPullerFails)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    auto &bufferPullerMsgQueueMock{bufferPullerWillBeCreated()};
    const int32_t kSourceId{attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO)};

    expectCallInEventLoop();
    expectPostMessage();
    EXPECT_CALL(bufferPullerMsgQueueMock, postMessage(_)).WillOnce(Return(false));
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, haveData(firebolt::rialto::MediaSourceStatus::ERROR, kNeedDataRequestId))
        .WillOnce(Return(true));
    m_sut->notifyNeedMediaData(kSourceId, kFrameCount, kNeedDataRequestId, kShmInfo);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotifyNeedMediaDataWithNoSamplesAvailable)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    auto &bufferPullerMsgQueueMock{bufferPullerWillBeCreated()};
    const int32_t kSourceId{attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO)};

    expectCallInEventLoop();
    expectPostMessage();
    EXPECT_CALL(bufferPullerMsgQueueMock, postMessage(_))
        .WillOnce(Invoke(
            [](const auto &msg)
            {
                msg->handle();
                return true;
            }));
    EXPECT_CALL(*m_mediaPlayerClientBackendMock,
                haveData(firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES, kNeedDataRequestId))
        .WillOnce(Return(true));
    m_sut->notifyNeedMediaData(kSourceId, kFrameCount, kNeedDataRequestId, kShmInfo);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotifyNeedMediaDataWithEos)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    audioSink->priv->m_isEos = true;
    auto &bufferPullerMsgQueueMock{bufferPullerWillBeCreated()};
    const int32_t kSourceId{attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO)};

    expectCallInEventLoop();
    expectPostMessage();
    EXPECT_CALL(bufferPullerMsgQueueMock, postMessage(_))
        .WillOnce(Invoke(
            [](const auto &msg)
            {
                msg->handle();
                return true;
            }));
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, haveData(firebolt::rialto::MediaSourceStatus::EOS, kNeedDataRequestId))
        .WillOnce(Return(true));
    m_sut->notifyNeedMediaData(kSourceId, kFrameCount, kNeedDataRequestId, kShmInfo);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotifyNeedMediaDataWithEmptySample)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    audioSink->priv->m_samples.push(gst_sample_new(nullptr, nullptr, nullptr, nullptr));
    auto &bufferPullerMsgQueueMock{bufferPullerWillBeCreated()};
    const int32_t kSourceId{attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO)};

    expectCallInEventLoop();
    expectPostMessage();
    EXPECT_CALL(bufferPullerMsgQueueMock, postMessage(_))
        .WillOnce(Invoke(
            [](const auto &msg)
            {
                msg->handle();
                return true;
            }));
    EXPECT_CALL(*m_mediaPlayerClientBackendMock,
                haveData(firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES, kNeedDataRequestId))
        .WillOnce(Return(true));
    m_sut->notifyNeedMediaData(kSourceId, kFrameCount, kNeedDataRequestId, kShmInfo);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotifyNeedMediaDataWithNoSpace)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstCaps *caps{gst_caps_new_simple("application/x-cenc", "rate", G_TYPE_INT, 1, "channels", G_TYPE_INT, 2, nullptr)};
    GstBuffer *buffer{gst_buffer_new()};
    audioSink->priv->m_samples.push(gst_sample_new(buffer, caps, nullptr, nullptr));
    auto &bufferPullerMsgQueueMock{bufferPullerWillBeCreated()};
    const int32_t kSourceId{attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO)};

    expectCallInEventLoop();
    expectPostMessage();
    EXPECT_CALL(bufferPullerMsgQueueMock, postMessage(_))
        .WillOnce(Invoke(
            [](const auto &msg)
            {
                msg->handle();
                return true;
            }));
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, addSegment(kNeedDataRequestId, _))
        .WillOnce(Return(firebolt::rialto::AddSegmentStatus::NO_SPACE));
    EXPECT_CALL(*m_mediaPlayerClientBackendMock,
                haveData(firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES, kNeedDataRequestId))
        .WillOnce(Return(true));
    m_sut->notifyNeedMediaData(kSourceId, kFrameCount, kNeedDataRequestId, kShmInfo);

    gst_caps_unref(caps);
    gst_buffer_unref(buffer);
    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotifyNeedMediaData)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstCaps *caps{gst_caps_new_simple("application/x-cenc", "rate", G_TYPE_INT, 1, "channels", G_TYPE_INT, 2, nullptr)};
    GstBuffer *buffer{gst_buffer_new()};
    audioSink->priv->m_samples.push(gst_sample_new(buffer, caps, nullptr, nullptr));
    auto &bufferPullerMsgQueueMock{bufferPullerWillBeCreated()};
    const int32_t kSourceId{attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO)};

    expectCallInEventLoop();
    expectPostMessage();
    EXPECT_CALL(bufferPullerMsgQueueMock, postMessage(_))
        .WillOnce(Invoke(
            [](const auto &msg)
            {
                msg->handle();
                return true;
            }));
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, addSegment(kNeedDataRequestId, _))
        .WillOnce(Return(firebolt::rialto::AddSegmentStatus::OK));
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, haveData(firebolt::rialto::MediaSourceStatus::OK, kNeedDataRequestId))
        .WillOnce(Return(true));
    m_sut->notifyNeedMediaData(kSourceId, kFrameCount, kNeedDataRequestId, kShmInfo);

    gst_caps_unref(caps);
    gst_buffer_unref(buffer);
    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldFailToNotifyQosWhenSourceIdIsNotKnown)
{
    expectPostMessage();
    expectCallInEventLoop();
    const firebolt::rialto::QosInfo kQosInfo{1, 2};
    m_sut->notifyQos(kUnknownSourceId, kQosInfo);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotifyQos)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);
    bufferPullerWillBeCreated();
    const int32_t kSourceId{attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO)};

    expectPostMessage();
    const firebolt::rialto::QosInfo kQosInfo{1, 2};
    m_sut->notifyQos(kSourceId, kQosInfo);

    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_QOS));

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotifyBufferUnderflow)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    g_signal_connect(audioSink, "buffer-underflow-callback", G_CALLBACK(underflowSignalCallback), nullptr);

    bufferPullerWillBeCreated();
    const int32_t kSourceId{attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO)};

    expectPostMessage();
    // No mutex/cv needed, signal emission is synchronous
    EXPECT_CALL(UnderflowSignalMock::instance(), callbackCalled());
    m_sut->notifyBufferUnderflow(kSourceId);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldFailToNotifyBufferUnderflowWhenSourceIdIsNotKnown)
{
    expectCallInEventLoop();
    expectPostMessage();
    m_sut->notifyBufferUnderflow(kUnknownSourceId);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotifyPlaybackError)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    RialtoGStreamerMSEBaseSinkCallbacks callbacks;
    callbacks.errorCallback = std::bind(playbackErrorCallback, audioSink, std::placeholders::_1);
    audioSink->priv->m_callbacks = callbacks;

    bufferPullerWillBeCreated();
    const int32_t kSourceId{attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO)};

    expectPostMessage();
    EXPECT_CALL(PlaybackErrorMock::instance(), callbackCalled());
    m_sut->notifyPlaybackError(kSourceId, firebolt::rialto::PlaybackError::DECRYPTION);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldFailToNotifyPlaybackErrorWhenSourceIdIsNotKnown)
{
    expectCallInEventLoop();
    expectPostMessage();
    m_sut->notifyPlaybackError(kUnknownSourceId, firebolt::rialto::PlaybackError::DECRYPTION);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldGetPosition)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    bufferPullerWillBeCreated();
    const int32_t kSourceId{attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO)};
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, getPosition(_)).WillOnce(DoAll(SetArgReferee<0>(kPosition), Return(true)));
    expectCallInEventLoop();
    EXPECT_EQ(m_sut->getPosition(kSourceId), kPosition);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldFailToCreateBackend)
{
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, createMediaPlayerBackend(_, kMaxVideoWidth, kMaxVideoHeight));
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, isMediaPlayerBackendCreated()).WillOnce(Return(false));
    expectCallInEventLoop();
    EXPECT_FALSE(m_sut->createBackend());
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldFailToLoad)
{
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, createMediaPlayerBackend(_, kMaxVideoWidth, kMaxVideoHeight));
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, isMediaPlayerBackendCreated()).WillOnce(Return(true));
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, load(kMediaType, kMimeType, kUrl)).WillOnce(Return(false));
    expectCallInEventLoop();
    EXPECT_FALSE(m_sut->createBackend());
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldCreateBackend)
{
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, createMediaPlayerBackend(_, kMaxVideoWidth, kMaxVideoHeight));
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, isMediaPlayerBackendCreated()).WillOnce(Return(true));
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, load(kMediaType, kMimeType, kUrl)).WillOnce(Return(true));
    expectCallInEventLoop();
    EXPECT_TRUE(m_sut->createBackend());
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotPauseWhenNotAttached)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, pause()).Times(0);
    m_sut->pause(0);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotPlayWhenNotAttached)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, play()).Times(0);
    m_sut->play(0);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldPlayWhenAllAttachedPlaying)
{
    attachAudioVideo();
    allSourcesWantToPause();
    serverTransitionedToPaused();
    allSourcesWantToPlay();
    serverTransitionedToPlaying();

    gst_object_unref(m_audioSink);
    gst_object_unref(m_videoSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotSendPlayWhenServerAlreadyPlaying)
{
    attachAudioVideo();
    allSourcesWantToPause();
    serverTransitionedToPaused();

    m_sut->notifyPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
    m_sut->play(m_audioSourceId);
    m_sut->play(m_videoSourceId);

    EXPECT_EQ(m_sut->getClientState(), ClientState::PLAYING);

    gst_object_unref(m_audioSink);
    gst_object_unref(m_videoSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotSendPausedWhenAlreadyPaused)
{
    attachAudioVideo();

    expectCallInEventLoop();
    expectPostMessage();
    m_sut->notifyPlaybackState(firebolt::rialto::PlaybackState::PAUSED);

    m_sut->pause(m_audioSourceId);
    m_sut->pause(m_videoSourceId);

    EXPECT_EQ(m_sut->getClientState(), ClientState::PAUSED);

    gst_object_unref(m_audioSink);
    gst_object_unref(m_videoSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotSendPausedWhenNotAllSourcesAttached)
{
    constexpr int32_t kVideoStreams{1};
    constexpr int32_t kAudioStreams{1};
    constexpr int32_t kTextStreams{0};
    expectCallInEventLoop();
    m_sut->handleStreamCollection(kAudioStreams, kVideoStreams, kTextStreams);

    RialtoMSEBaseSink *audioSink = createAudioSink();
    bufferPullerWillBeCreated();
    const auto kAudioSourceId = attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO);

    expectCallInEventLoop();
    expectPostMessage();

    m_sut->pause(kAudioSourceId);

    EXPECT_EQ(m_sut->getClientState(), ClientState::IDLE);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotOverwriteStreamCollectionSettings)
{
    expectCallInEventLoop();
    m_sut->handleStreamCollection(1, 0, 0);
    m_sut->handleStreamCollection(1, 1, 0);

    RialtoMSEBaseSink *audioSink = createAudioSink();
    bufferPullerWillBeCreated();

    EXPECT_CALL(*m_mediaPlayerClientBackendMock, allSourcesAttached()).WillOnce(Return(true));
    attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO);

    EXPECT_EQ(m_sut->getClientState(), ClientState::READY);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotSendPlayWhenNotAllSourcesAttached)
{
    constexpr int32_t kVideoStreams{1};
    constexpr int32_t kAudioStreams{1};
    constexpr int32_t kTextStreams{0};
    expectCallInEventLoop();
    m_sut->handleStreamCollection(kAudioStreams, kVideoStreams, kTextStreams);

    RialtoMSEBaseSink *audioSink = createAudioSink();
    bufferPullerWillBeCreated();
    const auto kAudioSourceId = attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO);

    expectCallInEventLoop();
    expectPostMessage();

    m_sut->play(kAudioSourceId);

    EXPECT_EQ(m_sut->getClientState(), ClientState::IDLE);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldSendPauseWhenLostStateFromPlaying)
{
    attachAudioVideo();
    allSourcesWantToPause();
    serverTransitionedToPaused();
    allSourcesWantToPlay();
    serverTransitionedToPlaying();

    EXPECT_CALL(*m_mediaPlayerClientBackendMock, pause()).WillOnce(Return(true));
    m_sut->notifyLostState(m_audioSourceId);

    EXPECT_EQ(m_sut->getClientState(), ClientState::AWAITING_PAUSED);

    gst_object_unref(m_audioSink);
    gst_object_unref(m_videoSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldLooseStateWhenLostStateFromPaused)
{
    attachAudioVideo();
    allSourcesWantToPause();
    serverTransitionedToPaused();

    m_sut->notifyLostState(m_audioSourceId);

    EXPECT_EQ(m_sut->getClientState(), ClientState::AWAITING_PAUSED);

    gst_object_unref(m_audioSink);
    gst_object_unref(m_videoSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldDoNothingWhenLostStateNotAttached)
{
    expectCallInEventLoop();
    m_sut->notifyLostState(kUnknownSourceId);
    EXPECT_EQ(m_sut->getClientState(), ClientState::IDLE);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldSendPlayWhenRecoveringFromLostStateFromPlaying)
{
    attachAudioVideo();
    allSourcesWantToPause();
    serverTransitionedToPaused();
    allSourcesWantToPlay();
    serverTransitionedToPlaying();

    EXPECT_CALL(*m_mediaPlayerClientBackendMock, pause()).WillOnce(Return(true));
    m_sut->notifyLostState(m_audioSourceId);
    EXPECT_EQ(m_sut->getClientState(), ClientState::AWAITING_PAUSED);

    serverTransitionedToPaused();
    EXPECT_EQ(m_sut->getClientState(), ClientState::PAUSED);

    EXPECT_CALL(*m_mediaPlayerClientBackendMock, play()).WillOnce(Return(true));
    m_sut->play(m_audioSourceId);
    EXPECT_EQ(m_sut->getClientState(), ClientState::AWAITING_PLAYING);

    serverTransitionedToPlaying();
    EXPECT_EQ(m_sut->getClientState(), ClientState::PLAYING);

    gst_object_unref(m_audioSink);
    gst_object_unref(m_videoSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotPlayWhenNotAllAttachedPlaying)
{
    attachAudioVideo();
    allSourcesWantToPause();
    serverTransitionedToPaused();

    m_sut->play(m_audioSourceId);
    EXPECT_EQ(m_sut->getClientState(), ClientState::PAUSED);

    gst_object_unref(m_audioSink);
    gst_object_unref(m_videoSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotPlayWhenClientNotInPaused)
{
    constexpr int32_t kVideoStreams{1};
    constexpr int32_t kAudioStreams{1};
    constexpr int32_t kTextStreams{0};
    expectCallInEventLoop();
    m_sut->handleStreamCollection(kAudioStreams, kVideoStreams, kTextStreams);

    EXPECT_CALL(*m_mediaPlayerClientBackendMock, allSourcesAttached()).WillOnce(Return(true));

    RialtoMSEBaseSink *audioSink = createAudioSink();
    bufferPullerWillBeCreated();
    const auto kAudioSourceId = attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO);

    RialtoMSEBaseSink *videoSink = createVideoSink();
    bufferPullerWillBeCreated();
    const auto kVideoSourceId = attachSource(videoSink, firebolt::rialto::MediaSourceType::VIDEO);

    expectCallInEventLoop();

    m_sut->play(kAudioSourceId);
    m_sut->play(kVideoSourceId);
    EXPECT_EQ(m_sut->getClientState(), ClientState::READY);

    gst_object_unref(audioSink);
    gst_object_unref(videoSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldPauseWhenWaitingForPlaying)
{
    attachAudioVideo();
    allSourcesWantToPause();
    serverTransitionedToPaused();
    allSourcesWantToPlay();

    EXPECT_CALL(*m_mediaPlayerClientBackendMock, pause()).WillOnce(Return(true));
    m_sut->pause(m_audioSourceId);
    EXPECT_EQ(m_sut->getClientState(), ClientState::AWAITING_PAUSED);

    gst_object_unref(m_audioSink);
    gst_object_unref(m_videoSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldPauseWhenPlaying)
{
    attachAudioVideo();
    allSourcesWantToPause();
    serverTransitionedToPaused();
    allSourcesWantToPlay();
    serverTransitionedToPlaying();

    EXPECT_CALL(*m_mediaPlayerClientBackendMock, pause()).WillOnce(Return(true));
    m_sut->pause(m_audioSourceId);
    EXPECT_EQ(m_sut->getClientState(), ClientState::AWAITING_PAUSED);

    gst_object_unref(m_audioSink);
    gst_object_unref(m_videoSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldPauseWhenAllAttachedPaused)
{
    attachAudioVideo();
    allSourcesWantToPause();
    serverTransitionedToPaused();

    gst_object_unref(m_audioSink);
    gst_object_unref(m_videoSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotPauseWhenNotAllAttachedPaused)
{
    attachAudioVideo();

    expectCallInEventLoop();
    m_sut->pause(m_audioSourceId);
    EXPECT_EQ(m_sut->getClientState(), ClientState::READY);

    gst_object_unref(m_audioSink);
    gst_object_unref(m_videoSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldStop)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, stop()).WillOnce(Return(true));
    m_sut->stop();
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldFailToNotifyThatSourceFinishedFlushWhenSourceIdIsNotFound)
{
    expectCallInEventLoop();
    expectPostMessage();
    m_sut->notifySourceFlushed(kUnknownSourceId);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldFlushWithoutPullingData)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    auto &bufferPullerMsgQueueMock{bufferPullerWillBeCreated()};
    const auto kSourceId = attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO);

    expectPostMessage();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, flush(kSourceId, kResetTime)).WillOnce(Return(true));
    EXPECT_CALL(bufferPullerMsgQueueMock, stop()).RetiresOnSaturation();
    m_sut->flush(kSourceId, kResetTime);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldFlush)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    auto &bufferPullerMsgQueueMock{bufferPullerWillBeCreated()};
    const auto kSourceId = attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO);

    expectPostMessage();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, flush(kSourceId, kResetTime)).WillOnce(Return(true));
    EXPECT_CALL(bufferPullerMsgQueueMock, stop()).RetiresOnSaturation();
    m_sut->flush(kSourceId, kResetTime);

    expectPostMessage();
    EXPECT_CALL(bufferPullerMsgQueueMock, start()).RetiresOnSaturation();
    m_sut->notifySourceFlushed(kSourceId);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldFailToFlush)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    bufferPullerWillBeCreated();
    const auto kSourceId = attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO);

    expectPostMessage();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, flush(kSourceId, kResetTime)).WillOnce(Return(false));
    m_sut->flush(kSourceId, kResetTime);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldFailToFlushWhenNotAttached)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, flush(_, _)).Times(0);
    m_sut->flush(0, kResetTime);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldFailToNotifySourceFlushedWhenSourceIsNotFlushing)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    bufferPullerWillBeCreated();
    const auto kSourceId = attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO);

    expectPostMessage();
    m_sut->notifySourceFlushed(kSourceId);

    expectPostMessage();

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldSetSourcePosition)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    bufferPullerWillBeCreated();
    const auto kSourceId = attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO);

    expectPostMessage();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock,
                setSourcePosition(kSourceId, kPosition, kResetTime, kAppliedRate, kRunningTime))
        .WillOnce(Return(true));
    m_sut->setSourcePosition(kSourceId, kPosition, kResetTime, kAppliedRate, kRunningTime);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldFailToSetSourcePosition)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    bufferPullerWillBeCreated();
    const auto kSourceId = attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO);

    expectPostMessage();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock,
                setSourcePosition(kSourceId, kPosition, kResetTime, kAppliedRate, kRunningTime))
        .WillOnce(Return(false));
    m_sut->setSourcePosition(kSourceId, kPosition, kResetTime, kAppliedRate, kRunningTime);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldSkipSetSourcePositionWhenSourceIdIsNotFound)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    bufferPullerWillBeCreated();
    const auto kSourceId = attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO);

    expectPostMessage();
    m_sut->setSourcePosition(kSourceId + 1, kPosition, kResetTime, kAppliedRate, kRunningTime);

    gst_object_unref(audioSink);
}
//
TEST_F(GstreamerMseMediaPlayerClientTests, ShouldProcessAudioGap)
{
    expectCallInEventLoop();
    expectPostMessage();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, processAudioGap(kPosition, kDuration, kDiscontinuityGap, kAudioAac))
        .WillOnce(Return(true));
    m_sut->processAudioGap(kPosition, kDuration, kDiscontinuityGap, kAudioAac);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldFailToProcessAudioGap)
{
    expectCallInEventLoop();
    expectPostMessage();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, processAudioGap(kPosition, kDuration, kDiscontinuityGap, kAudioAac))
        .WillOnce(Return(false));
    m_sut->processAudioGap(kPosition, kDuration, kDiscontinuityGap, kAudioAac);
}

//
TEST_F(GstreamerMseMediaPlayerClientTests, ShouldSetPlaybackRate)
{
    constexpr double kPlaybackRate{0.5};
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, setPlaybackRate(kPlaybackRate)).WillOnce(Return(true));
    m_sut->setPlaybackRate(kPlaybackRate);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldFailToRemoveSource)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, removeSource(kUnknownSourceId)).WillOnce(Return(false));
    m_sut->removeSource(kUnknownSourceId);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldRemoveSource)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, removeSource(kUnknownSourceId)).WillOnce(Return(true));
    m_sut->removeSource(kUnknownSourceId);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldFailToSetVideoRectangleWhenBackendIsNotCreated)
{
    const std::string kRectangleString{};
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, isMediaPlayerBackendCreated()).WillOnce(Return(false));
    m_sut->setVideoRectangle(kRectangleString);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldFailToSetVideoRectangleWhenStringIsEmpty)
{
    const std::string kRectangleString{};
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, isMediaPlayerBackendCreated()).WillOnce(Return(true));
    m_sut->setVideoRectangle(kRectangleString);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldFailToSetVideoRectangleWhenStringIsInvalid)
{
    const std::string kRectangleString{"invalid"};
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, isMediaPlayerBackendCreated()).WillOnce(Return(true));
    m_sut->setVideoRectangle(kRectangleString);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldSetVideoRectangle)
{
    constexpr int kX{1}, kY{2}, kWidth{3}, kHeight{4};
    const std::string kRectangleString{std::to_string(kX) + "," + std::to_string(kY) + "," + std::to_string(kWidth) +
                                       "," + std::to_string(kHeight)};
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, isMediaPlayerBackendCreated()).WillOnce(Return(true));
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, setVideoWindow(kX, kY, kWidth, kHeight)).WillOnce(Return(true));
    m_sut->setVideoRectangle(kRectangleString);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldGetVideoRectangle)
{
    constexpr int kX{1}, kY{2}, kWidth{3}, kHeight{4};
    const std::string kRectangleString{std::to_string(kX) + "," + std::to_string(kY) + "," + std::to_string(kWidth) +
                                       "," + std::to_string(kHeight)};
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, isMediaPlayerBackendCreated()).WillOnce(Return(true));
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, setVideoWindow(kX, kY, kWidth, kHeight)).WillOnce(Return(true));
    m_sut->setVideoRectangle(kRectangleString);
    EXPECT_EQ(m_sut->getVideoRectangle(), kRectangleString);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldFailToRenderFrame)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, renderFrame()).WillOnce(Return(false));
    EXPECT_FALSE(m_sut->renderFrame(audioSink));
    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldRenderFrame)
{
    constexpr int32_t kVideoStreams{1};
    constexpr int32_t kAudioStreams{0};
    constexpr int32_t kTextStreams{0};
    expectCallInEventLoop();
    m_sut->handleStreamCollection(kAudioStreams, kVideoStreams, kTextStreams);

    EXPECT_CALL(*m_mediaPlayerClientBackendMock, allSourcesAttached()).WillOnce(Return(true));

    RialtoMSEBaseSink *videoSink = createVideoSink();
    bufferPullerWillBeCreated();
    const auto kVideoSourceId = attachSource(videoSink, firebolt::rialto::MediaSourceType::VIDEO);

    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, pause()).WillOnce(Return(true));
    m_sut->pause(kVideoSourceId);
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, renderFrame()).WillOnce(Return(true));
    EXPECT_TRUE(m_sut->renderFrame(videoSink));

    gst_object_unref(videoSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldSetVolume)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, setVolume(kVolume, kVolumeDuration, kEaseType)).WillOnce(Return(true));
    m_sut->setVolume(kVolume, kVolumeDuration, kEaseType);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldGetVolume)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, getVolume(_)).WillOnce(DoAll(SetArgReferee<0>(kVolume), Return(true)));
    EXPECT_EQ(m_sut->getVolume(), kVolume);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldReturnLastKnownVolumeWhenOperationFails)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, getVolume(_)).WillOnce(DoAll(SetArgReferee<0>(kVolume), Return(true)));
    EXPECT_EQ(m_sut->getVolume(), kVolume);
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, getVolume(_)).WillOnce(Return(false));
    EXPECT_EQ(m_sut->getVolume(), kVolume);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldSetMute)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    bufferPullerWillBeCreated();
    const int32_t kAudioSourceId = attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO);

    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, setMute(kMute, kAudioSourceId)).WillOnce(Return(true));
    m_sut->setMute(kMute, kAudioSourceId);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldGetMute)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    bufferPullerWillBeCreated();
    const int32_t kAudioSourceId = attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO);

    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, getMute(_, kAudioSourceId))
        .WillOnce(DoAll(SetArgReferee<0>(kMute), Return(true)));
    EXPECT_EQ(m_sut->getMute(kAudioSourceId), kMute);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldSetImmediateOutput)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    bufferPullerWillBeCreated();
    const int32_t kAudioSourceId = attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO);

    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, setImmediateOutput(kAudioSourceId, kImmediateOutput)).WillOnce(Return(true));
    EXPECT_TRUE(m_sut->setImmediateOutput(kAudioSourceId, kImmediateOutput));

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotSetImmediateOutputIfNoClientBackend)
{
    // Need to create a new message queue as it has been moved
    m_messageQueue = std::make_unique<StrictMock<MessageQueueMock>>();
    StrictMock<MessageQueueMock> &messageQueueMock{*m_messageQueue};

    EXPECT_CALL(*m_messageQueueFactoryMock, createMessageQueue()).WillOnce(Return(ByMove(std::move(m_messageQueue))));
    EXPECT_CALL(messageQueueMock, start());
    EXPECT_CALL(messageQueueMock, stop());
    m_sut = std::make_shared<GStreamerMSEMediaPlayerClient>(m_messageQueueFactoryMock, nullptr, kMaxVideoWidth,
                                                            kMaxVideoHeight);

    const int32_t kAudioSourceId = 1;
    EXPECT_FALSE(m_sut->setImmediateOutput(kAudioSourceId, kImmediateOutput));
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldGetImmediateOutput)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    bufferPullerWillBeCreated();
    const int32_t kAudioSourceId = attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO);

    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, getImmediateOutput(kAudioSourceId, _))
        .WillOnce(DoAll(SetArgReferee<1>(kImmediateOutput), Return(true)));

    bool immediateOutput = 0;
    EXPECT_TRUE(m_sut->getImmediateOutput(kAudioSourceId, immediateOutput));
    EXPECT_EQ(immediateOutput, kImmediateOutput);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotGetImmediateOutputIfNoClientBackend)
{
    // Need to create a new message queue as it has been moved
    m_messageQueue = std::make_unique<StrictMock<MessageQueueMock>>();
    StrictMock<MessageQueueMock> &messageQueueMock{*m_messageQueue};

    EXPECT_CALL(*m_messageQueueFactoryMock, createMessageQueue()).WillOnce(Return(ByMove(std::move(m_messageQueue))));
    EXPECT_CALL(messageQueueMock, start());
    EXPECT_CALL(messageQueueMock, stop());
    m_sut = std::make_shared<GStreamerMSEMediaPlayerClient>(m_messageQueueFactoryMock, nullptr, kMaxVideoWidth,
                                                            kMaxVideoHeight);

    const int32_t kAudioSourceId = 1;
    bool immediateOutput = false;
    EXPECT_FALSE(m_sut->getImmediateOutput(kAudioSourceId, immediateOutput));
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldSetLowLatency)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, setLowLatency(kLowLatency)).WillOnce(Return(true));
    EXPECT_TRUE(m_sut->setLowLatency(kLowLatency));
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotSetLowLatencyIfNoClientBackend)
{
    // Need to create a new message queue as it has been moved
    m_messageQueue = std::make_unique<StrictMock<MessageQueueMock>>();
    StrictMock<MessageQueueMock> &messageQueueMock{*m_messageQueue};

    EXPECT_CALL(*m_messageQueueFactoryMock, createMessageQueue()).WillOnce(Return(ByMove(std::move(m_messageQueue))));
    EXPECT_CALL(messageQueueMock, start());
    EXPECT_CALL(messageQueueMock, stop());
    m_sut = std::make_shared<GStreamerMSEMediaPlayerClient>(m_messageQueueFactoryMock, nullptr, kMaxVideoWidth,
                                                            kMaxVideoHeight);

    EXPECT_FALSE(m_sut->setLowLatency(kLowLatency));
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldSetSync)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, setSync(kSync)).WillOnce(Return(true));
    EXPECT_TRUE(m_sut->setSync(kSync));
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotSetSyncIfNoClientBackend)
{
    // Need to create a new message queue as it has been moved
    m_messageQueue = std::make_unique<StrictMock<MessageQueueMock>>();
    StrictMock<MessageQueueMock> &messageQueueMock{*m_messageQueue};

    EXPECT_CALL(*m_messageQueueFactoryMock, createMessageQueue()).WillOnce(Return(ByMove(std::move(m_messageQueue))));
    EXPECT_CALL(messageQueueMock, start());
    EXPECT_CALL(messageQueueMock, stop());
    m_sut = std::make_shared<GStreamerMSEMediaPlayerClient>(m_messageQueueFactoryMock, nullptr, kMaxVideoWidth,
                                                            kMaxVideoHeight);

    EXPECT_FALSE(m_sut->setSync(kSync));
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldGetSync)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, getSync(_)).WillOnce(DoAll(SetArgReferee<0>(kSync), Return(true)));

    bool sync = false;
    EXPECT_TRUE(m_sut->getSync(sync));
    EXPECT_EQ(sync, kSync);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotGetSyncIfNoClientBackend)
{
    // Need to create a new message queue as it has been moved
    m_messageQueue = std::make_unique<StrictMock<MessageQueueMock>>();
    StrictMock<MessageQueueMock> &messageQueueMock{*m_messageQueue};

    EXPECT_CALL(*m_messageQueueFactoryMock, createMessageQueue()).WillOnce(Return(ByMove(std::move(m_messageQueue))));
    EXPECT_CALL(messageQueueMock, start());
    EXPECT_CALL(messageQueueMock, stop());
    m_sut = std::make_shared<GStreamerMSEMediaPlayerClient>(m_messageQueueFactoryMock, nullptr, kMaxVideoWidth,
                                                            kMaxVideoHeight);

    bool sync = false;
    EXPECT_FALSE(m_sut->getSync(sync));
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldSetSyncOff)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, setSyncOff(kSyncOff)).WillOnce(Return(true));
    EXPECT_TRUE(m_sut->setSyncOff(kSyncOff));
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotSetSyncOffIfNoClientBackend)
{
    // Need to create a new message queue as it has been moved
    m_messageQueue = std::make_unique<StrictMock<MessageQueueMock>>();
    StrictMock<MessageQueueMock> &messageQueueMock{*m_messageQueue};

    EXPECT_CALL(*m_messageQueueFactoryMock, createMessageQueue()).WillOnce(Return(ByMove(std::move(m_messageQueue))));
    EXPECT_CALL(messageQueueMock, start());
    EXPECT_CALL(messageQueueMock, stop());
    m_sut = std::make_shared<GStreamerMSEMediaPlayerClient>(m_messageQueueFactoryMock, nullptr, kMaxVideoWidth,
                                                            kMaxVideoHeight);

    EXPECT_FALSE(m_sut->setSyncOff(kSyncOff));
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldSetStreamSyncMode)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    bufferPullerWillBeCreated();
    const int32_t kAudioSourceId = attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO);

    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, setStreamSyncMode(kAudioSourceId, kStreamSyncMode)).WillOnce(Return(true));
    EXPECT_TRUE(m_sut->setStreamSyncMode(kAudioSourceId, kStreamSyncMode));

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotSetStreamSyncModeIfNoClientBackend)
{
    // Need to create a new message queue as it has been moved
    m_messageQueue = std::make_unique<StrictMock<MessageQueueMock>>();
    StrictMock<MessageQueueMock> &messageQueueMock{*m_messageQueue};

    EXPECT_CALL(*m_messageQueueFactoryMock, createMessageQueue()).WillOnce(Return(ByMove(std::move(m_messageQueue))));
    EXPECT_CALL(messageQueueMock, start());
    EXPECT_CALL(messageQueueMock, stop());
    m_sut = std::make_shared<GStreamerMSEMediaPlayerClient>(m_messageQueueFactoryMock, nullptr, kMaxVideoWidth,
                                                            kMaxVideoHeight);

    EXPECT_FALSE(m_sut->setStreamSyncMode(kUnknownSourceId, kStreamSyncMode));
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldGetStreamSyncMode)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, getStreamSyncMode(_))
        .WillOnce(DoAll(SetArgReferee<0>(kStreamSyncMode), Return(true)));

    int32_t streamSyncMode = 0;
    EXPECT_TRUE(m_sut->getStreamSyncMode(streamSyncMode));
    EXPECT_EQ(streamSyncMode, kStreamSyncMode);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotGetStreamSyncModeIfNoClientBackend)
{
    // Need to create a new message queue as it has been moved
    m_messageQueue = std::make_unique<StrictMock<MessageQueueMock>>();
    StrictMock<MessageQueueMock> &messageQueueMock{*m_messageQueue};

    EXPECT_CALL(*m_messageQueueFactoryMock, createMessageQueue()).WillOnce(Return(ByMove(std::move(m_messageQueue))));
    EXPECT_CALL(messageQueueMock, start());
    EXPECT_CALL(messageQueueMock, stop());
    m_sut = std::make_shared<GStreamerMSEMediaPlayerClient>(m_messageQueueFactoryMock, nullptr, kMaxVideoWidth,
                                                            kMaxVideoHeight);

    int32_t streamSyncMode = 0;
    EXPECT_FALSE(m_sut->getStreamSyncMode(streamSyncMode));
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldSetAudioStreams)
{
    constexpr int32_t kVideoStreams{-1};
    constexpr int32_t kAudioStreams{1};
    constexpr int32_t kTextStreams{-1};
    expectCallInEventLoop();
    m_sut->handleStreamCollection(kAudioStreams, kVideoStreams, kTextStreams);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldSetAudioStreamsOnly)
{
    constexpr int32_t kVideoStreams{0};
    constexpr int32_t kAudioStreams{1};
    constexpr int32_t kTextStreams{0};
    expectCallInEventLoop();
    m_sut->handleStreamCollection(kAudioStreams, kVideoStreams, kTextStreams);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldAddSegment)
{
    constexpr auto kStatus{firebolt::rialto::AddSegmentStatus::OK};
    std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> mediaSegment =
        std::make_unique<firebolt::rialto::IMediaPipeline::MediaSegmentAudio>();
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, addSegment(kNeedDataRequestId, PtrMatcher(mediaSegment.get())))
        .WillOnce(Return(kStatus));
    m_sut->addSegment(kNeedDataRequestId, mediaSegment);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldFailToAttachSourceWhenMediaTypeIsUnknown)
{
    std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> mediaSource{
        std::make_unique<StrictMock<MediaSourceMock>>()};
    StrictMock<MediaSourceMock> &mediaSourceMock{static_cast<StrictMock<MediaSourceMock> &>(*mediaSource)};
    EXPECT_CALL(mediaSourceMock, getType()).WillRepeatedly(Return(firebolt::rialto::MediaSourceType::UNKNOWN));
    RialtoMSEBaseSink *sink = createAudioSink();
    EXPECT_FALSE(m_sut->attachSource(mediaSource, sink));
    gst_object_unref(sink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldFailToAttachSourceWhenOperationFails)
{
    std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> mediaSource{
        std::make_unique<StrictMock<MediaSourceMock>>()};
    StrictMock<MediaSourceMock> &mediaSourceMock{static_cast<StrictMock<MediaSourceMock> &>(*mediaSource)};
    EXPECT_CALL(mediaSourceMock, getType()).WillRepeatedly(Return(firebolt::rialto::MediaSourceType::AUDIO));
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, attachSource(PtrMatcher(mediaSource.get()))).WillOnce(Return(false));
    RialtoMSEBaseSink *sink = createAudioSink();
    EXPECT_FALSE(m_sut->attachSource(mediaSource, sink));
    gst_object_unref(sink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldAttachAudioSource)
{
    RialtoMSEBaseSink *sink = createAudioSink();
    bufferPullerWillBeCreated();
    attachSource(sink, firebolt::rialto::MediaSourceType::AUDIO);
    gst_object_unref(sink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldAttachVideoSource)
{
    RialtoMSEBaseSink *sink = createVideoSink();
    bufferPullerWillBeCreated();
    attachSource(sink, firebolt::rialto::MediaSourceType::VIDEO);
    gst_object_unref(sink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldAttachSubtitleSource)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    bufferPullerWillBeCreated();
    attachSource(sink, firebolt::rialto::MediaSourceType::SUBTITLE);
    gst_object_unref(sink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldAttachAllSources)
{
    constexpr int32_t kVideoStreams{1};
    constexpr int32_t kAudioStreams{1};
    constexpr int32_t kTextStreams{0};
    expectCallInEventLoop();
    m_sut->handleStreamCollection(kAudioStreams, kVideoStreams, kTextStreams);
    RialtoMSEBaseSink *audioSink = createAudioSink();
    RialtoMSEBaseSink *videoSink = createVideoSink();
    bufferPullerWillBeCreated();
    attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO);
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, allSourcesAttached()).WillOnce(Return(true));
    bufferPullerWillBeCreated();
    attachSource(videoSink, firebolt::rialto::MediaSourceType::VIDEO);
    gst_object_unref(audioSink);
    gst_object_unref(videoSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotSendAllSourcesAttached)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    bufferPullerWillBeCreated();
    attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO);

    expectPostMessage();
    m_sut->sendAllSourcesAttachedIfPossible();

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldSendAllSourcesAttached)
{
    constexpr int kAudioStreams{1}, kVideoStreams{0}, kSubtitleStreams{0};
    RialtoMSEBaseSink *audioSink = createAudioSink();
    bufferPullerWillBeCreated();
    const int32_t kAudioSourceId = attachSource(audioSink, firebolt::rialto::MediaSourceType::AUDIO);
    m_sut->pause(kAudioSourceId);

    EXPECT_CALL(*m_mediaPlayerClientBackendMock, pause()).WillOnce(Return(true));
    expectPostMessage();
    m_sut->handleStreamCollection(kAudioStreams, kVideoStreams, kSubtitleStreams);

    EXPECT_CALL(*m_mediaPlayerClientBackendMock, allSourcesAttached()).WillOnce(Return(true));
    expectPostMessage();
    m_sut->sendAllSourcesAttachedIfPossible();
    EXPECT_EQ(m_sut->getClientState(), ClientState::AWAITING_PAUSED);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldSetTextTrackIdentifier)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, setTextTrackIdentifier(kTextTrackIdentifier)).WillOnce(Return(true));
    m_sut->setTextTrackIdentifier(kTextTrackIdentifier);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldGetTextTrackIdentifier)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, getTextTrackIdentifier(_))
        .WillOnce(DoAll(SetArgReferee<0>(kTextTrackIdentifier), Return(true)));
    EXPECT_EQ(m_sut->getTextTrackIdentifier(), kTextTrackIdentifier);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldSetBufferingLimit)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, setBufferingLimit(kBufferingLimit)).WillOnce(Return(true));
    m_sut->setBufferingLimit(kBufferingLimit);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldGetBufferingLimit)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, getBufferingLimit(_))
        .WillOnce(DoAll(SetArgReferee<0>(kBufferingLimit), Return(true)));
    EXPECT_EQ(m_sut->getBufferingLimit(), kBufferingLimit);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldSetUseBuffering)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, setUseBuffering(kUseBuffering)).WillOnce(Return(true));
    m_sut->setUseBuffering(kUseBuffering);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldGetUseBuffering)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, getUseBuffering(_))
        .WillOnce(DoAll(SetArgReferee<0>(kUseBuffering), Return(true)));
    EXPECT_EQ(m_sut->getUseBuffering(), kUseBuffering);
}
