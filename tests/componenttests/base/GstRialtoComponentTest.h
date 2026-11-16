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

#ifndef GST_RIALTO_COMPONENT_TEST_H_
#define GST_RIALTO_COMPONENT_TEST_H_

#include "ControlCommon.h"
#include "ControlModuleMock.h"
#include "MediaCapabilitiesModuleMock.h"
#include "MediaCommon.h"
#include "MediaKeysCapabilitiesModuleMock.h"
#include "MediaKeysModuleMock.h"
#include "MediaPipelineCapabilitiesModuleMock.h"
#include "MediaPipelineModuleMock.h"
#include "PrivateMetricsModuleMock.h"
#include "ServerStub.h"
#include "WebAudioPlayerModuleMock.h"
#include "metadata.pb.h"

#include <gmock/gmock.h>
#include <gst/gst.h>
#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

/**
 * Base fixture of the rialto-gstreamer component tests.
 *
 * Real:      GStreamer pipeline, rialto-gstreamer sinks (static plugin), RialtoClient, Rialto IPC, shared memory.
 * Simulated: RialtoServer - the ServerStub of the Rialto client component tests, whose modules are GoogleMock
 *            objects. Every request is answered by default (see setDefaultServerBehaviour()), so tests only add
 *            EXPECT_CALLs for the requests they want to check.
 *
 * A new ServerStub, new mocks and new shared memory are created for every test.
 */
class GstRialtoComponentTest : public ::testing::Test
{
public:
    static constexpr int32_t kControlId{1};
    static constexpr int32_t kSessionId{10};
    static constexpr int32_t kAudioSourceId{1};
    static constexpr std::chrono::seconds kTimeout{5};

    /** One haveData request received by the simulated server */
    struct HaveDataCall
    {
        ::firebolt::rialto::HaveDataRequest_MediaSourceStatus status;
        uint32_t numFrames;
        uint32_t requestId;
    };

    /** One media frame written by RialtoClient to the shared memory (metadata V2 layout) */
    struct ShmFrame
    {
        ::firebolt::rialto::MediaSegmentMetadata metadata;
        std::vector<uint8_t> data;
    };

protected:
    GstRialtoComponentTest();
    ~GstRialtoComponentTest() override;

    // ---- Simulated server: events sent to the client ----
    void sendApplicationStateRunning();
    void sendNetworkStateBuffering();
    void sendPlaybackState(::firebolt::rialto::PlaybackState state);
    uint32_t sendNeedData(int32_t sourceId, size_t frameCount); // returns the need data request id

    // ---- Simulated server: what it received ----
    bool waitForServerCall(const std::string &method, unsigned count = 1);
    bool waitForHaveData(uint32_t requestId, HaveDataCall &result);
    std::vector<ShmFrame> readFramesFromShm(uint32_t numFrames) const;

    // ---- GStreamer side ----
    void createAudioPipeline(const char *caps);
    GstStateChangeReturn startPlaying(); // also completes the NULL->READY handshake (application state RUNNING)
    void pushAudioBuffers(unsigned count, size_t size, GstClockTime duration);
    bool waitForQueuedInSink(GstClockTime lastPts);
    bool waitForPipelineState(GstState state);
    GstMessage *waitForBusMessage(GstMessageType types);
    void stopPipeline();

    // Simulated server
    std::shared_ptr<::testing::NiceMock<ControlModuleMock>> m_controlModuleMock;
    std::shared_ptr<::testing::NiceMock<MediaPipelineModuleMock>> m_mediaPipelineModuleMock;
    std::shared_ptr<::testing::NiceMock<MediaCapabilitiesModuleMock>> m_mediaCapabilitiesModuleMock;
    std::shared_ptr<::testing::NiceMock<MediaKeysModuleMock>> m_mediaKeysModuleMock;
    std::shared_ptr<::testing::NiceMock<MediaKeysCapabilitiesModuleMock>> m_mediaKeysCapabilitiesModuleMock;
    std::shared_ptr<::testing::NiceMock<MediaPipelineCapabilitiesModuleMock>> m_mediaPipelineCapabilitiesModuleMock;
    std::shared_ptr<::testing::NiceMock<WebAudioPlayerModuleMock>> m_webAudioPlayerModuleMock;
    std::shared_ptr<::testing::NiceMock<PrivateMetricsModuleMock>> m_privateMetricsModuleMock;
    std::shared_ptr<::firebolt::rialto::client::ct::ServerStub> m_serverStub;

    // Pipeline: appsrc ! rialtomseaudiosink
    GstElement *m_pipeline{nullptr};
    GstElement *m_appSrc{nullptr};
    GstElement *m_sink{nullptr};

private:
    std::function<void(::google::protobuf::RpcController *, ::google::protobuf::Closure *)>
    respond(const std::string &method);
    void recordCall(const std::string &method);
    void setDefaultServerBehaviour();
    void initSharedMemory();
    void termSharedMemory();
    void waitForClientDisconnected();

    std::mutex m_callsMutex;
    std::condition_variable m_callsCond;
    std::map<std::string, unsigned> m_calls;
    std::vector<HaveDataCall> m_haveDataCalls;

    int m_shmFd{-1};
    uint8_t *m_shmAddress{nullptr};
    uint32_t m_needDataRequestId{0};
};

#endif // GST_RIALTO_COMPONENT_TEST_H_
