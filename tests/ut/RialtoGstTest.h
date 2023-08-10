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

#pragma once

#include "ControlMock.h"
#include "MediaPipelineMock.h"
#include "RialtoGStreamerMSEBaseSink.h"
#include <gtest/gtest.h>

class RialtoGstTest : public testing::Test
{
public:
    RialtoGstTest();
    ~RialtoGstTest() override;

    class ReceivedMessages
    {
        friend class RialtoGstTest;

    public:
        std::size_t size() const;
        bool empty() const;
        bool contains(const GstMessageType &type) const;

    private:
        std::vector<GstMessageType> m_receivedMessages;
    };

    RialtoMSEBaseSink *createAudioSink() const;
    RialtoMSEBaseSink *createVideoSink() const;
    GstElement *createPipelineWithSink(RialtoMSEBaseSink *sink) const;
    ReceivedMessages getMessages(GstElement *pipeline) const;
    int32_t audioSourceWillBeAttached(const firebolt::rialto::IMediaPipeline::MediaSourceAudio &mediaSource);
    void setPausedState(GstElement *pipeline, RialtoMSEBaseSink *sink);
    void setNullState(GstElement *pipeline, int32_t sourceId);
    void setCaps(RialtoMSEBaseSink *sink, GstCaps *caps) const;
    void sendPlaybackStateNotification(RialtoMSEBaseSink *sink, const firebolt::rialto::PlaybackState &state) const;
    void installAudioVideoStreamsProperty(GstElement *pipeline) const;

private:
    void expectSinksInitialisation() const;

protected:
    std::shared_ptr<testing::StrictMock<firebolt::rialto::ControlFactoryMock>> m_controlFactoryMock{
        std::dynamic_pointer_cast<testing::StrictMock<firebolt::rialto::ControlFactoryMock>>(
            firebolt::rialto::IControlFactory::createFactory())};
    std::shared_ptr<testing::StrictMock<firebolt::rialto::ControlMock>> m_controlMock{
        std::make_shared<testing::StrictMock<firebolt::rialto::ControlMock>>()};
    std::shared_ptr<testing::StrictMock<firebolt::rialto::MediaPipelineFactoryMock>> m_mediaPipelineFactoryMock{
        std::dynamic_pointer_cast<testing::StrictMock<firebolt::rialto::MediaPipelineFactoryMock>>(
            firebolt::rialto::IMediaPipelineFactory::createFactory())};
    std::unique_ptr<testing::StrictMock<firebolt::rialto::MediaPipelineMock>> m_mediaPipeline{
        std::make_unique<testing::StrictMock<firebolt::rialto::MediaPipelineMock>>()};
    testing::StrictMock<firebolt::rialto::MediaPipelineMock> &m_mediaPipelineMock{*m_mediaPipeline};
};
