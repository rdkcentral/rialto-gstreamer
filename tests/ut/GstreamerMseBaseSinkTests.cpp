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
    ;
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);

    EXPECT_FALSE(gst_element_seek(GST_ELEMENT_CAST(audioSink), kPlaybackRate, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
                                  GST_SEEK_TYPE_SET, kStart, GST_SEEK_TYPE_NONE, kStop));

    setNullState(pipeline, kUnknownSourceId);
    gst_object_unref(pipeline);
}
