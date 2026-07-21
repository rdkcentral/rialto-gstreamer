# Proposal: HFP Schema v1.0.0 — GStreamer Capability Registration Update

---

## Why

- The `rialto` repo updated `AudioDecoderCapabilities` and
  `VideoDecoderCapabilities` structs to support the new HFP schema v1.0.0.
  The `rialto-gstreamer` repo reads these structs in `GStreamerMSEUtils.cpp`
  to register GStreamer sink pad capabilities. This file must be updated to
  use the new struct field layout.
- `DOLBY_AC3` and `DOLBY_EAC3` are now separate codecs. The old code
  incorrectly registered both `audio/x-ac3` AND `audio/x-eac3` under a
  single `dolbyAc3` check. This caused `audio/x-eac3` to be registered
  on any device that listed `DOLBY_AC3` STANDARD, even if it did not
  support Enhanced AC-3.
- `DOLBY_MAT` and `WMA` have been removed from the schema. The old code
  still checked for `dolbyMat` and `wma` fields which no longer exist in
  the struct, causing compile errors.
- `VideoCodecCapabilities` changed from flat profile vectors
  (`mpeg2Profiles`, `h264Profiles`, etc.) to `std::optional<*CodecCapability>`
  fields (`mpeg2`, `h264`, etc.). The old field access code no longer
  compiles against the new struct.

---

## What Changes

- Update `GStreamerMSEUtils.cpp` — Audio section:

  - Split `dolbyAc3` check: now only registers `audio/x-ac3` (Dolby Digital)
  - Add `dolbyEac3` check: registers `audio/x-eac3` (Dolby Digital Plus / Atmos)
  - Remove `dolbyMat` check (codec removed from schema)
  - Remove `wma` check (codec removed from schema)
- Update `GStreamerMSEUtils.cpp` — Video section:

  - Change codec presence checks from flat vector empty check to
    `has_value()` pattern for all 5 codecs
    (MPEG2, H264, H265, VP9, AV1)
- Update `tests/ut/GStreamerMSEUtilsTests.cpp`:

  - Update test fixtures to use new `AudioDecoderCapability` struct
    (per-profile `AudioProfileCapability` maps, `.base` fields)
  - Update test fixtures to use new `VideoCodecCapabilities` struct
    (optional per-codec structs)
  - Add test cases for `dolbyEac3` GStreamer cap registration
  - Add test cases verifying `audio/x-wma` and `audio/x-raw` (from dolbyMat)
    are NOT registered

---

## Non-Goals

- Will not change the GStreamer audio or video playback pipeline
- Will not change how `RialtoGStreamerMSEAudioSink` or
  `RialtoGStreamerMSEVideoSink` call the capabilities API
- Will not add new GStreamer codec caps beyond what the schema defines
- Will not handle dynamic capabilities (HDMI output, display HDR detection)
- Will not change `GStreamerMSEUtils.h` — function signatures are unchanged

---

## Impacted Areas

| Area                                 | Impact Type         | Location / Path                                 |
| ------------------------------------ | ------------------- | ----------------------------------------------- |
| GStreamer caps registration — Audio | Logic change        | `source/GStreamerMSEUtils.cpp`                |
| GStreamer caps registration — Video | Field access update | `source/GStreamerMSEUtils.cpp`                |
| GStreamer caps header                | No change           | `source/GStreamerMSEUtils.h`                  |
| Audio sink class                     | No change           | `source/RialtoGStreamerMSEAudioSink.cpp`      |
| Video sink class                     | No change           | `source/RialtoGStreamerMSEVideoSink.cpp`      |
| Unit tests                           | Test fixture update | `tests/ut/GStreamerMSEUtilsTests.cpp`         |
| Mock                                 | No change           | `tests/mocks/MediaPipelineCapabilitiesMock.h` |

---

## Success Criteria

- `audio/x-ac3` is registered when `dolbyAc3` is present (STANDARD only)
- `audio/x-eac3` is registered when `dolbyEac3` is present (PLUS / PLUS_JOC)
- `audio/x-ac3` and `audio/x-eac3` are registered **independently** — a
  device with only AC3 does NOT get EAC3 registered
- `audio/x-wma` is NOT registered (WMA removed)
- `audio/x-raw` from `dolbyMat` is NOT registered (DOLBY_MAT removed)
- Video codec checks use `has_value()` guard before accessing `->profiles`
- `gst-inspect rialtomseaudiosink` shows: `audio/x-raw`, `audio/mpeg`,
  `audio/x-ac3`, `audio/x-eac3`, `audio/x-dts`, `audio/x-flac`,
  `audio/x-opus` (and NOT `audio/x-wma`)
- `gst-inspect rialtomsevideosink` shows: `video/mpeg`, `video/x-h264`,
  `video/x-h265`, `video/x-vp9`, `video/x-av1`
- All existing unit tests pass with no regressions
- New unit tests cover: dolbyEac3 cap registration, split AC3/EAC3,
  absence of WMA/dolbyMat caps, optional codec `has_value()` checks

---

## Rollback Plan

- **Code:** Revert the following file via `git revert`:

  - `source/GStreamerMSEUtils.cpp`
  - `tests/ut/GStreamerMSEUtilsTests.cpp`
- **Dependency:** This repo depends on `rialto` struct definitions.
  If rialto is rolled back, this file must also be rolled back to
  avoid struct mismatch compile errors.
- **Runtime safety:** If only `rialto` is rolled back but
  `rialto-gstreamer` is not, the build will fail at compile time —
  no silent runtime failure risk.

---

## References

- Jira: RDKEMW-15078
- Epic: CPESP-9957
- Depends on: rialto repo proposal `hfp-schema-v1-migration`
- HLA: W3C Media Capabilities HLA [VL to Rialto]
