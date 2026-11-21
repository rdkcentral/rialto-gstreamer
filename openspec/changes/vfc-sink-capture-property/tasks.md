# Tasks

Unit tests run with `./build_ut.py` in rialto-gstreamer (`tests/ut`). "Spec" means `specs/sink-capture-property/spec.md`; "D#" means a decision in `design.md`.

## 1. Property type (D1)

- [x] 1.1 Add the `RialtoVideoFrameCaptureHandle` boxed type and the helper to create one from a `std::shared_ptr<IVideoFrameCapture>`; require the Rialto version with `IVideoFrameCapture.h` in CMake. Verify with a unit test that copying and freeing a GValue holding the box changes the `shared_ptr` use count correctly.

## 2. Sink property (D2, D3)

- [x] 2.1 Register `video-frame-capture` on the video sink and add `Property::VideoFrameCapture` to `IPlaybackDelegate`; store and read it in `PullModeVideoPlaybackDelegate`. Verify in `GstreamerMseVideoSinkTests`: set and get, and the reference is dropped on dispose.
- [x] 2.2 Reject setting the property after commit with a warning and no change. Verify with a test that commits and then sets the property.
- [ ] 2.3 Skip `setVideoRectangle` while a capture is set, keeping the stored rectangle readable. Verify with a test that sets the rectangle with and without a capture.

## 3. Attach at commit (D2)

- [ ] 3.1 Add `attachVideoFrameCapture()` to the media player client backend interface and implementation. Verify the backend mock compiles and forwards.
- [ ] 3.2 In `sendAllSourcesAttachedIfPossibleInternal()`, attach before `allSourcesAttached()`, post an error and skip commit when attach fails, and post an error when commit fails with a capture. Verify in `GstreamerMseMediaPlayerClientTests` with a mocked `IMediaPipeline`: order attach-then-commit; attach failure posts an error and sends no commit; commit failure posts an error; no capture means no attach call.

## 4. Integration check

- [ ] 4.1 Run the full rialto-gstreamer unit tests, then with valgrind, and confirm no regressions or leaks of the boxed handle.

## Workflow follow-up

- Run `openspec validate vfc-sink-capture-property --strict` in rialto-gstreamer before review.
- Merge after Rialto R4 and R6.
