# Tasks

## 1. Producer validation

- [x] 1.1 Add producer validation tests for `media-pipeline-metric` message structure and required field presence, and verify the test suite catches malformed GStreamer payloads.
- [x] 1.2 Add counter monotonicity and enum validation checks, and verify invalid metric_type or decreasing counters fail the tests as expected.
- [x] 1.3 Add producer component tests for drop and decode-error scenarios, and verify real pipeline metric emission matches the expected contract.

## 2. Client-side consumable API

- [x] 2.1 Implement `getFrameCounters` and callback registration in the client-side sink API, and verify the API compiles and exposes the expected query/notify shape.
- [x] 2.2 Add the per-source cache and callback dispatch logic, and verify the latest counters and event delivery stay consistent across repeated calls.
- [x] 2.3 Wire the client IPC layer to the server callback path, and verify push notifications reach the application callback without additional filtering.

## 3. Integration and regression checks

- [ ] 3.1 Add consumer unit tests for mock server RPC calls and callback behavior, and verify query and push flows pass under concurrent access patterns.
- [ ] 3.2 Add consumer integration tests for injected metric events and counter queries, and verify the full path from server push to client callback remains stable.
- [ ] 3.3 Run the targeted build and test commands for the modified gstreamer components, and verify no regressions appear in the existing media pipeline test set.
