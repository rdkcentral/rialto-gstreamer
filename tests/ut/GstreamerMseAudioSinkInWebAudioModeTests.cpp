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
#include "RialtoGStreamerMSEBaseSinkPrivate.h"
#include "RialtoGstTest.h"
#include "WebAudioPlayerMock.h"

using firebolt::rialto::IWebAudioPlayerClient;
using firebolt::rialto::IWebAudioPlayerFactory;
using firebolt::rialto::WebAudioPlayerFactoryMock;
using firebolt::rialto::WebAudioPlayerMock;
using firebolt::rialto::WebAudioPlayerState;

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;
using testing::SetArgReferee;
using testing::StrictMock;

namespace
{
constexpr int kChannels{1};
constexpr int kRate{48000};
const std::string kFormat{"S12BE"};
constexpr uint32_t kPriority{1};
constexpr uint32_t kFrames{18};
constexpr uint32_t kMaximumFrames{12};
constexpr bool kSupportDeferredPlay{true};
} // namespace

class GstreamerMseAudioSinkInWebAudioModeTests : public RialtoGstTest
{
public:
    GstreamerMseAudioSinkInWebAudioModeTests() = default;
    ~GstreamerMseAudioSinkInWebAudioModeTests() override = default;

    void attachWebAudioSource(RialtoMSEBaseSink *sink)
    {
        const std::string kMimeType{"audio/x-raw"};
        GstCaps *caps = gst_caps_new_simple(kMimeType.c_str(), "rate", G_TYPE_INT, kRate, "channels", G_TYPE_INT,
                                            kChannels, "format", G_TYPE_STRING, kFormat.c_str(), nullptr);
        EXPECT_CALL(m_playerMock, getDeviceInfo(_, _, _))
            .WillOnce(DoAll(SetArgReferee<0>(kFrames), SetArgReferee<1>(kMaximumFrames),
                            SetArgReferee<2>(kSupportDeferredPlay), Return(true)));
        EXPECT_CALL(*m_playerFactoryMock, createWebAudioPlayer(_, kMimeType, kPriority, _))
            .WillOnce(DoAll(SaveArg<0>(&m_webAudioClient), Return(ByMove(std::move(m_player)))));
        setCaps(sink, caps);
        gst_caps_unref(caps);
    }

    void setPlayingInPushMode(GstElement *pipeline)
    {
        EXPECT_CALL(m_playerMock, play()).WillOnce(Return(true));
        EXPECT_EQ(GST_STATE_CHANGE_ASYNC, gst_element_set_state(pipeline, GST_STATE_PLAYING));
    }

    void sendWebAudioStateNotification(RialtoMSEBaseSink *sink, const WebAudioPlayerState &state) const
    {
        auto webAudioClient = m_webAudioClient.lock();
        ASSERT_TRUE(webAudioClient);
        webAudioClient->notifyState(state);
    }

    void willPerformPlayingToPausedTransition() { EXPECT_CALL(m_playerMock, pause()).WillOnce(Return(true)); }

    std::shared_ptr<StrictMock<WebAudioPlayerFactoryMock>> m_playerFactoryMock{
        std::dynamic_pointer_cast<StrictMock<WebAudioPlayerFactoryMock>>(IWebAudioPlayerFactory::createFactory())};
    std::unique_ptr<StrictMock<WebAudioPlayerMock>> m_player{std::make_unique<StrictMock<WebAudioPlayerMock>>()};
    StrictMock<WebAudioPlayerMock> &m_playerMock{*m_player};
    std::weak_ptr<IWebAudioPlayerClient> m_webAudioClient;
};

TEST_F(GstreamerMseAudioSinkInWebAudioModeTests, ShouldCreatePushModeSink)
{
    RialtoMSEBaseSink *sink{createAudioSinkInWebAudioMode()};
    EXPECT_TRUE(sink);
    gst_element_set_state(GST_ELEMENT(sink), GST_STATE_NULL);
    gst_object_unref(sink);
}

TEST_F(GstreamerMseAudioSinkInWebAudioModeTests, ShouldNotReachReadyStateWhenAppStateIsInactiveInPushMode)
{
    EXPECT_CALL(*m_controlFactoryMock, createControl()).WillOnce(Return(m_controlMock));
    EXPECT_CALL(*m_controlMock, registerClient(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(firebolt::rialto::ApplicationState::INACTIVE), Return(true)));
    GstElement *sink = gst_element_factory_make("rialtomseaudiosink", "rialtomseaudiosink");
    g_object_set(sink, "web-audio", TRUE, nullptr);
    GstElement *pipeline = createPipelineWithSink(RIALTO_MSE_BASE_SINK(sink));

    EXPECT_EQ(GST_STATE_CHANGE_FAILURE, gst_element_set_state(pipeline, GST_STATE_READY));

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkInWebAudioModeTests, ShouldFailToAttachSourceInPushMode)
{
    RialtoMSEBaseSink *sink{createAudioSinkInWebAudioMode()};
    GstElement *pipeline = createPipelineWithSink(sink);

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_PAUSED));

    GstCaps *caps = gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, kRate, nullptr);
    setCaps(sink, caps);
    gst_caps_unref(caps);

    gst_element_set_state(pipeline, GST_STATE_NULL);

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkInWebAudioModeTests, ShouldAttachSourceInPushMode)
{
    RialtoMSEBaseSink *sink{createAudioSinkInWebAudioMode()};
    GstElement *pipeline = createPipelineWithSink(sink);

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_PAUSED));
    attachWebAudioSource(sink);
    gst_element_set_state(pipeline, GST_STATE_NULL);

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkInWebAudioModeTests, ShouldFailToReachPlayingStateInPushMode)
{
    RialtoMSEBaseSink *sink{createAudioSinkInWebAudioMode()};
    GstElement *pipeline = createPipelineWithSink(sink);

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_PAUSED));
    attachWebAudioSource(sink);

    EXPECT_CALL(m_playerMock, play()).WillOnce(Return(false));
    EXPECT_EQ(GST_STATE_CHANGE_FAILURE, gst_element_set_state(pipeline, GST_STATE_PLAYING));

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkInWebAudioModeTests, ShouldReachPlayingStateInPushMode)
{
    RialtoMSEBaseSink *sink{createAudioSinkInWebAudioMode()};
    GstElement *pipeline = createPipelineWithSink(sink);

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_PAUSED));
    attachWebAudioSource(sink);

    setPlayingInPushMode(pipeline);
    sendWebAudioStateNotification(sink, WebAudioPlayerState::PLAYING);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    willPerformPlayingToPausedTransition();
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkInWebAudioModeTests, ShouldDelayTransitionToPlayingWhenSourceIsNotAttachedInPushMode)
{
    RialtoMSEBaseSink *sink{createAudioSinkInWebAudioMode()};
    GstElement *pipeline = createPipelineWithSink(sink);

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_PAUSED));

    setPlayingInPushMode(pipeline);
    attachWebAudioSource(sink);
    sendWebAudioStateNotification(sink, WebAudioPlayerState::PLAYING);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    willPerformPlayingToPausedTransition();
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkInWebAudioModeTests, ShouldDelayTransitionToPlayingWhenSourceIsNotAttachedAndFailInPushMode)
{
    RialtoMSEBaseSink *sink{createAudioSinkInWebAudioMode()};
    GstElement *pipeline = createPipelineWithSink(sink);

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_PAUSED));

    EXPECT_EQ(GST_STATE_CHANGE_ASYNC, gst_element_set_state(pipeline, GST_STATE_PLAYING));
    EXPECT_CALL(m_playerMock, play()).WillOnce(Return(false));
    attachWebAudioSource(sink);

    willPerformPlayingToPausedTransition();
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkInWebAudioModeTests, ShouldFailToPauseInPushMode)
{
    RialtoMSEBaseSink *sink{createAudioSinkInWebAudioMode()};
    GstElement *pipeline = createPipelineWithSink(sink);

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_PAUSED));
    attachWebAudioSource(sink);

    setPlayingInPushMode(pipeline);
    sendWebAudioStateNotification(sink, WebAudioPlayerState::PLAYING);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    EXPECT_CALL(m_playerMock, pause()).WillOnce(Return(false));
    EXPECT_EQ(GST_STATE_CHANGE_FAILURE, gst_element_set_state(pipeline, GST_STATE_PAUSED));

    willPerformPlayingToPausedTransition();
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkInWebAudioModeTests, ShouldSetEosInPushMode)
{
    RialtoMSEBaseSink *sink{createAudioSinkInWebAudioMode()};
    GstElement *pipeline = createPipelineWithSink(sink);

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_PAUSED));
    attachWebAudioSource(sink);
    setPlayingInPushMode(pipeline);
    sendWebAudioStateNotification(sink, WebAudioPlayerState::PLAYING);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    sendWebAudioStateNotification(sink, WebAudioPlayerState::END_OF_STREAM);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_EOS));

    willPerformPlayingToPausedTransition();
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkInWebAudioModeTests, ShouldHandleErrorInPushMode)
{
    RialtoMSEBaseSink *sink{createAudioSinkInWebAudioMode()};
    GstElement *pipeline = createPipelineWithSink(sink);

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_PAUSED));
    attachWebAudioSource(sink);

    sendWebAudioStateNotification(sink, WebAudioPlayerState::FAILURE);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ERROR));

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkInWebAudioModeTests, ShouldHandleEosEventInPushMode)
{
    RialtoMSEBaseSink *sink{createAudioSinkInWebAudioMode()};
    GstElement *pipeline = createPipelineWithSink(sink);

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_PAUSED));
    attachWebAudioSource(sink);

    setPlayingInPushMode(pipeline);
    sendWebAudioStateNotification(sink, WebAudioPlayerState::PLAYING);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    EXPECT_CALL(m_playerMock, setEos()).WillOnce(Return(true));
    GstPad *sinkPad = gst_element_get_static_pad(GST_ELEMENT_CAST(sink), "sink");
    ASSERT_TRUE(sinkPad);
    gst_pad_send_event(sinkPad, gst_event_new_eos());

    willPerformPlayingToPausedTransition();
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(sinkPad);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkInWebAudioModeTests, ShouldHandleUnknownEventInPushMode)
{
    RialtoMSEBaseSink *sink{createAudioSinkInWebAudioMode()};
    GstElement *pipeline = createPipelineWithSink(sink);

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_PAUSED));
    attachWebAudioSource(sink);

    setPlayingInPushMode(pipeline);
    sendWebAudioStateNotification(sink, WebAudioPlayerState::PLAYING);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    GstPad *sinkPad = gst_element_get_static_pad(GST_ELEMENT_CAST(sink), "sink");
    ASSERT_TRUE(sinkPad);
    gst_pad_send_event(sinkPad, gst_event_new_gap(1, 1));

    willPerformPlayingToPausedTransition();
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(sinkPad);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkInWebAudioModeTests, ShouldNotifyNewSampleInPushMode)
{
    constexpr uint32_t kAvailableFrames{24};
    RialtoMSEBaseSink *sink{createAudioSinkInWebAudioMode()};
    GstElement *pipeline = createPipelineWithSink(sink);
    GstBuffer *buffer{gst_buffer_new()};

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_PAUSED));
    attachWebAudioSource(sink);

    setPlayingInPushMode(pipeline);
    sendWebAudioStateNotification(sink, WebAudioPlayerState::PLAYING);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    EXPECT_CALL(m_playerMock, getBufferAvailable(_, _)).WillOnce(DoAll(SetArgReferee<0>(kAvailableFrames), Return(true)));
    GstPad *sinkPad = gst_element_get_static_pad(GST_ELEMENT_CAST(sink), "sink");
    ASSERT_TRUE(sinkPad);
    EXPECT_EQ(GST_FLOW_OK, gst_pad_chain(sinkPad, buffer));

    willPerformPlayingToPausedTransition();
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(sinkPad);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkInWebAudioModeTests, ShouldReturnDefaultVolumeValueWhenPipelineIsBelowPausedStateInPushMode)
{
    RialtoMSEBaseSink *sink{createAudioSinkInWebAudioMode()};

    gdouble volume{-1.0};
    g_object_get(sink, "volume", &volume, nullptr);
    EXPECT_EQ(1.0, volume); // Default value should be returned

    gst_element_set_state(GST_ELEMENT(sink), GST_STATE_NULL);
    gst_object_unref(sink);
}

TEST_F(GstreamerMseAudioSinkInWebAudioModeTests, ShouldGetVolumePropertyInPushMode)
{
    RialtoMSEBaseSink *sink{createAudioSinkInWebAudioMode()};
    GstElement *pipeline = createPipelineWithSink(sink);
    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_PAUSED));
    attachWebAudioSource(sink);

    gdouble volume{-1.0};
    constexpr gdouble kVolume{0.8};
    EXPECT_CALL(m_playerMock, getVolume(_)).WillOnce(DoAll(SetArgReferee<0>(kVolume), Return(true)));
    g_object_get(sink, "volume", &volume, nullptr);
    EXPECT_EQ(kVolume, volume);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkInWebAudioModeTests, ShouldFailToSetVolumePropertyWhenPipelineIsBelowPausedStateInPushMode)
{
    RialtoMSEBaseSink *sink{createAudioSinkInWebAudioMode()};

    constexpr gdouble kVolume{0.8};
    g_object_set(sink, "volume", kVolume, nullptr);

    // Sink should return cached value, when get is called
    gdouble volume{-1.0};
    g_object_get(sink, "volume", &volume, nullptr);
    EXPECT_EQ(kVolume, volume);

    gst_element_set_state(GST_ELEMENT(sink), GST_STATE_NULL);
    gst_object_unref(sink);
}

TEST_F(GstreamerMseAudioSinkInWebAudioModeTests, ShouldSetVolumeInPushMode)
{
    RialtoMSEBaseSink *sink{createAudioSinkInWebAudioMode()};
    GstElement *pipeline = createPipelineWithSink(sink);
    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_PAUSED));
    attachWebAudioSource(sink);

    constexpr gdouble kVolume{0.8};
    EXPECT_CALL(m_playerMock, setVolume(kVolume)).WillOnce(Return(true));
    g_object_set(sink, "volume", kVolume, nullptr);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkInWebAudioModeTests, ShouldFailToSetVolumeInPushMode)
{
    RialtoMSEBaseSink *sink{createAudioSinkInWebAudioMode()};
    GstElement *pipeline = createPipelineWithSink(sink);
    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_PAUSED));
    attachWebAudioSource(sink);

    constexpr gdouble kVolume{0.8};
    // A log message is generated due to the following
    // false return value, but nothing else should be done...
    EXPECT_CALL(m_playerMock, setVolume(kVolume)).WillOnce(Return(false));

    g_object_set(sink, "volume", kVolume, nullptr);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkInWebAudioModeTests, ShouldSetCachedVolumeInPushMode)
{
    RialtoMSEBaseSink *sink{createAudioSinkInWebAudioMode()};

    constexpr gdouble kVolume{0.8};
    g_object_set(sink, "volume", kVolume, nullptr);

    EXPECT_CALL(m_playerMock, setVolume(kVolume)).WillOnce(Return(true));

    GstElement *pipeline = createPipelineWithSink(sink);
    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_PAUSED));
    attachWebAudioSource(sink);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkInWebAudioModeTests, ShouldFailToSetCachedVolumeInPushMode)
{
    RialtoMSEBaseSink *sink{createAudioSinkInWebAudioMode()};

    constexpr gdouble kVolume{0.8};
    g_object_set(sink, "volume", kVolume, nullptr);

    // A log message is generated due to the following
    // false return value, but nothing else should be done...
    EXPECT_CALL(m_playerMock, setVolume(kVolume)).WillOnce(Return(false));

    GstElement *pipeline = createPipelineWithSink(sink);

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_PAUSED));
    attachWebAudioSource(sink);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseAudioSinkInWebAudioModeTests, ShouldReturnLastKnownVolumeWhenOperationFailsInPushMode)
{
    RialtoMSEBaseSink *sink{createAudioSinkInWebAudioMode()};
    GstElement *pipeline = createPipelineWithSink(sink);
    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_PAUSED));
    attachWebAudioSource(sink);

    constexpr gdouble kVolume{0.7};
    {
        EXPECT_CALL(m_playerMock, getVolume(_)).WillOnce(DoAll(SetArgReferee<0>(kVolume), Return(true)));
        gdouble volume{-1.0};
        g_object_get(sink, "volume", &volume, nullptr);
        EXPECT_EQ(volume, kVolume);
    }

    {
        EXPECT_CALL(m_playerMock, getVolume(_)).WillOnce(DoAll(SetArgReferee<0>(1.0), Return(false)));
        gdouble volume{-1.0};
        g_object_get(sink, "volume", &volume, nullptr);
        EXPECT_EQ(volume, kVolume);
    }

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}
