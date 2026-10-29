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

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgReferee;

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

    int32_t startPlayingWithSubtitles(GstElement *pipeline, RialtoMSEBaseSink *sink, GstCaps *caps)
    {
        setPausedState(pipeline, sink);
        const int32_t sourceId{subtitleSourceWillBeAttached(createDefaultMediaSource())};
        allSourcesWillBeAttached();
        setCaps(sink, caps);
        sendPlaybackStateNotification(sink, firebolt::rialto::PlaybackState::PAUSED);
        EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));
        setPlayingState(pipeline);
        sendPlaybackStateNotification(sink, firebolt::rialto::PlaybackState::PLAYING);
        EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));
        return sourceId;
    }

    void expectPlaybackContinues(GstElement *pipeline) const
    {
        EXPECT_FALSE(getMessages(pipeline).contains(GST_MESSAGE_ERROR));
        GstState state{GST_STATE_NULL};
        EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_get_state(pipeline, &state, nullptr, 0));
        EXPECT_EQ(GST_STATE_PLAYING, state);
    }

    void stopPlaying(GstElement *pipeline, RialtoMSEBaseSink *sink, int32_t sourceId)
    {
        pipelineWillGoToPausedState(sink);
        setNullState(pipeline, sourceId);
    }

    void failSubtitleAttachmentAndWaitForPaused(RialtoMSEBaseSink *sink, GstElement *pipeline, GstCaps *caps)
    {
        setPausedState(pipeline, sink);
        EXPECT_CALL(m_mediaPipelineMock, attachSource(_)).WillOnce(Return(false));
        allSourcesWillBeAttached();
        setCaps(sink, caps);
        EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));
    }

    void stopPipelineWithoutSubtitleSource(GstElement *pipeline)
    {
        EXPECT_CALL(m_mediaPipelineMock, stop()).WillOnce(Return(true));
        EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_NULL));
    }
};

class TestableSubtitleDelegate : public PullModeSubtitlePlaybackDelegate
{
public:
    using PullModeSubtitlePlaybackDelegate::PullModeSubtitlePlaybackDelegate;

    bool isSourceAttached() const { return m_sourceAttached; }
    bool attachClient() { return attachToMediaClientAndSetStreamsNumber(); }
    void releaseClient() { m_mediaPlayerManager.releaseMediaPlayerClient(); }
    void setStreamCollection(int32_t audio, int32_t video, int32_t subtitles)
    {
        m_mediaPlayerManager.getMediaPlayerClient()->handleStreamCollection(audio, video, subtitles);
    }
};

TEST_F(GstreamerMseSubtitleSinkTests, ShouldRemainUnattachedWhenSubtitleCapsArriveWithoutClient)
{
    GstElement *sink = gst_element_factory_make("rialtomsesubtitlesink", nullptr);
    ASSERT_TRUE(sink);
    auto delegate = std::make_shared<TestableSubtitleDelegate>(sink);
    GstCaps *caps{createDefaultCaps()};

    EXPECT_TRUE(delegate->handleEvent(RIALTO_MSE_BASE_SINK(sink)->priv->m_sinkPad, GST_OBJECT_CAST(sink),
                                      gst_event_new_caps(caps)));
    EXPECT_FALSE(delegate->isSourceAttached());

    gst_caps_unref(caps);
    gst_object_unref(sink);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldDeferCapsUntilClientIsReadyAndPauseAttachedSource)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);
    GstCaps *caps{createDefaultCaps()};

    EXPECT_CALL(m_mediaPipelineMock, attachSource(_)).Times(0);
    ASSERT_TRUE(gst_pad_set_active(sink->priv->m_sinkPad, TRUE));
    EXPECT_TRUE(gst_pad_send_event(sink->priv->m_sinkPad, gst_event_new_caps(caps)));

    load(pipeline);
    EXPECT_EQ(GST_STATE_CHANGE_ASYNC, gst_element_set_state(pipeline, GST_STATE_PAUSED));
    const int32_t sourceId{subtitleSourceWillBeAttached(createDefaultMediaSource())};
    allSourcesWillBeAttached();
    EXPECT_CALL(m_mediaPipelineMock, pause()).WillOnce(Return(true));
    setCaps(sink, caps);
    sendPlaybackStateNotification(sink, firebolt::rialto::PlaybackState::PAUSED);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));
    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_get_state(pipeline, nullptr, nullptr, GST_SECOND));

    setNullState(pipeline, sourceId);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldNotAdvertiseUnsupportedOrEmptySubtitleCaps)
{
    GstElement *sink = gst_element_factory_make("rialtomsesubtitlesink", nullptr);
    ASSERT_TRUE(sink);
    GstPad *pad = gst_element_get_static_pad(sink, "sink");
    ASSERT_TRUE(pad);
    GstCaps *supported = gst_pad_get_pad_template_caps(pad);
    GstCaps *unsupported = gst_caps_new_empty_simple("application/x-subtitle-srt");
    GstCaps *empty = gst_caps_new_empty();

    EXPECT_FALSE(gst_caps_can_intersect(supported, unsupported));
    EXPECT_FALSE(gst_caps_can_intersect(supported, empty));

    gst_caps_unref(empty);
    gst_caps_unref(unsupported);
    gst_caps_unref(supported);
    gst_object_unref(pad);
    gst_object_unref(sink);
}

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

TEST_F(GstreamerMseSubtitleSinkTests, ShouldRemainPlayableWhenSubtitleAttachmentFails)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    setPausedState(pipeline, sink);
    EXPECT_CALL(m_mediaPipelineMock, attachSource(_)).WillOnce(Return(false));
    allSourcesWillBeAttached();
    GstCaps *caps{createDefaultCaps()};
    setCaps(sink, caps);

    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));
    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_PLAYING));
    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_get_state(pipeline, nullptr, nullptr, GST_SECOND));
    EXPECT_FALSE(getMessages(pipeline).contains(GST_MESSAGE_ERROR));

    setNullState(pipeline, -1);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldHandleConsecutiveFlushCyclesWhenSubtitleIsUnavailable)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);
    GstCaps *caps{createDefaultCaps()};
    failSubtitleAttachmentAndWaitForPaused(sink, pipeline, caps);

    for (int cycle = 0; cycle < 2; ++cycle)
    {
        EXPECT_TRUE(gst_pad_send_event(sink->priv->m_sinkPad, gst_event_new_flush_start()));
        EXPECT_TRUE(gst_pad_send_event(sink->priv->m_sinkPad, gst_event_new_flush_stop(FALSE)));
    }

    EXPECT_EQ(GST_STATE_PAUSED, GST_STATE(sink));
    GstState currentState{GST_STATE_NULL};
    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_get_state(pipeline, &currentState, nullptr, GST_SECOND));
    EXPECT_EQ(GST_STATE_PAUSED, currentState);
    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_PLAYING));
    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_get_state(pipeline, nullptr, nullptr, GST_SECOND));
    EXPECT_TRUE(gst_pad_send_event(sink->priv->m_sinkPad, gst_event_new_eos()));
    GstMessage *eosMessage{getMessage(pipeline, GST_MESSAGE_EOS)};
    EXPECT_NE(eosMessage, nullptr);
    EXPECT_FALSE(getMessages(pipeline).contains(GST_MESSAGE_ERROR));
    if (eosMessage)
    {
        gst_message_unref(eosMessage);
    }
    stopPipelineWithoutSubtitleSource(pipeline);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldClearSubtitleUnavailableStateWhenSwitchingClients)
{
    RialtoMSEBaseSink *sink = RIALTO_MSE_BASE_SINK(gst_element_factory_make("rialtomsesubtitlesink", nullptr));
    ASSERT_TRUE(sink);
    GstElement *firstPipeline = createPipelineWithSink(sink);
    auto delegate = std::make_shared<TestableSubtitleDelegate>(GST_ELEMENT_CAST(sink));
    GstCaps *caps{createDefaultCaps()};
    load(firstPipeline);
    ASSERT_TRUE(delegate->attachClient());
    EXPECT_CALL(m_mediaPipelineMock, attachSource(_)).WillOnce(Return(false));
    allSourcesWillBeAttached();
    EXPECT_TRUE(delegate->handleEvent(sink->priv->m_sinkPad, GST_OBJECT_CAST(sink), gst_event_new_caps(caps)));
    EXPECT_FALSE(delegate->isSourceAttached());

    GstElement *secondPipeline = gst_pipeline_new("replacement-pipeline");
    gst_object_ref(sink);
    ASSERT_TRUE(gst_bin_remove(GST_BIN(firstPipeline), GST_ELEMENT_CAST(sink)));
    ASSERT_TRUE(gst_bin_add(GST_BIN(secondPipeline), GST_ELEMENT_CAST(sink)));
    gst_object_unref(sink);

    EXPECT_CALL(m_mediaPipelineMock, stop()).WillOnce(Return(true));
    auto secondMediaPipeline{std::make_unique<testing::StrictMock<firebolt::rialto::MediaPipelineMock>>()};
    auto &secondMediaPipelineMock{*secondMediaPipeline};
    EXPECT_CALL(secondMediaPipelineMock, load(_, _, _, _)).WillOnce(Return(true));
    EXPECT_CALL(*m_mediaPipelineFactoryMock, createMediaPipeline(_, _))
        .WillOnce(Return(testing::ByMove(std::move(secondMediaPipeline))));
    EXPECT_EQ(GST_STATE_CHANGE_ASYNC, delegate->changeState(GST_STATE_CHANGE_READY_TO_PAUSED));
    delegate->setStreamCollection(0, 0, 1);
    EXPECT_CALL(secondMediaPipelineMock, attachSource(_))
        .WillOnce(testing::Invoke(
            [](auto &source)
            {
                source->setId(71);
                return true;
            }));
    EXPECT_CALL(secondMediaPipelineMock, allSourcesAttached()).WillOnce(Return(true));
    EXPECT_TRUE(delegate->handleEvent(sink->priv->m_sinkPad, GST_OBJECT_CAST(sink), gst_event_new_caps(caps)));
    EXPECT_TRUE(delegate->isSourceAttached());

    EXPECT_CALL(secondMediaPipelineMock, stop()).WillOnce(Return(true));
    delegate->releaseClient();
    delegate.reset();
    gst_caps_unref(caps);
    gst_object_unref(firstPipeline);
    gst_object_unref(secondPipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldDiscardUnavailableSubtitleBuffersWithoutQueueing)
{
    constexpr int kNumBuffersAboveInternalQueueLimit{32};
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);
    GstCaps *caps{createDefaultCaps()};
    failSubtitleAttachmentAndWaitForPaused(sink, pipeline, caps);

    for (int index = 0; index < kNumBuffersAboveInternalQueueLimit; ++index)
    {
        EXPECT_EQ(GST_FLOW_OK,
                  rialto_mse_base_sink_chain(sink->priv->m_sinkPad, GST_OBJECT_CAST(sink), gst_buffer_new()));
    }

    EXPECT_TRUE(rialto_mse_base_sink_event(sink->priv->m_sinkPad, GST_OBJECT_CAST(sink), gst_event_new_flush_start()));
    EXPECT_EQ(GST_FLOW_FLUSHING,
              rialto_mse_base_sink_chain(sink->priv->m_sinkPad, GST_OBJECT_CAST(sink), gst_buffer_new()));
    EXPECT_TRUE(rialto_mse_base_sink_event(sink->priv->m_sinkPad, GST_OBJECT_CAST(sink), gst_event_new_flush_stop(FALSE)));
    stopPipelineWithoutSubtitleSource(pipeline);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldCompleteEosLocallyWhenSubtitleIsUnavailable)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);
    GstCaps *caps{createDefaultCaps()};
    failSubtitleAttachmentAndWaitForPaused(sink, pipeline, caps);

    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(pipeline, GST_STATE_PLAYING));
    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_get_state(pipeline, nullptr, nullptr, GST_SECOND));
    EXPECT_TRUE(gst_pad_send_event(sink->priv->m_sinkPad, gst_event_new_eos()));
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_EOS));

    stopPipelineWithoutSubtitleSource(pipeline);
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

TEST_F(GstreamerMseSubtitleSinkTests, ShouldPauseAndResumeAttachedSubtitleSink)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    setPausedState(pipeline, sink);
    const int32_t sourceId{subtitleSourceWillBeAttached(createDefaultMediaSource())};
    allSourcesWillBeAttached();
    GstCaps *caps{createDefaultCaps()};
    setCaps(sink, caps);

    sendPlaybackStateNotification(sink, firebolt::rialto::PlaybackState::PAUSED);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    setPlayingState(pipeline);
    sendPlaybackStateNotification(sink, firebolt::rialto::PlaybackState::PLAYING);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    pipelineWillGoToPausedState(sink);
    EXPECT_EQ(GST_STATE_CHANGE_ASYNC, gst_element_set_state(pipeline, GST_STATE_PAUSED));
    sendPlaybackStateNotification(sink, firebolt::rialto::PlaybackState::PAUSED);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    setPlayingState(pipeline);
    sendPlaybackStateNotification(sink, firebolt::rialto::PlaybackState::PLAYING);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));

    pipelineWillGoToPausedState(sink);
    setNullState(pipeline, sourceId);
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

class GstreamerMseSubtitleCapsAliasTests : public RialtoGstTest,
                                           public testing::WithParamInterface<std::pair<const char *, const char *>>
{
};

TEST_P(GstreamerMseSubtitleCapsAliasTests, ShouldAttachSourceWithSupportedSubtitleCapsAlias)
{
    const auto &[capsName, mimeType] = GetParam();
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    setPausedState(pipeline, sink);
    const auto expectedSource{firebolt::rialto::IMediaPipeline::MediaSourceSubtitle{mimeType, ""}};
    const int32_t sourceId{subtitleSourceWillBeAttached(expectedSource)};
    allSourcesWillBeAttached();

    GstCaps *caps{gst_caps_new_empty_simple(capsName)};
    setCaps(sink, caps);
    setNullState(pipeline, sourceId);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

INSTANTIATE_TEST_SUITE_P(SupportedCaps, GstreamerMseSubtitleCapsAliasTests,
                         testing::Values(std::make_pair("text/vtt", "text/vtt"),
                                         std::make_pair("closedcaption/x-cea-608", "text/cc"),
                                         std::make_pair("application/x-cea-608", "text/cc"),
                                         std::make_pair("application/x-cea-708", "text/cc"),
                                         std::make_pair("application/x-subtitle-cc", "text/cc")));

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

TEST_F(GstreamerMseSubtitleSinkTests, ShouldNotReattachOnChangedCapsWhileSourceIsAttached)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    setPausedState(pipeline, sink);
    const int32_t sourceId{subtitleSourceWillBeAttached(createDefaultMediaSource())};
    allSourcesWillBeAttached();

    GstCaps *ttmlCaps{createDefaultCaps()};
    GstCaps *vttCaps{gst_caps_new_empty_simple("application/x-subtitle-vtt")};
    setCaps(sink, ttmlCaps);
    setCaps(sink, vttCaps);

    setNullState(pipeline, sourceId);
    gst_caps_unref(ttmlCaps);
    gst_caps_unref(vttCaps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldNotRetrySubtitleAttachmentAfterFailure)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    setPausedState(pipeline, sink);
    EXPECT_CALL(m_mediaPipelineMock, attachSource(_)).WillOnce(Return(false));
    allSourcesWillBeAttached();

    GstCaps *caps{createDefaultCaps()};
    GstCaps *replacementCaps{gst_caps_new_empty_simple("application/x-subtitle-vtt")};
    setCaps(sink, caps);
    setCaps(sink, replacementCaps);

    setNullState(pipeline, -1);

    gst_caps_unref(replacementCaps);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldNotApplyQueuedControlsAfterAttachmentFailure)
{
    constexpr guint64 offset{4325};
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    g_object_set(sink, "mute", kMute, nullptr);
    g_object_set(sink, "text-track-identifier", kTextTrackIdentifier.c_str(), nullptr);
    setPausedState(pipeline, sink);
    allSourcesWillBeAttached();
    GstStructure *structure{gst_structure_new("set-pts-offset", "pts-offset", G_TYPE_UINT64, offset, nullptr)};
    gst_pad_send_event(sink->priv->m_sinkPad, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure));

    EXPECT_CALL(m_mediaPipelineMock, attachSource(_)).WillOnce(Return(false));
    GstCaps *caps{createDefaultCaps()};
    setCaps(sink, caps);
    setCaps(sink, caps);

    setNullState(pipeline, -1);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldApplyLatestQueuedSubtitleControlsIncludingZeroOffset)
{
    const auto expectedSource{firebolt::rialto::IMediaPipeline::MediaSourceSubtitle{"text/ttml", kTextTrackIdentifier}};
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    g_object_set(sink, "mute", TRUE, nullptr);
    g_object_set(sink, "mute", FALSE, nullptr);
    g_object_set(sink, "text-track-identifier", "CC1", nullptr);
    g_object_set(sink, "text-track-identifier", kTextTrackIdentifier.c_str(), nullptr);

    setPausedState(pipeline, sink);
    for (guint64 offset : {guint64{4325}, guint64{0}})
    {
        GstStructure *structure{gst_structure_new("set-pts-offset", "pts-offset", G_TYPE_UINT64, offset, nullptr)};
        gst_pad_send_event(sink->priv->m_sinkPad, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure));
    }

    const int32_t sourceId{subtitleSourceWillBeAttached(expectedSource)};
    allSourcesWillBeAttached();
    EXPECT_CALL(m_mediaPipelineMock, setMute(sourceId, false)).WillOnce(Return(true));
    EXPECT_CALL(m_mediaPipelineMock, setTextTrackIdentifier(kTextTrackIdentifier)).WillOnce(Return(true));
    EXPECT_CALL(m_mediaPipelineMock, setSubtitleOffset(sourceId, 0)).WillOnce(Return(true));
    GstCaps *caps{createDefaultCaps()};
    setCaps(sink, caps);

    setNullState(pipeline, sourceId);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldIgnoreNullQueuedTextTrackIdentifier)
{
    const auto expectedSource{firebolt::rialto::IMediaPipeline::MediaSourceSubtitle{"text/ttml", kTextTrackIdentifier}};
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    g_object_set(sink, "text-track-identifier", kTextTrackIdentifier.c_str(), nullptr);
    g_object_set(sink, "text-track-identifier", nullptr, nullptr);

    setPausedState(pipeline, sink);
    const int32_t sourceId{subtitleSourceWillBeAttached(expectedSource)};
    allSourcesWillBeAttached();
    EXPECT_CALL(m_mediaPipelineMock, setTextTrackIdentifier(kTextTrackIdentifier)).WillOnce(Return(true));
    GstCaps *caps{createDefaultCaps()};
    setCaps(sink, caps);

    setNullState(pipeline, sourceId);
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

TEST_F(GstreamerMseSubtitleSinkTests, ShouldForwardRepeatedMuteAndTrackChangesAfterAttachment)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    setPausedState(pipeline, sink);
    const int32_t sourceId{subtitleSourceWillBeAttached(createDefaultMediaSource())};
    allSourcesWillBeAttached();
    GstCaps *caps{createDefaultCaps()};
    setCaps(sink, caps);

    {
        testing::InSequence sequence;
        EXPECT_CALL(m_mediaPipelineMock, setMute(sourceId, true)).WillOnce(Return(true));
        EXPECT_CALL(m_mediaPipelineMock, setTextTrackIdentifier("CC1")).WillOnce(Return(true));
        EXPECT_CALL(m_mediaPipelineMock, setMute(sourceId, false)).WillOnce(Return(true));
        EXPECT_CALL(m_mediaPipelineMock, setTextTrackIdentifier("SERVICE1")).WillOnce(Return(true));
    }
    g_object_set(sink, "mute", TRUE, nullptr);
    g_object_set(sink, "text-track-identifier", "CC1", nullptr);
    g_object_set(sink, "mute", FALSE, nullptr);
    g_object_set(sink, "text-track-identifier", "SERVICE1", nullptr);

    setNullState(pipeline, sourceId);
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

TEST_F(GstreamerMseSubtitleSinkTests, ShouldDiscardSubtitleBufferDuringFlush)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    setPausedState(pipeline, sink);
    const int32_t sourceId{subtitleSourceWillBeAttached(createDefaultMediaSource())};
    allSourcesWillBeAttached();
    GstCaps *caps{createDefaultCaps()};
    setCaps(sink, caps);

    EXPECT_TRUE(rialto_mse_base_sink_event(sink->priv->m_sinkPad, GST_OBJECT_CAST(sink), gst_event_new_flush_start()));
    EXPECT_EQ(GST_FLOW_FLUSHING,
              rialto_mse_base_sink_chain(sink->priv->m_sinkPad, GST_OBJECT_CAST(sink), gst_buffer_new()));

    setNullState(pipeline, sourceId);
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

TEST_F(GstreamerMseSubtitleSinkTests, ShouldForwardZeroAndLargeSubtitleOffsetsAfterAttachment)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    setPausedState(pipeline, sink);
    const int32_t sourceId{subtitleSourceWillBeAttached(createDefaultMediaSource())};
    allSourcesWillBeAttached();
    GstCaps *caps{createDefaultCaps()};
    setCaps(sink, caps);

    for (guint64 offset : {guint64{0}, static_cast<guint64>(G_MAXINT64)})
    {
        EXPECT_CALL(m_mediaPipelineMock, setSubtitleOffset(sourceId, offset)).WillOnce(Return(true));
        GstStructure *structure{gst_structure_new("set-pts-offset", "pts-offset", G_TYPE_UINT64, offset, nullptr)};
        gst_pad_send_event(sink->priv->m_sinkPad, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure));
    }

    setNullState(pipeline, sourceId);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldIgnoreSubtitleOffsetWithWrongValueType)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);

    setPausedState(pipeline, sink);
    const int32_t sourceId{subtitleSourceWillBeAttached(createDefaultMediaSource())};
    allSourcesWillBeAttached();
    GstCaps *caps{createDefaultCaps()};
    setCaps(sink, caps);

    GstStructure *structure{gst_structure_new("set-pts-offset", "pts-offset", G_TYPE_STRING, "123", nullptr)};
    gst_pad_send_event(sink->priv->m_sinkPad, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure));

    setNullState(pipeline, sourceId);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldKeepPlayingWhileUserTogglesSubtitlesRepeatedly)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);
    GstCaps *caps{createDefaultCaps()};
    const int32_t sourceId{startPlayingWithSubtitles(pipeline, sink, caps)};

    {
        testing::InSequence sequence;
        for (int i = 0; i < 100; ++i)
        {
            EXPECT_CALL(m_mediaPipelineMock, setMute(sourceId, i % 2 == 0)).WillOnce(Return(true));
        }
    }
    for (int i = 0; i < 100; ++i)
    {
        g_object_set(sink, "mute", i % 2 == 0, nullptr);
    }
    expectPlaybackContinues(pipeline);

    stopPlaying(pipeline, sink, sourceId);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldKeepPlayingWhenUserSwitchesCaptionTrackDuringPlayback)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);
    GstCaps *caps{createDefaultCaps()};
    const int32_t sourceId{startPlayingWithSubtitles(pipeline, sink, caps)};

    {
        testing::InSequence sequence;
        for (const char *track : {"CC1", "CC3", "SERVICE1", "TEXT1"})
        {
            EXPECT_CALL(m_mediaPipelineMock, setTextTrackIdentifier(std::string{track})).WillOnce(Return(true));
        }
    }
    for (const char *track : {"CC1", "CC3", "SERVICE1", "TEXT1"})
    {
        g_object_set(sink, "text-track-identifier", track, nullptr);
    }
    expectPlaybackContinues(pipeline);

    stopPlaying(pipeline, sink, sourceId);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldKeepPlayingWhenServerFailsToApplySubtitleControls)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);
    GstCaps *caps{createDefaultCaps()};
    const int32_t sourceId{startPlayingWithSubtitles(pipeline, sink, caps)};

    EXPECT_CALL(m_mediaPipelineMock, setMute(sourceId, false)).WillOnce(Return(false));
    EXPECT_CALL(m_mediaPipelineMock, setTextTrackIdentifier("CC1")).WillOnce(Return(false));
    EXPECT_CALL(m_mediaPipelineMock, setSubtitleOffset(sourceId, 1000)).WillOnce(Return(false));
    g_object_set(sink, "mute", FALSE, nullptr);
    g_object_set(sink, "text-track-identifier", "CC1", nullptr);
    GstStructure *structure{gst_structure_new("set-pts-offset", "pts-offset", G_TYPE_UINT64, guint64{1000}, nullptr)};
    gst_pad_send_event(sink->priv->m_sinkPad, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, structure));
    expectPlaybackContinues(pipeline);

    stopPlaying(pipeline, sink, sourceId);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldKeepPlayingWhenSubtitleSourceUnderflows)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);
    GstCaps *caps{createDefaultCaps()};
    const int32_t sourceId{startPlayingWithSubtitles(pipeline, sink, caps)};

    auto mediaPipelineClient{m_mediaPipelineClient.lock()};
    ASSERT_TRUE(mediaPipelineClient);
    mediaPipelineClient->notifyBufferUnderflow(sourceId);
    EXPECT_FALSE(waitForMessage(pipeline, GST_MESSAGE_ERROR));
    expectPlaybackContinues(pipeline);

    stopPlaying(pipeline, sink, sourceId);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldKeepPlayingWhenSubtitleSourceReportsPlaybackError)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);
    GstCaps *caps{createDefaultCaps()};
    const int32_t sourceId{startPlayingWithSubtitles(pipeline, sink, caps)};

    auto mediaPipelineClient{m_mediaPipelineClient.lock()};
    ASSERT_TRUE(mediaPipelineClient);
    mediaPipelineClient->notifyPlaybackError(sourceId, firebolt::rialto::PlaybackError::UNKNOWN);
    EXPECT_FALSE(waitForMessage(pipeline, GST_MESSAGE_ERROR));
    expectPlaybackContinues(pipeline);

    stopPlaying(pipeline, sink, sourceId);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldAllowUserControlsAndPauseResumeAfterSubtitleError)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);
    GstCaps *caps{createDefaultCaps()};
    const int32_t sourceId{startPlayingWithSubtitles(pipeline, sink, caps)};
    auto mediaPipelineClient{m_mediaPipelineClient.lock()};
    ASSERT_TRUE(mediaPipelineClient);
    mediaPipelineClient->notifyPlaybackError(sourceId, firebolt::rialto::PlaybackError::UNKNOWN);

    EXPECT_CALL(m_mediaPipelineMock, setMute(sourceId, true)).WillOnce(Return(true));
    g_object_set(sink, "mute", TRUE, nullptr);
    EXPECT_CALL(m_mediaPipelineMock, setTextTrackIdentifier("SERVICE2")).WillOnce(Return(true));
    g_object_set(sink, "text-track-identifier", "SERVICE2", nullptr);
    EXPECT_CALL(m_mediaPipelineMock, setMute(sourceId, false)).WillOnce(Return(true));
    g_object_set(sink, "mute", FALSE, nullptr);

    pipelineWillGoToPausedState(sink);
    EXPECT_EQ(GST_STATE_CHANGE_ASYNC, gst_element_set_state(pipeline, GST_STATE_PAUSED));
    sendPlaybackStateNotification(sink, firebolt::rialto::PlaybackState::PAUSED);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));
    setPlayingState(pipeline);
    sendPlaybackStateNotification(sink, firebolt::rialto::PlaybackState::PLAYING);
    EXPECT_TRUE(waitForMessage(pipeline, GST_MESSAGE_ASYNC_DONE));
    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_get_state(pipeline, nullptr, nullptr, GST_SECOND));
    expectPlaybackContinues(pipeline);

    stopPlaying(pipeline, sink, sourceId);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}

TEST_F(GstreamerMseSubtitleSinkTests, ShouldForwardNewSegmentPositionsAfterSubtitleError)
{
    RialtoMSEBaseSink *sink = createSubtitleSink();
    GstElement *pipeline = createPipelineWithSink(sink);
    GstCaps *caps{createDefaultCaps()};
    const int32_t sourceId{startPlayingWithSubtitles(pipeline, sink, caps)};
    auto mediaPipelineClient{m_mediaPipelineClient.lock()};
    ASSERT_TRUE(mediaPipelineClient);
    mediaPipelineClient->notifyPlaybackError(sourceId, firebolt::rialto::PlaybackError::UNKNOWN);

    for (guint64 position : {guint64{30 * GST_SECOND}, guint64{5 * GST_SECOND}})
    {
        GstSegment segment;
        gst_segment_init(&segment, GST_FORMAT_TIME);
        segment.start = position;
        segment.position = position;
        EXPECT_CALL(m_mediaPipelineMock, setSourcePosition(sourceId, position, false, 1.0, GST_CLOCK_TIME_NONE))
            .WillOnce(Return(true));
        EXPECT_TRUE(
            rialto_mse_base_sink_event(sink->priv->m_sinkPad, GST_OBJECT_CAST(sink), gst_event_new_segment(&segment)));
    }
    expectPlaybackContinues(pipeline);

    stopPlaying(pipeline, sink, sourceId);
    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}