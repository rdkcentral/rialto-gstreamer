## ADDED Requirements
### Requirement: Standalone playbin test CLI
The GStreamer test CLI SHALL create a `playbin` pipeline for a caller-supplied media URI and SHALL explicitly set a `rialtomsevideosink` as its video sink. It SHALL monitor pipeline errors and completion, clean up the pipeline, and return a failing exit status when setup or playback fails.

#### Scenario: Play media through the Rialto video sink
- **WHEN** the CLI receives a valid URI and the Rialto sink plugin is available
- **THEN** it creates `playbin`, assigns `rialtomsevideosink` to `video-sink`, and starts playback of the URI

#### Scenario: Required plugin or media is unavailable
- **WHEN** the sink cannot be created, the URI cannot be loaded, or the pipeline posts an error
- **THEN** the CLI reports the failure, tears down the pipeline, and exits unsuccessfully

#### Scenario: Fault injection is requested
- **WHEN** the CLI is run with an opted-in fault scenario
- **THEN** it invokes the configured GStreamer-level stimulus and enables the required metric-message check; it SHALL NOT fabricate a metric message or directly call a metric callback