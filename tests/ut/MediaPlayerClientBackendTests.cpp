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

#include "Matchers.h"
#include "MediaPipelineClientMock.h"
#include "MediaPipelineMock.h"
#include "MediaPlayerClientBackend.h"
#include <gtest/gtest.h>

using firebolt::rialto::IMediaPipelineFactory;
using firebolt::rialto::MediaPipelineClientMock;
using firebolt::rialto::MediaPipelineFactoryMock;
using firebolt::rialto::MediaPipelineMock;
using firebolt::rialto::VideoRequirements;
using firebolt::rialto::client::MediaPlayerClientBackend;
using testing::_;
using testing::ByMove;
using testing::DoAll;
using testing::Return;
using testing::SetArgReferee;
using testing::StrictMock;

namespace
{
constexpr VideoRequirements kVideoRequirements{1024, 768};
constexpr double kVolume{0.7};
constexpr uint32_t kVolumeDuration{1000};
constexpr firebolt::rialto::EaseType kEaseType{firebolt::rialto::EaseType::EASE_LINEAR};
constexpr bool kMute{true};
MATCHER_P(PtrMatcher, ptr, "")
{
    return ptr == arg.get();
}
} // namespace

class MediaPlayerClientBackendTests : public testing::Test
{
public:
    std::shared_ptr<StrictMock<MediaPipelineFactoryMock>> m_mediaPipelineFactoryMock{
        std::dynamic_pointer_cast<StrictMock<MediaPipelineFactoryMock>>(IMediaPipelineFactory::createFactory())};
    std::unique_ptr<StrictMock<MediaPipelineMock>> m_mediaPipelineMock{std::make_unique<StrictMock<MediaPipelineMock>>()};
    std::shared_ptr<StrictMock<MediaPipelineClientMock>> m_mediaPipelineClientMock{
        std::make_shared<StrictMock<MediaPipelineClientMock>>()};
    MediaPlayerClientBackend m_sut;

    void initializeMediaPipeline()
    {
        EXPECT_CALL(*m_mediaPipelineFactoryMock, createMediaPipeline(_, kVideoRequirements))
            .WillOnce(Return(ByMove(std::move(m_mediaPipelineMock))));
        m_sut.createMediaPlayerBackend(m_mediaPipelineClientMock, kVideoRequirements.maxWidth,
                                       kVideoRequirements.maxHeight);
    }
};

TEST_F(MediaPlayerClientBackendTests, MediaPlayerShouldNotBeCreated)
{
    EXPECT_FALSE(m_sut.isMediaPlayerBackendCreated());
}

TEST_F(MediaPlayerClientBackendTests, ShouldFailToCreateMediaPipeline)
{
    EXPECT_CALL(*m_mediaPipelineFactoryMock, createMediaPipeline(_, kVideoRequirements)).WillOnce(Return(nullptr));
    m_sut.createMediaPlayerBackend(m_mediaPipelineClientMock, kVideoRequirements.maxWidth, kVideoRequirements.maxHeight);
    EXPECT_FALSE(m_sut.isMediaPlayerBackendCreated());
}

TEST_F(MediaPlayerClientBackendTests, ShouldCreateMediaPipeline)
{
    initializeMediaPipeline();
    EXPECT_TRUE(m_sut.isMediaPlayerBackendCreated());
}

TEST_F(MediaPlayerClientBackendTests, ShouldAttachSource)
{
    std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> mediaSourceAudio{
        std::make_unique<firebolt::rialto::IMediaPipeline::MediaSourceAudio>("mime_type")};
    EXPECT_CALL(*m_mediaPipelineMock, attachSource(PtrMatcher(mediaSourceAudio.get()))).WillOnce(Return(true));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.attachSource(mediaSourceAudio));
}

TEST_F(MediaPlayerClientBackendTests, ShouldRemoveSource)
{
    constexpr int32_t id{123};
    EXPECT_CALL(*m_mediaPipelineMock, removeSource(id)).WillOnce(Return(true));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.removeSource(id));
}

TEST_F(MediaPlayerClientBackendTests, AllSourcesShouldBeAttached)
{
    EXPECT_CALL(*m_mediaPipelineMock, allSourcesAttached()).WillOnce(Return(true));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.allSourcesAttached());
}

TEST_F(MediaPlayerClientBackendTests, ShouldLoad)
{
    constexpr firebolt::rialto::MediaType kType{firebolt::rialto::MediaType::MSE};
    const std::string kMimeType{"mime_type"};
    const std::string kUrl{"url"};
    EXPECT_CALL(*m_mediaPipelineMock, load(kType, kMimeType, kUrl)).WillOnce(Return(true));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.load(kType, kMimeType, kUrl));
}

TEST_F(MediaPlayerClientBackendTests, ShouldPlay)
{
    EXPECT_CALL(*m_mediaPipelineMock, play()).WillOnce(Return(true));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.play());
}

TEST_F(MediaPlayerClientBackendTests, ShouldPause)
{
    EXPECT_CALL(*m_mediaPipelineMock, pause()).WillOnce(Return(true));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.pause());
}

TEST_F(MediaPlayerClientBackendTests, ShouldStop)
{
    EXPECT_CALL(*m_mediaPipelineMock, stop()).WillOnce(Return(true));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.stop());
}

TEST_F(MediaPlayerClientBackendTests, ShouldHaveData)
{
    constexpr auto kStatus{firebolt::rialto::MediaSourceStatus::EOS};
    constexpr unsigned int kNeedDataRequestId{12};
    EXPECT_CALL(*m_mediaPipelineMock, haveData(kStatus, kNeedDataRequestId)).WillOnce(Return(true));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.haveData(kStatus, kNeedDataRequestId));
}

TEST_F(MediaPlayerClientBackendTests, ShouldSeek)
{
    constexpr int64_t position{123};
    EXPECT_CALL(*m_mediaPipelineMock, setPosition(position)).WillOnce(Return(true));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.seek(position));
}

TEST_F(MediaPlayerClientBackendTests, ShouldSetPlaybackRate)
{
    constexpr double rate{1.25};
    EXPECT_CALL(*m_mediaPipelineMock, setPlaybackRate(rate)).WillOnce(Return(true));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.setPlaybackRate(rate));
}

TEST_F(MediaPlayerClientBackendTests, ShouldSetVideoWindow)
{
    constexpr unsigned int kX{1};
    constexpr unsigned int kY{2};
    constexpr unsigned int kWidth{3};
    constexpr unsigned int kHeight{4};
    EXPECT_CALL(*m_mediaPipelineMock, setVideoWindow(kX, kY, kWidth, kHeight)).WillOnce(Return(true));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.setVideoWindow(kX, kY, kWidth, kHeight));
}

TEST_F(MediaPlayerClientBackendTests, ShouldAddSegment)
{
    constexpr firebolt::rialto::AddSegmentStatus kStatus{firebolt::rialto::AddSegmentStatus::OK};
    constexpr unsigned int kNeedDataRequestId{12};
    const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> kMediaSegment{
        std::make_unique<firebolt::rialto::IMediaPipeline::MediaSegmentAudio>()};
    EXPECT_CALL(*m_mediaPipelineMock, addSegment(kNeedDataRequestId, PtrMatcher(kMediaSegment.get())))
        .WillOnce(Return(kStatus));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_EQ(kStatus, m_sut.addSegment(kNeedDataRequestId, kMediaSegment));
}

TEST_F(MediaPlayerClientBackendTests, ShouldGetPosition)
{
    int64_t resultPosition{0};
    constexpr int64_t kPosition{123};
    EXPECT_CALL(*m_mediaPipelineMock, getPosition(_)).WillOnce(DoAll(SetArgReferee<0>(kPosition), Return(true)));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.getPosition(resultPosition));
    EXPECT_EQ(kPosition, resultPosition);
}

TEST_F(MediaPlayerClientBackendTests, ShouldRenderFrame)
{
    EXPECT_CALL(*m_mediaPipelineMock, renderFrame()).WillOnce(Return(true));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.renderFrame());
}

TEST_F(MediaPlayerClientBackendTests, ShouldSetVolume)
{
    EXPECT_CALL(*m_mediaPipelineMock, setVolume(kVolume, kVolumeDuration, kEaseType)).WillOnce(Return(true));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.setVolume(kVolume, kVolumeDuration, kEaseType));
}

TEST_F(MediaPlayerClientBackendTests, ShouldGetVolume)
{
    double volume{0.0};
    EXPECT_CALL(*m_mediaPipelineMock, getVolume(_)).WillOnce(DoAll(SetArgReferee<0>(kVolume), Return(true)));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.getVolume(volume));
    EXPECT_EQ(kVolume, volume);
}

TEST_F(MediaPlayerClientBackendTests, ShouldSetMute)
{
    constexpr int32_t kSourceId{12};
    EXPECT_CALL(*m_mediaPipelineMock, setMute(kSourceId, kMute)).WillOnce(Return(true));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.setMute(kMute, kSourceId));
}

TEST_F(MediaPlayerClientBackendTests, ShouldGetMute)
{
    constexpr int32_t kSourceId{12};
    bool mute{false};
    EXPECT_CALL(*m_mediaPipelineMock, getMute(kSourceId, _)).WillOnce(DoAll(SetArgReferee<1>(kMute), Return(true)));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.getMute(mute, kSourceId));
    EXPECT_EQ(kMute, mute);
}

TEST_F(MediaPlayerClientBackendTests, ShouldFlush)
{
    constexpr int32_t kSourceId{12};
    constexpr bool kResetTime{false};
    EXPECT_CALL(*m_mediaPipelineMock, flush(kSourceId, kResetTime)).WillOnce(Return(true));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.flush(kSourceId, kResetTime));
}

TEST_F(MediaPlayerClientBackendTests, ShouldSetSourcePosition)
{
    constexpr int32_t kSourceId{12};
    constexpr int64_t kPosition{34};
    constexpr bool kResetTime{true};
    constexpr double kAppliedRate{2.0};
    constexpr uint64_t kStopPosition{1234};
    EXPECT_CALL(*m_mediaPipelineMock, setSourcePosition(kSourceId, kPosition, kResetTime, kAppliedRate, kStopPosition))
        .WillOnce(Return(true));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.setSourcePosition(kSourceId, kPosition, kResetTime, kAppliedRate, kStopPosition));
}

TEST_F(MediaPlayerClientBackendTests, ShouldProcessAudioGap)
{
    constexpr int64_t kPosition{34};
    constexpr uint32_t kDuration{23};
    constexpr int64_t kDiscontinuityGap{1};
    constexpr bool kAudioAac{false};

    EXPECT_CALL(*m_mediaPipelineMock, processAudioGap(kPosition, kDuration, kDiscontinuityGap, kAudioAac))
        .WillOnce(Return(true));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.processAudioGap(kPosition, kDuration, kDiscontinuityGap, kAudioAac));
}

TEST_F(MediaPlayerClientBackendTests, ShouldSetBufferingLimit)
{
    constexpr uint32_t kBufferingLimit{123};
    EXPECT_CALL(*m_mediaPipelineMock, setBufferingLimit(kBufferingLimit)).WillOnce(Return(true));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.setBufferingLimit(kBufferingLimit));
}

TEST_F(MediaPlayerClientBackendTests, ShouldGetBufferingLimit)
{
    constexpr uint32_t kBufferingLimit{123};
    uint32_t bufferingLimit{0};
    EXPECT_CALL(*m_mediaPipelineMock, getBufferingLimit(_)).WillOnce(DoAll(SetArgReferee<0>(kBufferingLimit), Return(true)));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.getBufferingLimit(bufferingLimit));
    EXPECT_EQ(kBufferingLimit, bufferingLimit);
}

TEST_F(MediaPlayerClientBackendTests, ShouldSetUseBuffering)
{
    constexpr bool kUseBuffering{true};
    EXPECT_CALL(*m_mediaPipelineMock, setUseBuffering(kUseBuffering)).WillOnce(Return(true));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.setUseBuffering(kUseBuffering));
}

TEST_F(MediaPlayerClientBackendTests, ShouldGetUseBuffering)
{
    constexpr bool kUseBuffering{true};
    bool useBuffering{false};
    EXPECT_CALL(*m_mediaPipelineMock, getUseBuffering(_)).WillOnce(DoAll(SetArgReferee<0>(kUseBuffering), Return(true)));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.getUseBuffering(useBuffering));
    EXPECT_EQ(kUseBuffering, useBuffering);
}
