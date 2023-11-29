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

#include "MediaPipelineMock.h"
#include "MediaPlayerManager.h"
#include <gst/gst.h>
#include <gtest/gtest.h>

using firebolt::rialto::IMediaPipelineFactory;
using firebolt::rialto::MediaPipelineFactoryMock;
using firebolt::rialto::MediaPipelineMock;
using testing::_;
using testing::ByMove;
using testing::Return;
using testing::StrictMock;

namespace
{
constexpr uint32_t kMaxVideoWidth{1920};
constexpr uint32_t kMaxVideoHeight{1080};
} // namespace

class MediaPlayerManagerTests : public testing::Test
{
public:
    MediaPlayerManagerTests() {}
    GstObject m_object{};
    std::shared_ptr<StrictMock<MediaPipelineFactoryMock>> m_mediaPipelineFactoryMock{
        std::dynamic_pointer_cast<StrictMock<MediaPipelineFactoryMock>>(IMediaPipelineFactory::createFactory())};
    std::unique_ptr<StrictMock<MediaPipelineMock>> m_mediaPipelineMock{std::make_unique<StrictMock<MediaPipelineMock>>()};
    MediaPlayerManager m_sut;
};

TEST_F(MediaPlayerManagerTests, ShouldFailToGetMediaPlayerClientWhenItsNotAttached)
{
    EXPECT_FALSE(m_sut.getMediaPlayerClient());
}

TEST_F(MediaPlayerManagerTests, ShouldNotHaveControlWhenClientIsNotAttached)
{
    EXPECT_FALSE(m_sut.hasControl());
}

TEST_F(MediaPlayerManagerTests, ShouldAttachMediaPlayerClient)
{
    EXPECT_CALL(*m_mediaPipelineMock, load(_, _, _)).WillOnce(Return(true));
    EXPECT_CALL(*m_mediaPipelineFactoryMock, createMediaPipeline(_, _))
        .WillOnce(Return(ByMove(std::move(m_mediaPipelineMock))));
    EXPECT_TRUE(m_sut.attachMediaPlayerClient(&m_object, kMaxVideoWidth, kMaxVideoHeight));
}

TEST_F(MediaPlayerManagerTests, ShouldFailToAttachMediaPlayerClient)
{
    EXPECT_CALL(*m_mediaPipelineFactoryMock, createMediaPipeline(_, _)).WillOnce(Return(nullptr));
    EXPECT_FALSE(m_sut.attachMediaPlayerClient(&m_object, kMaxVideoWidth, kMaxVideoHeight));
}

TEST_F(MediaPlayerManagerTests, ShouldAttachMediaPlayerClientForAnotherGstObject)
{
    EXPECT_CALL(*m_mediaPipelineMock, load(_, _, _)).WillOnce(Return(true));
    EXPECT_CALL(*m_mediaPipelineFactoryMock, createMediaPipeline(_, _))
        .WillOnce(Return(ByMove(std::move(m_mediaPipelineMock))));
    EXPECT_TRUE(m_sut.attachMediaPlayerClient(&m_object, kMaxVideoWidth, kMaxVideoHeight));

    m_mediaPipelineMock = std::make_unique<StrictMock<MediaPipelineMock>>();
    GstObject anotherObject{};
    EXPECT_CALL(*m_mediaPipelineMock, load(_, _, _)).WillOnce(Return(true));
    EXPECT_CALL(*m_mediaPipelineFactoryMock, createMediaPipeline(_, _))
        .WillOnce(Return(ByMove(std::move(m_mediaPipelineMock))));
    EXPECT_TRUE(m_sut.attachMediaPlayerClient(&anotherObject, kMaxVideoWidth, kMaxVideoHeight));
}

TEST_F(MediaPlayerManagerTests, ShouldHaveControl)
{
    EXPECT_CALL(*m_mediaPipelineMock, load(_, _, _)).WillOnce(Return(true));
    EXPECT_CALL(*m_mediaPipelineFactoryMock, createMediaPipeline(_, _))
        .WillOnce(Return(ByMove(std::move(m_mediaPipelineMock))));
    EXPECT_TRUE(m_sut.attachMediaPlayerClient(&m_object, kMaxVideoWidth, kMaxVideoHeight));
    EXPECT_TRUE(m_sut.hasControl());
}

TEST_F(MediaPlayerManagerTests, SecondMediaPlayerManagerShouldAttachMediaPlayerClient)
{
    EXPECT_CALL(*m_mediaPipelineMock, load(_, _, _)).WillOnce(Return(true));
    EXPECT_CALL(*m_mediaPipelineFactoryMock, createMediaPipeline(_, _))
        .WillOnce(Return(ByMove(std::move(m_mediaPipelineMock))));
    EXPECT_TRUE(m_sut.attachMediaPlayerClient(&m_object, kMaxVideoWidth, kMaxVideoHeight));
    MediaPlayerManager secondSut;
    EXPECT_TRUE(secondSut.attachMediaPlayerClient(&m_object, kMaxVideoWidth, kMaxVideoHeight));
}

TEST_F(MediaPlayerManagerTests, SecondMediaPlayerManagerShouldFailToAcquireControl)
{
    EXPECT_CALL(*m_mediaPipelineMock, load(_, _, _)).WillOnce(Return(true));
    EXPECT_CALL(*m_mediaPipelineFactoryMock, createMediaPipeline(_, _))
        .WillOnce(Return(ByMove(std::move(m_mediaPipelineMock))));
    EXPECT_TRUE(m_sut.attachMediaPlayerClient(&m_object, kMaxVideoWidth, kMaxVideoHeight));
    EXPECT_TRUE(m_sut.hasControl());
    MediaPlayerManager secondSut;
    EXPECT_TRUE(secondSut.attachMediaPlayerClient(&m_object, kMaxVideoWidth, kMaxVideoHeight));
    EXPECT_FALSE(secondSut.hasControl());
}

TEST_F(MediaPlayerManagerTests, ShouldAcquireControl)
{
    {
        MediaPlayerManager secondSut;
        EXPECT_CALL(*m_mediaPipelineMock, load(_, _, _)).WillOnce(Return(true));
        EXPECT_CALL(*m_mediaPipelineFactoryMock, createMediaPipeline(_, _))
            .WillOnce(Return(ByMove(std::move(m_mediaPipelineMock))));
        EXPECT_TRUE(secondSut.attachMediaPlayerClient(&m_object, kMaxVideoWidth, kMaxVideoHeight));
        EXPECT_TRUE(secondSut.hasControl());
        EXPECT_TRUE(m_sut.attachMediaPlayerClient(&m_object, kMaxVideoWidth, kMaxVideoHeight));
    }
    EXPECT_TRUE(m_sut.hasControl());
}
