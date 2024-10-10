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
#include "PlaybinStub.h"
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
};

TEST_F(GstreamerMseBaseSinkTests, ShouldSwitchAudioSinkToPausedWithAVStreamsProperty)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *playbin = createPlaybin2WithSink(audioSink);

    g_object_set(playbin, "n-audio", 1, nullptr);
    g_object_set(playbin, "n-video", 0, nullptr);
    g_object_set(playbin, "flags", GST_PLAY_FLAG_AUDIO, nullptr);

    setPausedState(playbin, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);

    sendPlaybackStateNotification(audioSink, firebolt::rialto::PlaybackState::PAUSED);
    EXPECT_TRUE(waitForMessage(playbin, GST_MESSAGE_ASYNC_DONE));

    setNullState(playbin, kSourceId);
    gst_caps_unref(caps);
    gst_object_unref(playbin);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldSwitchVideoSinkToPausedWithAVStreamsProperty)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();
    GstElement *playbin = createPlaybin2WithSink(videoSink);

    g_object_set(playbin, "n-audio", 0, nullptr);
    g_object_set(playbin, "n-video", 1, nullptr);
    g_object_set(playbin, "flags", GST_PLAY_FLAG_VIDEO, nullptr);

    setPausedState(playbin, videoSink);
    const int32_t kSourceId{videoSourceWillBeAttached(createVideoMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createVideoCaps()};
    setCaps(videoSink, caps);

    sendPlaybackStateNotification(videoSink, firebolt::rialto::PlaybackState::PAUSED);
    EXPECT_TRUE(waitForMessage(playbin, GST_MESSAGE_ASYNC_DONE));

    setNullState(playbin, kSourceId);
    gst_caps_unref(caps);
    gst_object_unref(playbin);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldReachPlayingState)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();

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
    allSourcesWillBeAttached();

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

TEST_F(GstreamerMseBaseSinkTests, ShouldSetImmediateOutputProperty)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();
    GstElement *pipeline = createPipelineWithSink(videoSink);

    setPausedState(pipeline, videoSink);
    const int32_t kSourceId{videoSourceWillBeAttached(createVideoMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createVideoCaps()};
    setCaps(videoSink, caps);

    EXPECT_CALL(m_mediaPipelineMock, setImmediateOutput(_, _)).WillOnce(Return(true));
    g_object_set(videoSink, "immediate-output", TRUE, nullptr);

    setNullState(pipeline, kSourceId);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldFailToSetImmediateOutputPropertyDueToPipelinedFailure)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();
    GstElement *pipeline = createPipelineWithSink(videoSink);

    setPausedState(pipeline, videoSink);
    const int32_t kSourceId{videoSourceWillBeAttached(createVideoMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createVideoCaps()};
    setCaps(videoSink, caps);

    EXPECT_CALL(m_mediaPipelineMock, setImmediateOutput(_, _)).WillOnce(Return(false));
    g_object_set(videoSink, "immediate-output", TRUE, nullptr);

    setNullState(pipeline, kSourceId);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldFailToSetImmediateOutputProperty)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();

    // No pipeline therefore the m_mediaPipelineMock method setImmediateOutput() will not be called
    g_object_set(videoSink, "immediate-output", TRUE, nullptr);

    gst_object_unref(videoSink);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldGetImmediateOutputProperty)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();
    GstElement *pipeline = createPipelineWithSink(videoSink);

    setPausedState(pipeline, videoSink);
    const int32_t kSourceId{videoSourceWillBeAttached(createVideoMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createVideoCaps()};
    setCaps(videoSink, caps);

    EXPECT_CALL(m_mediaPipelineMock, getImmediateOutput(_, _)).WillOnce(DoAll(SetArgReferee<1>(true), Return(true)));
    gboolean immediateOutput;
    g_object_get(videoSink, "immediate-output", &immediateOutput, nullptr);
    EXPECT_TRUE(immediateOutput);

    setNullState(pipeline, kSourceId);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldFailToGetImmediateOutputPropertyDueToPipelinedFailure)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();
    GstElement *pipeline = createPipelineWithSink(videoSink);

    setPausedState(pipeline, videoSink);
    const int32_t kSourceId{videoSourceWillBeAttached(createVideoMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createVideoCaps()};
    setCaps(videoSink, caps);

    EXPECT_CALL(m_mediaPipelineMock, getImmediateOutput(_, _)).WillOnce(Return(false));
    gboolean immediateOutput{true};
    g_object_get(videoSink, "immediate-output", &immediateOutput, nullptr);
    EXPECT_FALSE(immediateOutput); // The return value for failure

    setNullState(pipeline, kSourceId);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldFailToGetImmediateOutputProperty)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();

    // No pipeline therefore the m_mediaPipelineMock method getImmediateOutput() will not be called
    gboolean immediateOutput;
    g_object_get(videoSink, "immediate-output", &immediateOutput, nullptr);

    gst_object_unref(videoSink);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldGetStatsProperty)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);

    EXPECT_CALL(m_mediaPipelineMock, getStats(_, _, _)).WillOnce(Return(true));
    GstStructure *stats{nullptr};
    g_object_get(audioSink, "stats", &stats, nullptr);
    EXPECT_NE(stats, nullptr);

    gst_structure_free(stats);
    setNullState(pipeline, kSourceId);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldFailToGetStatsProperty)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    // No pipeline therefore the stats should still be null
    GstStructure *stats{nullptr};
    g_object_get(audioSink, "stats", &stats, nullptr);
    EXPECT_EQ(stats, nullptr);

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

TEST_F(GstreamerMseBaseSinkTests, ShouldFailToQueryPositionWhenSourceNotAttached)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    load(pipeline);
    EXPECT_EQ(GST_STATE_CHANGE_ASYNC, gst_element_set_state(pipeline, GST_STATE_PAUSED));

    gint64 position{0};
    EXPECT_FALSE(gst_element_query_position(GST_ELEMENT_CAST(audioSink), GST_FORMAT_TIME, &position));

    setNullState(pipeline, kUnknownSourceId);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldFailToQueryPositionWhenPositionIsInvalid)
{
    constexpr gint64 kInvalidPosition{-1};
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();
    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);

    gint64 position{0};
    EXPECT_CALL(m_mediaPipelineMock, getPosition(_)).WillOnce(DoAll(SetArgReferee<0>(kInvalidPosition), Return(true)));
    EXPECT_FALSE(gst_element_query_position(GST_ELEMENT_CAST(audioSink), GST_FORMAT_TIME, &position));

    setNullState(pipeline, kSourceId);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldQueryPosition)
{
    constexpr gint64 kPosition{1234};
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();
    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);

    gint64 position{0};
    EXPECT_CALL(m_mediaPipelineMock, getPosition(_)).WillOnce(DoAll(SetArgReferee<0>(kPosition), Return(true)));
    EXPECT_TRUE(gst_element_query_position(GST_ELEMENT_CAST(audioSink), GST_FORMAT_TIME, &position));
    EXPECT_EQ(position, kPosition);

    setNullState(pipeline, kSourceId);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldSkipQueryingPositionWithInvalidFormat)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);

    gint64 position{0};
    EXPECT_TRUE(gst_element_query_position(GST_ELEMENT_CAST(audioSink), GST_FORMAT_DEFAULT, &position));

    setNullState(pipeline, kSourceId);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldFailToSeekWhenFlagIsWrong)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    EXPECT_FALSE(gst_element_seek(GST_ELEMENT_CAST(textContext.m_sink), kPlaybackRate, GST_FORMAT_TIME,
                                  GST_SEEK_FLAG_NONE, GST_SEEK_TYPE_NONE, kStart, GST_SEEK_TYPE_NONE, kStop));

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldFailToSeekWithWrongFormat)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    EXPECT_FALSE(gst_element_seek(GST_ELEMENT_CAST(textContext.m_sink), kPlaybackRate, GST_FORMAT_DEFAULT,
                                  GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_NONE, kStart, GST_SEEK_TYPE_NONE, kStop));

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldFailToSeekWithWrongSeekType)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    EXPECT_FALSE(gst_element_seek(GST_ELEMENT_CAST(textContext.m_sink), kPlaybackRate, GST_FORMAT_TIME,
                                  GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_NONE, kStart, GST_SEEK_TYPE_NONE, kStop));

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldFailToSeekWithSeekTypeEnd)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    EXPECT_FALSE(gst_element_seek(GST_ELEMENT_CAST(textContext.m_sink), kPlaybackRate, GST_FORMAT_TIME,
                                  GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_END, kStart, GST_SEEK_TYPE_NONE, kStop));

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldFailToSeekWithWrongPosition)
{
    constexpr gint64 kWrongStart{-1};
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    EXPECT_FALSE(gst_element_seek(GST_ELEMENT_CAST(textContext.m_sink), kPlaybackRate, GST_FORMAT_TIME,
                                  GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, kWrongStart, GST_SEEK_TYPE_NONE, kStop));

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldFailToSeekBelowPausedState)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    EXPECT_FALSE(gst_element_seek(GST_ELEMENT_CAST(audioSink), kPlaybackRate, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
                                  GST_SEEK_TYPE_SET, kStart, GST_SEEK_TYPE_NONE, kStop));
    gst_object_unref(audioSink);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldFailToSeekWhenSendingUpstreamEventFails)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    EXPECT_FALSE(gst_element_seek(GST_ELEMENT_CAST(textContext.m_sink), kPlaybackRate, GST_FORMAT_TIME,
                                  GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, kStart, GST_SEEK_TYPE_NONE, kStop));

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldFailToSeekWhenSendingUpstreamEventFailsWithAttachedSource)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    sendPlaybackStateNotification(textContext.m_sink, firebolt::rialto::PlaybackState::PAUSED);
    EXPECT_TRUE(waitForMessage(textContext.m_pipeline, GST_MESSAGE_ASYNC_DONE));

    EXPECT_FALSE(gst_element_seek(GST_ELEMENT_CAST(textContext.m_sink), kPlaybackRate, GST_FORMAT_TIME,
                                  GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, kStart, GST_SEEK_TYPE_NONE, kStop));

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldFailToSeekWhenSendingUpstreamEventFailsWithAttachedSourceInPlayingState)
{
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    sendPlaybackStateNotification(textContext.m_sink, firebolt::rialto::PlaybackState::PAUSED);
    EXPECT_TRUE(waitForMessage(textContext.m_pipeline, GST_MESSAGE_ASYNC_DONE));

    setPlayingState(textContext.m_pipeline);
    sendPlaybackStateNotification(textContext.m_sink, firebolt::rialto::PlaybackState::PLAYING);

    EXPECT_TRUE(waitForMessage(textContext.m_pipeline, GST_MESSAGE_ASYNC_DONE));

    EXPECT_FALSE(gst_element_seek(GST_ELEMENT_CAST(textContext.m_sink), kPlaybackRate, GST_FORMAT_TIME,
                                  GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, kStart, GST_SEEK_TYPE_NONE, kStop));

    pipelineWillGoToPausedState(textContext.m_sink); // PLAYING -> PAUSED
    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
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
    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();

    EXPECT_CALL(m_mediaPipelineMock, setPlaybackRate(kPlaybackRate)).WillOnce(Return(true));
    EXPECT_TRUE(gst_element_seek(GST_ELEMENT_CAST(textContext.m_sink), kPlaybackRate, GST_FORMAT_TIME,
                                 GST_SEEK_FLAG_INSTANT_RATE_CHANGE, GST_SEEK_TYPE_NONE, kStart, GST_SEEK_TYPE_NONE,
                                 kStop));

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
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

TEST_F(GstreamerMseBaseSinkTests, ShouldSetSourcePosition)
{
    constexpr guint64 kPosition{1234};
    constexpr bool kResetTime{false};
    constexpr bool kAppliedRate = 1.0;
    constexpr uint64_t kStopPosition{GST_CLOCK_TIME_NONE};

    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);
    audioSink->priv->m_isFlushOngoing = true;

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);

    sendPlaybackStateNotification(audioSink, firebolt::rialto::PlaybackState::PAUSED);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    EXPECT_CALL(m_mediaPipelineMock, setSourcePosition(kSourceId, kPosition, kResetTime, kAppliedRate, kStopPosition))
        .WillOnce(Return(true));

    GstSegment *segment{gst_segment_new()};
    gst_segment_init(segment, GST_FORMAT_TIME);
    gst_segment_do_seek(segment, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_NONE, GST_SEEK_TYPE_SET, kPosition,
                        GST_SEEK_TYPE_SET, kStopPosition, nullptr);

    EXPECT_TRUE(rialto_mse_base_sink_event(audioSink->priv->m_sinkPad, GST_OBJECT_CAST(audioSink),
                                           gst_event_new_segment(segment)));
    EXPECT_EQ(GST_FORMAT_TIME, audioSink->priv->m_lastSegment.format);

    gst_segment_free(segment);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldSetSourcePositionWithResetTime)
{
    constexpr guint64 kPosition{1234};
    constexpr bool kResetTime{true};
    constexpr bool kAppliedRate = 1.0;
    constexpr uint64_t kStopPosition{GST_CLOCK_TIME_NONE};

    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);
    audioSink->priv->m_isFlushOngoing = true;

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);

    sendPlaybackStateNotification(audioSink, firebolt::rialto::PlaybackState::PAUSED);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    EXPECT_CALL(m_mediaPipelineMock, setSourcePosition(kSourceId, kPosition, kResetTime, kAppliedRate, kStopPosition))
        .WillOnce(Return(true));

    GstSegment *segment{gst_segment_new()};
    gst_segment_init(segment, GST_FORMAT_TIME);
    gst_segment_do_seek(segment, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, kPosition,
                        GST_SEEK_TYPE_SET, kStopPosition, nullptr);

    EXPECT_TRUE(rialto_mse_base_sink_event(audioSink->priv->m_sinkPad, GST_OBJECT_CAST(audioSink),
                                           gst_event_new_segment(segment)));
    EXPECT_EQ(GST_FORMAT_TIME, audioSink->priv->m_lastSegment.format);

    gst_segment_free(segment);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldSetSourcePositionWithNonDefaultAppliedRate)
{
    constexpr guint64 kPosition{1234};
    constexpr bool kResetTime{false};
    constexpr bool kAppliedRate = 5.0;
    constexpr uint64_t kStopPosition{GST_CLOCK_TIME_NONE};

    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);
    audioSink->priv->m_isFlushOngoing = true;

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);

    sendPlaybackStateNotification(audioSink, firebolt::rialto::PlaybackState::PAUSED);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    EXPECT_CALL(m_mediaPipelineMock, setSourcePosition(kSourceId, kPosition, kResetTime, kAppliedRate, kStopPosition))
        .WillOnce(Return(true));

    GstSegment *segment{gst_segment_new()};
    gst_segment_init(segment, GST_FORMAT_TIME);
    gst_segment_do_seek(segment, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_NONE, GST_SEEK_TYPE_SET, kPosition,
                        GST_SEEK_TYPE_SET, kStopPosition, nullptr);
    segment->applied_rate = kAppliedRate;

    EXPECT_TRUE(rialto_mse_base_sink_event(audioSink->priv->m_sinkPad, GST_OBJECT_CAST(audioSink),
                                           gst_event_new_segment(segment)));
    EXPECT_EQ(GST_FORMAT_TIME, audioSink->priv->m_lastSegment.format);

    gst_segment_free(segment);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
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
    allSourcesWillBeAttached();

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
    allSourcesWillBeAttached();

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
    GstStructure *structure{
        gst_structure_new("custom-instant-rate-change", "rate", G_TYPE_DOUBLE, kPlaybackRate, nullptr)};

    TestContext textContext = createPipelineWithAudioSinkAndSetToPaused();
    EXPECT_CALL(m_mediaPipelineMock, setPlaybackRate(kPlaybackRate)).WillOnce(Return(true));
    EXPECT_TRUE(rialto_mse_base_sink_event(textContext.m_sink->priv->m_sinkPad, GST_OBJECT_CAST(textContext.m_sink),
                                           gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure)));

    setNullState(textContext.m_pipeline, textContext.m_sourceId);
    gst_object_unref(textContext.m_pipeline);
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
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);

    EXPECT_TRUE(rialto_mse_base_sink_event(audioSink->priv->m_sinkPad, GST_OBJECT_CAST(audioSink),
                                           gst_event_new_flush_start()));
    EXPECT_TRUE(audioSink->priv->m_isFlushOngoing);
    EXPECT_FALSE(audioSink->priv->m_isEos);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
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

TEST_F(GstreamerMseBaseSinkTests, ShouldHandleFlushStopWithoutAttachedSource)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);
    audioSink->priv->m_isFlushOngoing = true;

    load(pipeline);
    EXPECT_EQ(GST_STATE_CHANGE_ASYNC, gst_element_set_state(pipeline, GST_STATE_PAUSED));

    EXPECT_TRUE(rialto_mse_base_sink_event(audioSink->priv->m_sinkPad, GST_OBJECT_CAST(audioSink),
                                           gst_event_new_flush_stop(kResetTime)));
    EXPECT_FALSE(audioSink->priv->m_isFlushOngoing);

    setNullState(pipeline, kUnknownSourceId);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldHandleFlushStop)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);
    audioSink->priv->m_isFlushOngoing = true;

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);

    sendPlaybackStateNotification(audioSink, firebolt::rialto::PlaybackState::PAUSED);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    EXPECT_CALL(m_mediaPipelineMock, flush(kSourceId, kResetTime)).WillOnce(Return(true));

    std::mutex flushMutex;
    std::condition_variable flushCond;
    bool flushFlag{false};
    std::thread t{[&]()
                  {
                      std::unique_lock<std::mutex> lock{flushMutex};
                      flushCond.wait_for(lock, std::chrono::milliseconds{500}, [&]() { return flushFlag; });
                      rialto_mse_base_handle_rialto_server_completed_flush(audioSink);
                  }};

    EXPECT_TRUE(rialto_mse_base_sink_event(audioSink->priv->m_sinkPad, GST_OBJECT_CAST(audioSink),
                                           gst_event_new_flush_stop(kResetTime)));
    EXPECT_FALSE(audioSink->priv->m_isFlushOngoing);

    {
        std::unique_lock<std::mutex> lock{flushMutex};
        flushFlag = true;
        flushCond.notify_one();
    }

    t.join();

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
    allSourcesWillBeAttached();
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
    allSourcesWillBeAttached();
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
    allSourcesWillBeAttached();
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
    allSourcesWillBeAttached();
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
    allSourcesWillBeAttached();
    GstCaps *caps{gst_caps_new_simple("audio/mpeg", "mpegversion", G_TYPE_INT, 4, "channels", G_TYPE_INT, kChannels,
                                      "rate", G_TYPE_INT, kRate, "stream-format", G_TYPE_STRING, kStreamFormat.c_str(),
                                      nullptr)};
    setCaps(audioSink, caps);

    EXPECT_TRUE(audioSink->priv->m_sourceAttached);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldAttachSourceWithHvcStreamFormat)
{
    const std::string kStreamFormat{"hvc1"};
    constexpr int32_t kWidth{1920};
    constexpr int32_t kHeight{1080};

    RialtoMSEBaseSink *videoSink = createVideoSink();
    GstElement *pipeline = createPipelineWithSink(videoSink);

    setPausedState(pipeline, videoSink);

    const firebolt::rialto::IMediaPipeline::MediaSourceVideo kExpectedSource{"video/h265",
                                                                             kHasDrm,
                                                                             kWidth,
                                                                             kHeight,
                                                                             firebolt::rialto::SegmentAlignment::UNDEFINED,
                                                                             firebolt::rialto::StreamFormat::HVC1};
    const int32_t kSourceId{videoSourceWillBeAttached(kExpectedSource)};
    allSourcesWillBeAttached();
    GstCaps *caps{gst_caps_new_simple("video/x-h265", "width", G_TYPE_INT, kWidth, "height", G_TYPE_INT, kHeight,
                                      "stream-format", G_TYPE_STRING, kStreamFormat.c_str(), nullptr)};
    setCaps(videoSink, caps);

    EXPECT_TRUE(videoSink->priv->m_sourceAttached);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldAttachSourceWithHevStreamFormat)
{
    const std::string kStreamFormat{"hev1"};
    constexpr int32_t kWidth{1920};
    constexpr int32_t kHeight{1080};

    RialtoMSEBaseSink *videoSink = createVideoSink();
    GstElement *pipeline = createPipelineWithSink(videoSink);

    setPausedState(pipeline, videoSink);

    const firebolt::rialto::IMediaPipeline::MediaSourceVideo kExpectedSource{"video/h265",
                                                                             kHasDrm,
                                                                             kWidth,
                                                                             kHeight,
                                                                             firebolt::rialto::SegmentAlignment::UNDEFINED,
                                                                             firebolt::rialto::StreamFormat::HEV1};
    const int32_t kSourceId{videoSourceWillBeAttached(kExpectedSource)};
    allSourcesWillBeAttached();
    GstCaps *caps{gst_caps_new_simple("video/x-h265", "width", G_TYPE_INT, kWidth, "height", G_TYPE_INT, kHeight,
                                      "stream-format", G_TYPE_STRING, kStreamFormat.c_str(), nullptr)};
    setCaps(videoSink, caps);

    EXPECT_TRUE(videoSink->priv->m_sourceAttached);

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
    allSourcesWillBeAttached();
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
    allSourcesWillBeAttached();
    GstCaps *caps{gst_caps_new_simple("audio/mpeg", "mpegversion", G_TYPE_INT, 4, "channels", G_TYPE_INT, kChannels,
                                      "rate", G_TYPE_INT, kRate, "alignment", G_TYPE_STRING, kAlignment.c_str(), nullptr)};
    setCaps(audioSink, caps);

    EXPECT_TRUE(audioSink->priv->m_sourceAttached);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldPostDecryptError)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    audioSink->priv->m_callbacks.errorCallback(firebolt::rialto::PlaybackError::DECRYPTION);

    GstMessage *receivedMessage{getMessage(pipeline, GST_MESSAGE_ERROR)};
    ASSERT_NE(receivedMessage, nullptr);

    GError *err = nullptr;
    gchar *debug = nullptr;
    gst_message_parse_error(receivedMessage, &err, &debug);
    EXPECT_EQ(err->domain, GST_STREAM_ERROR);
    EXPECT_EQ(err->code, GST_STREAM_ERROR_DECRYPT);
    EXPECT_NE(err->message, nullptr);
    EXPECT_NE(debug, nullptr);

    g_free(debug);
    g_error_free(err);
    gst_message_unref(receivedMessage);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, LostStateWhenTransitioningToPlaying)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);

    sendPlaybackStateNotification(audioSink, firebolt::rialto::PlaybackState::PAUSED);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    GST_STATE(audioSink) = GST_STATE_PAUSED;
    GST_STATE_NEXT(audioSink) = GST_STATE_PLAYING;
    GST_STATE_PENDING(audioSink) = GST_STATE_PLAYING;
    GST_STATE_RETURN(audioSink) = GST_STATE_CHANGE_ASYNC;

    rialto_mse_base_sink_lost_state(audioSink);

    EXPECT_CALL(m_mediaPipelineMock, play()).WillOnce(Return(true));
    audioSink->priv->m_callbacks.stateChangedCallback(firebolt::rialto::PlaybackState::PAUSED);

    pipelineWillGoToPausedState(audioSink); // PLAYING -> PAUSED
    gst_caps_unref(caps);
    setNullState(pipeline, kSourceId);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldPostGenericError)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    audioSink->priv->m_callbacks.errorCallback(firebolt::rialto::PlaybackError::UNKNOWN);

    GstMessage *receivedMessage{getMessage(pipeline, GST_MESSAGE_ERROR)};
    ASSERT_NE(receivedMessage, nullptr);

    GError *err = nullptr;
    gchar *debug = nullptr;
    gst_message_parse_error(receivedMessage, &err, &debug);
    EXPECT_EQ(err->domain, GST_STREAM_ERROR);
    EXPECT_EQ(err->code, 0);
    EXPECT_NE(err->message, nullptr);
    EXPECT_NE(debug, nullptr);

    g_free(debug);
    g_error_free(err);
    gst_message_unref(receivedMessage);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldFailToHandleStreamCollectionEvent)
{
    GstStreamCollection *streamCollection{gst_stream_collection_new("test_stream")};

    RialtoMSEBaseSink *audioSink{createAudioSink()};

    EXPECT_FALSE(rialto_mse_base_sink_event(audioSink->priv->m_sinkPad, GST_OBJECT_CAST(audioSink),
                                            gst_event_new_stream_collection(streamCollection)));
    gst_object_unref(audioSink);
    gst_object_unref(streamCollection);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldHandleStreamCollectionEventAllAttached)
{
    GstStreamCollection *streamCollection{gst_stream_collection_new("test_stream")};
    gst_stream_collection_add_stream(streamCollection,
                                     gst_stream_new("s_audio", nullptr, GST_STREAM_TYPE_AUDIO, GST_STREAM_FLAG_NONE));

    RialtoMSEBaseSink *audioSink{createAudioSink()};
    GstElement *pipeline{createPipelineWithSink(audioSink)};

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);

    EXPECT_TRUE(rialto_mse_base_sink_event(audioSink->priv->m_sinkPad, GST_OBJECT_CAST(audioSink),
                                           gst_event_new_stream_collection(streamCollection)));

    setNullState(pipeline, kSourceId);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
    gst_object_unref(streamCollection);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldHandleStreamCollectionEventNotAllAttached)
{
    GstStreamCollection *streamCollection{gst_stream_collection_new("test_stream")};
    gst_stream_collection_add_stream(streamCollection,
                                     gst_stream_new("s_audio", nullptr, GST_STREAM_TYPE_AUDIO, GST_STREAM_FLAG_NONE));
    gst_stream_collection_add_stream(streamCollection,
                                     gst_stream_new("s_video", nullptr, GST_STREAM_TYPE_VIDEO, GST_STREAM_FLAG_NONE));
    gst_stream_collection_add_stream(streamCollection,
                                     gst_stream_new("s_text", nullptr, GST_STREAM_TYPE_TEXT, GST_STREAM_FLAG_NONE));

    RialtoMSEBaseSink *audioSink{createAudioSink()};
    GstElement *pipeline{createPipelineWithSink(audioSink)};

    load(pipeline);
    EXPECT_EQ(GST_STATE_CHANGE_ASYNC, gst_element_set_state(pipeline, GST_STATE_PAUSED));

    EXPECT_TRUE(rialto_mse_base_sink_event(audioSink->priv->m_sinkPad, GST_OBJECT_CAST(audioSink),
                                           gst_event_new_stream_collection(streamCollection)));
    setNullState(pipeline, kUnknownSourceId);
    gst_object_unref(pipeline);
    gst_object_unref(streamCollection);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldHandleGstContextStreamsInfoAllAttached)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    GstElement *pipeline = gst_pipeline_new("test-pipeline");
    gst_bin_add(GST_BIN(pipeline), GST_ELEMENT_CAST(audioSink));

    GstContext *context = gst_context_new("streams-info", false);
    GstStructure *contextStructure = gst_context_writable_structure(context);
    gst_structure_set(contextStructure, "video-streams", G_TYPE_UINT, 0x0u, "audio-streams", G_TYPE_UINT, 0x1u,
                      "text-streams", G_TYPE_UINT, 0x0u, nullptr);
    gst_element_set_context(GST_ELEMENT(pipeline), context);

    setPausedState(pipeline, audioSink);
    const int32_t kSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createAudioCaps()};
    setCaps(audioSink, caps);

    setNullState(pipeline, kSourceId);
    gst_caps_unref(caps);
    gst_context_unref(context);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldHandleDefaultStreamSetting)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    RialtoMSEBaseSink *videoSink = createVideoSink();

    GstElement *pipeline = gst_pipeline_new("test-pipeline");
    gst_bin_add(GST_BIN(pipeline), GST_ELEMENT_CAST(audioSink));
    gst_bin_add(GST_BIN(pipeline), GST_ELEMENT_CAST(videoSink));

    setPausedState(pipeline, audioSink);
    const int32_t kAudioSourceId{audioSourceWillBeAttached(createAudioMediaSource())};
    const int32_t kVideoSourceId{videoSourceWillBeAttached(createVideoMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *audioCaps{createAudioCaps()};
    GstCaps *videoCaps{createVideoCaps()};
    setCaps(audioSink, audioCaps);
    setCaps(videoSink, videoCaps);

    EXPECT_CALL(m_mediaPipelineMock, removeSource(kAudioSourceId)).WillOnce(Return(true));
    EXPECT_CALL(m_mediaPipelineMock, removeSource(kVideoSourceId)).WillOnce(Return(true));
    EXPECT_CALL(m_mediaPipelineMock, stop()).WillOnce(Return(true));
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_caps_unref(audioCaps);
    gst_caps_unref(videoCaps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldHandleGstContextStreamsInfoNotAllAttached)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    GstElement *pipeline = gst_pipeline_new("test-pipeline");
    gst_bin_add(GST_BIN(pipeline), GST_ELEMENT_CAST(audioSink));

    GstContext *context = gst_context_new("streams-info", false);
    GstStructure *contextStructure = gst_context_writable_structure(context);
    gst_structure_set(contextStructure, "video-streams", G_TYPE_UINT, 0x1u, "audio-streams", G_TYPE_UINT, 0x1u,
                      "text-streams", G_TYPE_UINT, 0x1u, nullptr);
    gst_element_set_context(GST_ELEMENT(pipeline), context);

    load(pipeline);
    EXPECT_EQ(GST_STATE_CHANGE_ASYNC, gst_element_set_state(pipeline, GST_STATE_PAUSED));

    setNullState(pipeline, kUnknownSourceId);
    gst_context_unref(context);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseBaseSinkTests, ShouldHandleGstContextStreamsInfoStreamsNumberToBig)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();

    GstElement *pipeline = gst_pipeline_new("test-pipeline");
    gst_bin_add(GST_BIN(pipeline), GST_ELEMENT_CAST(audioSink));

    GstContext *context = gst_context_new("streams-info", false);
    GstStructure *contextStructure = gst_context_writable_structure(context);
    gst_structure_set(contextStructure, "video-streams", G_TYPE_UINT, 0xffffffff, "audio-streams", G_TYPE_UINT, 0x1u,
                      "text-streams", G_TYPE_UINT, 0x1u, nullptr);
    gst_element_set_context(GST_ELEMENT(pipeline), context);

    load(pipeline);
    EXPECT_EQ(GST_STATE_CHANGE_FAILURE, gst_element_set_state(pipeline, GST_STATE_PAUSED));

    EXPECT_CALL(m_mediaPipelineMock, stop()).WillOnce(Return(true));
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_context_unref(context);
    gst_object_unref(pipeline);
}
