## Why

GStreamer applications use Rialto through the rialto-gstreamer video sink, not through `IMediaPipeline` directly, so they have no way to request Video Frame Capture. Proposal §7 (V1.1-RG-3) and Q9 define a `video-frame-capture` property on the video sink that carries the application's capture and attaches it before the pipeline commits. Rialto (R4, R6) now provides `attachVideoFrameCapture()`; this change exposes it in rialto-gstreamer.

## What Changes

- Add a `video-frame-capture` property to the Rialto MSE video sink. Its value is a boxed GType that owns a copy of the application's `std::shared_ptr<IVideoFrameCapture>`.
- The property is accepted only before the sink's pipeline commits. Setting it later is rejected with a GStreamer warning and does not change the value. Setting it to null before commit clears it.
- When a capture is set, the sink calls `attachVideoFrameCapture()` before sending `allSourcesAttached()`. If attaching fails, or `allSourcesAttached()` fails while a capture is attached, the sink posts a GStreamer error instead of silently falling back to the video plane.
- While a capture is set, the sink does not call `setVideoWindow()`; the rectangle property is stored but not sent, because Rialto rejects it during capture.
- The sink never destroys the capture; it drops its reference on dispose, and Rialto detaches the capture when the pipeline is destroyed.

## Capabilities

### New Capabilities

- `sink-capture-property`: How the rialto-gstreamer video sink accepts a Video Frame Capture from the application and attaches it to the Rialto pipeline before commit.

### Modified Capabilities

None.

## Impact

- **Code:** `source/RialtoGStreamerMSEVideoSink.cpp` (property registration), `source/IPlaybackDelegate.h` and `source/PullModeVideoPlaybackDelegate.cpp` (new `VideoFrameCapture` property, rectangle suppression), `source/GStreamerMSEMediaPlayerClient.cpp` (attach before `allSourcesAttached()`), `source/MediaPlayerClientBackendInterface.h`/`MediaPlayerClientBackend.h` (forward attach), and a small boxed-type helper.
- **Tests:** unit tests in `tests/ut` with the mocked `IMediaPipeline`.
- **Dependencies:** Rialto R4 (header) and R6 (attach behaviour). Builds against Rialto headers that include `IVideoFrameCapture.h`.
