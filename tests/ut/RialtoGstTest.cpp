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

#include "RialtoGstTest.h"
#include "Matchers.h"
#include "MediaPipelineCapabilitiesMock.h"
#include "RialtoGSteamerPlugin.cpp" // urgh... disgusting!
#include "RialtoGStreamerMSEBaseSinkPrivate.h"
#include <algorithm>
#include <gst/gst.h>
#include <string>
#include <vector>

using firebolt::rialto::ApplicationState;
using firebolt::rialto::IMediaPipelineCapabilitiesFactory;
using firebolt::rialto::MediaPipelineCapabilitiesFactoryMock;
using firebolt::rialto::MediaPipelineCapabilitiesMock;
using testing::_;
using testing::ByMove;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::SetArgReferee;
using testing::StrictMock;

namespace
{
const std::vector<std::string> kSupportedAudioMimeTypes{"audio/mp4", "audio/aac", "audio/x-eac3", "audio/x-opus"};
const std::vector<std::string> kSupportedVideoMimeTypes{"video/h264", "video/h265", "video/x-av1", "video/x-vp9",
                                                        "video/unsupported"};
constexpr firebolt::rialto::VideoRequirements kDefaultRequirements{3840, 2160};
int32_t generateSourceId()
{
    static int32_t sourceId{0};
    return sourceId++;
}
bool matchCodecData(const std::shared_ptr<firebolt::rialto::CodecData> &lhs,
                    const std::shared_ptr<firebolt::rialto::CodecData> &rhs)
{
    if (lhs == rhs) // If ptrs are both null or point to the same objects
    {
        return true;
    }
    if (lhs && rhs)
    {
        return lhs->data == rhs->data && lhs->type == rhs->type;
    }
    return false;
}

MATCHER_P(MediaSourceAudioMatcher, mediaSource, "")
{
    try
    {
        auto &matchedSource{dynamic_cast<firebolt::rialto::IMediaPipeline::MediaSourceAudio &>(*arg)};
        return matchedSource.getType() == mediaSource.getType() &&
               matchedSource.getMimeType() == mediaSource.getMimeType() &&
               matchedSource.getHasDrm() == mediaSource.getHasDrm() &&
               matchedSource.getAudioConfig() == mediaSource.getAudioConfig() &&
               matchedSource.getSegmentAlignment() == mediaSource.getSegmentAlignment() &&
               matchedSource.getStreamFormat() == mediaSource.getStreamFormat() &&
               matchCodecData(matchedSource.getCodecData(), mediaSource.getCodecData()) &&
               matchedSource.getConfigType() == mediaSource.getConfigType();
    }
    catch (std::exception &)
    {
        return false;
    }
}
MATCHER_P(MediaSourceVideoMatcher, mediaSource, "")
{
    try
    {
        auto &matchedSource{dynamic_cast<firebolt::rialto::IMediaPipeline::MediaSourceVideo &>(*arg)};
        return matchedSource.getType() == mediaSource.getType() &&
               matchedSource.getMimeType() == mediaSource.getMimeType() &&
               matchedSource.getHasDrm() == mediaSource.getHasDrm() &&
               matchedSource.getWidth() == mediaSource.getWidth() &&
               matchedSource.getHeight() == mediaSource.getHeight() &&
               matchedSource.getSegmentAlignment() == mediaSource.getSegmentAlignment() &&
               matchedSource.getStreamFormat() == mediaSource.getStreamFormat() &&
               matchedSource.getCodecData() == mediaSource.getCodecData() &&
               matchedSource.getConfigType() == mediaSource.getConfigType();
    }
    catch (std::exception &)
    {
        return false;
    }
}
MATCHER_P(MediaSourceDolbyVisionMatcher, mediaSource, "")
{
    try
    {
        auto &matchedSource{dynamic_cast<firebolt::rialto::IMediaPipeline::MediaSourceVideoDolbyVision &>(*arg)};
        return matchedSource.getType() == mediaSource.getType() &&
               matchedSource.getMimeType() == mediaSource.getMimeType() &&
               matchedSource.getHasDrm() == mediaSource.getHasDrm() &&
               matchedSource.getWidth() == mediaSource.getWidth() &&
               matchedSource.getHeight() == mediaSource.getHeight() &&
               matchedSource.getDolbyVisionProfile() == mediaSource.getDolbyVisionProfile() &&
               matchedSource.getSegmentAlignment() == mediaSource.getSegmentAlignment() &&
               matchedSource.getStreamFormat() == mediaSource.getStreamFormat() &&
               matchedSource.getCodecData() == mediaSource.getCodecData() &&
               matchedSource.getConfigType() == mediaSource.getConfigType();
    }
    catch (std::exception &)
    {
        return false;
    }
}
} // namespace

RialtoGstTest::RialtoGstTest()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag,
                   [this]()
                   {
                       expectSinksInitialisation();
                       gst_init(nullptr, nullptr);
                       const auto registerResult =
                           gst_plugin_register_static(GST_VERSION_MAJOR, GST_VERSION_MINOR, "rialtosinks",
                                                      "Sinks which communicate with RialtoServer", rialto_mse_sinks_init,
                                                      "1.0", "LGPL", PACKAGE, PACKAGE, "http://gstreamer.net/");
                       EXPECT_TRUE(registerResult);
                   });
}

RialtoGstTest::~RialtoGstTest()
{
    testing::Mock::VerifyAndClearExpectations(&m_controlFactoryMock);
}

std::size_t RialtoGstTest::ReceivedMessages::size() const
{
    return m_receivedMessages.size();
}

bool RialtoGstTest::ReceivedMessages::empty() const
{
    return m_receivedMessages.empty();
}

bool RialtoGstTest::ReceivedMessages::contains(const GstMessageType &type) const
{
    return std::find(m_receivedMessages.begin(), m_receivedMessages.end(), type) != m_receivedMessages.end();
}

RialtoMSEBaseSink *RialtoGstTest::createAudioSink() const
{
    EXPECT_CALL(*m_controlFactoryMock, createControl()).WillOnce(Return(m_controlMock));
    EXPECT_CALL(*m_controlMock, registerClient(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(ApplicationState::RUNNING), Return(true)));
    GstElement *audioSink = gst_element_factory_make("rialtomseaudiosink", "rialtomseaudiosink");
    return RIALTO_MSE_BASE_SINK(audioSink);
}

RialtoMSEBaseSink *RialtoGstTest::createVideoSink() const
{
    EXPECT_CALL(*m_controlFactoryMock, createControl()).WillOnce(Return(m_controlMock));
    EXPECT_CALL(*m_controlMock, registerClient(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(ApplicationState::RUNNING), Return(true)));
    GstElement *videoSink = gst_element_factory_make("rialtomsevideosink", "rialtomsevideosink");
    return RIALTO_MSE_BASE_SINK(videoSink);
}

RialtoWebAudioSink *RialtoGstTest::createWebAudioSink() const
{
    EXPECT_CALL(*m_controlFactoryMock, createControl()).WillOnce(Return(m_controlMock));
    EXPECT_CALL(*m_controlMock, registerClient(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(ApplicationState::RUNNING), Return(true)));
    GstElement *webAudioSink = gst_element_factory_make("rialtowebaudiosink", "rialtowebaudiosink");
    return RIALTO_WEB_AUDIO_SINK(webAudioSink);
}

GstElement *RialtoGstTest::createPipelineWithSink(RialtoMSEBaseSink *sink) const
{
    GstElement *pipeline = gst_pipeline_new("test-pipeline");
    gst_bin_add(GST_BIN(pipeline), GST_ELEMENT_CAST(sink));
    return pipeline;
}

GstElement *RialtoGstTest::createPipelineWithSink(RialtoWebAudioSink *sink) const
{
    GstElement *pipeline = gst_pipeline_new("test-pipeline");
    gst_bin_add(GST_BIN(pipeline), GST_ELEMENT_CAST(sink));
    return pipeline;
}

RialtoGstTest::ReceivedMessages RialtoGstTest::getMessages(GstElement *pipeline) const
{
    RialtoGstTest::ReceivedMessages result;
    GstBus *bus = gst_element_get_bus(pipeline);
    if (!bus)
    {
        return result;
    }
    GstMessage *msg{gst_bus_pop(bus)};
    while (msg)
    {
        result.m_receivedMessages.push_back(GST_MESSAGE_TYPE(msg));
        gst_message_unref(msg);
        msg = gst_bus_pop(bus);
    }
    gst_object_unref(bus);
    return result;
}

bool RialtoGstTest::waitForMessage(GstElement *pipeline, const GstMessageType &messageType) const
{
    constexpr GstClockTime kTimeout{1000000000}; // 1 second
    GstBus *bus = gst_element_get_bus(pipeline);
    if (!bus)
    {
        return false;
    }
    bool result{false};
    GstMessage *msg{gst_bus_timed_pop_filtered(bus, kTimeout, messageType)};
    if (msg)
    {
        result = true;
        gst_message_unref(msg);
    }
    gst_object_unref(bus);
    return result;
}

int32_t RialtoGstTest::audioSourceWillBeAttached(const firebolt::rialto::IMediaPipeline::MediaSourceAudio &mediaSource) const
{
    const int32_t kSourceId{generateSourceId()};
    EXPECT_CALL(m_mediaPipelineMock, attachSource(MediaSourceAudioMatcher(mediaSource)))
        .WillOnce(Invoke(
            [=](auto &source)
            {
                source->setId(kSourceId);
                return true;
            }));
    return kSourceId;
}

int32_t RialtoGstTest::videoSourceWillBeAttached(const firebolt::rialto::IMediaPipeline::MediaSourceVideo &mediaSource) const
{
    const int32_t kSourceId{generateSourceId()};
    EXPECT_CALL(m_mediaPipelineMock, attachSource(MediaSourceVideoMatcher(mediaSource)))
        .WillOnce(Invoke(
            [=](auto &source)
            {
                source->setId(kSourceId);
                return true;
            }));
    return kSourceId;
}

int32_t RialtoGstTest::dolbyVisionSourceWillBeAttached(
    const firebolt::rialto::IMediaPipeline::MediaSourceVideoDolbyVision &mediaSource) const
{
    const int32_t kSourceId{generateSourceId()};
    EXPECT_CALL(m_mediaPipelineMock, attachSource(MediaSourceVideoMatcher(mediaSource)))
        .WillOnce(Invoke(
            [=](auto &source)
            {
                source->setId(kSourceId);
                return true;
            }));
    return kSourceId;
}

void RialtoGstTest::setPausedState(GstElement *pipeline, RialtoMSEBaseSink *sink)
{
    constexpr firebolt::rialto::MediaType kMediaType{firebolt::rialto::MediaType::MSE};
    const std::string kMimeType{};
    const std::string kUrl{"mse://1"};
    EXPECT_CALL(m_mediaPipelineMock, load(kMediaType, kMimeType, kUrl)).WillOnce(Return(true));
    EXPECT_CALL(m_mediaPipelineMock, pause()).WillOnce(Return(true));
    EXPECT_CALL(*m_mediaPipelineFactoryMock, createMediaPipeline(_, kDefaultRequirements))
        .WillOnce(Return(ByMove(std::move(m_mediaPipeline))));
    EXPECT_EQ(GST_STATE_CHANGE_ASYNC, gst_element_set_state(pipeline, GST_STATE_PAUSED));
}

void RialtoGstTest::setPlayingState(GstElement *pipeline) const
{
    EXPECT_CALL(m_mediaPipelineMock, play()).WillOnce(Return(true));
    EXPECT_EQ(GST_STATE_CHANGE_ASYNC, gst_element_set_state(pipeline, GST_STATE_PLAYING));
}

void RialtoGstTest::setNullState(GstElement *pipeline, int32_t sourceId) const
{
    EXPECT_CALL(m_mediaPipelineMock, removeSource(sourceId)).WillOnce(Return(true));
    EXPECT_CALL(m_mediaPipelineMock, stop()).WillOnce(Return(true));
    gst_element_set_state(pipeline, GST_STATE_NULL);
}

void RialtoGstTest::pipelineWillGoToPausedState(RialtoMSEBaseSink *sink) const
{
    EXPECT_CALL(m_mediaPipelineMock, pause())
        .WillOnce(Invoke(
            [=]()
            {
                sendPlaybackStateNotification(sink, firebolt::rialto::PlaybackState::PAUSED);
                return true;
            }));
}

void RialtoGstTest::setCaps(RialtoMSEBaseSink *sink, GstCaps *caps) const
{
    gst_pad_send_event(sink->priv->m_sinkPad, gst_event_new_caps(caps));
}

void RialtoGstTest::setCaps(RialtoWebAudioSink *sink, GstCaps *caps) const
{
    GstPad *sinkPad = gst_element_get_static_pad(GST_ELEMENT_CAST(sink), "sink");
    ASSERT_TRUE(sinkPad);
    gst_pad_send_event(sinkPad, gst_event_new_caps(caps));
    gst_object_unref(sinkPad);
}

void RialtoGstTest::sendPlaybackStateNotification(RialtoMSEBaseSink *sink,
                                                  const firebolt::rialto::PlaybackState &state) const
{
    auto mediaPlayerClient{sink->priv->m_mediaPlayerManager.getMediaPlayerClient()};
    ASSERT_TRUE(mediaPlayerClient);
    mediaPlayerClient->handlePlaybackStateChange(state);
}

void RialtoGstTest::installAudioVideoStreamsProperty(GstElement *pipeline) const
{
    static std::once_flag flag;
    std::call_once(flag,
                   [&]()
                   {
                       g_object_class_install_property(G_OBJECT_GET_CLASS(pipeline), 123,
                                                       g_param_spec_int("n-video", "n-video", "num of video streams", 1,
                                                                        G_MAXINT, 1, GParamFlags(G_PARAM_READWRITE)));
                       g_object_class_install_property(G_OBJECT_GET_CLASS(pipeline), 124,
                                                       g_param_spec_int("n-audio", "n-audio", "num of audio streams", 1,
                                                                        G_MAXINT, 1, GParamFlags(G_PARAM_READWRITE)));

                       g_object_class_install_property(G_OBJECT_GET_CLASS(pipeline), 124,
                                                       g_param_spec_uint("flags", "flags", "flags", 1, G_MAXINT, 1,
                                                                         GParamFlags(G_PARAM_READWRITE)));
                   });
}

void RialtoGstTest::expectSinksInitialisation() const
{
    // Media Pipeline Capabilities will be created two times during class_init of audio and video sink
    std::unique_ptr<StrictMock<MediaPipelineCapabilitiesMock>> capabilitiesMockAudio{
        std::make_unique<StrictMock<MediaPipelineCapabilitiesMock>>()};
    std::unique_ptr<StrictMock<MediaPipelineCapabilitiesMock>> capabilitiesMockVideo{
        std::make_unique<StrictMock<MediaPipelineCapabilitiesMock>>()};
    EXPECT_CALL(*capabilitiesMockAudio, getSupportedMimeTypes(firebolt::rialto::MediaSourceType::AUDIO))
        .WillOnce(Return(kSupportedAudioMimeTypes));
    EXPECT_CALL(*capabilitiesMockVideo, getSupportedMimeTypes(firebolt::rialto::MediaSourceType::VIDEO))
        .WillOnce(Return(kSupportedVideoMimeTypes));
    std::shared_ptr<StrictMock<MediaPipelineCapabilitiesFactoryMock>> capabilitiesFactoryMock{
        std::dynamic_pointer_cast<StrictMock<MediaPipelineCapabilitiesFactoryMock>>(
            IMediaPipelineCapabilitiesFactory::createFactory())};
    ASSERT_TRUE(capabilitiesFactoryMock);
    // Video sink is registered first
    EXPECT_CALL(*capabilitiesFactoryMock, createMediaPipelineCapabilities())
        .WillOnce(Return(ByMove(std::move(capabilitiesMockVideo))))
        .WillOnce(Return(ByMove(std::move(capabilitiesMockAudio))));
}
