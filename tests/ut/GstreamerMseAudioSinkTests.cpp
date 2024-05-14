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
#include "RialtoGStreamerMSEBaseSinkPrivate.h"
#include "RialtoGstTest.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgReferee;

namespace
{
constexpr int32_t kUnknownSourceId{-1};
constexpr bool kHasDrm{true};
constexpr int kChannels{1};
constexpr int kRate{48000};
const firebolt::rialto::AudioConfig kAudioConfig{kChannels, kRate, {}};
} // namespace

class GstreamerMseAudioSinkTests : public RialtoGstTest
{
public:
    GstreamerMseAudioSinkTests() = default;
    ~GstreamerMseAudioSinkTests() override = default;

    GstCaps *createDefaultCaps() const
    {
        return gst_caps_new_simple("audio/mpeg", "mpegversion", G_TYPE_INT, 4, "channels", G_TYPE_INT, kChannels,
                                   "rate", G_TYPE_INT, kRate, nullptr);
    }

    firebolt::rialto::IMediaPipeline::MediaSourceAudio createDefaultMediaSource() const
    {
        return firebolt::rialto::IMediaPipeline::MediaSourceAudio{"audio/mp4", kHasDrm, kAudioConfig};
    }
};

TEST_F(GstreamerMseAudioSinkTests, ShouldFailToReachPausedStateWhenMediaPipelineCantBeCreated)
{
    constexpr firebolt::rialto::VideoRequirements kDefaultRequirements{3840, 2160};
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    EXPECT_CALL(*m_mediaPipelineFactoryMock, createMediaPipeline(_, kDefaultRequirements)).WillOnce(Return(nullptr));
    EXPECT_EQ(GST_STATE_CHANGE_FAILURE, gst_element_set_state(pipeline, GST_STATE_PAUSED));
    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_NULL));

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldNotHandleUnknownEvent)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_READY));

    gst_pad_set_active(audioSink->priv->m_sinkPad, TRUE);
    gst_pad_send_event(audioSink->priv->m_sinkPad, gst_event_new_gap(1, 1));

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_NULL));

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldNotAttachSourceWhenPipelineIsBelowPausedState)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_READY));

    gst_pad_set_active(audioSink->priv->m_sinkPad, TRUE);
    GstCaps *caps{createDefaultCaps()};
    setCaps(audioSink, caps);

    EXPECT_FALSE(audioSink->priv->m_sourceAttached);

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_NULL));

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldNotAttachSourceTwice)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createDefaultMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createDefaultCaps()};
    setCaps(audioSink, caps);
    setCaps(audioSink, caps);

    EXPECT_TRUE(audioSink->priv->m_sourceAttached);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldAttachSourceWithMpeg)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createDefaultMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createDefaultCaps()};
    setCaps(audioSink, caps);

    EXPECT_TRUE(audioSink->priv->m_sourceAttached);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldAttachSourceWithEac3)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);
    const firebolt::rialto::IMediaPipeline::MediaSourceAudio kExpectedSource{"audio/x-eac3", kHasDrm, kAudioConfig};
    const int32_t kSourceId{audioSourceWillBeAttached(kExpectedSource)};
    allSourcesWillBeAttached();

    GstCaps *caps{gst_caps_new_simple("audio/x-eac3", "mpegversion", G_TYPE_INT, 2, "channels", G_TYPE_INT, kChannels,
                                      "rate", G_TYPE_INT, kRate, nullptr)};
    setCaps(audioSink, caps);

    EXPECT_TRUE(audioSink->priv->m_sourceAttached);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldAttachSourceWithAc3)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);
    const firebolt::rialto::IMediaPipeline::MediaSourceAudio kExpectedSource{"audio/x-eac3", kHasDrm, kAudioConfig};
    const int32_t kSourceId{audioSourceWillBeAttached(kExpectedSource)};
    allSourcesWillBeAttached();

    GstCaps *caps{gst_caps_new_simple("audio/x-ac3", "framed", G_TYPE_BOOLEAN, TRUE, "channels", G_TYPE_INT, kChannels,
                                      "rate", G_TYPE_INT, kRate, "alignment", G_TYPE_STRING, "frame", nullptr)};
    setCaps(audioSink, caps);

    EXPECT_TRUE(audioSink->priv->m_sourceAttached);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldFailToAttachSourceWithOpus)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

constexpr firebolt::rialto::VideoRequirements kDefaultRequirements{3840, 2160};
    constexpr firebolt::rialto::MediaType kMediaType{firebolt::rialto::MediaType::MSE};
    const std::string kMimeType{};
    const std::string kUrl{"mse://1"};
    EXPECT_CALL(m_mediaPipelineMock, load(kMediaType, kMimeType, kUrl)).WillOnce(Return(true));
    //EXPECT_CALL(m_mediaPipelineMock, pause()).WillOnce(Return(true));
    EXPECT_CALL(*m_mediaPipelineFactoryMock, createMediaPipeline(_, kDefaultRequirements))
        .WillOnce(Return(ByMove(std::move(m_mediaPipeline))));
    EXPECT_EQ(GST_STATE_CHANGE_ASYNC, gst_element_set_state(pipeline, GST_STATE_PAUSED));

    GstCaps *caps{
        gst_caps_new_simple("audio/x-opus", "channels", G_TYPE_INT, kChannels, "rate", G_TYPE_INT, kRate, nullptr)};
    setCaps(audioSink, caps);

    EXPECT_FALSE(audioSink->priv->m_sourceAttached);
setNullState(pipeline, kUnknownSourceId);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldAttachSourceWithOpus)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);

    const firebolt::rialto::IMediaPipeline::MediaSourceAudio kExpectedSource{"audio/x-opus", kHasDrm, kAudioConfig};
    const int32_t kSourceId{audioSourceWillBeAttached(kExpectedSource)};
    allSourcesWillBeAttached();

    GstCaps *caps{gst_caps_new_simple("audio/x-opus", "channels", G_TYPE_INT, kChannels, "rate", G_TYPE_INT, kRate,
                                      "channel-mapping-family", G_TYPE_INT, 0, nullptr)};
    setCaps(audioSink, caps);

    EXPECT_TRUE(audioSink->priv->m_sourceAttached);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldReachPausedState)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createDefaultMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createDefaultCaps()};
    setCaps(audioSink, caps);

    sendPlaybackStateNotification(audioSink, firebolt::rialto::PlaybackState::PAUSED);

    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldReturnDefaultVolumeValueWhenPipelineIsBelowPausedState)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    gdouble volume{-1.0};
    g_object_get(audioSink, "volume", &volume, nullptr);
    EXPECT_EQ(1.0, volume); // Default value should be returned

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldGetVolumeProperty)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    gdouble volume{-1.0};
    constexpr gdouble kVolume{0.8};
    EXPECT_CALL(m_mediaPipelineMock, getVolume(_)).WillOnce(DoAll(SetArgReferee<0>(kVolume), Return(true)));
    g_object_get(textContext.m_sink, "volume", &volume, nullptr);
    EXPECT_EQ(kVolume, volume);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldReturnDefaultMuteValueWhenPipelineIsBelowPausedState)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    gboolean mute{FALSE};
    g_object_get(audioSink, "mute", &mute, nullptr);
    EXPECT_FALSE(mute); // Default value should be returned

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldGetMuteProperty)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    gboolean mute{FALSE};
    EXPECT_CALL(m_mediaPipelineMock, getMute(_)).WillOnce(DoAll(SetArgReferee<0>(true), Return(true)));
    g_object_get(textContext.m_sink, "mute", &mute, nullptr);
    EXPECT_TRUE(mute);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldFailToSetVolumePropertyWhenPipelineIsBelowPausedState)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    constexpr gdouble kVolume{0.8};
    g_object_set(audioSink, "volume", kVolume, nullptr);

    // Sink should return cached value, when get is called
    gdouble volume{-1.0};
    g_object_get(audioSink, "volume", &volume, nullptr);
    EXPECT_EQ(kVolume, volume);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldSetVolume)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    constexpr gdouble kVolume{0.8};
    EXPECT_CALL(m_mediaPipelineMock, setVolume(kVolume)).WillOnce(Return(true));
    g_object_set(textContext.m_sink, "volume", kVolume, nullptr);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldSetCachedVolume)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    constexpr gdouble kVolume{0.8};
    g_object_set(audioSink, "volume", kVolume, nullptr);

    EXPECT_CALL(m_mediaPipelineMock, setVolume(kVolume)).WillOnce(Return(true));
    setPausedState(pipeline, audioSink);

    setNullState(pipeline, kUnknownSourceId);

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldFailToSetMutePropertyWhenPipelineIsBelowPausedState)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    constexpr gboolean kMute{TRUE};
    g_object_set(audioSink, "mute", kMute, nullptr);

    // Sink should return cached value, when get is called
    gboolean mute{FALSE};
    g_object_get(audioSink, "mute", &mute, nullptr);
    EXPECT_EQ(kMute, mute);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldSetMute)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    constexpr gboolean kMute{TRUE};
    EXPECT_CALL(m_mediaPipelineMock, setMute(kMute)).WillOnce(Return(true));
    g_object_set(textContext.m_sink, "mute", kMute, nullptr);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldSetCachedMute)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    constexpr gboolean kMute{TRUE};
    g_object_set(audioSink, "mute", kMute, nullptr);

    EXPECT_CALL(m_mediaPipelineMock, setMute(kMute)).WillOnce(Return(true));
    setPausedState(pipeline, audioSink);

    setNullState(pipeline, kUnknownSourceId);

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldFailToGetOrSetUnknownProperty)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    g_object_class_install_property(G_OBJECT_GET_CLASS(audioSink), 123,
                                    g_param_spec_boolean("surprise", "surprise", "surprise", FALSE,
                                                         GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    gboolean value{FALSE};
    g_object_get(audioSink, "surprise", &value, nullptr);
    EXPECT_FALSE(value);

    constexpr gboolean kValue{FALSE};
    g_object_set(audioSink, "surprise", kValue, nullptr);

    gst_object_unref(audioSink);
}
