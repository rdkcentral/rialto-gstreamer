## Context
`rialtomsevideosink` already exposes a read-only `frame-counters` pointer property. Its getter queries the Rialto client and returns a `frame-counters` `GstStructure` containing `rendered`, `dropped`, `corrupted`, and `timestamp` as `G_TYPE_UINT64`. The GStreamer client also receives metric callbacks from Rialto and posts a local `GST_MESSAGE_APPLICATION` named `media-pipeline-metric` on the sink's bus.

The Westeros `media-pipeline-metric` element message is posted on the Rialto server's GStreamer bus. The proposed test CLI does not have direct access to that server bus; it validates the corresponding application message reconstructed by the `rialto-gstreamer` client after IPC delivery. A real server/client and target pipeline are therefore required for end-to-end verification.

The repository has unit tests for the property and app-bus bridge, but no standalone test CLI. The requested `rialto-gstreamer/project.md` is absent; this proposal follows the repository README and existing OpenSpec conventions, while placing files under the explicitly requested `docs/changes` directory.

## Goals / Non-Goals
- Goals: exercise a real `playbin` with an explicit `rialtomsevideosink`; query and validate frame-counter structure; observe and validate client-visible metric messages for fault-injection runs.
- Non-goals: fabricate metric events; inspect the server's private GStreamer bus from the CLI; assert exact counter values; change the public Rialto API; decide the numeric metric mapping before its source contract is clarified.

## Decisions
- Decision: Create the pipeline with `playbin`, set its URI, and assign a `rialtomsevideosink` to `video-sink`. Treat missing plugin, pipeline error, query failure, and timed-out required metric as failed CLI runs with clear log messages on what caused the failure.
- Decision: Query `frame-counters` while the pipeline and video source remain alive. Validate the structure name and the `rendered`, `dropped`, `corrupted`, and `timestamp` fields as `G_TYPE_UINT64`; print values but do not assert fixed or non-zero totals.
- Decision: Watch the playbin bus for `GST_MESSAGE_APPLICATION` and inspect only structures named `media-pipeline-metric`. Validate required field presence/types and optional field types; do not require `session_id` or server-only observation timestamps in this client-side message.
- Decision: Treat the `metric` field as an opaque `G_TYPE_UINT` for this change. Do not assert its numeric category or match a selected fault to a category until the callback mapping is resolved.
- Decision: Make metric presence mandatory only when a fault-injection scenario is selected. In that mode, require at least one valid metric application message before timeout; in normal playback, absence of such a message is not a failure.
- Decision: Omit `count` and `duration-ns` from occurrence application messages. This aligns the bridge with the occurrence field contract; zero is not a substitute for an absent optional field.
- Decision: Keep fault injection opt-in and GStreamer-level. The CLI must not directly invoke `notifyMetric` or synthesize a `media-pipeline-metric` message as a successful test result.

## Risks / Trade-offs
- Fault injection behavior and metric production are platform-dependent; → make target runs explicit and fail only when an opted-in run does not observe a valid application message.
- The bridge currently infers a metric number from counters and error text and emits zero-based literals, while the Rialto metric contract is 1-based; → do not validate numeric metric categories until the upstream callback preserves the authoritative type and the mapping is agreed.
- Removing zero-valued `count`/`duration-ns` from occurrence messages changes local bus structure shape; → treat absent fields according to the existing contract and add regression coverage.
- The property may be unavailable before an attached video source has initialized; → query during playback while the pipeline and source remain alive, and report query failure distinctly.

## Migration Plan
This is an additive test executable plus a contract-correctness fix to the client bus bridge. No public API or migration is required. Build and run the CLI only in an environment with the plugin, active Rialto server/client, compatible media, and the selected fault mechanism.

## Open Questions
- Which GStreamer-level mechanisms will induce each fault scenario, and which scenario names/options should the CLI expose? The user will provide the concrete mechanisms.
- What is the authoritative numeric mapping for `metric` on the client application bus, and how should Rialto's `MetricType` be preserved instead of inferred from counters/error text?
- Which fault scenarios are expected to produce a metric on each supported target, and what timeout is appropriate for those runs?