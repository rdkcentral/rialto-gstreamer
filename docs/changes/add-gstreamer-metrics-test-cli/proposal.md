## Why
A small real-pipeline test application is needed to exercise `rialtomsevideosink` through GStreamer's normal `playbin` and bus paths, rather than relying only on unit tests with mocked backends. It will query the sink's `frame-counters` property and validate client-visible metric application messages when a fault is injected.

## What Changes
- Add a standalone GStreamer test CLI that creates a `playbin`, assigns a `rialtomsevideosink`, plays a supplied URI, and monitors the pipeline bus.
- Query and validate the sink's `frame-counters` structure while the video source is attached, and report the returned values without assuming target-specific counter totals.
- For an opted-in fault-injection run, validate the local `GST_MESSAGE_APPLICATION` with structure name `media-pipeline-metric`; fail if no valid message arrives before the configured timeout.
- Correct application-bus serialization so occurrence messages omit absent `count` and `duration-ns` fields. Do not validate the numeric `metric` category until its mapping is resolved.
- Keep fault generation on GStreamer-level mechanisms. The per-fault mechanisms and metric-number mapping remain open design questions.

No public Rialto API changes are proposed. Correcting occurrence messages changes the local bus payload by removing `count` and `duration-ns` fields that were previously emitted as zero; consumers relying on those non-contractual zero fields may need adjustment.

## Impact
- Affected capabilities: `playbin-test-cli`, `frame-counter-query`, `metric-app-bus-validation`.
- Affected code: new CLI/test target under `tests/`; GStreamer bus and property handling; `GStreamerMSEMediaPlayerClient::postMetricEventOnBus()` and its unit tests.
- Runtime dependencies: built Rialto GStreamer plugin, Rialto client/server, active server-manager application, compatible media, and a target supporting the selected fault stimulus.