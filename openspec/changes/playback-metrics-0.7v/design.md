# Design

## Context

The GStreamer client continues to validate and consume a legacy metric payload that does not match the agreed 0.7 contract. See proposal.md - Why and Normative Contract.

## Goals / Non-Goals

**Goals:**
- Validate the enriched 0.7 metric envelope before dispatch
- Enforce required `source_id` and `session_id` context from the Rialto server
- Reject legacy field names and non-conformant enum values before callback processing
- Preserve the existing client callback and bus-posting flow while tightening validation

**Non-Goals:**
- Redesign the broader playback-metrics feature
- Change the server-side SoC producer contract
- Add compatibility fallbacks for legacy payload names once the 0.7 contract is in force

## Decisions

- Validate the full enriched message at the client boundary before parsing metrics or invoking callbacks. This keeps the fix narrow and ensures malformed or legacy payloads fail early.
- Treat `source_id` and `session_id` as required enriched context from the Rialto server. This preserves the source/session traceability required for downstream reporting and callback dispatch.
- Keep the validation logic aligned to exact GTypes and numeric enum ranges from the 0.7 spec rather than accepting broad coercions. This reduces silent incompatibility and matches the contract from the server producer.
- Enforce metric-specific rules for `count` and `duration-ns` at validation time. This catches semantic mismatches before they reach application logic or the bus.

## Risks / Trade-offs

- [Strict validation may reject older or partially populated payloads earlier than before] → Mitigation: keep the compatibility boundary limited to the agreed 0.7 contract and reject malformed events before dispatch.
- [Server-side enrichment may occasionally be missing or incomplete] → Mitigation: require populated `source_id` and `session_id` and fail closed rather than dispatching ambiguous data.
- [Narrow validation can surface edge-case incompatibilities during rollout] → Mitigation: use targeted unit coverage for valid/invalid enum combinations and required field handling to catch integration regressions early.

## Migration Plan

1. Update the client validator to require the 0.7 field names and GTypes.
2. Add explicit checks for required source/session enrichment and prohibited legacy names.
3. Run targeted validation and dispatch unit tests covering occurrence and episode metric messages.
4. Revert only the compatibility fix if the contract changes or is clarified, while preserving unrelated playback-metrics work.
