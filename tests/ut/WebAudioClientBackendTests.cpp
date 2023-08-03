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

#include "WebAudioClientBackend.h"
#include "WebAudioPlayerClientMock.h"
#include "WebAudioPlayerMock.h"
#include <gtest/gtest.h>

using firebolt::rialto::IWebAudioPlayerFactory;
using firebolt::rialto::WebAudioPlayerClientMock;
using firebolt::rialto::WebAudioPlayerFactoryMock;
using firebolt::rialto::WebAudioPlayerMock;
using firebolt::rialto::client::WebAudioClientBackend;
using testing::_;
using testing::ByMove;
using testing::DoAll;
using testing::Return;
using testing::SetArgReferee;
using testing::StrictMock;

namespace
{
const std::string kAudioMimeType{"mime_type"};
constexpr uint32_t kPriority{123};
constexpr firebolt::rialto::WebAudioConfig kConfig{firebolt::rialto::WebAudioPcmConfig{1, 2, 3, false, true, false}};
constexpr uint32_t kFrames{18};
constexpr double kVolume{0.5};
} // namespace

class WebAudioClientBackendTests : public testing::Test
{
public:
    std::shared_ptr<StrictMock<WebAudioPlayerFactoryMock>> m_playerFactoryMock{
        std::dynamic_pointer_cast<StrictMock<WebAudioPlayerFactoryMock>>(IWebAudioPlayerFactory::createFactory())};
    std::unique_ptr<StrictMock<WebAudioPlayerMock>> m_playerMock{std::make_unique<StrictMock<WebAudioPlayerMock>>()};
    std::shared_ptr<StrictMock<WebAudioPlayerClientMock>> m_clientMock{
        std::make_shared<StrictMock<WebAudioPlayerClientMock>>()};
    WebAudioClientBackend m_sut;

    bool createBackend()
    {
        EXPECT_CALL(*m_playerFactoryMock, createWebAudioPlayer(_, kAudioMimeType, kPriority, &kConfig))
            .WillOnce(Return(ByMove(std::move(m_playerMock))));
        return m_sut.createWebAudioBackend(m_clientMock, kAudioMimeType, kPriority, &kConfig);
    }
};

TEST_F(WebAudioClientBackendTests, ShouldFailToCreateBackend)
{
    EXPECT_CALL(*m_playerFactoryMock, createWebAudioPlayer(_, kAudioMimeType, kPriority, &kConfig)).WillOnce(Return(nullptr));
    EXPECT_FALSE(m_sut.createWebAudioBackend(m_clientMock, kAudioMimeType, kPriority, &kConfig));
}

TEST_F(WebAudioClientBackendTests, ShouldCreateBackend)
{
    EXPECT_TRUE(createBackend());
}

TEST_F(WebAudioClientBackendTests, ShouldDestroyBackend)
{
    EXPECT_TRUE(createBackend());
    m_sut.destroyWebAudioBackend();
}

TEST_F(WebAudioClientBackendTests, ShouldPlay)
{
    EXPECT_CALL(*m_playerMock, play()).WillOnce(Return(true));
    ASSERT_TRUE(createBackend());
    EXPECT_TRUE(m_sut.play());
}

TEST_F(WebAudioClientBackendTests, ShouldPause)
{
    EXPECT_CALL(*m_playerMock, pause()).WillOnce(Return(true));
    ASSERT_TRUE(createBackend());
    EXPECT_TRUE(m_sut.pause());
}

TEST_F(WebAudioClientBackendTests, ShouldSetEos)
{
    EXPECT_CALL(*m_playerMock, setEos()).WillOnce(Return(true));
    ASSERT_TRUE(createBackend());
    EXPECT_TRUE(m_sut.setEos());
}

TEST_F(WebAudioClientBackendTests, ShouldGetBufferAvailable)
{
    uint32_t frames{0};
    EXPECT_CALL(*m_playerMock, getBufferAvailable(_, _)).WillOnce(DoAll(SetArgReferee<0>(kFrames), Return(true)));
    ASSERT_TRUE(createBackend());
    EXPECT_TRUE(m_sut.getBufferAvailable(frames));
    EXPECT_EQ(frames, kFrames);
}

TEST_F(WebAudioClientBackendTests, ShouldGetBufferDelay)
{
    uint32_t frames{0};
    EXPECT_CALL(*m_playerMock, getBufferDelay(_)).WillOnce(DoAll(SetArgReferee<0>(kFrames), Return(true)));
    ASSERT_TRUE(createBackend());
    EXPECT_TRUE(m_sut.getBufferDelay(frames));
    EXPECT_EQ(frames, kFrames);
}

TEST_F(WebAudioClientBackendTests, ShouldWriteBuffer)
{
    uint32_t data{0};
    EXPECT_CALL(*m_playerMock, writeBuffer(kFrames, &data)).WillOnce(Return(true));
    ASSERT_TRUE(createBackend());
    EXPECT_TRUE(m_sut.writeBuffer(kFrames, &data));
}

TEST_F(WebAudioClientBackendTests, ShouldGetDeviceInfo)
{
    uint32_t preferredFrames{0};
    uint32_t maximumFrames{0};
    bool supportDeferredPlay{false};
    constexpr uint32_t kMaximumFrames{12};
    constexpr bool kSupportDeferredPlay{true};
    EXPECT_CALL(*m_playerMock, getDeviceInfo(_, _, _))
        .WillOnce(DoAll(SetArgReferee<0>(kFrames), SetArgReferee<1>(kMaximumFrames),
                        SetArgReferee<2>(kSupportDeferredPlay), Return(true)));
    ASSERT_TRUE(createBackend());
    EXPECT_TRUE(m_sut.getDeviceInfo(preferredFrames, maximumFrames, supportDeferredPlay));
    EXPECT_EQ(preferredFrames, kFrames);
    EXPECT_EQ(maximumFrames, kMaximumFrames);
    EXPECT_EQ(supportDeferredPlay, kSupportDeferredPlay);
}

TEST_F(WebAudioClientBackendTests, ShouldSetVolume)
{
    EXPECT_CALL(*m_playerMock, setVolume(kVolume)).WillOnce(Return(true));
    ASSERT_TRUE(createBackend());
    EXPECT_TRUE(m_sut.setVolume(kVolume));
}

TEST_F(WebAudioClientBackendTests, ShouldGetVolume)
{
    double volume{0.0};
    EXPECT_CALL(*m_playerMock, getVolume(_)).WillOnce(DoAll(SetArgReferee<0>(kVolume), Return(true)));
    ASSERT_TRUE(createBackend());
    EXPECT_TRUE(m_sut.getVolume(volume));
    EXPECT_EQ(volume, kVolume);
}
