# Tasks

## 1. Update metric validation

- [ ] 1.1 Update the GStreamer client validator to accept the enriched 0.7 payload and verify required `metric`, `observation-kind`, `source_id`, and `session_id` fields are present and correctly typed.
- [ ] 1.2 Reject legacy names (`metric_type`, `phase`, `pts_ns`) and invalid enum values before dispatch, and verify the negative validation tests fail as expected.
- [ ] 1.3 Enforce metric-specific `count` and `duration-ns` rules for occurrence vs. episode metrics, and verify the scenario tests for valid and invalid combinations pass.

## 2. Preserve dispatch behavior

- [ ] 2.1 Keep the existing bus-posting and callback dispatch flow intact while ensuring the enriched source/session context is propagated unchanged. 
- [ ] 2.2 Confirm repeated metric events from the same source retain correct sequencing and source/session context when posted to the GStreamer bus. 

## 3. Regression coverage and validation

- [ ] 3.1 Add or update unit tests for valid metric combinations, invalid enum values, missing source/session enrichment, and legacy field rejection. Verify the targeted GStreamer client test suite passes.
- [ ] 3.2 Run the focused playback-metrics validation suite for 0.7 contract compliance and verify all targeted checks complete successfully.

## Workflow follow-up

- Review the change for contract alignment with the 0.7 specification before archive.
- Archive the change after review requirements are satisfied and verify the archived result.
