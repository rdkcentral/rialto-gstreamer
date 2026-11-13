# Tasks

## 1. Update metric validation

- [x] 1.1 Update the GStreamer client validator to accept the 0.7 raw metric payload and verify required `metric`, `observation-kind`, and `source_id` fields are present and correctly typed. `session_id` is not part of the SoC raw payload; it is added by Rialto from `GST_MESSAGE_SRC()` at the enrichment boundary and is validated only on the enriched IPC payload.
- [x] 1.2 Enforce metric-specific `count` and `duration-ns` rules for occurrence vs. episode metrics, and verify the scenario tests for valid and invalid combinations pass.

## 2. Preserve dispatch behavior

- [ ] 2.1 Keep the existing bus-posting and callback dispatch flow intact while ensuring the enriched source/session context is propagated unchanged. 
- [ ] 2.2 Confirm repeated metric events from the same source retain correct sequencing and source/session context when posted to the GStreamer bus. 

## 3. Regression coverage and validation

- [ ] 3.1 Add or update unit tests for valid metric combinations, invalid enum values, missing source/session enrichment, and legacy field rejection. Verify the targeted GStreamer client test suite passes.
- [ ] 3.2 Run the focused playback-metrics validation suite for 0.7 contract compliance and verify all targeted checks complete successfully.

## Workflow follow-up

- Review the change for contract alignment with the 0.7 specification before archive.
- Archive the change after review requirements are satisfied and verify the archived result.
