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

#include "Constants.h"
#include "Matchers.h"
#include "RialtoGStreamerMSEAudioSinkPrivate.h"
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

    load(pipeline);
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

TEST_F(GstreamerMseAudioSinkTests, ShouldAttachSourceWithBwav)
{
    constexpr firebolt::rialto::Format kExpectedFormat{firebolt::rialto::Format::S16LE};
    constexpr firebolt::rialto::Layout kExpectedLayout{firebolt::rialto::Layout::INTERLEAVED};
    constexpr uint64_t kExpectedChannelMask{0x0000000000000003};
    const firebolt::rialto::AudioConfig kExpectedAudioConfig{kChannels,       kRate,           {},
                                                             kExpectedFormat, kExpectedLayout, kExpectedChannelMask};
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);

    const firebolt::rialto::IMediaPipeline::MediaSourceAudio kExpectedSource{"audio/b-wav", kHasDrm,
                                                                             kExpectedAudioConfig};
    const int32_t kSourceId{audioSourceWillBeAttached(kExpectedSource)};
    allSourcesWillBeAttached();

    GstCaps *caps{gst_caps_new_simple("audio/b-wav", "channels", G_TYPE_INT, kChannels, "rate", G_TYPE_INT, kRate,
                                      "format", G_TYPE_STRING, "S16LE", "enable-svp", G_TYPE_STRING, "true",
                                      "channel-mask", GST_TYPE_BITMASK, kExpectedChannelMask, "layout", G_TYPE_STRING,
                                      "interleaved", nullptr)};
    setCaps(audioSink, caps);

    EXPECT_TRUE(audioSink->priv->m_sourceAttached);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

#ifdef RIALTO_ENABLE_X_RAW
TEST_F(GstreamerMseAudioSinkTests, ShouldAttachSourceWithXraw)
{
    constexpr firebolt::rialto::Format kExpectedFormat{firebolt::rialto::Format::S32BE};
    constexpr firebolt::rialto::Layout kExpectedLayout{firebolt::rialto::Layout::NON_INTERLEAVED};
    constexpr uint64_t kExpectedChannelMask{0x0000000000000004};
    const firebolt::rialto::AudioConfig kExpectedAudioConfig{kChannels,       kRate,           {},
                                                             kExpectedFormat, kExpectedLayout, kExpectedChannelMask};
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);

    const firebolt::rialto::IMediaPipeline::MediaSourceAudio kExpectedSource{"audio/x-raw", kHasDrm,
                                                                             kExpectedAudioConfig};
    const int32_t kSourceId{audioSourceWillBeAttached(kExpectedSource)};
    allSourcesWillBeAttached();

    GstCaps *caps{gst_caps_new_simple("audio/x-raw", "channels", G_TYPE_INT, kChannels, "rate", G_TYPE_INT, kRate,
                                      "format", G_TYPE_STRING, "S32BE", "enable-svp", G_TYPE_STRING, "true",
                                      "channel-mask", GST_TYPE_BITMASK, kExpectedChannelMask, "layout", G_TYPE_STRING,
                                      "non-interleaved", nullptr)};
    setCaps(audioSink, caps);

    EXPECT_TRUE(audioSink->priv->m_sourceAttached);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}
#endif

TEST_F(GstreamerMseAudioSinkTests, ShouldAttachSourceWithFlac)
{
    const std::vector<uint8_t> kExpectedStreamHeader{1, 2, 3, 4, 5};
    constexpr bool kExpectedFramed{true};
    const firebolt::rialto::AudioConfig kExpectedAudioConfig{kChannels,
                                                             kRate,
                                                             {},
                                                             std::nullopt,
                                                             std::nullopt,
                                                             std::nullopt,
                                                             {kExpectedStreamHeader},
                                                             kExpectedFramed};
    GValue streamHeaderArray = G_VALUE_INIT;
    g_value_init(&streamHeaderArray, GST_TYPE_ARRAY);
    GstBuffer *streamHeaderBuffer{gst_buffer_new_allocate(nullptr, kExpectedStreamHeader.size(), nullptr)};
    gst_buffer_fill(streamHeaderBuffer, 0, kExpectedStreamHeader.data(), kExpectedStreamHeader.size());
    GST_BUFFER_FLAG_SET(streamHeaderBuffer, GST_BUFFER_FLAG_HEADER);

    GValue value = G_VALUE_INIT;
    g_value_init(&value, GST_TYPE_BUFFER);
    gst_value_set_buffer(&value, streamHeaderBuffer);
    gst_value_array_append_value(&streamHeaderArray, &value);

    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);

    const firebolt::rialto::IMediaPipeline::MediaSourceAudio kExpectedSource{"audio/x-flac", kHasDrm,
                                                                             kExpectedAudioConfig};
    const int32_t kSourceId{audioSourceWillBeAttached(kExpectedSource)};
    allSourcesWillBeAttached();

    GstCaps *caps{gst_caps_new_simple("audio/x-flac", "channels", G_TYPE_INT, kChannels, "rate", G_TYPE_INT, kRate,
                                      "framed", G_TYPE_BOOLEAN, kExpectedFramed, nullptr)};
    gst_structure_set_value(gst_caps_get_structure(caps, 0), "streamheader", &streamHeaderArray);
    setCaps(audioSink, caps);

    EXPECT_TRUE(audioSink->priv->m_sourceAttached);

    setNullState(pipeline, kSourceId);

    gst_buffer_unref(streamHeaderBuffer);
    g_value_unset(&value);
    g_value_unset(&streamHeaderArray);
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
    EXPECT_CALL(m_mediaPipelineMock, getMute(textContext.m_sourceId, _))
        .WillOnce(DoAll(SetArgReferee<1>(true), Return(true)));
    g_object_get(textContext.m_sink, "mute", &mute, nullptr);
    EXPECT_TRUE(mute);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldReturnDefaultSyncValueWhenPipelineIsBelowPausedState)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    gboolean sync{FALSE};
    g_object_get(audioSink, "sync", &sync, nullptr);
    EXPECT_EQ(kDefaultSync, sync); // Default value should be returned

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldReturnDefaultSyncPropertyOnRialtoFailure)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    gboolean sync{!kDefaultSync};
    EXPECT_CALL(m_mediaPipelineMock, getSync(_)).WillOnce(Return(false));
    g_object_get(textContext.m_sink, "sync", &sync, nullptr);
    EXPECT_EQ(sync, kDefaultSync);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldGetSyncProperty)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    gboolean sync{FALSE};
    EXPECT_CALL(m_mediaPipelineMock, getSync(_)).WillOnce(DoAll(SetArgReferee<0>(true), Return(true)));
    g_object_get(textContext.m_sink, "sync", &sync, nullptr);
    EXPECT_TRUE(sync);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldReturnDefaultStreamSyncModeValueWhenPipelineIsBelowPausedState)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    gboolean streamSyncMode{FALSE};
    g_object_get(audioSink, "stream-sync-mode", &streamSyncMode, nullptr);
    EXPECT_EQ(kDefaultStreamSyncMode, streamSyncMode); // Default value should be returned

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldReturnDefaultStreamSyncModePropertyOnRialtoFailure)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    int32_t streamSyncMode{kDefaultStreamSyncMode - 1};
    EXPECT_CALL(m_mediaPipelineMock, getStreamSyncMode(_)).WillOnce(Return(false));
    g_object_get(textContext.m_sink, "stream-sync-mode", &streamSyncMode, nullptr);
    EXPECT_EQ(streamSyncMode, kDefaultStreamSyncMode);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldGetStreamSyncModeProperty)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    gint streamSyncMode{0};
    EXPECT_CALL(m_mediaPipelineMock, getStreamSyncMode(_)).WillOnce(DoAll(SetArgReferee<0>(1), Return(true)));
    g_object_get(textContext.m_sink, "stream-sync-mode", &streamSyncMode, nullptr);
    EXPECT_EQ(streamSyncMode, 1);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldReturnDefaultFadeVolumeValueWhenPipelineIsBelowPausedState)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    guint fadeVolume{1};
    g_object_get(audioSink, "fade-volume", &fadeVolume, nullptr);
    EXPECT_EQ(kDefaultFadeVolume, fadeVolume);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldGetFadeVolumeProperty)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    guint fadeVolume{0};
    constexpr guint kFadeVolume{5};
    EXPECT_CALL(m_mediaPipelineMock, getVolume(_)).WillOnce(DoAll(SetArgReferee<0>(kFadeVolume / 100.0), Return(true)));
    g_object_get(textContext.m_sink, "fade-volume", &fadeVolume, nullptr);
    EXPECT_EQ(fadeVolume, kFadeVolume);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldSetCacheAudioFade)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    const gdouble kVolume{0.01};
    const guint kVolumeDuration{100};
    const firebolt::rialto::EaseType kEaseType{firebolt::rialto::EaseType::EASE_OUT_CUBIC};
    const gchar *kAudioFade{"1,100,O"};

    g_object_set(audioSink, "audio-fade", kAudioFade, nullptr);

    EXPECT_CALL(m_mediaPipelineMock, setVolume(kVolume, kVolumeDuration, kEaseType)).WillOnce(Return(true));
    load(pipeline);
    EXPECT_EQ(GST_STATE_CHANGE_ASYNC, gst_element_set_state(pipeline, GST_STATE_PAUSED));

    setNullState(pipeline, kUnknownSourceId);

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldFailWhenParsingInvalidAudioFadeString)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    const gchar *kInvalidFadeConfig{"invalid"};

    EXPECT_CALL(m_mediaPipelineMock, setVolume(_, _, _)).Times(0);
    g_object_set(textContext.m_sink, "audio-fade", kInvalidFadeConfig, nullptr);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldWarnWhenParsingAudioFadeStringWithOneValue)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    const gchar *kPartialFadeConfig{"50"};
    gdouble targetVolume{0.5};

    EXPECT_CALL(m_mediaPipelineMock, setVolume(targetVolume, kDefaultVolumeDuration, kDefaultEaseType))
        .WillOnce(Return(true));

    g_object_set(textContext.m_sink, "audio-fade", kPartialFadeConfig, nullptr);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldApplyAudioFadeWhenClientIsAvailable)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    gdouble targetVolume{0.5};
    guint volumeDuration{1000};
    firebolt::rialto::EaseType easeType{firebolt::rialto::EaseType::EASE_IN_CUBIC};
    const gchar *kFadeConfig{"50,1000,I"};

    EXPECT_CALL(m_mediaPipelineMock, setVolume(targetVolume, volumeDuration, easeType)).WillOnce(Return(true));
    g_object_set(textContext.m_sink, "audio-fade", kFadeConfig, nullptr);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldReturnDefaultBufferingLimitWhenPipelineIsBelowPausedState)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    guint bufferingLimit{0};
    g_object_get(audioSink, "limit-buffering-ms", &bufferingLimit, nullptr);
    EXPECT_EQ(kDefaultBufferingLimit, bufferingLimit); // Default value should be returned

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldGetBufferingLimitProperty)
{
    constexpr guint kBufferingLimit{123};
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    guint bufferingLimit{0};
    EXPECT_CALL(m_mediaPipelineMock, getBufferingLimit(_)).WillOnce(DoAll(SetArgReferee<0>(kBufferingLimit), Return(true)));
    g_object_get(textContext.m_sink, "limit-buffering-ms", &bufferingLimit, nullptr);
    EXPECT_EQ(bufferingLimit, kBufferingLimit);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldSetBufferingLimitProperty)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    constexpr guint kBufferingLimit{1};
    EXPECT_CALL(m_mediaPipelineMock, setBufferingLimit(kBufferingLimit)).WillOnce(Return(true));
    g_object_set(textContext.m_sink, "limit-buffering-ms", kBufferingLimit, nullptr);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldSetCachedBufferingLimit)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    constexpr guint kBufferingLimit{1};
    g_object_set(audioSink, "limit-buffering-ms", kBufferingLimit, nullptr);

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();

    EXPECT_CALL(m_mediaPipelineMock, setBufferingLimit(kBufferingLimit)).WillOnce(Return(true));

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);
    gst_caps_unref(caps);

    setNullState(pipeline, kSourceId);

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldFailToSetBufferingLimitPropertyWhenPipelineIsBelowPausedState)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    constexpr guint kBufferingLimit{123};
    g_object_set(audioSink, "limit-buffering-ms", kBufferingLimit, nullptr);

    // Sink should return cached value, when get is called
    guint bufferingLimit{0};
    g_object_get(audioSink, "limit-buffering-ms", &bufferingLimit, nullptr);
    EXPECT_EQ(kBufferingLimit, bufferingLimit);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldReturnDefaultUseBufferingWhenPipelineIsBelowPausedState)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    gboolean useBuffering{FALSE};
    g_object_get(audioSink, "use-buffering", &useBuffering, nullptr);
    EXPECT_EQ(kDefaultUseBuffering, useBuffering); // Default value should be returned

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldGetUseBufferingProperty)
{
    constexpr gboolean kUseBuffering{TRUE};
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    gboolean useBuffering{FALSE};
    EXPECT_CALL(m_mediaPipelineMock, getUseBuffering(_)).WillOnce(DoAll(SetArgReferee<0>(kUseBuffering), Return(true)));
    g_object_get(textContext.m_sink, "use-buffering", &useBuffering, nullptr);
    EXPECT_EQ(useBuffering, kUseBuffering);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldSetUseBufferingProperty)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    constexpr gboolean kUseBuffering{TRUE};
    EXPECT_CALL(m_mediaPipelineMock, setUseBuffering(kUseBuffering)).WillOnce(Return(true));
    g_object_set(textContext.m_sink, "use-buffering", kUseBuffering, nullptr);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldSetCachedUseBuffering)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    constexpr gboolean kUseBuffering{TRUE};
    g_object_set(audioSink, "use-buffering", kUseBuffering, nullptr);

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();

    EXPECT_CALL(m_mediaPipelineMock, setUseBuffering(kUseBuffering)).WillOnce(Return(true));

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);
    gst_caps_unref(caps);

    setNullState(pipeline, kSourceId);

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldFailToSetUseBufferingPropertyWhenPipelineIsBelowPausedState)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    constexpr gboolean kUseBuffering{TRUE};
    g_object_set(audioSink, "use-buffering", kUseBuffering, nullptr);

    // Sink should return cached value, when get is called
    gboolean useBuffering{FALSE};
    g_object_get(audioSink, "use-buffering", &useBuffering, nullptr);
    EXPECT_EQ(kUseBuffering, useBuffering);

    gst_object_unref(audioSink);
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
    constexpr uint32_t kVolumeDuration{0};
    constexpr firebolt::rialto::EaseType kEaseType{firebolt::rialto::EaseType::EASE_LINEAR};

    EXPECT_CALL(m_mediaPipelineMock, setVolume(kVolume, kVolumeDuration, kEaseType)).WillOnce(Return(true));
    g_object_set(textContext.m_sink, "volume", kVolume, nullptr);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldSetCachedVolume)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    constexpr gdouble kVolume{0.8};
    constexpr uint32_t kVolumeDuration{0};
    constexpr firebolt::rialto::EaseType kEaseType{firebolt::rialto::EaseType::EASE_LINEAR};

    g_object_set(audioSink, "volume", kVolume, nullptr);

    EXPECT_CALL(m_mediaPipelineMock, setVolume(kVolume, kVolumeDuration, kEaseType)).WillOnce(Return(true));
    load(pipeline);
    EXPECT_EQ(GST_STATE_CHANGE_ASYNC, gst_element_set_state(pipeline, GST_STATE_PAUSED));

    setNullState(pipeline, kUnknownSourceId);

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldReturnLastKnownVolumeWhenOperationFails)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    constexpr gdouble kVolume{0.7};
    {
        EXPECT_CALL(m_mediaPipelineMock, getVolume(_)).WillOnce(DoAll(SetArgReferee<0>(kVolume), Return(true)));
        gdouble volume{-1.0};
        g_object_get(textContext.m_sink, "volume", &volume, nullptr);
        EXPECT_EQ(volume, kVolume);
    }

    {
        EXPECT_CALL(m_mediaPipelineMock, getVolume(_)).WillOnce(DoAll(SetArgReferee<0>(1.0), Return(false)));
        gdouble volume{-1.0};
        g_object_get(textContext.m_sink, "volume", &volume, nullptr);
        EXPECT_EQ(volume, kVolume);
    }

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
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
    EXPECT_CALL(m_mediaPipelineMock, setMute(textContext.m_sourceId, kMute)).WillOnce(Return(true));
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

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();

    EXPECT_CALL(m_mediaPipelineMock, setMute(kSourceId, kMute)).WillOnce(Return(true));

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);
    gst_caps_unref(caps);

    setNullState(pipeline, kSourceId);

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldFailToSetLowLatencyPropertyWhenPipelineIsBelowPausedState)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    constexpr gboolean kLowLatency{TRUE};
    g_object_set(audioSink, "low-latency", kLowLatency, nullptr);

    // low-latency is a read only property so we cant check whats set here

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldFailToSetLowLatencyPropertyOnRialtoFailure)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    constexpr gboolean kLowLatency{TRUE};
    EXPECT_CALL(m_mediaPipelineMock, setLowLatency(kLowLatency)).WillOnce(Return(false));
    g_object_set(textContext.m_sink, "low-latency", kLowLatency, nullptr);

    // Error is logged

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldSetLowLatency)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    constexpr gboolean kLowLatency{TRUE};
    EXPECT_CALL(m_mediaPipelineMock, setLowLatency(kLowLatency)).WillOnce(Return(true));
    g_object_set(textContext.m_sink, "low-latency", kLowLatency, nullptr);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldSetCachedLowLatency)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    constexpr gboolean kLowLatency{TRUE};
    g_object_set(audioSink, "low-latency", kLowLatency, nullptr);

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();

    EXPECT_CALL(m_mediaPipelineMock, setLowLatency(kLowLatency)).WillOnce(Return(true));

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);
    gst_caps_unref(caps);

    setNullState(pipeline, kSourceId);

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldNotSetCachedLowLatencyOnRialtoFailure)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    constexpr gboolean kLowLatency{TRUE};
    g_object_set(audioSink, "low-latency", kLowLatency, nullptr);

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();

    EXPECT_CALL(m_mediaPipelineMock, setLowLatency(kLowLatency)).WillOnce(Return(false));

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);
    gst_caps_unref(caps);

    // Error is logged

    setNullState(pipeline, kSourceId);

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldFailToSetSyncPropertyWhenPipelineIsBelowPausedState)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    constexpr gboolean kSync{TRUE};
    g_object_set(audioSink, "sync", kSync, nullptr);

    // Sink should return cached value, when get is called
    gboolean sync{FALSE};
    g_object_get(audioSink, "sync", &sync, nullptr);
    EXPECT_EQ(kSync, sync);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldFailToSetSyncPropertyOnRialtoFailure)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    constexpr gboolean kSync{TRUE};
    EXPECT_CALL(m_mediaPipelineMock, setSync(kSync)).WillOnce(Return(false));
    g_object_set(textContext.m_sink, "sync", kSync, nullptr);

    // Error is logged

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldSetSync)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    constexpr gboolean kSync{TRUE};
    EXPECT_CALL(m_mediaPipelineMock, setSync(kSync)).WillOnce(Return(true));
    g_object_set(textContext.m_sink, "sync", kSync, nullptr);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldSetCachedSync)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    constexpr gboolean kSync{TRUE};
    g_object_set(audioSink, "sync", kSync, nullptr);

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();

    EXPECT_CALL(m_mediaPipelineMock, setSync(kSync)).WillOnce(Return(true));

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);
    gst_caps_unref(caps);

    setNullState(pipeline, kSourceId);

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldNotSetCachedSyncOnRialtoFailure)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    constexpr gboolean kSync{TRUE};
    g_object_set(audioSink, "sync", kSync, nullptr);

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();

    EXPECT_CALL(m_mediaPipelineMock, setSync(kSync)).WillOnce(Return(false));

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);
    gst_caps_unref(caps);

    // Error is logged

    setNullState(pipeline, kSourceId);

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldFailToSetSyncOffPropertyWhenPipelineIsBelowPausedState)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    constexpr gboolean kSyncOff{TRUE};
    g_object_set(audioSink, "sync-off", kSyncOff, nullptr);

    // low-latency is a read only property so we cant check whats set here

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldFailToSetSyncOffPropertyOnRialtoFailure)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    constexpr gboolean kSyncOff{TRUE};
    EXPECT_CALL(m_mediaPipelineMock, setSyncOff(kSyncOff)).WillOnce(Return(false));
    g_object_set(textContext.m_sink, "sync-off", kSyncOff, nullptr);

    // Error is logged

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldSetSyncOff)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    constexpr gboolean kSyncOff{TRUE};
    EXPECT_CALL(m_mediaPipelineMock, setSyncOff(kSyncOff)).WillOnce(Return(true));
    g_object_set(textContext.m_sink, "sync-off", kSyncOff, nullptr);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldSetCachedSyncOff)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    constexpr gboolean kSyncOff{TRUE};
    g_object_set(audioSink, "sync-off", kSyncOff, nullptr);

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();

    EXPECT_CALL(m_mediaPipelineMock, setSyncOff(kSyncOff)).WillOnce(Return(true));

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);
    gst_caps_unref(caps);

    setNullState(pipeline, kSourceId);

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldNotSetCachedSyncOffOnRialtoFailure)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    constexpr gboolean kSyncOff{TRUE};
    g_object_set(audioSink, "sync-off", kSyncOff, nullptr);

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();

    EXPECT_CALL(m_mediaPipelineMock, setSyncOff(kSyncOff)).WillOnce(Return(false));

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);
    gst_caps_unref(caps);

    // Error is logged

    setNullState(pipeline, kSourceId);

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldFailToSetStreamSyncModePropertyOnRialtoFailure)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    constexpr gint kStreamSyncMode{1};
    EXPECT_CALL(m_mediaPipelineMock, setStreamSyncMode(textContext.m_sourceId, kStreamSyncMode)).WillOnce(Return(false));
    g_object_set(textContext.m_sink, "stream-sync-mode", kStreamSyncMode, nullptr);

    // Error is logged

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldSetStreamSyncMode)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    constexpr gint kStreamSyncMode{1};
    EXPECT_CALL(m_mediaPipelineMock, setStreamSyncMode(textContext.m_sourceId, kStreamSyncMode)).WillOnce(Return(true));
    g_object_set(textContext.m_sink, "stream-sync-mode", kStreamSyncMode, nullptr);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldSetCachedStreamSyncMode)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    constexpr gint kStreamSyncMode{1};
    g_object_set(audioSink, "stream-sync-mode", kStreamSyncMode, nullptr);

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();

    EXPECT_CALL(m_mediaPipelineMock, setStreamSyncMode(kSourceId, kStreamSyncMode)).WillOnce(Return(true));

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);
    gst_caps_unref(caps);

    setNullState(pipeline, kSourceId);

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldNotSetCachedStreamSyncModeOnRialtoFailure)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    constexpr gint kStreamSyncMode{1};
    g_object_set(audioSink, "stream-sync-mode", kStreamSyncMode, nullptr);

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();

    EXPECT_CALL(m_mediaPipelineMock, setStreamSyncMode(kSourceId, kStreamSyncMode)).WillOnce(Return(false));

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);
    gst_caps_unref(caps);

    // Error is logged

    setNullState(pipeline, kSourceId);

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldFailToSetStreamSyncModePropertyWhenPipelineIsBelowPausedState)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    constexpr gint kStreamSyncMode{1};
    g_object_set(audioSink, "stream-sync-mode", kStreamSyncMode, nullptr);

    // Sink should return cached value, when get is called
    gint streamSyncMode{0};
    g_object_get(audioSink, "stream-sync-mode", &streamSyncMode, nullptr);
    EXPECT_EQ(kStreamSyncMode, streamSyncMode);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldSetGap)
{
    constexpr int64_t kPosition{123};
    constexpr uint32_t kDuration{456};
    constexpr int64_t kDiscontinuityGap{1};
    constexpr bool kAudioAac{false};

    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    GstStructure *dataStruct = gst_structure_new("gap-params", "position", G_TYPE_INT64, kPosition, "duration",
                                                 G_TYPE_UINT, kDuration, "discontinuity-gap", G_TYPE_INT64,
                                                 kDiscontinuityGap, "audio-aac", G_TYPE_BOOLEAN, kAudioAac, nullptr);
    EXPECT_CALL(m_mediaPipelineMock, processAudioGap(kPosition, kDuration, kDiscontinuityGap, kAudioAac))
        .WillOnce(Return(true));
    g_object_set(textContext.m_sink, "gap", dataStruct, nullptr);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);

    gst_structure_free(dataStruct);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldSetGapWithoutParamsAndDoNotCrash)
{
    constexpr int64_t kPosition{0};
    constexpr uint32_t kDuration{0};
    constexpr int64_t kDiscontinuityGap{0};
    constexpr bool kAudioAac{false};

    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    GstStructure *dataStruct = gst_structure_new_empty("gap-params");
    EXPECT_CALL(m_mediaPipelineMock, processAudioGap(kPosition, kDuration, kDiscontinuityGap, kAudioAac))
        .WillOnce(Return(true));
    g_object_set(textContext.m_sink, "gap", dataStruct, nullptr);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);

    gst_structure_free(dataStruct);
    gst_object_unref(textContext.m_pipeline);
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

TEST_F(GstreamerMseAudioSinkTests, ShouldGetAsyncProperty)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    gboolean async{FALSE};
    g_object_get(textContext.m_sink, "async", &async, nullptr);
    EXPECT_EQ(async, TRUE);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldSetAsyncProperty)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    g_object_set(textContext.m_sink, "async", FALSE, nullptr);

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldHandleSwitchSourceEvent)
{
    GstCaps *caps{createDefaultCaps()};
    GstStructure *structure{gst_structure_new("switch-source", "caps", GST_TYPE_CAPS, caps, nullptr)};

    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    EXPECT_CALL(m_mediaPipelineMock, switchSource(MediaSourceAudioMatcher(createDefaultMediaSource())))
        .WillOnce(Return(true));
    gst_pad_send_event(textContext.m_sink->priv->m_sinkPad, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure));

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_caps_unref(caps);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldFailToHandleSwitchSourceEventWithoutCaps)
{
    GstStructure *structure{gst_structure_new_empty("switch-source")};

    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    gst_pad_send_event(textContext.m_sink->priv->m_sinkPad, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure));

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldFailToHandleSwitchSourceEventWithEmptyCaps)
{
    GstCaps *caps{gst_caps_new_empty()};
    GstStructure *structure{gst_structure_new("switch-source", "caps", GST_TYPE_CAPS, caps, nullptr)};

    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    gst_pad_send_event(textContext.m_sink->priv->m_sinkPad, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure));

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_caps_unref(caps);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseAudioSinkTests, ShouldFailToHandleSwitchSourceEventWhenOperationFails)
{
    GstCaps *caps{createDefaultCaps()};
    GstStructure *structure{gst_structure_new("switch-source", "caps", GST_TYPE_CAPS, caps, nullptr)};

    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    EXPECT_CALL(m_mediaPipelineMock, switchSource(MediaSourceAudioMatcher(createDefaultMediaSource())))
        .WillOnce(Return(false));
    gst_pad_send_event(textContext.m_sink->priv->m_sinkPad, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure));

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_caps_unref(caps);
    gst_object_unref(textContext.m_pipeline);
}
