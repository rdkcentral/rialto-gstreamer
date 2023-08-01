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
#include "MessageQueueMock.h"
#include <gtest/gtest.h>

using firebolt::rialto::client::MediaPlayerClientBackendMock;
using testing::_;
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
constexpr int32_t kSourceId{0};
constexpr size_t kFrameCount{1};
constexpr uint32_t kNeedDataRequestId{2};
const std::shared_ptr<firebolt::rialto::MediaPlayerShmInfo> kShmInfo{nullptr};
const std::string kUrl{"mse://1"};
constexpr firebolt::rialto::MediaType kMediaType{firebolt::rialto::MediaType::MSE};
const std::string kMimeType{""};
constexpr double kVolume{1.0};
constexpr bool kMute{true};
MATCHER_P(PtrMatcher, ptr, "")
{
    return ptr == arg.get();
}
} // namespace

class GstreamerMseMediaPlayerClientTests : public testing::Test
{
public:
    GstreamerMseMediaPlayerClientTests()
    {
        EXPECT_CALL(m_messageQueueMock, start());
        EXPECT_CALL(m_messageQueueMock, stop());
        m_sut = std::make_shared<GStreamerMSEMediaPlayerClient>(std::move(m_messageQueue), m_mediaPlayerClientBackendMock,
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

    std::shared_ptr<StrictMock<MediaPlayerClientBackendMock>> m_mediaPlayerClientBackendMock{
        std::make_shared<StrictMock<MediaPlayerClientBackendMock>>()};
    std::unique_ptr<StrictMock<MessageQueueMock>> m_messageQueue{std::make_unique<StrictMock<MessageQueueMock>>()};
    StrictMock<MessageQueueMock> &m_messageQueueMock{*m_messageQueue};
    std::shared_ptr<GStreamerMSEMediaPlayerClient> m_sut;
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
    expectPostMessage();
    expectCallInEventLoop();
    m_sut->notifyPosition(kPosition);
    m_sut->destroyClientBackend();
    EXPECT_EQ(m_sut->getPosition(), kPosition);
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

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotifyNeedMediaData)
{
    expectCallInEventLoop();
    expectPostMessage();
    m_sut->notifyNeedMediaData(kSourceId, kFrameCount, kNeedDataRequestId, kShmInfo);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotifyQos)
{
    expectPostMessage();
    expectCallInEventLoop();
    const firebolt::rialto::QosInfo kQosInfo{1, 2};
    m_sut->notifyQos(kSourceId, kQosInfo);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotifyBufferUnderflow)
{
    expectCallInEventLoop();
    expectPostMessage();
    m_sut->notifyBufferUnderflow(kSourceId);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldGetPosition)
{
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, getPosition(_)).WillOnce(DoAll(SetArgReferee<0>(kPosition), Return(true)));
    expectCallInEventLoop();
    EXPECT_EQ(m_sut->getPosition(), kPosition);
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

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldPlay)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, play()).WillOnce(Return(true));
    m_sut->play();
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldPause)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, pause()).WillOnce(Return(true));
    m_sut->pause();
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldStop)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, stop()).WillOnce(Return(true));
    m_sut->stop();
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldFailToNotifyThatSourceStartedSeekingWhenSourceIsNotFound)
{
    expectCallInEventLoop();
    m_sut->notifySourceStartedSeeking(kSourceId);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldSeek)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, seek(kPosition)).WillOnce(Return(true));
    m_sut->seek(kPosition);
}

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
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, removeSource(kSourceId)).WillOnce(Return(false));
    m_sut->removeSource(kSourceId);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldRemoveSource)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, removeSource(kSourceId)).WillOnce(Return(true));
    m_sut->removeSource(kSourceId);
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

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldSetVolume)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, setVolume(kVolume)).WillOnce(Return(true));
    m_sut->setVolume(kVolume);
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
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, setMute(kMute)).WillOnce(Return(true));
    m_sut->setMute(kMute);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldGetMute)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, getMute(_)).WillOnce(DoAll(SetArgReferee<0>(kMute), Return(true)));
    EXPECT_EQ(m_sut->getMute(), kMute);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldReturnLastKnownMuteWhenOperationFails)
{
    expectCallInEventLoop();
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, getMute(_)).WillOnce(DoAll(SetArgReferee<0>(kMute), Return(true)));
    EXPECT_EQ(m_sut->getMute(), kMute);
    EXPECT_CALL(*m_mediaPlayerClientBackendMock, getMute(_)).WillOnce(Return(false));
    EXPECT_EQ(m_sut->getMute(), kMute);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldSetAudioStreams)
{
    constexpr int32_t kAudioStreams{1};
    constexpr bool kIsAudioOnly{false};
    expectCallInEventLoop();
    m_sut->setAudioStreamsInfo(kAudioStreams, kIsAudioOnly);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldSetAudioStreamsOnly)
{
    constexpr int32_t kAudioStreams{1};
    constexpr bool kIsAudioOnly{true};
    expectCallInEventLoop();
    m_sut->setAudioStreamsInfo(kAudioStreams, kIsAudioOnly);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldSetVideoStreams)
{
    constexpr int32_t kVideoStreams{1};
    constexpr bool kIsVideoOnly{false};
    expectCallInEventLoop();
    m_sut->setVideoStreamsInfo(kVideoStreams, kIsVideoOnly);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldSetVideoStreamsOnly)
{
    constexpr int32_t kVideoStreams{1};
    constexpr bool kIsVideoOnly{true};
    expectCallInEventLoop();
    m_sut->setVideoStreamsInfo(kVideoStreams, kIsVideoOnly);
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
