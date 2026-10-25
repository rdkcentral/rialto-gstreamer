## Context

See proposal.md for the cross-repository motivation. The `rialto` repo owns the server-side relay and RPC contract, while this repo is responsible for validating producer-side metric emission and implementing client-side consumption APIs. The design constraint is that production validation remains a test/guardrail concern rather than a source-code modification to SoC producers, while the client-side public API remains a thin adaptor over the existing IPC layer.

## Goals / Non-Goals

**Goals:**
- Validate that producer elements emit `media-pipeline-metric` messages with the expected structure and field semantics.
- Add a client-side API that can query the latest frame counters and receive push notifications from the server.
- Use the same `GetFrameCounters` and `NotifyMetricEvent` contract defined by the server-side relay.
- Keep the client implementation pass-through only, with no additional subscription or buffering behavior.

**Non-Goals:**
- Modifying the GStreamer SoC producer source code in this repo.
- Implementing per-client subscription logic or server-side filtering.
- Adding app-specific analytics or business logic to the client library.
- Creating a new metrics model beyond the phase-1 query/push contract.

## Decisions

### D1: Treat producer validation as a contract test, not a production code change

The gstreamer repo validates that `westeros-sink` and decoder elements emit correctly shaped metric messages. This keeps the upstream producer contract explicit and prevents the client layer from accidentally depending on undocumented behavior.

Alternatives considered:
- Patch producer code in-place: rejected because the proposal makes the validation layer separate from the production source path and avoids unnecessary repository drift.
- Skip validation and trust the producer output: rejected because the relay depends on a strict message shape and counter semantics.

### D2: Use a client-side callback plus a local cache for latest counters

The client library stores the last snapshot per source and exposes a callback registration method for push events. This gives the application simple access to on-demand queries and event-driven updates without introducing a separate state machine.

Alternatives considered:
- Callback-only without cache: rejected because queries still need a local, immediate value path for existing client usage patterns.
- Store counters only in the server and force every query through IPC: rejected because it makes the client slower and less ergonomic for repeated access.

### D3: Keep the IPC integration thin and aligned with the server contract

The media pipeline client in this repo is responsible for forwarding the server request and callback to the public client API, without adding extra logic beyond plumbing and validation. This maintains separation of concerns and limits behavioral drift across repos.

Alternatives considered:
- Add client-side filtering or aggregation: rejected because it would duplicate server semantics and produce inconsistent views across different consumers.
- Re-encode the protocol in a new client-side model: rejected because it would break the cross-repo contract the server exposes.

## Risks / Trade-offs

- [Risk] Producer messages may not always match the expected enum values or field semantics. Mitigation: add validation tests that fail loudly when the message format drifts.
- [Risk] Callback delivery and query access can race if the same source is updated concurrently. Mitigation: guard the cache with a mutex and keep callback dispatch isolated from the query path.
- [Risk] The client API may accidentally become application-specific if it carries business logic. Mitigation: keep the implementation a pass-through adaptor only.
- [Risk] New APIs might be skipped on some builds if the CMake wiring is incomplete. Mitigation: add build-target registration at the same time as the implementation.

## Migration Plan

1. Add producer validation unit tests for message structure, enum values, and counter monotonicity.
2. Add producer component tests that exercise real metric emission paths in a GStreamer pipeline.
3. Implement the client-side `getFrameCounters` call and callback registration API.
4. Add the client-side cache and event forwarding logic.
5. Wire the media pipeline client to the server RPC callbacks.
6. Add integration tests verifying callback flow and query behavior using mock or injected server events.
7. Validate the full metrics path from producer validation to client callback invocation.

Rollback: keep the validation tests and API additions behind the same feature flag or build target pattern used for the rest of the repo, then remove the new client callbacks and CMake registrations if the server-side contract is unavailable.

## Open Questions

None. The design uses the server contract and the producer validation requirements already defined in the proposal without introducing new product assumptions.
