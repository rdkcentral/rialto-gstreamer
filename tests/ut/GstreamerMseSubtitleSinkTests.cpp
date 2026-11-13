/*
 * Copyright (C) 2024 Sky UK
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
#include "PullModeSubtitlePlaybackDelegate.h"
#include "RialtoGStreamerMSEBaseSinkPrivate.h"
#include "RialtoGStreamerMSESubtitleSink.h"
#include "RialtoGstTest.h"

#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

using testing::_;
using testing::DoAll;
using testing::InSequence;
using testing::Invoke;
using testing::Return;
using testing::SetArgReferee;

class PullModeSubtitlePlaybackDelegateTestPeer
{
public:
    static std::mutex &getMutex(const PullModeSubtitlePlaybackDelegate &delegate) { return delegate.m_mutex; }
};

namespace
{
const std::string kTextTrackIdentifier{"TEXT"};
constexpr bool kMute{true};
constexpr uint32_t kWindowId{12};
constexpr bool kIsAsync{true};
} // namespace

class GstreamerMseSubtitleSinkTests : public RialtoGstTest
{
public:
    GstreamerMseSubtitleSinkTests() = default;
    ~GstreamerMseSubtitleSinkTests() override = default;

    GstCaps *createDefaultCaps() const { return gst_caps_new_empty_simple("application/ttml+xml"); }

    firebolt::rialto::IMediaPipeline::MediaSourceSubtitle createDefaultMediaSource() const
    {
        return firebolt::rialto::IMediaPipeline::MediaSourceSubtitle{"text/ttml", ""};
    }
};

TEST_F(GstreamerMseSubtitleSinkTests, ShouldFailToReachPausedStateWhenMediaPipelineCantBeCreated)
{
    constexpr firebolt::rialto::VideoRequirements kDefaultRequirements{3840, 2160};

    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    EXPECT_CALL(*m_mediaPipelineFactoryMock, createMediaPipeline(_, kDefaultRequirements)).WillOnce(Return(nullptr));
    EXPECT_EQ(GST_STATE_CHANGE_FAILURE, gst_element_set_state(pipeline, GST_STATE_PAUSED));
    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_NULL));

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldNotHandleUnknownEvent)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_READY));

    gst_pad_set_active(sink->priv->m_sinkPad, TRUE);
    gst_pad_send_event(sink->priv->m_sinkPad, gst_event_new_gap(1, 1));

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_NULL));

    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldNotAttachSourceWhenPipelineIsBelowPausedState)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_READY));

    gst_pad_set_active(sink->priv->m_sinkPad, TRUE);
    GstCaps *caps{createDefaultCaps()};
    setCaps(sink, caps);

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_NULL));

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldAttachSourceWithTtml)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    setPausedState(pipeline, sink);
    const int32_t kSourceId{subtitleSourceWillBeAttached(createDefaultMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createDefaultCaps()};
    setCaps(sink, caps);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldAttachSourceWithVtt)
{
    const auto kExpectedSource{firebolt::rialto::IMediaPipeline::MediaSourceSubtitle{"text/vtt", ""}};
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    setPausedState(pipeline, sink);
    const int32_t kSourceId{subtitleSourceWillBeAttached(kExpectedSource)};
    allSourcesWillBeAttached();

    GstCaps *caps{gst_caps_new_empty_simple("application/x-subtitle-vtt")};
    setCaps(sink, caps);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldAttachSourceWithCC)
{
    const auto kExpectedSource{firebolt::rialto::IMediaPipeline::MediaSourceSubtitle{"text/cc", ""}};
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    setPausedState(pipeline, sink);
    const int32_t kSourceId{subtitleSourceWillBeAttached(kExpectedSource)};
    allSourcesWillBeAttached();

    GstCaps *caps{gst_caps_new_empty_simple("closedcaption/x-cea-708")};
    setCaps(sink, caps);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldNotAttachSourceTwice)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    setPausedState(pipeline, sink);
    const int32_t kSourceId{subtitleSourceWillBeAttached(createDefaultMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createDefaultCaps()};
    setCaps(sink, caps);
    setCaps(sink, caps);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldAttachSourceWithQueuedProperties)
{
    const auto kExpectedSource{firebolt::rialto::IMediaPipeline::MediaSourceSubtitle{"text/ttml", kTextTrackIdentifier}};
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    g_object_set(sink, "mute", kMute, nullptr);
    g_object_set(sink, "text-track-identifier", kTextTrackIdentifier.c_str(), nullptr);

    setPausedState(pipeline, sink);
    const int32_t kSourceId{subtitleSourceWillBeAttached(kExpectedSource)};
    allSourcesWillBeAttached();

    EXPECT_CALL(m_mediaPipelineMock, setMute(kSourceId, kMute)).WillOnce(Return(true));
    EXPECT_CALL(m_mediaPipelineMock, setTextTrackIdentifier(kTextTrackIdentifier)).WillOnce(Return(true));

    GstCaps *caps{createDefaultCaps()};
    setCaps(sink, caps);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldHandleConcurrentMuteUpdatesDuringSourceAttachment)
{
    constexpr int32_t kSourceId{12};
    constexpr auto kBarrierTimeout{std::chrono::seconds{5}};
    constexpr auto kSetterBlockingWindow{std::chrono::milliseconds{250}};
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    setPausedState(pipeline, sink);
    g_object_set(sink, "mute", TRUE, nullptr);
    allSourcesWillBeAttached();

    std::promise<void> attachCallEntered;
    std::promise<void> allowAttachSourceToReturn;
    std::promise<void> propertySetterStarted;
    std::promise<void> propertySetterFinished;
    auto attachCallEnteredFuture{attachCallEntered.get_future()};
    auto allowAttachSourceToReturnFuture{allowAttachSourceToReturn.get_future()};
    auto propertySetterStartedFuture{propertySetterStarted.get_future()};
    auto propertySetterFinishedFuture{propertySetterFinished.get_future()};
    std::atomic<bool> synchronizationFailed{false};
    std::mutex muteCallsMutex;
    std::vector<int> appliedMuteValues;
    EXPECT_CALL(m_mediaPipelineMock, attachSource(_))
        .WillOnce(Invoke(
            [&](auto &source)
            {
                source->setId(kSourceId);
                attachCallEntered.set_value();
                if (allowAttachSourceToReturnFuture.wait_for(kBarrierTimeout) != std::future_status::ready)
                {
                    synchronizationFailed = true;
                }
                return true;
            }));
    EXPECT_CALL(m_mediaPipelineMock, setMute(kSourceId, _))
        .Times(testing::AnyNumber())
        .WillRepeatedly(Invoke(
            [&](int32_t, bool mute)
            {
                std::lock_guard<std::mutex> lock{muteCallsMutex};
                appliedMuteValues.push_back(mute ? 1 : 0);
                return true;
            }));

    GstCaps *caps{createDefaultCaps()};
    auto delegate{std::dynamic_pointer_cast<PullModeSubtitlePlaybackDelegate>(sink->priv->m_delegate)};
    ASSERT_TRUE(delegate);
    std::thread attachmentThread{[&]() { setCaps(sink, caps); }};
    if (attachCallEnteredFuture.wait_for(kBarrierTimeout) != std::future_status::ready)
    {
        synchronizationFailed = true;
        allowAttachSourceToReturn.set_value();
        attachmentThread.join();
        gst_caps_unref(caps);
        setNullState(pipeline, kSourceId);
        gst_object_unref(pipeline);
        FAIL() << "Source attachment did not reach the queue handoff";
        return;
    }
    std::unique_lock<std::mutex> attachmentQueueLock{PullModeSubtitlePlaybackDelegateTestPeer::getMutex(*delegate)};
    allowAttachSourceToReturn.set_value();

    std::thread propertyThread{[&]()
                               {
                                   propertySetterStarted.set_value();
                                   GValue muteValue = G_VALUE_INIT;
                                   g_value_init(&muteValue, G_TYPE_BOOLEAN);
                                   g_value_set_boolean(&muteValue, FALSE);
                                   sink->priv->m_delegate->setProperty(IPlaybackDelegate::Property::Mute, &muteValue);
                                   g_value_unset(&muteValue);
                                   propertySetterFinished.set_value();
                               }};
    if (propertySetterStartedFuture.wait_for(kBarrierTimeout) != std::future_status::ready)
    {
        synchronizationFailed = true;
    }
    const bool setterFinishedWhileAttachmentLockWasHeld = propertySetterFinishedFuture.wait_for(kSetterBlockingWindow) ==
                                                          std::future_status::ready;
    attachmentQueueLock.unlock();
    attachmentThread.join();
    propertyThread.join();
    EXPECT_FALSE(synchronizationFailed.load());
    EXPECT_FALSE(setterFinishedWhileAttachmentLockWasHeld);
    std::vector<int> appliedMuteValuesSnapshot;
    {
        std::lock_guard<std::mutex> lock{muteCallsMutex};
        appliedMuteValuesSnapshot = appliedMuteValues;
    }
    ASSERT_FALSE(appliedMuteValuesSnapshot.empty());
    EXPECT_EQ(0, appliedMuteValuesSnapshot.back());
    if (appliedMuteValuesSnapshot.size() == 2)
    {
        EXPECT_EQ(1, appliedMuteValuesSnapshot.front());
    }
    else
    {
        EXPECT_EQ(1U, appliedMuteValuesSnapshot.size());
        EXPECT_EQ(0, appliedMuteValuesSnapshot.front());
    }

    setNullState(pipeline, kSourceId);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldNotHoldMuteMutexWhileApplyingQueuedMute)
{
    constexpr int32_t kSourceId{14};
    constexpr auto kCallbackTimeout{std::chrono::milliseconds{250}};
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    setPausedState(pipeline, sink);
    g_object_set(sink, "mute", TRUE, nullptr);
    allSourcesWillBeAttached();
    auto delegate{std::dynamic_pointer_cast<PullModeSubtitlePlaybackDelegate>(sink->priv->m_delegate)};
    ASSERT_TRUE(delegate);

    std::promise<void> followupMuteApplied;
    auto followupMuteAppliedFuture{followupMuteApplied.get_future()};
    std::atomic<bool> backendCallbackTimedOutWaitingForMuteMutex{false};
    std::atomic<bool> backendCallbackCouldAcquireMuteMutex{false};

    EXPECT_CALL(m_mediaPipelineMock, attachSource(_))
        .WillOnce(Invoke(
            [&](auto &source)
            {
                source->setId(kSourceId);
                return true;
            }));
    {
        InSequence orderedBackendCalls;
        EXPECT_CALL(m_mediaPipelineMock, setMute(kSourceId, true))
            .WillOnce(Invoke(
                [&](int32_t, bool)
                {
                    auto deadline{std::chrono::steady_clock::now() + kCallbackTimeout};
                    std::unique_lock<std::mutex> lock{PullModeSubtitlePlaybackDelegateTestPeer::getMutex(*delegate),
                                                      std::defer_lock};
                    while (!lock.try_lock() && std::chrono::steady_clock::now() < deadline)
                    {
                        std::this_thread::yield();
                    }
                    backendCallbackCouldAcquireMuteMutex = lock.owns_lock();
                    if (lock.owns_lock())
                    {
                        lock.unlock();
                        GValue muteValue = G_VALUE_INIT;
                        g_value_init(&muteValue, G_TYPE_BOOLEAN);
                        g_value_set_boolean(&muteValue, FALSE);
                        delegate->setProperty(IPlaybackDelegate::Property::Mute, &muteValue);
                        g_value_unset(&muteValue);
                    }
                    else
                    {
                        backendCallbackTimedOutWaitingForMuteMutex = true;
                    }
                    return true;
                }));
        EXPECT_CALL(m_mediaPipelineMock, setMute(kSourceId, false))
            .WillOnce(Invoke(
                [&](int32_t, bool)
                {
                    followupMuteApplied.set_value();
                    return true;
                }));
    }

    GstCaps *caps{createDefaultCaps()};
    setCaps(sink, caps);
    EXPECT_EQ(std::future_status::ready, followupMuteAppliedFuture.wait_for(std::chrono::seconds{5}));
    EXPECT_FALSE(backendCallbackTimedOutWaitingForMuteMutex.load());
    EXPECT_TRUE(backendCallbackCouldAcquireMuteMutex.load());

    setNullState(pipeline, kSourceId);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldHandleConcurrentOffsetEventsDuringSourceAttachment)
{
    constexpr int32_t kSourceId{13};
    constexpr guint64 kQueuedOffset{111};
    constexpr guint64 kFollowupOffset{222};
    constexpr auto kBarrierTimeout{std::chrono::seconds{5}};
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    setPausedState(pipeline, sink);
    allSourcesWillBeAttached();

    std::promise<void> attachmentStarted;
    std::promise<void> initialUpdatesQueued;
    std::promise<void> triggerFollowupUpdate;
    auto attachmentStartedFuture{attachmentStarted.get_future()};
    auto initialUpdatesQueuedFuture{initialUpdatesQueued.get_future()};
    auto triggerFollowupUpdateFuture{triggerFollowupUpdate.get_future()};
    std::atomic<bool> synchronizationFailed{false};
    std::atomic<int64_t> lastAppliedOffset{-1};
    EXPECT_CALL(m_mediaPipelineMock, attachSource(_))
        .WillOnce(Invoke(
            [&](auto &source)
            {
                source->setId(kSourceId);
                attachmentStarted.set_value();
                if (initialUpdatesQueuedFuture.wait_for(kBarrierTimeout) != std::future_status::ready)
                {
                    synchronizationFailed = true;
                }
                return true;
            }));
    {
        InSequence orderedBackendCalls;
        EXPECT_CALL(m_mediaPipelineMock, setMute(kSourceId, true))
            .WillOnce(Invoke(
                [&](int32_t, bool)
                {
                    triggerFollowupUpdate.set_value();
                    return true;
                }));
        EXPECT_CALL(m_mediaPipelineMock, setSubtitleOffset(kSourceId, kQueuedOffset))
            .WillOnce(Invoke(
                [&](int32_t, int64_t offset)
                {
                    lastAppliedOffset = offset;
                    return true;
                }));
        EXPECT_CALL(m_mediaPipelineMock, setSubtitleOffset(kSourceId, kFollowupOffset))
            .WillOnce(Invoke(
                [&](int32_t, int64_t offset)
                {
                    lastAppliedOffset = offset;
                    return true;
                }));
    }

    GstCaps *caps{createDefaultCaps()};
    std::thread eventThread{
        [&]()
        {
            if (attachmentStartedFuture.wait_for(kBarrierTimeout) != std::future_status::ready)
            {
                synchronizationFailed = true;
                return;
            }
            g_object_set(sink, "mute", TRUE, nullptr);
            GstStructure *queuedStructure{
                gst_structure_new("set-pts-offset", "pts-offset", G_TYPE_UINT64, kQueuedOffset, nullptr)};
            gboolean queuedEventAccepted =
                gst_pad_send_event(sink->priv->m_sinkPad,
                                   gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM_OOB, queuedStructure));
            initialUpdatesQueued.set_value();
            if (triggerFollowupUpdateFuture.wait_for(kBarrierTimeout) != std::future_status::ready)
            {
                synchronizationFailed = true;
            }
            GstStructure *followupStructure{
                gst_structure_new("set-pts-offset", "pts-offset", G_TYPE_UINT64, kFollowupOffset, nullptr)};
            gboolean followupEventAccepted =
                gst_pad_send_event(sink->priv->m_sinkPad,
                                   gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM_OOB, followupStructure));
            if (!queuedEventAccepted || !followupEventAccepted)
            {
                synchronizationFailed = true;
            }
        }};

    setCaps(sink, caps);
    eventThread.join();
    EXPECT_FALSE(synchronizationFailed.load());

    setNullState(pipeline, kSourceId);
    EXPECT_EQ(kFollowupOffset, lastAppliedOffset.load());
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldSetAndGetMuteProperty)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    setPausedState(pipeline, sink);
    const int32_t kSourceId{subtitleSourceWillBeAttached(createDefaultMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createDefaultCaps()};
    setCaps(sink, caps);

    EXPECT_CALL(m_mediaPipelineMock, setMute(kSourceId, kMute)).WillOnce(Return(true));
    g_object_set(sink, "mute", kMute, nullptr);

    EXPECT_CALL(m_mediaPipelineMock, getMute(kSourceId, _)).WillOnce(DoAll(SetArgReferee<1>(kMute), Return(true)));
    gboolean mute{FALSE};
    g_object_get(sink, "mute", &mute, nullptr);
    EXPECT_EQ(kMute, mute);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldSetAndGetMutePropertyWithoutSourceAttached)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();

    g_object_set(sink, "mute", kMute, nullptr);
    gboolean mute{FALSE};
    g_object_get(sink, "mute", &mute, nullptr);
    EXPECT_EQ(kMute, mute);

    gst_element_set_state(GST_ELEMENT_CAST(sink), GST_STATE_NULL);
    gst_object_unref(sink);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldSetAndGetTextTrackIdProperty)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    setPausedState(pipeline, sink);
    const int32_t kSourceId{subtitleSourceWillBeAttached(createDefaultMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createDefaultCaps()};
    setCaps(sink, caps);

    EXPECT_CALL(m_mediaPipelineMock, setTextTrackIdentifier(kTextTrackIdentifier)).WillOnce(Return(true));
    g_object_set(sink, "text-track-identifier", kTextTrackIdentifier.c_str(), nullptr);

    EXPECT_CALL(m_mediaPipelineMock, getTextTrackIdentifier(_))
        .WillOnce(DoAll(SetArgReferee<0>(kTextTrackIdentifier), Return(true)));
    gchar *textTrackId{nullptr};
    g_object_get(sink, "text-track-identifier", &textTrackId, nullptr);
    EXPECT_EQ(kTextTrackIdentifier, std::string{textTrackId});
    g_free(textTrackId);

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldSetAndGetTextTrackIdPropertyWithoutSourceAttached)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();

    g_object_set(sink, "text-track-identifier", kTextTrackIdentifier.c_str(), nullptr);

    gchar *textTrackId{nullptr};
    g_object_get(sink, "text-track-identifier", &textTrackId, nullptr);
    EXPECT_EQ(kTextTrackIdentifier, std::string{textTrackId});
    g_free(textTrackId);

    gst_element_set_state(GST_ELEMENT_CAST(sink), GST_STATE_NULL);
    gst_object_unref(sink);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldNotSetTextTrackIdPropertyWhenItsEmpty)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();

    g_object_set(sink, "text-track-identifier", nullptr, nullptr);

    gst_element_set_state(GST_ELEMENT_CAST(sink), GST_STATE_NULL);
    gst_object_unref(sink);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldSetAndGetWindowIdProperty)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();

    g_object_set(sink, "window-id", kWindowId, nullptr);

    guint windowId{0};
    g_object_get(sink, "window-id", &windowId, nullptr);
    EXPECT_EQ(kWindowId, windowId);

    gst_element_set_state(GST_ELEMENT_CAST(sink), GST_STATE_NULL);
    gst_object_unref(sink);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldSetAndGetAsyncProperty)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();

    g_object_set(sink, "async", kIsAsync, nullptr);

    gboolean async{FALSE};
    g_object_get(sink, "async", &async, nullptr);
    EXPECT_EQ(kIsAsync, async);

    gst_element_set_state(GST_ELEMENT_CAST(sink), GST_STATE_NULL);
    gst_object_unref(sink);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldNotSetAndGetInvalidProperty)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();

    g_object_class_install_property(G_OBJECT_GET_CLASS(sink), 123,
                                    g_param_spec_boolean("surprise", "surprise", "surprise", FALSE,
                                                         GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_set(sink, "surprise", kIsAsync, nullptr);
    gboolean async{FALSE};
    g_object_get(sink, "surprise", &async, nullptr);

    gst_element_set_state(GST_ELEMENT_CAST(sink), GST_STATE_NULL);
    gst_object_unref(sink);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldSendQosEvent)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    setPausedState(pipeline, sink);
    const int32_t kSourceId{subtitleSourceWillBeAttached(createDefaultMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createDefaultCaps()};
    setCaps(sink, caps);

    sendPlaybackStateNotification(sink, firebolt::rialto::PlaybackState::PAUSED);

    auto mediaPlayerClient{m_mediaPipelineClient.lock()};
    ASSERT_TRUE(mediaPlayerClient);
    const firebolt::rialto::QosInfo kQosInfo{1, 2};
    mediaPlayerClient->notifyQos(kSourceId, kQosInfo);

    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_QOS));

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldHandleSetPtsOffsetEventValueNotPresent)
{
    constexpr guint64 kOffset{4325};

    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    setPausedState(pipeline, sink);
    const int32_t kSourceId{subtitleSourceWillBeAttached(createDefaultMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createDefaultCaps()};
    setCaps(sink, caps);

    sendPlaybackStateNotification(sink, firebolt::rialto::PlaybackState::PAUSED);

    GstStructure *structure{gst_structure_new("set-pts-offset", "different-value", G_TYPE_UINT64, kOffset, nullptr)};
    gst_pad_send_event(sink->priv->m_sinkPad, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure));

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldHandleSetPtsOffsetEventSetPosition)
{
    constexpr guint64 kOffset{4325};

    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    setPausedState(pipeline, sink);
    const int32_t kSourceId{subtitleSourceWillBeAttached(createDefaultMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *caps{createDefaultCaps()};
    setCaps(sink, caps);

    sendPlaybackStateNotification(sink, firebolt::rialto::PlaybackState::PAUSED);

    EXPECT_CALL(m_mediaPipelineMock, setSourcePosition(kSourceId, 0, _, _, _)).WillOnce(Return(true));
    GstSegment *segment{gst_segment_new()};
    gst_segment_init(segment, GST_FORMAT_TIME);
    EXPECT_TRUE(rialto_mse_base_sink_event(sink->priv->m_sinkPad, GST_OBJECT_CAST(sink), gst_event_new_segment(segment)));

    gst_segment_free(segment);

    EXPECT_CALL(m_mediaPipelineMock, setSubtitleOffset(kSourceId, kOffset)).WillOnce(Return(true));

    GstStructure *structure{gst_structure_new("set-pts-offset", "pts-offset", G_TYPE_UINT64, kOffset, nullptr)};
    gst_pad_send_event(sink->priv->m_sinkPad, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure));

    setNullState(pipeline, kSourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}