## ADDED Requirements
### Requirement: Query and validate Rialto sink frame counters
The test CLI SHALL query the attached `rialtomsevideosink` `frame-counters` property while the pipeline and video source are alive. It SHALL validate a `frame-counters` structure containing `rendered`, `dropped`, `corrupted`, and `timestamp` fields, each with type `G_TYPE_UINT64`, and SHALL report the returned values without assuming fixed or non-zero counts.

#### Scenario: Counter snapshot is returned
- **WHEN** the sink property query succeeds and returns all required fields with the specified types
- **THEN** the CLI reports rendered, dropped, corrupted, and timestamp values and continues successfully

#### Scenario: Counter query or structure is invalid
- **WHEN** the property query fails, returns no structure, has the wrong structure name, or omits a required field or type
- **THEN** the CLI reports a counter-query failure and exits unsuccessfully

#### Scenario: Counts are zero or target-dependent
- **WHEN** the returned structure is valid but one or more counters are zero or differ across targets
- **THEN** the CLI reports the values without treating them as a failure solely because of their numeric values

#### Scenario: Counts are negative values
- **WHEN** the returned structure contains one or more counters with negative values
- **THEN** the CLI reports the invalid counter values and exits unsuccessfully