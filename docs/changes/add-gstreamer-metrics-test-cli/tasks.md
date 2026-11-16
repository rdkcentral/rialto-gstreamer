## 1. Test CLI
- [x] 1.1 Add a standalone test CLI target with URI, timeout, and optional fault-scenario arguments; document startup, parse errors, and deterministic exit codes.
- [x] 1.2 Construct a GStreamer `playbin`, set the supplied URI, explicitly assign a `rialtomsevideosink` as `video-sink`, and handle missing plugin, pipeline errors, EOS, and cleanup.

## 2. Frame Counter Query
- [x] 2.1 Query the sink's `frame-counters` property while its video source is attached and the pipeline is alive; validate the structure name and required `G_TYPE_UINT64` fields.
- [x] 2.2 Print rendered, dropped, corrupted, and timestamp values; fail on query/structure errors without asserting fixed or non-zero target-dependent counts.
- [ ] 2.3 Add focused tests for successful property parsing and query/field failures using the existing GStreamer test conventions.

## 3. Metric Bus Validation
- [ ] 3.1 Monitor `GST_MESSAGE_APPLICATION` messages and select the `media-pipeline-metric` structure; validate its agreed envelope and print observed fields.
- [ ] 3.2 Update the bus bridge to omit absent `count` and `duration-ns` fields for occurrence messages; add regression tests for occurrence and episode message serialization.
- [ ] 3.3 In fault-injection mode, require at least one valid metric application message before timeout and fail if it is absent; without injection, report observed messages but do not require one.
- [ ] 3.4 Leave numeric metric-category validation disabled until the producer-to-client mapping and fault-to-metric mapping are agreed; capture that decision in the design/spec before enabling assertions.

## 4. Target Verification
- [ ] 4.1 Build the test CLI and plugin in the supported target environment, and verify a normal URI plays and the frame-counter structure is readable.
- [ ] 4.2 Run each agreed GStreamer-level fault stimulus against a real Rialto client/server; verify a valid `media-pipeline-metric` application message arrives before timeout and record target-specific prerequisites.
- [ ] 4.3 Run the focused GStreamer unit tests and the relevant Rialto client/server metric suites; record which checks require hardware or a platform Westeros sink.