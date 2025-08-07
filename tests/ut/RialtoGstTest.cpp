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
#include "PlaybinStub.h"
#include "RialtoGSteamerPlugin.cpp"
#include "RialtoGStreamerMSEBaseSinkPrivate.h"

#include <gst/gst.h>

#include <algorithm>
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
using testing::SaveArg;
using testing::SetArgReferee;
using testing::StrictMock;

namespace
{
constexpr bool kHasDrm{true};
constexpr int kChannels{1};
constexpr int kRate{48000};
const firebolt::rialto::AudioConfig kAudioConfig{kChannels, kRate, {}};
const std::vector<std::string> kSupportedAudioMimeTypes{"audio/mp4",    "audio/mp3",   "audio/aac",   "audio/x-eac3",
                                                        "audio/x-opus", "audio/b-wav", "audio/x-raw", "audio/x-flac"};
const std::vector<std::string> kSupportedVideoMimeTypes{"video/h264", "video/h265", "video/x-av1", "video/x-vp9",
                                                        "video/unsupported"};
const std::vector<std::string> kSupportedSubtitlesMimeTypes{"text/vtt", "text/ttml"};
constexpr firebolt::rialto::VideoRequirements kDefaultRequirements{3840, 2160};
int32_t generateSourceId()
{
    static int32_t sourceId{0};
    return sourceId++;
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
MATCHER_P(MediaSourceSubtitleMatcher, mediaSource, "")
{
    try
    {
        auto &matchedSource{dynamic_cast<firebolt::rialto::IMediaPipeline::MediaSourceSubtitle &>(*arg)};
        return matchedSource.getType() == mediaSource.getType() &&
               matchedSource.getMimeType() == mediaSource.getMimeType() &&
               matchedSource.getHasDrm() == mediaSource.getHasDrm() &&
               matchedSource.getTextTrackIdentifier() == mediaSource.getTextTrackIdentifier();
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
                       EXPECT_CALL(*m_clientLogControlFactoryMock, createClientLogControl())
                           .WillOnce(ReturnRef(m_clientLogControlMock));
                       EXPECT_CALL(m_clientLogControlMock, registerLogHandler(_, _)).WillOnce(Return(true));
                       expectSinksInitialisation();
                       gst_init(nullptr, nullptr);
                       const auto registerResult =
                           gst_plugin_register_static(GST_VERSION_MAJOR, GST_VERSION_MINOR, "rialtosinks",
                                                      "Sinks which communicate with RialtoServer", rialto_mse_sinks_init,
                                                      "1.0", "LGPL", PACKAGE, PACKAGE, "http://gstreamer.net/");
                       EXPECT_TRUE(registerResult);
                       EXPECT_TRUE(register_play_bin_stub());
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

GstCaps *RialtoGstTest::createAudioCaps() const
{
    return gst_caps_new_simple("audio/mpeg", "mpegversion", G_TYPE_INT, 4, "channels", G_TYPE_INT, kChannels, "rate",
                               G_TYPE_INT, kRate, nullptr);
}

GstCaps *RialtoGstTest::createVideoCaps() const
{
    return gst_caps_new_empty_simple("video/x-h264");
}

RialtoMSEBaseSink *RialtoGstTest::createAudioSink() const
{
    EXPECT_CALL(*m_controlFactoryMock, createControl()).WillOnce(Return(m_controlMock));
    EXPECT_CALL(*m_controlMock, registerClient(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(ApplicationState::RUNNING), Return(true)));
    GstElement *audioSink = gst_element_factory_make("rialtomseaudiosink", "rialtomseaudiosink");
    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(audioSink, GST_STATE_READY));
    return RIALTO_MSE_BASE_SINK(audioSink);
}

RialtoMSEBaseSink *RialtoGstTest::createVideoSink() const
{
    EXPECT_CALL(*m_controlFactoryMock, createControl()).WillOnce(Return(m_controlMock));
    EXPECT_CALL(*m_controlMock, registerClient(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(ApplicationState::RUNNING), Return(true)));
    GstElement *videoSink = gst_element_factory_make("rialtomsevideosink", "rialtomsevideosink");
    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(videoSink, GST_STATE_READY));
    return RIALTO_MSE_BASE_SINK(videoSink);
}

RialtoMSEBaseSink *RialtoGstTest::createSubtitleSink() const
{
    EXPECT_CALL(*m_controlFactoryMock, createControl()).WillOnce(Return(m_controlMock));
    EXPECT_CALL(*m_controlMock, registerClient(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(ApplicationState::RUNNING), Return(true)));
    GstElement *videoSink = gst_element_factory_make("rialtomsesubtitlesink", "rialtomsesubtitlesink");
    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(videoSink, GST_STATE_READY));
    return RIALTO_MSE_BASE_SINK(videoSink);
}

RialtoWebAudioSink *RialtoGstTest::createWebAudioSink() const
{
    EXPECT_CALL(*m_controlFactoryMock, createControl()).WillOnce(Return(m_controlMock));
    EXPECT_CALL(*m_controlMock, registerClient(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(ApplicationState::RUNNING), Return(true)));
    GstElement *webAudioSink = gst_element_factory_make("rialtowebaudiosink", "rialtowebaudiosink");
    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(webAudioSink, GST_STATE_READY));
    return RIALTO_WEB_AUDIO_SINK(webAudioSink);
}

GstElement *RialtoGstTest::createPlaybin2WithSink(RialtoMSEBaseSink *sink) const
{
    GstElement *playbin = gst_element_factory_make("playbinstub", "test-playbin");
    gst_bin_add(GST_BIN(playbin), GST_ELEMENT_CAST(sink));
    return playbin;
}

GstElement *RialtoGstTest::createPipelineWithSink(RialtoMSEBaseSink *sink) const
{
    GstElement *pipeline = gst_pipeline_new("test-pipeline");
    g_object_set(sink, "single-path-stream", true, nullptr);
    g_object_set(sink, "streams-number", 1, nullptr);
    gst_bin_add(GST_BIN(pipeline), GST_ELEMENT_CAST(sink));
    return pipeline;
}

GstElement *RialtoGstTest::createPipelineWithSink(RialtoWebAudioSink *sink) const
{
    GstElement *pipeline = gst_pipeline_new("test-pipeline");
    gst_bin_add(GST_BIN(pipeline), GST_ELEMENT_CAST(sink));
    return pipeline;
}

TestContext RialtoGstTest::createPipelineWithAudioSinkAndSetToPaused()
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);
    gst_caps_unref(caps);

    return TestContext{pipeline, audioSink, kSourceId};
}

TestContext RialtoGstTest::createPipelineWithVideoSinkAndSetToPaused()
{
    RialtoMSEBaseSink *videoSink = createVideoSink();
    GstElement *pipeline = createPipelineWithSink(videoSink);

    setPausedState(pipeline, videoSink);
    const int32_t kSourceId{videoSourceWillBeAttached(createVideoMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createVideoCaps()};
    setCaps(videoSink, caps);
    gst_caps_unref(caps);

    return TestContext{pipeline, videoSink, kSourceId};
}

firebolt::rialto::IMediaPipeline::MediaSourceAudio RialtoGstTest::createAudioMediaSource() const
{
    return firebolt::rialto::IMediaPipeline::MediaSourceAudio{"audio/mp4", kHasDrm, kAudioConfig};
}

firebolt::rialto::IMediaPipeline::MediaSourceVideo RialtoGstTest::createVideoMediaSource() const
{
    return firebolt::rialto::IMediaPipeline::MediaSourceVideo{"video/h264"};
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

GstMessage *RialtoGstTest::getMessage(GstElement *pipeline, const GstMessageType &messageType) const
{
    constexpr GstClockTime kTimeout{1000000000}; // 1 second
    GstBus *bus = gst_element_get_bus(pipeline);
    if (!bus)
    {
        return nullptr;
    }
    GstMessage *msg{gst_bus_timed_pop_filtered(bus, kTimeout, messageType)};
    gst_object_unref(bus);
    return msg;
}

void RialtoGstTest::allSourcesWillBeAttached() const
{
    EXPECT_CALL(m_mediaPipelineMock, allSourcesAttached()).WillRepeatedly(Return(true));
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

int32_t
RialtoGstTest::subtitleSourceWillBeAttached(const firebolt::rialto::IMediaPipeline::MediaSourceSubtitle &mediaSource) const
{
    const int32_t kSourceId{generateSourceId()};
    EXPECT_CALL(m_mediaPipelineMock, attachSource(MediaSourceSubtitleMatcher(mediaSource)))
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

void RialtoGstTest::load(GstElement *pipeline)
{
    constexpr firebolt::rialto::MediaType kMediaType{firebolt::rialto::MediaType::MSE};
    const std::string kMimeType{};
    const std::string kUrl{"mse://1"};
    EXPECT_CALL(m_mediaPipelineMock, load(kMediaType, kMimeType, kUrl)).WillOnce(Return(true));
    EXPECT_CALL(*m_mediaPipelineFactoryMock, createMediaPipeline(_, kDefaultRequirements))
        .WillOnce(DoAll(SaveArg<0>(&m_mediaPipelineClient), Return(ByMove(std::move(m_mediaPipeline)))));
}

void RialtoGstTest::setPausedState(GstElement *pipeline, RialtoMSEBaseSink *sink)
{
    load(pipeline);
    EXPECT_CALL(m_mediaPipelineMock, pause()).WillOnce(Return(true));
    EXPECT_EQ(GST_STATE_CHANGE_ASYNC, gst_element_set_state(pipeline, GST_STATE_PAUSED));
}

void RialtoGstTest::setPlayingState(GstElement *pipeline) const
{
    EXPECT_CALL(m_mediaPipelineMock, play()).WillOnce(Return(true));
    EXPECT_EQ(GST_STATE_CHANGE_ASYNC, gst_element_set_state(pipeline, GST_STATE_PLAYING));
}

void RialtoGstTest::setNullState(GstElement *pipeline, int32_t sourceId) const
{
    EXPECT_CALL(m_mediaPipelineMock, removeSource(sourceId)).WillRepeatedly(Return(true));
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
    auto mediaPipelineClient = m_mediaPipelineClient.lock();
    ASSERT_TRUE(mediaPipelineClient);
    mediaPipelineClient->notifyPlaybackState(state);
}

void RialtoGstTest::expectSinksInitialisation() const
{
    // Media Pipeline Capabilities will be created two times during class_init of audio and video sink
    std::unique_ptr<StrictMock<MediaPipelineCapabilitiesMock>> capabilitiesMockAudio{
        std::make_unique<StrictMock<MediaPipelineCapabilitiesMock>>()};
    std::unique_ptr<StrictMock<MediaPipelineCapabilitiesMock>> capabilitiesMockVideo{
        std::make_unique<StrictMock<MediaPipelineCapabilitiesMock>>()};
    std::unique_ptr<StrictMock<MediaPipelineCapabilitiesMock>> capabilitiesMockSubtitles{
        std::make_unique<StrictMock<MediaPipelineCapabilitiesMock>>()};
    EXPECT_CALL(*capabilitiesMockAudio, getSupportedMimeTypes(firebolt::rialto::MediaSourceType::AUDIO))
        .WillOnce(Return(kSupportedAudioMimeTypes));
    EXPECT_CALL(*capabilitiesMockVideo, getSupportedMimeTypes(firebolt::rialto::MediaSourceType::VIDEO))
        .WillOnce(Return(kSupportedVideoMimeTypes));
    EXPECT_CALL(*capabilitiesMockSubtitles, getSupportedMimeTypes(firebolt::rialto::MediaSourceType::SUBTITLE))
        .WillOnce(Return(kSupportedSubtitlesMimeTypes));
    EXPECT_CALL(*capabilitiesMockVideo, getSupportedProperties(firebolt::rialto::MediaSourceType::VIDEO, _))
        .WillOnce(Invoke(
            [&](firebolt::rialto::MediaSourceType source, const std::vector<std::string> &propertiesToSearch)
            {
                return propertiesToSearch; // Mock that all are supported
            }));
    EXPECT_CALL(*capabilitiesMockAudio,
                getSupportedProperties(firebolt::rialto::MediaSourceType::AUDIO, _)) // TODO check props
        .WillOnce(Invoke(
            [&](firebolt::rialto::MediaSourceType source, const std::vector<std::string> &propertiesToSearch)
            {
                return propertiesToSearch; // Mock that all are supported
            }));

    std::shared_ptr<StrictMock<MediaPipelineCapabilitiesFactoryMock>> capabilitiesFactoryMock{
        std::dynamic_pointer_cast<StrictMock<MediaPipelineCapabilitiesFactoryMock>>(
            IMediaPipelineCapabilitiesFactory::createFactory())};
    ASSERT_TRUE(capabilitiesFactoryMock);
    // Video sink is registered first
    EXPECT_CALL(*capabilitiesFactoryMock, createMediaPipelineCapabilities())
        .WillOnce(Return(ByMove(std::move(capabilitiesMockVideo))))
        .WillOnce(Return(ByMove(std::move(capabilitiesMockAudio))))
        .WillOnce(Return(ByMove(std::move(capabilitiesMockSubtitles))));
}
