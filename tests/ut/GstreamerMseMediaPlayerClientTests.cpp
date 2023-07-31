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
#include <gtest/gtest.h>

using firebolt::rialto::client::MediaPlayerClientBackendMock;
using testing::Return;
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
} // namespace

class GstreamerMseMediaPlayerClientTests : public testing::Test
{
public:
    GstreamerMseMediaPlayerClientTests() = default;
    ~GstreamerMseMediaPlayerClientTests() override = default;

    std::shared_ptr<StrictMock<MediaPlayerClientBackendMock>> m_mediaPlayerClientBackendMock{
        std::make_shared<StrictMock<MediaPlayerClientBackendMock>>()};
    std::unique_ptr<GStreamerMSEMediaPlayerClient> m_sut{
        std::make_unique<GStreamerMSEMediaPlayerClient>(m_mediaPlayerClientBackendMock, kMaxVideoWidth, kMaxVideoHeight)};
};

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldDestroyBackend)
{
    m_sut->destroyClientBackend();
    EXPECT_FALSE(m_sut->createBackend()); // Operation should fail when client backend is null
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotifyDuration)
{
    constexpr int64_t kDuration{1234};
    m_sut->notifyDuration(kDuration);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotifyPosition)
{
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
    m_sut->notifyNeedMediaData(kSourceId, kFrameCount, kNeedDataRequestId, kShmInfo);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotifyQos)
{
    const firebolt::rialto::QosInfo kQosInfo{1, 2};
    m_sut->notifyQos(kSourceId, kQosInfo);
}

TEST_F(GstreamerMseMediaPlayerClientTests, ShouldNotifyBufferUnderflow)
{
    m_sut->notifyBufferUnderflow(kSourceId);
}
