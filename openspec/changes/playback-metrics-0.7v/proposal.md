Proposal: GStreamer Client Compatibility Update for Southbound SoC Metric Contract (0.7v)

## Implementation Guidance

**IMPORTANT:** Do not make assumptions or infer behavior if there is any ambiguity in this proposal or the specification. If you have any doubt about the correct implementation approach, field semantics, validation rules, contract details, or client callback behavior, ask for clarification before proceeding. Incorrect assumptions can lead to subtle compatibility issues that are difficult to debug.

## Specification Reference

**Baseline specification:** SPECIFICATION-soc-media-metrics_new-soc.md, version 0.7 (dated 2026-11-12)

This proposal is written against and must remain aligned with the normative SoC media metrics specification version 0.7. The GStreamer client consumer must accept exactly what the server-side producer emits per this specification.

Why

The GStreamer client still validates and consumes a legacy metric payload that does not match the normative SoC contract. The current consumer path continues to expect older names and semantics such as `metric_type`, `phase`, `pts_ns`, and legacy producer-scoped fields, even though the server-side contract and the agreed 0.7v specification require the `media-pipeline-metric` structure with `metric` and `observation-kind` semantics.

This change is required because the client is the component that interprets the SoC metric stream. If the consumer accepts the wrong field names or wrong GTypes, it will mis-handle valid metric events even when the upstream producer is correct. This proposal therefore updates only the client-side validation and event-processing path to match the delivered 0.7v contract without altering the broader playback-metrics design.

This proposal is intentionally a client-side compatibility fix. It keeps the existing GStreamer consumer flow and API behavior intact while updating the metric validation and parsing logic so it conforms to the agreed 0.7 contract.

What Changes

- Update the GStreamer client validator to accept the normative `media-pipeline-metric` envelope enriched with server-provided source/session context.
- Require the current 0.7 fields: `metric` and `observation-kind`.
- Accept the exact optional fields and GTypes required by the SoC contract: `pts-ns` as `G_TYPE_INT64`, `duration-ns` as `G_TYPE_UINT64`, and `count` as `G_TYPE_UINT64`.
- Accept and validate `source_id` and `session_id` as required enriched fields provided by the Rialto server (extracted from GStreamer message source element, not from SoC payload).
- Reject legacy field names and semantics such as `metric_type`, `phase`, and `pts_ns` (underscore version).
- Keep the existing GStreamer client event-dispatch logic, callback model, and public consumer flow intact while enforcing the new payload rules.
- Ensure malformed or non-normative metric messages are rejected before they are forwarded or interpreted as valid playback-metrics events.
- Keep the scope limited to the client consumer path; this proposal does not redesign the server-side SoC producer contract or the wider playback-metrics design.

Normative Contract

The authoritative payload for this change is the **enriched message** that the Rialto server sends to the GStreamer client via IPC:

**Core 0.7v fields (from SoC, passed through by server):**
- Required field: `metric` with type `G_TYPE_UINT`
- Required field: `observation-kind` with type `G_TYPE_UINT`
- Optional field: `pts-ns` with type `G_TYPE_INT64`
- Optional field: `duration-ns` with type `G_TYPE_UINT64`
- Optional field: `count` with type `G_TYPE_UINT64`

**Server-enriched fields (added by Rialto, required in client message):**
- Required field: `source_id` with type appropriate to Rialto's source tracking (Rialto-internal source identifier)
- Required field: `session_id` with type appropriate to Rialto's session tracking (Rialto-internal session identifier)

**NOT present in SoC raw message but extracted by server:** `source_id` and `session_id` are extracted by the Rialto server from GStreamer message source element (`GST_MESSAGE_SRC()`), NOT received as data fields in the SoC `media-pipeline-metric` payload. The server enriches the message before sending to client.

**Legacy field names rejected by this compatibility change:** `metric_type`, `phase`, `pts_ns` (legacy name with underscore). Do NOT accept these legacy field names at any stage.

**Accepted observation kinds:** `occurrence` (value 1), `episode-started` (value 2), and `episode-resolved` (value 3)

This is the contract the GStreamer client validator and unit tests must enforce. The client receives and accepts the enriched message structure from the Rialto server and must validate all fields according to this 0.7v contract.

### Field semantics by observation kind

**`pts-ns` (optional, G_TYPE_INT64):**
- **occurrence:** The affected frame or sample PTS when known; otherwise the best available stream position at detection time. Zero is a valid PTS.
- **episode-started:** The PTS at which the episode condition was first detected.
- **episode-resolved:** The episode-start PTS (repeated), allowing the resolution message to be self-describing.

**`duration-ns` (present in episode-resolved):**
- Monotonic elapsed time (in nanoseconds) from episode start to resolution.
- Uses GStreamer's monotonic clock, not media PTS or pipeline running time.
- Always non-negative.

**`count` (present in episode-resolved for VIDEO_FRAME_REPEAT and AUDIO_GAP only):**
- For VIDEO_FRAME_REPEAT: number of unintended repeated presentations of the same frame.
- For AUDIO_GAP: number of substitute audio frames.
- Absent for BUFFER_UNDERFLOW episodes and all OCCURRENCE messages.

### Metric type and observation kind enum values

For reference, the server-side Rialto producer emits metrics with these numeric values:

| Metric | Value | Observation kinds used |
|---|---|---|
| `MEDIA_METRIC_VIDEO_FRAME_DROP` | 1 | OCCURRENCE only |
| `MEDIA_METRIC_AUDIO_FRAME_DROP` | 2 | OCCURRENCE only |
| `MEDIA_METRIC_VIDEO_DECODE_ERROR` | 3 | OCCURRENCE only |
| `MEDIA_METRIC_AUDIO_DECODE_ERROR` | 4 | OCCURRENCE only |
| `MEDIA_METRIC_VIDEO_FRAME_REPEAT` | 5 | EPISODE_STARTED, EPISODE_RESOLVED (with `count`) |
| `MEDIA_METRIC_AUDIO_GAP` | 6 | EPISODE_STARTED, EPISODE_RESOLVED (with `count`) |
| `MEDIA_METRIC_BUFFER_UNDERFLOW` | 7 | EPISODE_STARTED, EPISODE_RESOLVED (no `count`) |

The GStreamer client must accept and validate these numeric values exactly as emitted by the server.

### Source element tracking and enrichment

**Server-side enrichment (Rialto producer):**
The Rialto server parser does NOT expect `source_id` or `session_id` as fields in the raw SoC metric message posted on the GStreamer bus. Instead, the server performs enrichment at the message-parsing stage:

- **Message source extraction:** Extract the authoritative producing element from `GST_MESSAGE_SRC(message)`, which identifies which SoC element (video decoder, audio decoder, output element, etc.) posted the metric.
- **Source-to-session mapping:** Maintain Rialto's internal session/source association to map the GStreamer element source to the corresponding Rialto source identifier and session context.
- **Enrichment with payload addition:** The normalized metric event forwarded downstream now includes `source_id` and `session_id` alongside the 0.7 contract fields. These enriched fields are added to the IPC message sent to the client.

**Client-side consumption (GStreamer client):**
- The GStreamer client receives an **enriched** metric event from the Rialto server via IPC (protobuf), which includes:
  - Core 0.7v fields: `metric`, `observation-kind`, optional `pts-ns`, `duration-ns`, `count`
  - Server-enriched fields: `source_id`, `session_id` (REQUIRED, always present)
- The client does NOT receive the raw GStreamer bus message; therefore it does not directly call `GST_MESSAGE_SRC()`.
- The client receives the metric with source/session context already populated and enriched by the server.
- The client validates that ALL received fields match the enriched contract: 0.7v fields with correct GTypes, PLUS required `source_id` and `session_id`.
- Legacy field names (only `metric_type`, `phase`, `pts_ns`) must be rejected if present; source_id/session_id are ACCEPTED as enriched fields.
- Source/session enrichment is complete by the time the client receives the message via IPC; the client's responsibility is to validate the complete enriched contract and relay all context to application callbacks.

### Data flow: Source ID and Session ID tracking at client

From the client's perspective, the complete data flow shows where source_id and session_id enrichment happens:

| Stage | Contains source_id/session_id? | What the client sees/does |
|---|---|---|
| **SoC Producer → Rialto Server (GStreamer bus)** | ❌ NO | Client not involved; raw `media-pipeline-metric` message contains only 0.7v fields (no source tracking) |
| **Rialto Parser (enrichment point)** | ✅ YES (extracted by Rialto) | Server enriches: extracts `GST_MESSAGE_SRC()`, maps to internal source/session context, adds `source_id`/`session_id` to metric |
| **Rialto Server → Client (via IPC proto)** | ✅ YES (REQUIRED) | Client receives enriched message including 0.7v fields PLUS `source_id` and `session_id` populated by server |
| **Client validation** | ✅ YES (must validate) | Client validates that `source_id` and `session_id` are present, populated, and valid. Client passes enriched context to application callbacks. |

**Key principle:** 
- SoC raw bus message: minimal, no source tracking fields
- Rialto enrichment boundary: server extracts source from GStreamer message metadata and adds source_id/session_id to the metric
- Client receives: fully enriched message with source_id/session_id as REQUIRED fields
- Client responsibility: validate enriched contract completeness and pass all context to application

Validation rules

- The consumer must validate the structure name before any metric interpretation is attempted.
- `metric` must be present and must be a `G_TYPE_UINT`.
- `observation-kind` must be present and must be a `G_TYPE_UINT`.
- `source_id` must be present and must be populated by Rialto server enrichment.
- `session_id` must be present and must be populated by Rialto server enrichment.
- `pts-ns` is accepted only as `G_TYPE_INT64` when present.
- `duration-ns` is accepted only as `G_TYPE_UINT64` when present.
- `count` is accepted only as `G_TYPE_UINT64` when present.
- Legacy field names (`metric_type`, `phase`, `pts_ns`) must not be treated as valid contract fields.
- Unknown enum values must be rejected rather than silently coerced into a fallback.
- Messages that are structurally valid but semantically inconsistent with the 0.7 contract must be rejected in the client validation layer, not accepted downstream.
- `duration-ns` when present uses monotonic clock elapsed time, not media PTS or pipeline running time.
- `count` must be present in episode-resolved for VIDEO_FRAME_REPEAT and AUDIO_GAP; must be absent for BUFFER_UNDERFLOW and all OCCURRENCE messages.

Non-Goals

- Will not redesign the playback-metrics feature beyond the client-side compatibility update.
- Will not change the server-side SoC producer contract or northbound API design.
- Will not broaden into unrelated playback logic or non-metric consumer behavior.
- Will not add subscription management, buffering, or analytics logic to the client path.
- Will not treat legacy names as acceptable contract unless explicitly retained only as a temporary compatibility translation.

Impacted Areas

This change is scoped to the GStreamer client validation and parsing path that must match the agreed 0.7v contract.

| Component | Impact Type | Location / Path | Details |
|-----------|------------|-----------------|---------|
| **Client-side validator** | Compatibility fix | `rialto-gstreamer/source/GStreamerMSEMediaPlayerClient.cpp` | Update `validateMediaPipelineMetricMessage()` to enforce the 0.7 field names, required fields, types, and numeric enum values (metric: 1-7, observation-kind: 1-3). Validate metric/observation-kind combinations per spec. Validate that enriched `source_id` and `session_id` are present and populated. Reject legacy field names (`metric_type`, `phase`, `pts_ns`). |
| **Client-side event model** | Compatibility fix | `rialto-gstreamer/source/GStreamerMSEMediaPlayerClient.h` | Keep callback/event handling aligned with the 0.7 observation-kind model and optional field semantics. Ensure enriched source/session context from the Rialto server (populated via GST_MESSAGE_SRC extraction at server-side) is properly carried through to client application callbacks. Event structure MUST include and validate `source_id` and `session_id` from server enrichment. |
| **Unit validation tests** | Regression coverage | `rialto-gstreamer/tests/ut/GstreamerMseMediaPlayerClientTests.cpp` | Add or adjust tests to validate all metric types (1-7) with their correct observation kinds, reject invalid enum values, enforce required vs. optional fields, validate GTypes, and reject legacy field names. Include tests for all valid metric/observation-kind combinations. CRITICAL: Validate that enriched `source_id` and `session_id` are REQUIRED in every enriched message and properly validated by client. Test rejection of messages missing source/session enrichment. |
| **OpenSpec task tracking** | Documentation alignment | `rialto-gstreamer/openspec/changes/playback-metrics-0.7v/tasks.md` | Align task wording to the exact client-side compatibility work including source_id/session_id enrichment validation |
| **Reference contract** | Source of truth | `rialto/openspec/changes/playback-metrics-0.7v/proposal.md` | Provides the authoritative 0.7 contract, server-side enrichment strategy (GST_MESSAGE_SRC extraction), and enriched message structure (including source_id/session_id) that the client must consume correctly |

Success Criteria

- The GStreamer client accepts only `media-pipeline-metric` messages enriched by Rialto server with source/session context.
- `metric` is validated as `G_TYPE_UINT` with numeric values 1-7 (as defined in spec).
- `observation-kind` is validated as `G_TYPE_UINT` with numeric values 1-3 (OCCURRENCE, EPISODE_STARTED, EPISODE_RESOLVED).
- Metric and observation-kind combinations are validated per spec (e.g., frame drop/decode error metrics use only OCCURRENCE; repeat/gap/underflow use EPISODE_STARTED and EPISODE_RESOLVED).
- `pts-ns`, `duration-ns`, and `count` are accepted only with the correct names and GTypes.
- `count` is validated as present (EPISODE_RESOLVED with VIDEO_FRAME_REPEAT or AUDIO_GAP) or absent (OCCURRENCE, EPISODE_STARTED, BUFFER_UNDERFLOW).
- `source_id` and `session_id` are REQUIRED fields, always present in enriched messages from Rialto server, and validated as populated.
- The client rejects legacy names such as `metric_type`, `phase`, and `pts_ns` in the normative validation path.
- Unknown enum values (invalid metric or observation-kind numbers) are rejected, not coerced into fallback values.
- The client-side validation remains consistent with the server-side implementation and SoC 0.7 contract without broadening the design scope.
- Malformed or non-conformant metric payloads are rejected before client-side event handling or dispatch.
- Source/session context enriched by the Rialto server is properly carried through in the client callbacks to the application.

## Test Coverage Requirements

**Unit tests — Message Posting and Dispatch:**
- ✅ `ShouldPostMetricEventAsApplicationMessageOnBus` — Verify client receives enriched metric and posts it to GStreamer bus with correct field names, types, and values
- ✅ `ShouldPostRepeatedMetricEventsToBus` — Verify multiple metric events from same source are posted sequentially with correct incremental field values
- ✅ `ShouldDispatchMetricToApplicationCallbacks` — Verify enriched metrics with source_id/session_id context reach application callback handlers

**Unit tests — Counter State and Monotonicity:**
- ✅ `ShouldRejectNonMonotonicCountersForTheSameSource` — Detect and reject counter regressions (rendered_frames, dropped_frames, corrupted_frames must never decrease for same source_id)
- ✅ `ShouldAcceptDropScenarioMetricMessages` — Accept valid monotonic counter sequences for frame drop scenarios
- ✅ `ShouldAcceptDecodeErrorScenarioMetricMessages` — Accept valid monotonic counter sequences for decode error scenarios

Rollback Plan

- Revert the validator and test updates if the normative 0.7 contract is changed or clarified.
- Restore the legacy validation only as a short-term compatibility bridge if the final contract is unresolved.
- Keep the rollback limited to the client-side compatibility fix and avoid reverting unrelated playback-metrics work.
