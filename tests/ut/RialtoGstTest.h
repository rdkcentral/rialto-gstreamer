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

#include <gtest/gtest.h>

#include "ClientLogControlMock.h"
#include "ControlMock.h"
#include "MediaPipelineMock.h"
#include "RialtoGStreamerMSEBaseSink.h"
#include "RialtoGStreamerWebAudioSink.h"

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
    RialtoWebAudioSink *createWebAudioSink() const;
    GstElement *createPipelineWithSink(RialtoMSEBaseSink *sink) const;
    GstElement *createPipelineWithSink(RialtoWebAudioSink *sink) const;
    ReceivedMessages getMessages(GstElement *pipeline) const;
    bool waitForMessage(GstElement *pipeline, const GstMessageType &messageType) const;
    GstMessage *getMessage(GstElement *pipeline, const GstMessageType &messageType) const;
    int32_t audioSourceWillBeAttached(const firebolt::rialto::IMediaPipeline::MediaSourceAudio &mediaSource) const;
    int32_t videoSourceWillBeAttached(const firebolt::rialto::IMediaPipeline::MediaSourceVideo &mediaSource) const;
    int32_t dolbyVisionSourceWillBeAttached(
        const firebolt::rialto::IMediaPipeline::MediaSourceVideoDolbyVision &mediaSource) const;
    void setPausedState(GstElement *pipeline, RialtoMSEBaseSink *sink);
    void setPlayingState(GstElement *pipeline) const;
    void setNullState(GstElement *pipeline, int32_t sourceId) const;
    void pipelineWillGoToPausedState(RialtoMSEBaseSink *sink) const;
    void setCaps(RialtoMSEBaseSink *sink, GstCaps *caps) const;
    void setCaps(RialtoWebAudioSink *sink, GstCaps *caps) const;
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

    std::shared_ptr<testing::StrictMock<firebolt::rialto::ClientLogControlFactoryMock>> m_clientLogControlFactoryMock{
        std::dynamic_pointer_cast<testing::StrictMock<firebolt::rialto::ClientLogControlFactoryMock>>(
            firebolt::rialto::IClientLogControlFactory::createFactory())};
    testing::StrictMock<firebolt::rialto::ClientLogControlMock> m_clientLogControlMock;

    std::shared_ptr<testing::StrictMock<firebolt::rialto::MediaPipelineFactoryMock>> m_mediaPipelineFactoryMock{
        std::dynamic_pointer_cast<testing::StrictMock<firebolt::rialto::MediaPipelineFactoryMock>>(
            firebolt::rialto::IMediaPipelineFactory::createFactory())};
    std::unique_ptr<testing::StrictMock<firebolt::rialto::MediaPipelineMock>> m_mediaPipeline{
        std::make_unique<testing::StrictMock<firebolt::rialto::MediaPipelineMock>>()};
    testing::StrictMock<firebolt::rialto::MediaPipelineMock> &m_mediaPipelineMock{*m_mediaPipeline};
};
