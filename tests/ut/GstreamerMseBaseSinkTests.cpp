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
const std::string kUri{"location"};
constexpr int kNumOfStreams{1};
constexpr gdouble kPlaybackRate{1.0};
constexpr gint64 kStart{12};
constexpr gint64 kStop{0};
constexpr bool kResetTime{true};
} // namespace

class GstreamerMseBaseSinkTests : public RialtoGstTest
{
public:
    GstreamerMseBaseSinkTests() = default;
    ~GstreamerMseBaseSinkTests() override = default;

    GstCaps *createAudioCaps() const
    {
        return gst_caps_new_simple("audio/mpeg", "mpegversion", G_TYPE_INT, 4, "channels", G_TYPE_INT, kChannels,
                                   "rate", G_TYPE_INT, kRate, nullptr);
    }

    firebolt::rialto::IMediaPipeline::MediaSourceAudio createAudioMediaSource() const
    {
        return firebolt::rialto::IMediaPipeline::MediaSourceAudio{"audio/mp4", kHasDrm, kAudioConfig};
    }
};

TEST_F(GstreamerMseBaseSinkTests, ShouldSwitchAudioSinkToPausedWithoutAVStreamsProperty)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);

    setNullState(pipeline, kUnknownSourceId);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldSwitchVideoSinkToPausedWithoutAVStreamsProperty)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();
    GstElement *pipeline = createPipelineWithSink(videoSink);

    setPausedState(pipeline, videoSink);

    setNullState(pipeline, kUnknownSourceId);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldSwitchAudioSinkToPausedWithAVStreamsProperty)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    installAudioVideoStreamsProperty(pipeline);

    setPausedState(pipeline, audioSink);

    setNullState(pipeline, kUnknownSourceId);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldSwitchVideoSinkToPausedWithAVStreamsProperty)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();
    GstElement *pipeline = createPipelineWithSink(videoSink);

    installAudioVideoStreamsProperty(pipeline);

    setPausedState(pipeline, videoSink);

    setNullState(pipeline, kUnknownSourceId);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldReachPlayingState)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);

    sendPlaybackStateNotification(audioSink, firebolt::rialto::PlaybackState::PAUSED);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    setPlayingState(pipeline);
    sendPlaybackStateNotification(audioSink, firebolt::rialto::PlaybackState::PLAYING);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    pipelineWillGoToPausedState(audioSink); // PLAYING -> PAUSED
    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldSendEos)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);

    sendPlaybackStateNotification(audioSink, firebolt::rialto::PlaybackState::PAUSED);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    setPlayingState(pipeline);
    sendPlaybackStateNotification(audioSink, firebolt::rialto::PlaybackState::PLAYING);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    sendPlaybackStateNotification(audioSink, firebolt::rialto::PlaybackState::END_OF_STREAM);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_EOS));

    pipelineWillGoToPausedState(audioSink);
    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldGetLocationProperty)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    audioSink->priv->m_uri = kUri;
    gchar *uri{nullptr};
    g_object_get(audioSink, "location", &uri, nullptr);
    ASSERT_TRUE(uri);
    EXPECT_EQ(std::string{uri}, kUri);
    g_free(uri);
    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldGetHandleResetTimeMessageProperty)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    audioSink->priv->m_handleResetTimeMessage = true;
    gboolean value{FALSE};
    g_object_get(audioSink, "handle-reset-time-message", &value, nullptr);
    EXPECT_TRUE(value);
    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldGetIsSinglePathStreamProperty)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    audioSink->priv->m_isSinglePathStream = true;
    gboolean value{FALSE};
    g_object_get(audioSink, "single-path-stream", &value, nullptr);
    EXPECT_TRUE(value);
    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldGetStreamsNumberProperty)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    audioSink->priv->m_numOfStreams = kNumOfStreams;
    int value{0};
    g_object_get(audioSink, "streams-number", &value, nullptr);
    EXPECT_EQ(value, kNumOfStreams);
    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldGetHasDrmProperty)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    audioSink->priv->m_hasDrm = true;
    gboolean value{FALSE};
    g_object_get(audioSink, "has-drm", &value, nullptr);
    EXPECT_TRUE(value);
    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldSetLocationProperty)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    g_object_set(audioSink, "location", kUri.c_str(), nullptr);
    EXPECT_EQ(audioSink->priv->m_uri, kUri);
    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldSetHandleResetTimeMessageProperty)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    g_object_set(audioSink, "handle-reset-time-message", true, nullptr);
    EXPECT_TRUE(audioSink->priv->m_handleResetTimeMessage);
    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldSetIsSinglePathStreamProperty)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    g_object_set(audioSink, "single-path-stream", true, nullptr);
    EXPECT_TRUE(audioSink->priv->m_isSinglePathStream);
    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldSetStreamsNumberProperty)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    g_object_set(audioSink, "streams-number", kNumOfStreams, nullptr);
    EXPECT_EQ(audioSink->priv->m_numOfStreams, kNumOfStreams);
    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldSetHasDrmProperty)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    g_object_set(audioSink, "has-drm", true, nullptr);
    EXPECT_TRUE(audioSink->priv->m_hasDrm);
    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldQuerySeeking)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstQuery *query{gst_query_new_seeking(GST_FORMAT_DEFAULT)};
    EXPECT_TRUE(gst_element_query(GST_ELEMENT_CAST(audioSink), query));
    gst_query_unref(query);
    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldFailToQueryPositionWhenPipelineIsBelowPaused)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    gint64 position{0};
    EXPECT_FALSE(gst_element_query_position(GST_ELEMENT_CAST(audioSink), GST_FORMAT_TIME, &position));
    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldFailToQueryPositionWhenPositionIsInvalid)
{
    constexpr gint64 kInvalidPosition{-1};
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);

    gint64 position{0};
    EXPECT_CALL(m_mediaPipelineMock, getPosition(_)).WillOnce(DoAll(SetArgReferee<0>(kInvalidPosition), Return(true)));
    EXPECT_FALSE(gst_element_query_position(GST_ELEMENT_CAST(audioSink), GST_FORMAT_TIME, &position));

    setNullState(pipeline, kUnknownSourceId);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldQueryPosition)
{
    constexpr gint64 kPosition{1234};
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);

    gint64 position{0};
    EXPECT_CALL(m_mediaPipelineMock, getPosition(_)).WillOnce(DoAll(SetArgReferee<0>(kPosition), Return(true)));
    EXPECT_TRUE(gst_element_query_position(GST_ELEMENT_CAST(audioSink), GST_FORMAT_TIME, &position));
    EXPECT_EQ(position, kPosition);

    setNullState(pipeline, kUnknownSourceId);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldSkipQueryingPositionWithInvalidFormat)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);

    gint64 position{0};
    EXPECT_TRUE(gst_element_query_position(GST_ELEMENT_CAST(audioSink), GST_FORMAT_DEFAULT, &position));

    setNullState(pipeline, kUnknownSourceId);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldFailToSeekWhenFlagIsWrong)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);

    EXPECT_FALSE(gst_element_seek(GST_ELEMENT_CAST(audioSink), kPlaybackRate, GST_FORMAT_TIME, GST_SEEK_FLAG_NONE,
                                  GST_SEEK_TYPE_NONE, kStart, GST_SEEK_TYPE_NONE, kStop));

    setNullState(pipeline, kUnknownSourceId);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldFailToSeekWithWrongFormat)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);

    EXPECT_FALSE(gst_element_seek(GST_ELEMENT_CAST(audioSink), kPlaybackRate, GST_FORMAT_DEFAULT, GST_SEEK_FLAG_FLUSH,
                                  GST_SEEK_TYPE_NONE, kStart, GST_SEEK_TYPE_NONE, kStop));

    setNullState(pipeline, kUnknownSourceId);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldFailToSeekWithWrongSeekType)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);

    EXPECT_FALSE(gst_element_seek(GST_ELEMENT_CAST(audioSink), kPlaybackRate, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
                                  GST_SEEK_TYPE_NONE, kStart, GST_SEEK_TYPE_NONE, kStop));

    setNullState(pipeline, kUnknownSourceId);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldFailToSeekWithSeekTypeEnd)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);

    EXPECT_FALSE(gst_element_seek(GST_ELEMENT_CAST(audioSink), kPlaybackRate, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
                                  GST_SEEK_TYPE_END, kStart, GST_SEEK_TYPE_NONE, kStop));

    setNullState(pipeline, kUnknownSourceId);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldFailToSeekWithWrongPosition)
{
    constexpr gint64 kWrongStart{-1};

    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);

    EXPECT_FALSE(gst_element_seek(GST_ELEMENT_CAST(audioSink), kPlaybackRate, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
                                  GST_SEEK_TYPE_SET, kWrongStart, GST_SEEK_TYPE_NONE, kStop));

    setNullState(pipeline, kUnknownSourceId);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldFailToSeekWhenSendingUpstreamEventFails)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);

    EXPECT_FALSE(gst_element_seek(GST_ELEMENT_CAST(audioSink), kPlaybackRate, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
                                  GST_SEEK_TYPE_SET, kStart, GST_SEEK_TYPE_NONE, kStop));

    setNullState(pipeline, kUnknownSourceId);
    gst_object_unref(pipeline);
}

#if GST_CHECK_VERSION(1, 18, 0)
TEST_F(GstreamerMseBaseSinkTests, ShouldFailToSeekWithPlaybackRateChangeWhenPipelineIsBelowPaused)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    EXPECT_FALSE(gst_element_seek(GST_ELEMENT_CAST(audioSink), kPlaybackRate, GST_FORMAT_TIME,
                                  GST_SEEK_FLAG_INSTANT_RATE_CHANGE, GST_SEEK_TYPE_NONE, kStart, GST_SEEK_TYPE_NONE,
                                  kStop));

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldSeekWithPlaybackRateChange)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);

    EXPECT_CALL(m_mediaPipelineMock, setPlaybackRate(kPlaybackRate)).WillOnce(Return(true));
    EXPECT_TRUE(gst_element_seek(GST_ELEMENT_CAST(audioSink), kPlaybackRate, GST_FORMAT_TIME,
                                 GST_SEEK_FLAG_INSTANT_RATE_CHANGE, GST_SEEK_TYPE_NONE, kStart, GST_SEEK_TYPE_NONE,
                                 kStop));

    setNullState(pipeline, kUnknownSourceId);
    gst_object_unref(pipeline);
}
#endif

TEST_F(GstreamerMseBaseSinkTests, ShouldDiscardBufferInChainFunctionWhenFlushing)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstBuffer *buffer = gst_buffer_new();
    audioSink->priv->m_isFlushOngoing = true;

    EXPECT_EQ(GST_FLOW_FLUSHING,
              rialto_mse_base_sink_chain(audioSink->priv->m_sinkPad, GST_OBJECT_CAST(audioSink), buffer));

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldAddBufferInChainFunction)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstBuffer *buffer = gst_buffer_new();

    EXPECT_EQ(GST_FLOW_OK, rialto_mse_base_sink_chain(audioSink->priv->m_sinkPad, GST_OBJECT_CAST(audioSink), buffer));

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldWaitAndAddBufferInChainFunction)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstBuffer *buffer = gst_buffer_new();

    for (int i = 0; i < 24; ++i)
    {
        audioSink->priv->m_samples.push(
            gst_sample_new(buffer, audioSink->priv->m_caps, &audioSink->priv->m_lastSegment, nullptr));
    }

    std::thread t{
        [&]() {
            EXPECT_EQ(GST_FLOW_OK,
                      rialto_mse_base_sink_chain(audioSink->priv->m_sinkPad, GST_OBJECT_CAST(audioSink), buffer));
        }};
    EXPECT_TRUE(t.joinable());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    rialto_mse_base_sink_pop_sample(audioSink);
    t.join();

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldHandleNewSegment)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    GstSegment *segment{gst_segment_new()};
    gst_segment_init(segment, GST_FORMAT_TIME);

    EXPECT_TRUE(rialto_mse_base_sink_event(audioSink->priv->m_sinkPad, GST_OBJECT_CAST(audioSink),
                                           gst_event_new_segment(segment)));
    EXPECT_EQ(GST_FORMAT_TIME, audioSink->priv->m_lastSegment.format);

    gst_segment_free(segment);
    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldHandleEos)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    EXPECT_TRUE(rialto_mse_base_sink_event(audioSink->priv->m_sinkPad, GST_OBJECT_CAST(audioSink), gst_event_new_eos()));
    EXPECT_TRUE(audioSink->priv->m_isEos);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldHandleCapsEvent)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);
    EXPECT_TRUE(gst_caps_is_equal(caps, audioSink->priv->m_caps));

    GstCaps *newCaps{gst_caps_new_simple("audio/x-eac3", "mpegversion", G_TYPE_INT, 2, "channels", G_TYPE_INT,
                                         kChannels, "rate", G_TYPE_INT, kRate, nullptr)};
    EXPECT_TRUE(rialto_mse_base_sink_event(audioSink->priv->m_sinkPad, GST_OBJECT_CAST(audioSink),
                                           gst_event_new_caps(newCaps)));
    EXPECT_TRUE(gst_caps_is_equal(newCaps, audioSink->priv->m_caps));

    setNullState(pipeline, kSourceId);
    gst_caps_unref(newCaps);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldHandleSinkMessage)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);
    GError *gError{g_error_new_literal(GST_STREAM_ERROR, 0, "Test error")};
    GstMessage *message{gst_message_new_error(GST_OBJECT_CAST(audioSink), gError, "test error")};

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);

    sendPlaybackStateNotification(audioSink, firebolt::rialto::PlaybackState::PAUSED);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    EXPECT_TRUE(rialto_mse_base_sink_event(audioSink->priv->m_sinkPad, GST_OBJECT_CAST(audioSink),
                                           gst_event_new_sink_message("test_eos", message)));

    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ERROR));

    setNullState(pipeline, kSourceId);
    g_error_free(gError);
    gst_message_unref(message);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldHandleCustomDownstreamMessage)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);
    GstStructure *structure{
        gst_structure_new("custom-instant-rate-change", "rate", G_TYPE_DOUBLE, kPlaybackRate, nullptr)};

    setPausedState(pipeline, audioSink);

    EXPECT_CALL(m_mediaPipelineMock, setPlaybackRate(kPlaybackRate)).WillOnce(Return(true));
    EXPECT_TRUE(rialto_mse_base_sink_event(audioSink->priv->m_sinkPad, GST_OBJECT_CAST(audioSink),
                                           gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure)));

    setNullState(pipeline, kUnknownSourceId);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldHandleCustomDownstreamMessageWithoutChangingPlaybackRateWhenBelowPaused)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstStructure *structure{
        gst_structure_new("custom-instant-rate-change", "rate", G_TYPE_DOUBLE, kPlaybackRate, nullptr)};

    EXPECT_TRUE(rialto_mse_base_sink_event(audioSink->priv->m_sinkPad, GST_OBJECT_CAST(audioSink),
                                           gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure)));

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldHandleFlushStart)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    EXPECT_TRUE(rialto_mse_base_sink_event(audioSink->priv->m_sinkPad, GST_OBJECT_CAST(audioSink),
                                           gst_event_new_flush_start()));
    EXPECT_TRUE(audioSink->priv->m_isFlushOngoing);
    EXPECT_FALSE(audioSink->priv->m_isEos);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldHandleFlushStopBelowPausedState)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    audioSink->priv->m_isFlushOngoing = true;

    EXPECT_TRUE(rialto_mse_base_sink_event(audioSink->priv->m_sinkPad, GST_OBJECT_CAST(audioSink),
                                           gst_event_new_flush_stop(kResetTime)));
    EXPECT_FALSE(audioSink->priv->m_isFlushOngoing);

    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldHandleFlushStopInPausedState)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);
    audioSink->priv->m_isFlushOngoing = true;

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);

    sendPlaybackStateNotification(audioSink, firebolt::rialto::PlaybackState::PAUSED);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    EXPECT_CALL(m_mediaPipelineMock, setPosition(0)).WillOnce(Return(true));

    std::thread t{[&]()
                  {
                      std::this_thread::sleep_for(std::chrono::milliseconds(100));
                      rialto_mse_base_handle_rialto_server_completed_seek(audioSink);
                  }};

    EXPECT_TRUE(rialto_mse_base_sink_event(audioSink->priv->m_sinkPad, GST_OBJECT_CAST(audioSink),
                                           gst_event_new_flush_stop(kResetTime)));
    EXPECT_FALSE(audioSink->priv->m_isFlushOngoing);

    t.join();

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldHandleFlushStopInPlayingState)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);
    audioSink->priv->m_isFlushOngoing = true;

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);

    sendPlaybackStateNotification(audioSink, firebolt::rialto::PlaybackState::PAUSED);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    setPlayingState(pipeline);
    sendPlaybackStateNotification(audioSink, firebolt::rialto::PlaybackState::PLAYING);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    EXPECT_CALL(m_mediaPipelineMock, setPosition(0)).WillOnce(Return(true));
    pipelineWillGoToPausedState(audioSink);
    EXPECT_CALL(m_mediaPipelineMock, play()).WillOnce(Return(true));

    std::thread t{[&]()
                  {
                      std::this_thread::sleep_for(std::chrono::milliseconds(10));
                      rialto_mse_base_handle_rialto_server_completed_seek(audioSink);
                  }};

    EXPECT_TRUE(rialto_mse_base_sink_event(audioSink->priv->m_sinkPad, GST_OBJECT_CAST(audioSink),
                                           gst_event_new_flush_stop(kResetTime)));
    EXPECT_FALSE(audioSink->priv->m_isFlushOngoing);

    t.join();

    pipelineWillGoToPausedState(audioSink); // PLAYING -> PAUSED
    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldAttachSourceWithBufferCodecData)
{
    const std::vector<uint8_t> kCodecDataVec{1, 2, 3, 4};
    auto codecDataPtr{std::make_shared<firebolt::rialto::CodecData>()};
    codecDataPtr->data = kCodecDataVec;
    codecDataPtr->type = firebolt::rialto::CodecDataType::BUFFER;
    GstBuffer *codecDataBuf{gst_buffer_new_allocate(nullptr, kCodecDataVec.size(), nullptr)};
    gst_buffer_fill(codecDataBuf, 0, kCodecDataVec.data(), kCodecDataVec.size());

    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);

    const firebolt::rialto::IMediaPipeline::MediaSourceAudio kExpectedSource{"audio/mp4",
                                                                             kHasDrm,
                                                                             kAudioConfig,
                                                                             firebolt::rialto::SegmentAlignment::UNDEFINED,
                                                                             firebolt::rialto::StreamFormat::UNDEFINED,
                                                                             codecDataPtr};
    const int32_t kSourceId{audioSourceWillBeAttached(kExpectedSource)};
    GstCaps *caps{gst_caps_new_simple("audio/mpeg", "mpegversion", G_TYPE_INT, 4, "channels", G_TYPE_INT, kChannels,
                                      "rate", G_TYPE_INT, kRate, "codec_data", GST_TYPE_BUFFER, codecDataBuf, nullptr)};
    setCaps(audioSink, caps);

    EXPECT_TRUE(audioSink->priv->m_sourceAttached);

    setNullState(pipeline, kSourceId);

    gst_buffer_unref(codecDataBuf);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldAttachSourceWithCodecDataString)
{
    const std::string kCodecDataStr{"abcd"};
    auto codecDataPtr{std::make_shared<firebolt::rialto::CodecData>()};
    codecDataPtr->data = std::vector<uint8_t>{kCodecDataStr.begin(), kCodecDataStr.end()};
    codecDataPtr->type = firebolt::rialto::CodecDataType::STRING;

    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);

    const firebolt::rialto::IMediaPipeline::MediaSourceAudio kExpectedSource{"audio/mp4",
                                                                             kHasDrm,
                                                                             kAudioConfig,
                                                                             firebolt::rialto::SegmentAlignment::UNDEFINED,
                                                                             firebolt::rialto::StreamFormat::UNDEFINED,
                                                                             codecDataPtr};
    const int32_t kSourceId{audioSourceWillBeAttached(kExpectedSource)};
    GstCaps *caps{gst_caps_new_simple("audio/mpeg", "mpegversion", G_TYPE_INT, 4, "channels", G_TYPE_INT, kChannels,
                                      "rate", G_TYPE_INT, kRate, "codec_data", G_TYPE_STRING, kCodecDataStr.c_str(),
                                      nullptr)};
    setCaps(audioSink, caps);

    EXPECT_TRUE(audioSink->priv->m_sourceAttached);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldAttachSourceWithRawStreamFormat)
{
    const std::string kStreamFormat{"raw"};

    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);

    const firebolt::rialto::IMediaPipeline::MediaSourceAudio kExpectedSource{"audio/mp4", kHasDrm, kAudioConfig,
                                                                             firebolt::rialto::SegmentAlignment::UNDEFINED,
                                                                             firebolt::rialto::StreamFormat::RAW};
    const int32_t kSourceId{audioSourceWillBeAttached(kExpectedSource)};
    GstCaps *caps{gst_caps_new_simple("audio/mpeg", "mpegversion", G_TYPE_INT, 4, "channels", G_TYPE_INT, kChannels,
                                      "rate", G_TYPE_INT, kRate, "stream-format", G_TYPE_STRING, kStreamFormat.c_str(),
                                      nullptr)};
    setCaps(audioSink, caps);

    EXPECT_TRUE(audioSink->priv->m_sourceAttached);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldAttachSourceWithAvcStreamFormat)
{
    const std::string kStreamFormat{"avc"};

    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);

    const firebolt::rialto::IMediaPipeline::MediaSourceAudio kExpectedSource{"audio/mp4", kHasDrm, kAudioConfig,
                                                                             firebolt::rialto::SegmentAlignment::UNDEFINED,
                                                                             firebolt::rialto::StreamFormat::AVC};
    const int32_t kSourceId{audioSourceWillBeAttached(kExpectedSource)};
    GstCaps *caps{gst_caps_new_simple("audio/mpeg", "mpegversion", G_TYPE_INT, 4, "channels", G_TYPE_INT, kChannels,
                                      "rate", G_TYPE_INT, kRate, "stream-format", G_TYPE_STRING, kStreamFormat.c_str(),
                                      nullptr)};
    setCaps(audioSink, caps);

    EXPECT_TRUE(audioSink->priv->m_sourceAttached);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldAttachSourceWithByteStreamStreamFormat)
{
    const std::string kStreamFormat{"byte-stream"};

    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);

    const firebolt::rialto::IMediaPipeline::MediaSourceAudio kExpectedSource{"audio/mp4", kHasDrm, kAudioConfig,
                                                                             firebolt::rialto::SegmentAlignment::UNDEFINED,
                                                                             firebolt::rialto::StreamFormat::BYTE_STREAM};
    const int32_t kSourceId{audioSourceWillBeAttached(kExpectedSource)};
    GstCaps *caps{gst_caps_new_simple("audio/mpeg", "mpegversion", G_TYPE_INT, 4, "channels", G_TYPE_INT, kChannels,
                                      "rate", G_TYPE_INT, kRate, "stream-format", G_TYPE_STRING, kStreamFormat.c_str(),
                                      nullptr)};
    setCaps(audioSink, caps);

    EXPECT_TRUE(audioSink->priv->m_sourceAttached);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldAttachSourceWithAuSegmentAlignment)
{
    const std::string kAlignment{"au"};

    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);

    const firebolt::rialto::IMediaPipeline::MediaSourceAudio kExpectedSource{"audio/mp4", kHasDrm, kAudioConfig,
                                                                             firebolt::rialto::SegmentAlignment::AU};
    const int32_t kSourceId{audioSourceWillBeAttached(kExpectedSource)};
    GstCaps *caps{gst_caps_new_simple("audio/mpeg", "mpegversion", G_TYPE_INT, 4, "channels", G_TYPE_INT, kChannels,
                                      "rate", G_TYPE_INT, kRate, "alignment", G_TYPE_STRING, kAlignment.c_str(), nullptr)};
    setCaps(audioSink, caps);

    EXPECT_TRUE(audioSink->priv->m_sourceAttached);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldAttachSourceWithNalSegmentAlignment)
{
    const std::string kAlignment{"nal"};

    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);

    const firebolt::rialto::IMediaPipeline::MediaSourceAudio kExpectedSource{"audio/mp4", kHasDrm, kAudioConfig,
                                                                             firebolt::rialto::SegmentAlignment::NAL};
    const int32_t kSourceId{audioSourceWillBeAttached(kExpectedSource)};
    GstCaps *caps{gst_caps_new_simple("audio/mpeg", "mpegversion", G_TYPE_INT, 4, "channels", G_TYPE_INT, kChannels,
                                      "rate", G_TYPE_INT, kRate, "alignment", G_TYPE_STRING, kAlignment.c_str(), nullptr)};
    setCaps(audioSink, caps);

    EXPECT_TRUE(audioSink->priv->m_sourceAttached);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}
