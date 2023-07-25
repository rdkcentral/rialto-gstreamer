/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 Sky UK
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

namespace firebolt::rialto
{
bool operator==(const VideoRequirements &lhs, const VideoRequirements &rhs)
{
    return lhs.maxWidth == rhs.maxWidth && lhs.maxHeight == rhs.maxHeight;
}
} // namespace firebolt::rialto

namespace
{
constexpr VideoRequirements kVideoRequirements{1024, 768};
constexpr double kVolume{0.7};
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
    EXPECT_CALL(*m_mediaPipelineMock, setVolume(kVolume)).WillOnce(Return(true));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.setVolume(kVolume));
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
    EXPECT_CALL(*m_mediaPipelineMock, setMute(kMute)).WillOnce(Return(true));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.setMute(kMute));
}

TEST_F(MediaPlayerClientBackendTests, ShouldGetMute)
{
    bool mute{false};
    EXPECT_CALL(*m_mediaPipelineMock, getMute(_)).WillOnce(DoAll(SetArgReferee<0>(kMute), Return(true)));
    initializeMediaPipeline();
    ASSERT_TRUE(m_sut.isMediaPlayerBackendCreated());
    EXPECT_TRUE(m_sut.getMute(mute));
    EXPECT_EQ(kMute, mute);
}
