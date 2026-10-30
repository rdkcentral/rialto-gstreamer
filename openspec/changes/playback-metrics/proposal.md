# Proposal: Implement Metrics Integration (GStreamer Producer Validation & Client Consumer)

**Proposal: Establish metrics validation for GStreamer producer elements and implement client-side Sink APIs for query/callback**

---

## Why

**Problem Statement:**
- Rialto-Server relay (separate proposal) exposes two RPC flows for metrics (QUERY, PUSH), but rialto-gstreamer lacks producer validation and client-side implementation
- GStreamer SoC elements (westeros-sink, audio/video decoders) emit `media-pipeline-metric` bus messages, but there's no formalized validation that these messages conform to the expected format, enum values, and counter semantics
- RialtoMSE Sink (client library) currently has no methods to query frame counters or receive metric event callbacks
- Consumer applications cannot access metrics without client-side implementation bridges

**Value Statement:**
- Ensure producer (westeros-sink, decoders) correctness before metrics reach server layer
- Provide complete RialtoMSE Sink APIs matching Phase 1 two RPC flows (QUERY, PUSH)
- Enable consumer applications (Netflix, Cobalt, WPE) to access metrics through formalized client interfaces
- Foundation for end-to-end metrics pipeline validation from GStreamer → Server → Client → Apps

---

## What Changes

**Architecture Overview:**
Producer validation + client consumer implementation for metrics passthrough (consuming RialtoMetrics relay from rialto repo):

```
GStreamer SoC (Validation)
        ↓ produces
westeros-sink + Decoders (must emit correct media-pipeline-metric messages)
        ↓ consumed by
    RialtoMetrics (in rialto repo - relay layer)
        ↓ served via IPC to
RialtoMSE Sink (client library - implements two RPC flows)
        ↓ exposes APIs to
Consumer Applications (Netflix DPI, Cobalt, WPE-WebKit)
```

**Relationship to Rialto:**
- **Rialto** (separate proposal): Creates RialtoMetrics module, exposes IPC service (GetFrameCounters, NotifyMetricEvent RPCs) - Phase 1 with no SetMetricSubscription
- **Rialto-GStreamer** (this proposal): Validates producer message format; implements client-side consumer APIs that call into Rialto's IPC service
- **Two RPC Flows (Phase 1)** (defined in Rialto, consumed by Rialto-GStreamer):
  - QUERY FLOW: getFrameCounters() → GetFrameCountersRequest RPC → Rialto returns cached counters
  - PUSH FLOW: NotifyMetricEvent RPC ← Rialto pushes events → onMetricEvent() callback
- **Session Context:** Rialto's `GetFrameCountersRequest` carries `session_id` + `source_id`; Rialto-GStreamer's client APIs only expose `sourceId` because `session_id` is implicit to the IPC channel (one MediaPipeline session per client connection) and is populated by `MediaPipelineClient` before the RPC is sent


**High-Level Modifications:**

### 1. **Create Producer Validation Tests** (New)
- **File:** `rialto-gstreamer/tests/unittests/metrics_producer_validation_test.cpp`
- **Test Cases:**
  - **Westeros-Sink Message Structure**: Verify westeros-sink emits GStreamer custom message with structure name exactly `"media-pipeline-metric"`
  - **Required Fields**: Verify message contains all required fields:
    - `metric_type` (uint): 0-6 corresponding to MetricType enum
    - `source_id` (uint): valid source identifier
    - `rendered_frames` (uint64): non-zero cumulative count
    - `dropped_frames` (uint64): non-zero when drops occur
    - `corrupted_frames` (uint64): frames with decode errors
    - `pts_ns` (uint64): valid presentation timestamp
  - **Counter Monotonicity**: Verify rendered_frames, dropped_frames, corrupted_frames never decrease over sequence of messages
  - **Enum Validation**: Verify MetricType values 0-6 are valid
  - **Decoder Error Types**: Verify VIDEO_DECODE_ERROR (type=2) and AUDIO_DECODE_ERROR (type=3) are posted on decode failures
  - **PTS Accuracy**: Verify PTS field matches affected frame's presentation time

### 2. **Create Producer Component Tests** (New)
- **File:** `rialto-gstreamer/tests/componenttests/metrics_producer_integration_test.cpp`
- **Test Cases:**
  - Real GStreamer pipeline with simulated frame drops
  - Verify westeros-sink posts metric messages when drops occur
  - Verify counter values increase correctly with drop events
  - Real GStreamer pipeline with simulated decode errors
  - Verify decoders post metric messages for decode failures
  - Verify metric messages propagate to Rialto-Server (if available in test environment)

### 3. **Add RialtoMSE Sink Metrics API Methods** (New)
- **File:** `rialto-gstreamer/source/GStreamerMSEMediaPlayerClient.cpp` / `.h`
- **Changes (Phase 1):**
  - Add member variables:
    - `std::map<int32_t, FrameCounters> m_lastCounters` (sourceId → latest counter snapshot: rendered/dropped/corrupted/timestamp)
    - `std::function<void(int32_t, const NotifyMetricEvent &)> m_metricCallback` (application callback)
  - Add public method: `int32_t getFrameCounters(int32_t sourceId, FrameCounters *out)` (QUERY FLOW)
    - Call `MediaPipelineClient::getFrameCounters(sourceId, out)`
    - Return 0 on success, non-zero on error
  - Add callback method: `void onMetricEvent(int32_t sourceId, const NotifyMetricEvent &event)` (PUSH FLOW - Phase 1: all events received, no filtering)
    - Forward to m_metricCallback if application registered one
    - Update m_lastCounters cache (for rapid access without RPC)
  - Add registration method: `void setMetricCallback(std::function<void(int32_t, const NotifyMetricEvent &)> callback)`
    - Store application callback in m_metricCallback
    - Called to enable event delivery

### 4. **Integrate with IPC Client Layer** (Modification)
- **File:** `rialto-gstreamer/source/MediaPipelineClient.cpp` / `.h`
- **Changes (Phase 1):**
  - Implement `bool getFrameCounters(int32_t sourceId, FrameCounters &out)` (QUERY FLOW)
    - Build GetFrameCountersRequest protobuf message
    - Send IPC RPC call to Rialto-Server (blocking)
    - Copy response counters to out
    - Return true on success
  - Register async callback handler for incoming NotifyMetricEvent from server (PUSH FLOW)
    - Forward to RialtoMSEMediaPlayerClient::onMetricEvent()

### 5. **Create Consumer Unit Tests** (New)
- **File:** `rialto-gstreamer/tests/unittests/metrics_consumer_test.cpp`
- **Test Cases (Phase 1):**
  - Mock Rialto-Server: Verify RialtoMSE Sink correctly calls getFrameCounters() RPC
  - Counter Cache: Verify m_lastCounters updated on metric event
  - Callback Registration: Verify application callback invoked when metric event received
  - Thread Safety: Verify concurrent getFrameCounters and callback delivery don't cause races

### 6. **Create Consumer Component Tests** (New)
- **File:** `rialto-gstreamer/tests/componenttests/metrics_consumer_integration_test.cpp`
- **Test Cases (Phase 1):**
  - Mock Rialto-Server injecting NotifyMetricEvent messages
  - Verify RialtoMSE Sink callback delivers all received events (no filtering in Phase 1)
  - Verify getFrameCounters() blocking behavior and response handling
  - Verify counter monotonicity across multiple queries

### 7. **Add Client API Documentation** (New)
- **File:** `rialto-gstreamer/docs/MetricsIntegration.md`
- **Content:**
  - Architecture: How client layer connects to server metrics relay
  - API Usage Guide:
    - Code example: Register callback
    - Code example: Query frame counters
  - Thread Safety Notes: Safe to call from multiple threads
  - Example Consumer Code:
    - Netflix DPI: Register callback to receive frame drop events
    - Cobalt: Query frame counters on-demand
    - WPE-WebKit: Access frame-counters via property

### 8. **Update CMakeLists.txt** (Modification)
- **File:** `rialto-gstreamer/tests/CMakeLists.txt`
- **Changes:**
  - Add metrics_producer_validation_test to unit test targets
  - Add metrics_consumer_test to unit test targets
  - Add metrics_producer_integration_test to component test targets
  - Add metrics_consumer_integration_test to component test targets
  - Link protobuf message stubs for MetricType, GetFrameCounters*, NotifyMetricEvent

---

## Non-Goals (Phase 1)

- **GStreamer SoC Source Code Modification**: Will not modify westeros-sink or decoder source code (assume they emit correct metric messages; validation is testing only)
- **Rialto-Server Implementation**: Will not implement server relay (separate proposal; assume server provides getFrameCounters, NotifyMetricEvent callbacks)
- **Subscription Management**: Phase 1 has no per-client filtering (all metrics pushed to all clients); SetMetricSubscription deferred to Phase 2
- **Consumer Application Logic**: Will not implement Netflix DPI, Cobalt, or WPE-WebKit specific business logic (only provide APIs for applications to use)
- **Per-Session Aggregation**: Metrics handled per-source; session-level aggregation is application responsibility
- **Metrics Buffering on Client**: Client is passthrough only; no buffering (Phase 1 delivers all events immediately)

---

## Impacted Areas

| Component | Impact Type | Location / Path | Details |
|-----------|------------|-----------------|---------|
| **Producer Validation Tests** | New Test Suite | `rialto-gstreamer/tests/unittests/metrics_producer_validation_test.cpp` | Verify westeros-sink/decoder message format, enum values, counter fields, monotonicity |
| **Producer Component Tests** | New Test Suite | `rialto-gstreamer/tests/componenttests/metrics_producer_integration_test.cpp` | Real GStreamer pipeline: inject drops/errors, verify metric messages generated |
| **RialtoMSE Sink Metrics API** | New Public Methods | `rialto-gstreamer/source/GStreamerMSEMediaPlayerClient.cpp/.h` | Add `getFrameCounters()`, `onMetricEvent()`, `setMetricCallback()` (Phase 1, no subscription) |
| **Counter Cache** | New Data Structure | `rialto-gstreamer/source/GStreamerMSEMediaPlayerClient.cpp` | Add map: `std::map<int32_t, FrameCounters>` (sourceId → last snapshot: rendered/dropped/corrupted/timestamp) |
| **Callback Handler** | New Data Member | `rialto-gstreamer/source/GStreamerMSEMediaPlayerClient.cpp` | Add function pointer: `std::function<void(int32_t, const NotifyMetricEvent &)>` for app callback (Phase 1: receives all events) |
| **IPC Client RPC Callers** | New RPC Stubs | `rialto-gstreamer/source/MediaPipelineClient.cpp/.h` | Implement `getFrameCounters()`, async callback handler for NotifyMetricEvent (Phase 1, no subscription) |
| **Consumer Unit Tests** | New Test Suite | `rialto-gstreamer/tests/unittests/metrics_consumer_test.cpp` | Mock server: verify RPC calls, callback delivery, thread safety (Phase 1, no subscription filtering) |
| **Consumer Component Tests** | New Test Suite | `rialto-gstreamer/tests/componenttests/metrics_consumer_integration_test.cpp` | Injected NotifyMetricEvent, verify callback delivery, query responses (Phase 1, no filtering) |
| **Client API Documentation** | New Specification | `rialto-gstreamer/docs/MetricsIntegration.md` | API usage guide, code examples for QUERY and PUSH flows, consumer application patterns (Phase 1) |
| **Build System** | Configuration Updates | `rialto-gstreamer/tests/CMakeLists.txt` | Add new test targets; link protobuf message stubs |

---

## Success Criteria

**Producer-Side Validation:**
- ✅ westeros-sink emits GStreamer custom message with structure name exactly `"media-pipeline-metric"`
- ✅ Message contains all required fields: metric_type, source_id, rendered_frames, dropped_frames, corrupted_frames, pts_ns
- ✅ MetricType enum values 0-6 match expected types (VIDEO_FRAME_DROP, AUDIO_FRAME_DROP, VIDEO_DECODE_ERROR, AUDIO_DECODE_ERROR, VIDEO_FRAME_REPEAT, AUDIO_GAP, BUFFER_UNDERFLOW)
- ✅ Frame counters strictly monotonic (never decrease) over sequence of metric messages
- ✅ VIDEO_DECODE_ERROR (type=2) posted on video decoder failures
- ✅ AUDIO_DECODE_ERROR (type=3) posted on audio decoder failures
- ✅ PTS field accurate for affected frames
- ✅ Producer validation unit tests pass (message parsing, field validation, monotonicity checks)
- ✅ Producer component tests pass (real pipeline, frame drop/error scenarios)

**Consumer-Side Implementation (Phase 1):**
- ✅ RialtoMSEMediaPlayerClient::getFrameCounters() method compiles and callable
- ✅ getFrameCounters() calls MediaPipelineClient::getFrameCounters() RPC and returns result
- ✅ onMetricEvent() callback delivered for all NotifyMetricEvent messages from server (no filtering in Phase 1)
- ✅ setMetricCallback() successfully registers application callback
- ✅ Application callback invoked when server sends NotifyMetricEvent (PUSH FLOW)
- ✅ Counter cache (m_lastCounters) updated on metric events for rapid access

**Thread Safety & Robustness (Phase 1):**
- ✅ Concurrent getFrameCounters calls and callback delivery do not cause deadlocks
- ✅ Concurrent callback delivery operations do not cause data races
- ✅ Thread-safe map operations (mutex or atomic protection)
- ✅ No segfaults on concurrent operations

**Testing Coverage (Phase 1):**
- ✅ Producer validation unit tests verify message structure and field correctness
- ✅ Producer component tests verify real pipeline metric emission
- ✅ Consumer unit tests verify RPC calls, callback delivery, thread safety (mocked server)
- ✅ Consumer component tests verify callback delivery and counter queries (mocked server with injected events)
- ✅ All tests pass without segfaults or memory leaks
- ✅ No regressions in existing Rialto-GStreamer tests

**Documentation & Examples (Phase 1):**
- ✅ MetricsIntegration.md includes code examples for getFrameCounters()
- ✅ MetricsIntegration.md includes code examples for callback registration
- ✅ Example consumer code for Netflix DPI (callback to receive events)\n- ✅ Example consumer code for Cobalt (on-demand queries)\n- ✅ Example consumer code for WPE-WebKit (callback-based integration)

---

## Rollback Plan

### **If Producer Validation Tests Fail:**

1. **Disable Producer Tests (Keep Infrastructure):**
   ```bash
   # Comment out test targets in CMakeLists.txt
   git checkout HEAD rialto-gstreamer/tests/CMakeLists.txt
   # Manually remove: metrics_producer_validation_test, metrics_producer_integration_test
   
   # Rebuild without producer tests
   cd build && cmake .. && make -j$(nproc)
   ```

2. **Full Revert If Validation Reveals SoC Issues:**
   ```bash
   # Revert test files
   git rm rialto-gstreamer/tests/unittests/metrics_producer_validation_test.cpp
   git rm rialto-gstreamer/tests/componenttests/metrics_producer_integration_test.cpp
   
   # Revert CMakeLists
   git checkout HEAD rialto-gstreamer/tests/CMakeLists.txt
   
   # Rebuild
   cd build && cmake .. && make -j$(nproc)
   ```

### **If RialtoMSE Sink API Implementation Fails:**

1. **Revert Sink Implementation:**
   ```bash
   # Revert all RialtoMSEMediaPlayerClient changes
   git checkout HEAD rialto-gstreamer/source/GStreamerMSEMediaPlayerClient.cpp
   git checkout HEAD rialto-gstreamer/source/GStreamerMSEMediaPlayerClient.h
   
   # Rebuild without metrics APIs
   cd build && cmake .. && make -j$(nproc)
   
   # Verify existing client functionality unchanged
   ./tests/componenttests/MediaPipelineClientTest
   ```

### **If IPC Client RPC Callers Fail:**

1. **Revert IPC Client Changes:**
   ```bash
   # Revert MediaPipelineClient metric stubs
   git checkout HEAD rialto-gstreamer/source/MediaPipelineClient.cpp
   git checkout HEAD rialto-gstreamer/source/MediaPipelineClient.h
   
   # Rebuild
   cd build && cmake .. && make -j$(nproc)
   ```

### **If Consumer Tests Fail:**

1. **Disable Consumer Tests (Keep Implementation):**
   ```bash
   # Revert CMakeLists to remove consumer test targets
   git checkout HEAD rialto-gstreamer/tests/CMakeLists.txt
   
   # Rebuild without tests (keeps implementation)
   cd build && cmake .. && make -j$(nproc)
   ```

2. **Full Revert of Consumer Tests:**
   ```bash
   # Remove test files
   git rm rialto-gstreamer/tests/unittests/metrics_consumer_test.cpp
   git rm rialto-gstreamer/tests/componenttests/metrics_consumer_integration_test.cpp
   
   # Revert CMakeLists
   git checkout HEAD rialto-gstreamer/tests/CMakeLists.txt
   
   # Rebuild
   cd build && cmake .. && make -j$(nproc)
   ```

### **Complete Rollback (All Changes):**

1. **Revert All Commits:**
   ```bash
   # Revert entire change by commit hash
   git revert <commit-hash-metrics-integration>
   
   # Or revert range
   git revert <first-commit>^..<last-commit>
   
   # Clean rebuild
   cd build && cmake --fresh .. && make -j$(nproc) clean all
   ```

2. **Verification:**
   ```bash
   # Confirm no metric code remains
   grep -r "setMetricSubscription\|getFrameCounters\|onMetricEvent\|media-pipeline-metric" \
     rialto-gstreamer/source/ rialto-gstreamer/tests/ || echo "VERIFIED CLEAN"
   
   # Run full test suite
   ./tests/unittests/*Test
   ./tests/componenttests/*Test
   ```

### **Feature Flag Approach (Gradual Rollback):**

```cpp
// In GStreamerMSEMediaPlayerClient.h
#define ENABLE_METRICS_CONSUMER_API 1  // Set to 0 to disable

#if ENABLE_METRICS_CONSUMER_API
    int32_t getFrameCounters(int32_t sourceId, FrameCounters *out);
    void setMetricCallback(std::function<void(int32_t, const NotifyMetricEvent &)> callback);
    void onMetricEvent(int32_t sourceId, const NotifyMetricEvent &event);
#else
    // Stubs return failure
    int32_t getFrameCounters(...) { return -1; }
    void setMetricCallback(...) { }
    void onMetricEvent(...) { }
#endif
```

Set `ENABLE_METRICS_CONSUMER_API = 0` at compile time to disable metrics without full source revert.
