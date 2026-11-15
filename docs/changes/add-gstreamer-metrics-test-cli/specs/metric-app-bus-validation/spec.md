## ADDED Requirements
### Requirement: Validate client-visible metric application messages
The test CLI SHALL monitor `GST_MESSAGE_APPLICATION` messages on the playbin bus and validate structures named `media-pipeline-metric`. Validation SHALL check the required `metric`, `observation-kind`, and `source_id` fields and their `G_TYPE_UINT` types, valid observation-kind values, optional field GTypes (`pts-ns` as `G_TYPE_INT64`; `duration-ns` and `count` as `G_TYPE_UINT64`), and the rule that occurrence messages omit `duration-ns` and `count`. A `session_id` field, if present, SHALL be `G_TYPE_UINT`. The CLI SHALL treat `metric` as an opaque unsigned value and SHALL NOT validate its numeric category until the mapping is resolved.

#### Scenario: Valid client application message is observed
- **WHEN** the playbin bus receives a `GST_MESSAGE_APPLICATION` with a `media-pipeline-metric` structure satisfying the defined field rules
- **THEN** the CLI reports the message and marks a required fault-injection metric as observed

#### Scenario: Malformed client application message is observed
- **WHEN** a matching application message has an invalid required field, optional field type, or occurrence field combination
- **THEN** the CLI reports the invalid message and does not count it as a valid metric observation

#### Scenario: Fault-injection run receives no valid metric message
- **WHEN** a fault-injection run reaches its metric timeout without a valid `media-pipeline-metric` application message
- **THEN** the CLI reports the missing metric message and exits unsuccessfully

#### Scenario: Normal run receives no metric message
- **WHEN** no fault injection was requested and playback completes without a metric application message
- **THEN** the CLI completes without failing solely because no metric message was emitted

#### Scenario: Metric category mapping is unresolved
- **WHEN** a structurally valid message contains a `metric` unsigned value whose category mapping has not been agreed
- **THEN** the CLI reports the raw value but does not pass or fail the message based on its numeric category