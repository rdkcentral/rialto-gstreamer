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
#include "RialtoGStreamerMSEVideoSink.h"
#include "RialtoGStreamerMSEVideoSinkPrivate.h"
#include "RialtoGstTest.h"

using testing::_;
using testing::Return;

namespace
{
constexpr bool kHasDrm{true};
constexpr int32_t kWidth{1920};
constexpr int32_t kHeight{1080};
constexpr bool kFrameStepOnPreroll{true};
constexpr int32_t kUnknownSourceId{-1};
const std::string kDefaultWindowSet{"0,0,1920,1080"};
const std::string kCustomWindowSet{"20,40,640,480"};
} // namespace

class GstreamerMseVideoSinkTests : public RialtoGstTest
{
public:
    GstreamerMseVideoSinkTests() = default;
    ~GstreamerMseVideoSinkTests() override = default;

    GstCaps *createDefaultCaps() const
    {
        return gst_caps_new_simple("video/x-h264", "width", G_TYPE_INT, kWidth, "height", G_TYPE_INT, kHeight, nullptr);
    }

    firebolt::rialto::IMediaPipeline::MediaSourceVideo createDefaultMediaSource() const
    {
        return firebolt::rialto::IMediaPipeline::MediaSourceVideo{"video/h264", kHasDrm, kWidth, kHeight};
    }
};

TEST_F(GstreamerMseVideoSinkTests, ShouldFailToReachPausedStateWhenMediaPipelineCantBeCreated)
{
    constexpr firebolt::rialto::VideoRequirements kDefaultRequirements{3840, 2160};
    RialtoMSEBaseSink *videoSink = createVideoSink();
    GstElement *pipeline = createPipelineWithSink(videoSink);

    EXPECT_CALL(*m_mediaPipelineFactoryMock, createMediaPipeline(_, kDefaultRequirements)).WillOnce(Return(nullptr));
    EXPECT_EQ(GST_STATE_CHANGE_FAILURE, gst_element_set_state(pipeline, GST_STATE_PAUSED));
    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_NULL));

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseVideoSinkTests, ShouldNotHandleUnknownEvent)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();
    GstElement *pipeline = createPipelineWithSink(videoSink);

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_READY));

    gst_pad_set_active(videoSink->priv->m_sinkPad, TRUE);
    gst_pad_send_event(videoSink->priv->m_sinkPad, gst_event_new_gap(1, 1));

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_NULL));

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseVideoSinkTests, ShouldNotAttachSourceWhenPipelineIsBelowPausedState)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();
    GstElement *pipeline = createPipelineWithSink(videoSink);

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_READY));

    gst_pad_set_active(videoSink->priv->m_sinkPad, TRUE);
    GstCaps *caps{createDefaultCaps()};
    setCaps(videoSink, caps);

    EXPECT_FALSE(videoSink->priv->m_sourceAttached);

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_NULL));

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseVideoSinkTests, ShouldAttachSourceWithH264)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();
    GstElement *pipeline = createPipelineWithSink(videoSink);

    setPausedState(pipeline, videoSink);
    const int32_t kSourceId{videoSourceWillBeAttached(createDefaultMediaSource())};

    GstCaps *caps{createDefaultCaps()};
    setCaps(videoSink, caps);

    EXPECT_TRUE(videoSink->priv->m_sourceAttached);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseVideoSinkTests, ShouldNotAttachSourceTwice)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();
    GstElement *pipeline = createPipelineWithSink(videoSink);

    setPausedState(pipeline, videoSink);
    const int32_t kSourceId{videoSourceWillBeAttached(createDefaultMediaSource())};

    GstCaps *caps{createDefaultCaps()};
    setCaps(videoSink, caps);
    setCaps(videoSink, caps);

    EXPECT_TRUE(videoSink->priv->m_sourceAttached);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseVideoSinkTests, ShouldAttachSourceWithVp9)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();
    GstElement *pipeline = createPipelineWithSink(videoSink);

    setPausedState(pipeline, videoSink);
    const int32_t kSourceId{videoSourceWillBeAttached(
        firebolt::rialto::IMediaPipeline::MediaSourceVideo{"video/x-vp9", kHasDrm, kWidth, kHeight})};

    GstCaps *caps{gst_caps_new_simple("video/x-vp9", "width", G_TYPE_INT, kWidth, "height", G_TYPE_INT, kHeight, nullptr)};
    setCaps(videoSink, caps);

    EXPECT_TRUE(videoSink->priv->m_sourceAttached);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseVideoSinkTests, ShouldAttachSourceWithH265)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();
    GstElement *pipeline = createPipelineWithSink(videoSink);

    setPausedState(pipeline, videoSink);
    const int32_t kSourceId{videoSourceWillBeAttached(
        firebolt::rialto::IMediaPipeline::MediaSourceVideo{"video/h265", kHasDrm, kWidth, kHeight})};

    GstCaps *caps{
        gst_caps_new_simple("video/x-h265", "width", G_TYPE_INT, kWidth, "height", G_TYPE_INT, kHeight, nullptr)};
    setCaps(videoSink, caps);

    EXPECT_TRUE(videoSink->priv->m_sourceAttached);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseVideoSinkTests, ShouldAttachSourceWithDolbyVision)
{
    constexpr unsigned kDvProfile{123};
    RialtoMSEBaseSink *videoSink = createVideoSink();
    GstElement *pipeline = createPipelineWithSink(videoSink);

    setPausedState(pipeline, videoSink);
    const int32_t kSourceId{dolbyVisionSourceWillBeAttached(
        firebolt::rialto::IMediaPipeline::MediaSourceVideoDolbyVision{"video/h265", kDvProfile, kHasDrm, kWidth,
                                                                      kHeight})};

    GstCaps *caps{gst_caps_new_simple("video/x-h265", "width", G_TYPE_INT, kWidth, "height", G_TYPE_INT, kHeight,
                                      "dovi-stream", G_TYPE_BOOLEAN, TRUE, "dv_profile", G_TYPE_UINT, kDvProfile,
                                      nullptr)};
    setCaps(videoSink, caps);

    EXPECT_TRUE(videoSink->priv->m_sourceAttached);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseVideoSinkTests, ShouldReachPausedState)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();
    GstElement *pipeline = createPipelineWithSink(videoSink);

    setPausedState(pipeline, videoSink);
    const int32_t kSourceId{videoSourceWillBeAttached(createDefaultMediaSource())};

    GstCaps *caps{createDefaultCaps()};
    setCaps(videoSink, caps);

    sendPlaybackStateNotification(videoSink, firebolt::rialto::PlaybackState::PAUSED);

    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseVideoSinkTests, ShouldFailToGetRectanglePropertyWhenPipelineIsBelowPausedState)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();

    gchar *rectangle{nullptr};
    g_object_get(videoSink, "rectangle", rectangle, nullptr);
    EXPECT_FALSE(rectangle);

    gst_object_unref(videoSink);
}

TEST_F(GstreamerMseVideoSinkTests, ShouldGetRectangleProperty)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();
    GstElement *pipeline = createPipelineWithSink(videoSink);

    setPausedState(pipeline, videoSink);

    gchar *rectangle{nullptr};
    g_object_get(videoSink, "rectangle", &rectangle, nullptr);
    ASSERT_TRUE(rectangle);
    EXPECT_EQ(std::string(rectangle), kDefaultWindowSet);

    g_free(rectangle);
    setNullState(pipeline, kUnknownSourceId);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseVideoSinkTests, ShouldGetMaxVideoWidthProperty)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();
    RIALTO_MSE_VIDEO_SINK(videoSink)->priv->maxWidth = kWidth;

    unsigned maxVideoWidth{0};
    g_object_get(videoSink, "maxVideoWidth", &maxVideoWidth, nullptr);
    EXPECT_EQ(kWidth, maxVideoWidth);

    gst_object_unref(videoSink);
}

TEST_F(GstreamerMseVideoSinkTests, ShouldGetMaxVideoHeightProperty)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();
    RIALTO_MSE_VIDEO_SINK(videoSink)->priv->maxHeight = kHeight;

    unsigned maxVideoHeight{0};
    g_object_get(videoSink, "maxVideoHeight", &maxVideoHeight, nullptr);
    EXPECT_EQ(maxVideoHeight, kHeight);

    gst_object_unref(videoSink);
}

TEST_F(GstreamerMseVideoSinkTests, ShouldGetFrameStepOnPrerollProperty)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();
    RIALTO_MSE_VIDEO_SINK(videoSink)->priv->stepOnPrerollEnabled = kFrameStepOnPreroll;

    bool frameStepOnPreroll{false};
    g_object_get(videoSink, "frame-step-on-preroll", &frameStepOnPreroll, nullptr);
    EXPECT_EQ(frameStepOnPreroll, kFrameStepOnPreroll);

    gst_object_unref(videoSink);
}

TEST_F(GstreamerMseVideoSinkTests, ShouldFailToSetRectanglePropertyWhenPipelineIsBelowPausedState)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();

    g_object_set(videoSink, "rectangle", kCustomWindowSet.c_str(), nullptr);

    gst_object_unref(videoSink);
}

TEST_F(GstreamerMseVideoSinkTests, ShouldSetRectangleProperty)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();
    GstElement *pipeline = createPipelineWithSink(videoSink);

    setPausedState(pipeline, videoSink);

    EXPECT_CALL(m_mediaPipelineMock, setVideoWindow(20, 40, 640, 480)).WillOnce(Return(true));
    g_object_set(videoSink, "rectangle", kCustomWindowSet.c_str(), nullptr);

    gchar *rectangle{nullptr};
    g_object_get(videoSink, "rectangle", &rectangle, nullptr);
    ASSERT_TRUE(rectangle);
    EXPECT_EQ(std::string(rectangle), kCustomWindowSet);

    g_free(rectangle);
    setNullState(pipeline, kUnknownSourceId);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseVideoSinkTests, ShouldSetMaxVideoWidthProperty)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();

    g_object_set(videoSink, "maxVideoWidth", kWidth, nullptr);
    EXPECT_EQ(RIALTO_MSE_VIDEO_SINK(videoSink)->priv->maxWidth, kWidth);

    gst_object_unref(videoSink);
}

TEST_F(GstreamerMseVideoSinkTests, ShouldSetMaxVideoHeightProperty)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();

    g_object_set(videoSink, "maxVideoHeight", kHeight, nullptr);
    EXPECT_EQ(RIALTO_MSE_VIDEO_SINK(videoSink)->priv->maxHeight, kHeight);

    gst_object_unref(videoSink);
}

TEST_F(GstreamerMseVideoSinkTests, ShouldFailToSetFrameStepOnPrerollPropertyWhenPipelineIsBelowPausedState)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();

    g_object_set(videoSink, "frame-step-on-preroll", kFrameStepOnPreroll, nullptr);

    gst_object_unref(videoSink);
}

TEST_F(GstreamerMseVideoSinkTests, ShouldSetFrameStepOnPrerollProperty)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();
    GstElement *pipeline = createPipelineWithSink(videoSink);

    setPausedState(pipeline, videoSink);

    EXPECT_CALL(m_mediaPipelineMock, renderFrame()).WillOnce(Return(true));
    g_object_set(videoSink, "frame-step-on-preroll", kFrameStepOnPreroll, nullptr);

    setNullState(pipeline, kUnknownSourceId);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseVideoSinkTests, ShouldNotRenderFrameTwice)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();
    GstElement *pipeline = createPipelineWithSink(videoSink);

    setPausedState(pipeline, videoSink);

    EXPECT_CALL(m_mediaPipelineMock, renderFrame()).WillOnce(Return(true));
    g_object_set(videoSink, "frame-step-on-preroll", kFrameStepOnPreroll, nullptr);
    g_object_set(videoSink, "frame-step-on-preroll", kFrameStepOnPreroll, nullptr);

    setNullState(pipeline, kUnknownSourceId);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseVideoSinkTests, ShouldFailToGetOrSetUnknownProperty)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();

    g_object_class_install_property(G_OBJECT_GET_CLASS(videoSink), 123,
                                    g_param_spec_boolean("surprise", "surprise", "surprise", FALSE,
                                                         GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    gboolean value{FALSE};
    g_object_get(videoSink, "surprise", &value, nullptr);
    EXPECT_FALSE(value);

    constexpr gboolean kValue{FALSE};
    g_object_set(videoSink, "surprise", kValue, nullptr);

    gst_object_unref(videoSink);
}

TEST_F(GstreamerMseVideoSinkTests, ShouldSendQosEvent)
{
    RialtoMSEBaseSink *videoSink = createVideoSink();
    GstElement *pipeline = createPipelineWithSink(videoSink);

    setPausedState(pipeline, videoSink);
    const int32_t kSourceId{videoSourceWillBeAttached(createDefaultMediaSource())};

    GstCaps *caps{createDefaultCaps()};
    setCaps(videoSink, caps);

    sendPlaybackStateNotification(videoSink, firebolt::rialto::PlaybackState::PAUSED);

    auto mediaPlayerClient{videoSink->priv->m_mediaPlayerManager.getMediaPlayerClient()};
    ASSERT_TRUE(mediaPlayerClient);
    const firebolt::rialto::QosInfo kQosInfo{1, 2};
    mediaPlayerClient->notifyQos(kSourceId, kQosInfo);

    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_QOS));

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}
