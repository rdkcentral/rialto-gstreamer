# Spec Delta

## Purpose

This capability ensures the GStreamer client accepts only the Rialto-enriched 0.7 SoC metric payload and rejects legacy or malformed payloads before they are dispatched to application callbacks.

## ADDED Requirements

### Requirement: Client accepts only the enriched 0.7 metric envelope
The GStreamer client SHALL validate the metric structure name, required fields, and field types before handling a metric event. It MUST accept the 0.7 names `metric`, `observation-kind`, optional `pts-ns`, `duration-ns`, and `count`, plus required server-enriched `source_id` and `session_id` values.

#### Scenario: Valid 0.7 enriched message is accepted
- **WHEN** the client receives a `media-pipeline-metric` message containing `metric` and `observation-kind` as `G_TYPE_UINT`, populated `source_id` and `session_id`, and only valid optional fields
- **THEN** the client accepts the message and continues through the normal dispatch path

#### Scenario: Message missing required server context is rejected
- **WHEN** the client receives a metric message without `source_id` or `session_id`
- **THEN** the client rejects the message before callback dispatch or bus posting

### Requirement: Legacy metric fields are rejected
The GStreamer client SHALL reject legacy field names and semantics such as `metric_type`, `phase`, and `pts_ns` instead of silently translating or accepting them as compatible input.

#### Scenario: Legacy field names are ignored as invalid
- **WHEN** a message contains `metric_type`, `phase`, or `pts_ns`
- **THEN** the client treats it as invalid and does not process it as a 0.7 metric event

#### Scenario: Unknown or invalid enum values are rejected
- **WHEN** a message contains a metric or observation-kind value outside the allowed 0.7 range
- **THEN** the client rejects the message instead of coercing it to a default or fallback value

### Requirement: Metric semantics and count rules are enforced
The GStreamer client SHALL validate metric-to-observation-kind combinations and the optional field rules defined by the 0.7 contract, including the required presence or absence of `count` and `duration-ns` for episode-based metrics.

#### Scenario: Occurrence metrics are validated as occurrence-only events
- **WHEN** a metric such as frame drop or decode error is sent with `observation-kind` set to occurrence
- **THEN** the client accepts it only when it matches the valid occurrence-only rules for that metric

#### Scenario: Episode metrics require correct count and duration semantics
- **WHEN** the client receives a `VIDEO_FRAME_REPEAT`, `AUDIO_GAP`, or `BUFFER_UNDERFLOW` metric with episode observation kinds
- **THEN** the client validates the `count` and `duration-ns` semantics according to the 0.7 rules and rejects non-conforming messages
