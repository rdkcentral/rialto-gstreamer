/*
 * Copyright (C) 2026 Sky UK
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

#include "GstRialtoComponentTest.h"

#include <gst/app/gstappsrc.h>

#include <string>

using ::testing::_;
using ::testing::AtLeast;

namespace
{
const std::string kAacCodecData{"\x11\x90", 2}; // AAC LC, 48 kHz, stereo
const char *kAacCaps{"audio/mpeg, mpegversion=(int)4, stream-format=(string)raw, rate=(int)48000, channels=(int)2, "
                     "codec_data=(buffer)1190"};
constexpr unsigned kNumFrames{10};
constexpr size_t kFrameSize{64};
constexpr GstClockTime kFrameDuration{1024 * GST_SECOND / 48000};
constexpr unsigned kMaxNeedDataRetries{20};

MATCHER_P(SessionIs, sessionId, "")
{
    return arg->session_id() == sessionId;
}

MATCHER(LoadsMseUrl, "")
{
    return arg->session_id() == GstRialtoComponentTest::kSessionId &&
           arg->type() == firebolt::rialto::LoadRequest::MSE && arg->url() == "mse://1";
}

MATCHER_P3(AttachesAudioSource, mimeType, numberOfChannels, sampleRate, "")
{
    return arg->session_id() == GstRialtoComponentTest::kSessionId && arg->mime_type() == mimeType &&
           !arg->has_drm() && arg->has_audio_config() &&
           arg->audio_config().number_of_channels() == numberOfChannels &&
           arg->audio_config().sample_rate() == sampleRate && arg->codec_data().data() == kAacCodecData &&
           !arg->switch_source();
}

MATCHER_P2(RemovesSource, sessionId, sourceId, "")
{
    return arg->session_id() == sessionId && arg->source_id() == sourceId;
}
} // namespace

class AudioSinkPlaybackTest : public GstRialtoComponentTest
{
protected:
    /** Requests the server must receive during a normal audio playback from start to stop */
    void expectAudioPlaybackRequests()
    {
        EXPECT_CALL(*m_controlModuleMock, registerClient(_, _, _, _));
        EXPECT_CALL(*m_controlModuleMock, getSharedMemory(_, _, _, _));
        EXPECT_CALL(*m_mediaPipelineModuleMock, createSession(_, _, _, _));
        EXPECT_CALL(*m_mediaPipelineModuleMock, load(_, LoadsMseUrl(), _, _));
        EXPECT_CALL(*m_mediaPipelineModuleMock, attachSource(_, AttachesAudioSource("audio/mp4", 2u, 48000u), _, _));
        EXPECT_CALL(*m_mediaPipelineModuleMock, allSourcesAttached(_, SessionIs(kSessionId), _, _));
        EXPECT_CALL(*m_mediaPipelineModuleMock, pause(_, SessionIs(kSessionId), _, _)).Times(AtLeast(1));
        EXPECT_CALL(*m_mediaPipelineModuleMock, play(_, SessionIs(kSessionId), _, _));
        EXPECT_CALL(*m_mediaPipelineModuleMock, removeSource(_, RemovesSource(kSessionId, kAudioSourceId), _, _));
        EXPECT_CALL(*m_mediaPipelineModuleMock, stop(_, SessionIs(kSessionId), _, _));
        EXPECT_CALL(*m_mediaPipelineModuleMock, destroySession(_, SessionIs(kSessionId), _, _));
    }

    /** Common steps: start the pipeline, deliver kNumFrames AAC frames to the server and reach PLAYING */
    void playAacUntilPlaying()
    {
        // Step 1: appsrc ! rialtomseaudiosink, audio only, clear content
        createAudioPipeline(kAacCaps);
        ASSERT_FALSE(HasFatalFailure());

        // Step 2: NULL -> READY registers the client, the server reports RUNNING and shares its memory.
        //         READY -> PAUSED creates the session and loads it; the sink waits for the server (ASYNC).
        EXPECT_EQ(startPlaying(), GST_STATE_CHANGE_ASYNC);
        ASSERT_TRUE(waitForServerCall("load"));
        sendNetworkStateBuffering();

        // Step 3: the caps attach the AAC source, then the sink asks the server to pause (preroll)
        pushAudioBuffers(kNumFrames, kFrameSize, kFrameDuration);
        ASSERT_FALSE(HasFatalFailure());
        ASSERT_TRUE(waitForServerCall("pause"));
        ASSERT_TRUE(waitForQueuedInSink((kNumFrames - 1) * kFrameDuration));

        // Step 4: the server asks for data; all frames are written to the shared memory
        const uint32_t kRequestId{sendNeedData(kAudioSourceId, kNumFrames)};
        HaveDataCall haveData{};
        ASSERT_TRUE(waitForHaveData(kRequestId, haveData)) << "haveData not received";
        EXPECT_EQ(haveData.status, firebolt::rialto::HaveDataRequest::OK);
        ASSERT_EQ(haveData.numFrames, kNumFrames);

        const std::vector<ShmFrame> kFrames{readFramesFromShm(kNumFrames)};
        ASSERT_EQ(kFrames.size(), kNumFrames);
        for (unsigned i = 0; i < kNumFrames; ++i)
        {
            SCOPED_TRACE("frame " + std::to_string(i));
            EXPECT_EQ(kFrames[i].metadata.length(), kFrameSize);
            EXPECT_EQ(kFrames[i].metadata.time_position(), static_cast<int64_t>(i * kFrameDuration));
            EXPECT_EQ(kFrames[i].metadata.sample_duration(), static_cast<int64_t>(kFrameDuration));
            EXPECT_EQ(kFrames[i].metadata.stream_id(), static_cast<uint32_t>(kAudioSourceId));
            EXPECT_EQ(kFrames[i].metadata.sample_rate(), 48000u);
            EXPECT_EQ(kFrames[i].metadata.channels_num(), 2u);
            EXPECT_EQ(kFrames[i].data, std::vector<uint8_t>(kFrameSize, static_cast<uint8_t>(i + 1)));
        }

        // Step 5: the server prerolled (PAUSED); the sink continues to PLAYING and the server confirms it
        sendPlaybackState(firebolt::rialto::PlaybackState::PAUSED);
        ASSERT_TRUE(waitForServerCall("play"));
        sendPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
        ASSERT_TRUE(waitForPipelineState(GST_STATE_PLAYING));
    }
};

/*
 * Component Test: AAC audio playback
 * Test Objective:
 *  Check that rialtomseaudiosink, together with the real RialtoClient, sends the correct requests to the server,
 *  writes the pushed AAC frames to the shared memory and reaches PLAYING when the server confirms the playback.
 *
 * Test Setup:
 *  Language: C++
 *  Testing Framework: Google Test
 *  Components: rialtomseaudiosink, RialtoClient, Rialto IPC, ServerStub with module mocks (simulated RialtoServer)
 *
 * Test Steps:
 *  1. Create appsrc ! rialtomseaudiosink with AAC caps (48 kHz, stereo)
 *  2. Request PLAYING; the server reports RUNNING after registration, then network state BUFFERING
 *  3. Push 10 AAC frames
 *  4. The server requests 10 frames
 *  5. The server reports PAUSED, then PLAYING
 *  6. Stop the pipeline
 *
 * Expected Results:
 *  - registerClient, getSharedMemory, createSession, load("mse://1") are received
 *  - attachSource with audio/mp4, 2 channels, 48000 Hz, codec data 0x1190, no DRM, then allSourcesAttached and pause
 *  - haveData(OK, 10 frames); the shared memory contains the 10 frames with the pushed sizes, timestamps and data
 *  - play is received after PAUSED, and the pipeline reaches PLAYING after PLAYING; no error on the bus
 *  - removeSource, stop and destroySession are received when the pipeline is stopped
 */
TEST_F(AudioSinkPlaybackTest, attachesSourceDeliversDataAndPlays)
{
    expectAudioPlaybackRequests();

    playAacUntilPlaying();
    ASSERT_FALSE(HasFatalFailure());

    GstBus *bus = gst_element_get_bus(m_pipeline);
    GstMessage *error = gst_bus_pop_filtered(bus, GST_MESSAGE_ERROR);
    EXPECT_EQ(error, nullptr) << "Unexpected error on the bus";
    if (error)
        gst_message_unref(error);
    gst_object_unref(bus);

    // Step 6: stop - the source is removed and the session destroyed
    stopPipeline();
}

/*
 * Component Test: end of stream
 * Test Objective:
 *  Check that the end of stream travels from the pipeline to the server, and from the server back to the
 *  application as an EOS message on the bus.
 *
 * Test Setup:
 *  Language: C++
 *  Testing Framework: Google Test
 *  Components: rialtomseaudiosink, RialtoClient, Rialto IPC, ServerStub with module mocks (simulated RialtoServer)
 *
 * Test Steps:
 *  1. Play 10 AAC frames until PLAYING (same steps as attachesSourceDeliversDataAndPlays)
 *  2. appsrc sends end of stream
 *  3. The server requests more data until the sink answers with EOS
 *  4. The server reports END_OF_STREAM
 *  5. Stop the pipeline
 *
 * Expected Results:
 *  - haveData with status EOS and 0 frames is received
 *  - an EOS message (and no error) is posted on the pipeline bus
 */
TEST_F(AudioSinkPlaybackTest, reportsEndOfStream)
{
    expectAudioPlaybackRequests();

    playAacUntilPlaying();
    ASSERT_FALSE(HasFatalFailure());

    // Step 2: no more data
    ASSERT_EQ(gst_app_src_end_of_stream(GST_APP_SRC(m_appSrc)), GST_FLOW_OK);

    // Step 3: like the real server, ask again until the sink reports EOS (the EOS event may still be on its way)
    HaveDataCall haveData{};
    for (unsigned attempt = 0; attempt < kMaxNeedDataRetries; ++attempt)
    {
        const uint32_t kRequestId{sendNeedData(kAudioSourceId, kNumFrames)};
        ASSERT_TRUE(waitForHaveData(kRequestId, haveData)) << "haveData not received";
        if (haveData.status != firebolt::rialto::HaveDataRequest::NO_AVAILABLE_SAMPLES)
        {
            break;
        }
    }
    EXPECT_EQ(haveData.status, firebolt::rialto::HaveDataRequest::EOS);
    EXPECT_EQ(haveData.numFrames, 0u);

    // Step 4: the server played everything
    sendPlaybackState(firebolt::rialto::PlaybackState::END_OF_STREAM);
    GstMessage *message = waitForBusMessage(static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
    ASSERT_NE(message, nullptr) << "No EOS message on the bus";
    EXPECT_EQ(GST_MESSAGE_TYPE(message), GST_MESSAGE_EOS) << "Received " << GST_MESSAGE_TYPE_NAME(message);
    gst_message_unref(message);

    // Step 5: stop
    stopPipeline();
}
