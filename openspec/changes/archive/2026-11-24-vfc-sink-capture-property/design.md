# Design

## Context

See proposal.md for the motivation. Video sink properties are registered in `RialtoGStreamerMSEVideoSink.cpp` and handled by `PullModeVideoPlaybackDelegate::setProperty/getProperty` through the `IPlaybackDelegate::Property` enum. `GStreamerMSEMediaPlayerClient::sendAllSourcesAttachedIfPossibleInternal()` sends `allSourcesAttached()` once all streams are attached, on the backend event loop, and sets `m_wasAllSourcesAttachedSent`. The rectangle property reaches Rialto as `setVideoRectangle` → `setVideoWindow`. Rialto R6 rejects `setVideoWindow()` while a capture is attached.

## Goals / Non-Goals

**Goals:**
- A property type that safely carries a `shared_ptr` through GObject (Q9).
- Attach on the same event loop as commit, so ordering is guaranteed.

**Non-Goals:**
- Acquiring frames: the application uses its own capture object directly.
- Runtime switching between capture and video plane (not in V1.1).

## Decisions

### D1. Boxed GType owning a `shared_ptr` copy
Register `RialtoVideoFrameCaptureHandle` with `g_boxed_type_register_static`. The boxed struct holds `std::shared_ptr<IVideoFrameCapture>`; its copy function copies the `shared_ptr`, and its free function deletes the struct. A small public header gives applications `rialto_video_frame_capture_handle_new(std::shared_ptr<...>)`.

*Why:* a raw pointer cannot keep the capture alive while the pipeline holds it (proposal §7); a boxed type gives GValue correct copy and free semantics.
*Alternative:* `G_TYPE_POINTER` to a heap `shared_ptr`. Rejected: no ownership semantics, leaks or double frees on GValue copies.

### D2. Store in the delegate, attach in the media player client
`PullModeVideoPlaybackDelegate` stores the handle and forwards it to `GStreamerMSEMediaPlayerClient::setVideoFrameCapture()`, which queues it on the backend event loop. In `sendAllSourcesAttachedIfPossibleInternal()`, if a capture is set, call `m_clientBackend->attachVideoFrameCapture()` first; on `false`, post an error and return without sending `allSourcesAttached()`. Check the return of `allSourcesAttached()` when a capture is attached.

*Why:* the event loop already serializes commit; doing attach there guarantees "attach before commit" and the "after commit" check uses the same `m_wasAllSourcesAttachedSent` flag.

### D3. Rectangle suppression
`PullModeVideoPlaybackDelegate` skips `client->setVideoRectangle()` while a capture is set, keeping `m_videoRectangle` for reads.

## Risks / Trade-offs

- [Application expects a silent fallback to the video plane] → V1.1 has no runtime switching; an explicit error is safer than unexpected plane output.
- [Rialto headers without capture support] → rialto-gstreamer requires the Rialto version that includes R4; enforce in CMake with a version check.

## Migration Plan

None. Pipelines that never set the property behave exactly as before.
