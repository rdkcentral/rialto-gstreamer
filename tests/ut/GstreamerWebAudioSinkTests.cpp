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
#include "RialtoGstTest.h"
#include "WebAudioPlayerMock.h"

using firebolt::rialto::IWebAudioPlayerFactory;
using firebolt::rialto::WebAudioPlayerFactoryMock;
using firebolt::rialto::WebAudioPlayerMock;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgReferee;
using testing::StrictMock;

namespace
{
constexpr int kChannels{1};
constexpr int kRate{48000};
const std::string kMimeType{"audio/x-raw"};
const std::string kFormat{"S12BE"};
constexpr uint32_t kPriority{1};
constexpr uint32_t kFrames{18};
constexpr uint32_t kMaximumFrames{12};
constexpr bool kSupportDeferredPlay{true};
} // namespace

class GstreamerWebAudioSinkTests : public RialtoGstTest
{
public:
    GstreamerWebAudioSinkTests() = default;
    ~GstreamerWebAudioSinkTests() = default;

    void setPaused(GstElement *pipeline)
    {
        EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_PAUSED));
    }

    void setNull(GstElement *pipeline)
    {
        EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_NULL));
    }

    void setPlaying(GstElement *pipeline)
    {
        EXPECT_CALL(m_playerMock, play()).WillOnce(Return(true));
        EXPECT_EQ(GST_STATE_CHANGE_ASYNC, gst_element_set_state(pipeline, GST_STATE_PLAYING));
    }

    void sendPlayingNotification(GstElement *pipeline, RialtoWebAudioSink *sink)
    {
        ASSERT_TRUE(sink->priv->m_webAudioClient);
        sink->priv->m_webAudioClient->notifyState(firebolt::rialto::WebAudioPlayerState::PLAYING);
        EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));
    }

    void willPerformPlayingToPausedTransition() { EXPECT_CALL(m_playerMock, pause()).WillOnce(Return(true)); }

    void attachSource(RialtoWebAudioSink *sink)
    {
        GstCaps *caps = gst_caps_new_simple(kMimeType.c_str(), "rate", G_TYPE_INT, kRate, "channels", G_TYPE_INT,
                                            kChannels, "format", G_TYPE_STRING, kFormat.c_str(), nullptr);
        EXPECT_CALL(m_playerMock, getDeviceInfo(_, _, _))
            .WillOnce(DoAll(SetArgReferee<0>(kFrames), SetArgReferee<1>(kMaximumFrames),
                            SetArgReferee<2>(kSupportDeferredPlay), Return(true)));
        EXPECT_CALL(*m_playerFactoryMock, createWebAudioPlayer(_, kMimeType, kPriority, _))
            .WillOnce(Return(ByMove(std::move(m_player))));
        setCaps(sink, caps);
        gst_caps_unref(caps);
    }

    std::shared_ptr<StrictMock<WebAudioPlayerFactoryMock>> m_playerFactoryMock{
        std::dynamic_pointer_cast<StrictMock<WebAudioPlayerFactoryMock>>(IWebAudioPlayerFactory::createFactory())};
    std::unique_ptr<StrictMock<WebAudioPlayerMock>> m_player{std::make_unique<StrictMock<WebAudioPlayerMock>>()};
    StrictMock<WebAudioPlayerMock> &m_playerMock{*m_player};
};

TEST_F(GstreamerWebAudioSinkTests, ShouldCreateSink)
{
    RialtoWebAudioSink *sink{createWebAudioSink()};
    EXPECT_TRUE(sink);
    gst_object_unref(sink);
}

TEST_F(GstreamerWebAudioSinkTests, ShouldNotReachReadyStateWhenAppStateIsInactive)
{
    EXPECT_CALL(*m_controlFactoryMock, createControl()).WillOnce(Return(m_controlMock));
    EXPECT_CALL(*m_controlMock, registerClient(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(firebolt::rialto::ApplicationState::INACTIVE), Return(true)));
    GstElement *sink = gst_element_factory_make("rialtowebaudiosink", "rialtowebaudiosink");
    GstElement *pipeline = createPipelineWithSink(RIALTO_WEB_AUDIO_SINK(sink));

    EXPECT_EQ(GST_STATE_CHANGE_FAILURE, gst_element_set_state(pipeline, GST_STATE_READY));

    gst_object_unref(pipeline);
}

TEST_F(GstreamerWebAudioSinkTests, ShouldFailToAttachSource)
{
    RialtoWebAudioSink *sink{createWebAudioSink()};
    GstElement *pipeline = createPipelineWithSink(sink);

    setPaused(pipeline);

    GstCaps *caps = gst_caps_new_simple(kMimeType.c_str(), "rate", G_TYPE_INT, kRate, nullptr);
    setCaps(sink, caps);
    gst_caps_unref(caps);

    setNull(pipeline);

    gst_object_unref(pipeline);
}

TEST_F(GstreamerWebAudioSinkTests, ShouldAttachSource)
{
    RialtoWebAudioSink *sink{createWebAudioSink()};
    GstElement *pipeline = createPipelineWithSink(sink);

    setPaused(pipeline);
    attachSource(sink);
    setNull(pipeline);

    gst_object_unref(pipeline);
}

TEST_F(GstreamerWebAudioSinkTests, ShouldFailToReachPlayingState)
{
    RialtoWebAudioSink *sink{createWebAudioSink()};
    GstElement *pipeline = createPipelineWithSink(sink);

    setPaused(pipeline);
    attachSource(sink);

    EXPECT_CALL(m_playerMock, play()).WillOnce(Return(false));
    EXPECT_EQ(GST_STATE_CHANGE_FAILURE, gst_element_set_state(pipeline, GST_STATE_PLAYING));

    setNull(pipeline);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerWebAudioSinkTests, ShouldReachPlayingState)
{
    RialtoWebAudioSink *sink{createWebAudioSink()};
    GstElement *pipeline = createPipelineWithSink(sink);

    setPaused(pipeline);
    attachSource(sink);

    setPlaying(pipeline);
    sendPlayingNotification(pipeline, sink);

    willPerformPlayingToPausedTransition();
    setNull(pipeline);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerWebAudioSinkTests, ShouldDelayTransitionToPlayingWhenSourceIsNotAttached)
{
    RialtoWebAudioSink *sink{createWebAudioSink()};
    GstElement *pipeline = createPipelineWithSink(sink);

    setPaused(pipeline);

    setPlaying(pipeline);
    attachSource(sink);
    sendPlayingNotification(pipeline, sink);

    willPerformPlayingToPausedTransition();
    setNull(pipeline);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerWebAudioSinkTests, ShouldDelayTransitionToPlayingWhenSourceIsNotAttachedAndFail)
{
    RialtoWebAudioSink *sink{createWebAudioSink()};
    GstElement *pipeline = createPipelineWithSink(sink);

    setPaused(pipeline);

    EXPECT_EQ(GST_STATE_CHANGE_ASYNC, gst_element_set_state(pipeline, GST_STATE_PLAYING));
    EXPECT_CALL(m_playerMock, play()).WillOnce(Return(false));
    attachSource(sink);

    willPerformPlayingToPausedTransition();
    setNull(pipeline);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerWebAudioSinkTests, ShouldFailToPause)
{
    RialtoWebAudioSink *sink{createWebAudioSink()};
    GstElement *pipeline = createPipelineWithSink(sink);

    setPaused(pipeline);
    attachSource(sink);

    setPlaying(pipeline);
    sendPlayingNotification(pipeline, sink);

    EXPECT_CALL(m_playerMock, pause()).WillOnce(Return(false));
    EXPECT_EQ(GST_STATE_CHANGE_FAILURE, gst_element_set_state(pipeline, GST_STATE_PAUSED));

    willPerformPlayingToPausedTransition();
    setNull(pipeline);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerWebAudioSinkTests, ShouldSetEos)
{
    RialtoWebAudioSink *sink{createWebAudioSink()};
    GstElement *pipeline = createPipelineWithSink(sink);

    setPaused(pipeline);
    attachSource(sink);
    setPlaying(pipeline);
    sendPlayingNotification(pipeline, sink);

    sink->priv->m_webAudioClient->notifyState(firebolt::rialto::WebAudioPlayerState::END_OF_STREAM);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_EOS));

    willPerformPlayingToPausedTransition();
    setNull(pipeline);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerWebAudioSinkTests, ShouldFailToSetEosWhenBelowPaused)
{
    RialtoWebAudioSink *sink{createWebAudioSink()};
    GstElement *pipeline = createPipelineWithSink(sink);

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_READY));

    sink->priv->m_webAudioClient->notifyState(firebolt::rialto::WebAudioPlayerState::END_OF_STREAM);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ERROR));

    setNull(pipeline);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerWebAudioSinkTests, ShouldHandleError)
{
    RialtoWebAudioSink *sink{createWebAudioSink()};
    GstElement *pipeline = createPipelineWithSink(sink);

    setPaused(pipeline);
    attachSource(sink);

    sink->priv->m_webAudioClient->notifyState(firebolt::rialto::WebAudioPlayerState::FAILURE);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ERROR));

    setNull(pipeline);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerWebAudioSinkTests, ShouldFailToSendCapsEventWhenPadIsNotLinked)
{
    RialtoWebAudioSink *sink{createWebAudioSink()};
    GstElement *pipeline = createPipelineWithSink(sink);

    setPaused(pipeline);
    attachSource(sink);

    GstCaps *caps = gst_caps_new_empty_simple(kMimeType.c_str());

    EXPECT_FALSE(gst_element_send_event(GST_ELEMENT_CAST(sink), gst_event_new_caps(caps)));

    setNull(pipeline);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerWebAudioSinkTests, ShouldHandleEosEvent)
{
    RialtoWebAudioSink *sink{createWebAudioSink()};
    GstElement *pipeline = createPipelineWithSink(sink);

    setPaused(pipeline);
    attachSource(sink);

    setPlaying(pipeline);
    sendPlayingNotification(pipeline, sink);

    EXPECT_CALL(m_playerMock, setEos()).WillOnce(Return(true));
    GstPad *sinkPad = gst_element_get_static_pad(GST_ELEMENT_CAST(sink), "sink");
    ASSERT_TRUE(sinkPad);
    gst_pad_send_event(sinkPad, gst_event_new_eos());

    willPerformPlayingToPausedTransition();
    setNull(pipeline);
    gst_object_unref(sinkPad);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerWebAudioSinkTests, ShouldHandleUnknownEvent)
{
    RialtoWebAudioSink *sink{createWebAudioSink()};
    GstElement *pipeline = createPipelineWithSink(sink);

    setPaused(pipeline);
    attachSource(sink);

    setPlaying(pipeline);
    sendPlayingNotification(pipeline, sink);

    GstPad *sinkPad = gst_element_get_static_pad(GST_ELEMENT_CAST(sink), "sink");
    ASSERT_TRUE(sinkPad);
    gst_pad_send_event(sinkPad, gst_event_new_gap(1, 1));

    willPerformPlayingToPausedTransition();
    setNull(pipeline);
    gst_object_unref(sinkPad);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerWebAudioSinkTests, ShouldGetAndSetTsOffsetProperty)
{
    RialtoWebAudioSink *sink{createWebAudioSink()};

    constexpr int64_t kValue{1234};
    // Set is not supported, so value should not be changed
    g_object_set(sink, "ts-offset", kValue, nullptr);

    int64_t value{0};
    g_object_get(sink, "ts-offset", &value, nullptr);
    EXPECT_EQ(0, value); // Default value should be returned

    gst_object_unref(sink);
}

TEST_F(GstreamerWebAudioSinkTests, ShouldFailToGetOrSetUnknownProperty)
{
    RialtoWebAudioSink *sink{createWebAudioSink()};
    g_object_class_install_property(G_OBJECT_GET_CLASS(sink), 123,
                                    g_param_spec_boolean("surprise", "surprise", "surprise", FALSE,
                                                         GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    constexpr bool kValue{true};
    // Set should do nothing
    g_object_set(sink, "surprise", kValue, nullptr);

    bool value{false};
    g_object_get(sink, "surprise", &value, nullptr);
    EXPECT_FALSE(value); // Default value should be returned

    gst_object_unref(sink);
}

TEST_F(GstreamerWebAudioSinkTests, ShouldNotifyNewSample)
{
    constexpr uint32_t kAvailableFrames{24};
    RialtoWebAudioSink *sink{createWebAudioSink()};
    GstElement *pipeline = createPipelineWithSink(sink);
    GstBuffer *buffer{gst_buffer_new()};

    setPaused(pipeline);
    attachSource(sink);

    setPlaying(pipeline);
    sendPlayingNotification(pipeline, sink);

    EXPECT_CALL(m_playerMock, getBufferAvailable(_, _)).WillOnce(DoAll(SetArgReferee<0>(kAvailableFrames), Return(true)));
    GstPad *sinkPad = gst_element_get_static_pad(GST_ELEMENT_CAST(sink), "sink");
    ASSERT_TRUE(sinkPad);
    EXPECT_EQ(GST_FLOW_OK, gst_pad_chain(sinkPad, buffer));

    willPerformPlayingToPausedTransition();
    setNull(pipeline);
    gst_object_unref(sinkPad);
    gst_object_unref(pipeline);
}
