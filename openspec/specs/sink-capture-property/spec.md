# sink-capture-property Specification

## Purpose

Defines the rialto-gstreamer video sink property through which a GStreamer application supplies a Rialto Video Frame Capture, and how the sink attaches it to the Rialto pipeline before commit.

## Requirements

### Requirement: Capture property
The Rialto MSE video sink SHALL provide a writable `video-frame-capture` property whose value is a boxed type holding a shared reference to an `IVideoFrameCapture`. The sink SHALL keep its own reference for as long as the property is set and SHALL never destroy the capture itself.

#### Scenario: Property set
- **WHEN** an application sets `video-frame-capture` to a boxed capture
- **THEN** the sink holds a reference to that capture

#### Scenario: Sink disposed
- **WHEN** the sink is disposed
- **THEN** it drops its reference, and the capture is destroyed only if no other reference remains

### Requirement: Property accepted only before commit
The sink SHALL accept the property, including clearing it with null, only before its pipeline has sent `allSourcesAttached()`. After that, setting the property SHALL be rejected with a GStreamer warning and SHALL NOT change the value or call Rialto.

#### Scenario: Set after commit
- **WHEN** the property is set after `allSourcesAttached()` was sent
- **THEN** a warning is logged, the stored value is unchanged, and no attach is called

#### Scenario: Cleared before commit
- **WHEN** the property is set and then set to null before commit
- **THEN** no capture is attached at commit

### Requirement: Attach before commit
When a capture is set, the sink SHALL call `attachVideoFrameCapture()` with it before sending `allSourcesAttached()`. If attaching fails, the sink SHALL post a GStreamer error and SHALL NOT send `allSourcesAttached()`. If `allSourcesAttached()` fails while a capture is attached, the sink SHALL post a GStreamer error.

#### Scenario: Normal commit with a capture
- **WHEN** a capture is set and all sources are attached
- **THEN** `attachVideoFrameCapture()` is called, then `allSourcesAttached()`

#### Scenario: Attach rejected
- **WHEN** `attachVideoFrameCapture()` returns `false`
- **THEN** an error is posted on the sink and `allSourcesAttached()` is not sent

#### Scenario: Commit fails with a capture
- **WHEN** `allSourcesAttached()` returns `false` with a capture attached
- **THEN** an error is posted on the sink

#### Scenario: No capture set
- **WHEN** no capture is set
- **THEN** no attach is called and commit behaves exactly as before

### Requirement: No video window while capturing
While a capture is set, the sink SHALL NOT call `setVideoWindow()`. The rectangle property SHALL still be stored and readable.

#### Scenario: Rectangle set during capture
- **WHEN** the rectangle property is set on a sink that has a capture
- **THEN** the value is stored and readable, and `setVideoWindow()` is not called
