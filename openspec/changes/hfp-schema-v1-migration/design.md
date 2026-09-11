## Context

The `rialto-gstreamer` plugin registers GStreamer sink pad capabilities by calling overloaded
versions of `rialto_mse_sink_setup_supported_caps()` in `GStreamerMSEUtils.cpp`. Two of those
overloads take the rialto capability structs directly:
- `rialto_mse_sink_setup_supported_caps(GstElementClass*, const AudioDecoderCapabilities&)`
- `rialto_mse_sink_setup_supported_caps(GstElementClass*, const VideoDecoderCapabilities&)`

The `rialto` repo migrated both structs to HFP schema v1.0.0:
- `AudioDecoderCapabilities`: removed `dolbyMat` and `wma` fields; added `dolbyEac3`; codec
  capability structs switched from flat fields to `std::map<Profile, AudioProfileCapability>` or
  `AudioProfileCapability base`
- `VideoDecoderCapabilities`: `VideoCodecCapabilities` changed from flat profile vectors
  (`mpeg2Profiles`, `h264Profiles`, …) to `std::optional<*CodecCapability>` fields (`mpeg2`,
  `h264`, …)

The `GStreamerMSEUtils.cpp` implementation and its unit tests still reference the old field layout
and will not compile against the new rialto headers without this change.

## Goals / Non-Goals

**Goals:**
- Fix compile errors caused by removed fields (`dolbyMat`, `wma`, flat profile vectors)
- Correctly split AC3 (`audio/x-ac3`) and EAC3 (`audio/x-eac3`) into independent registration
  paths driven by separate `dolbyAc3` / `dolbyEac3` capability fields
- Update video codec presence checks to use `has_value()` on `std::optional` fields
- Update unit test fixtures to construct the new struct shapes

**Non-Goals:**
- Changing audio/video playback pipeline logic
- Adding new GStreamer codec caps beyond the schema
- Handling dynamic capabilities (HDMI, display HDR detection)

## Decisions

### D1: Change typed overload return type to `bool` and update sink call sites with a legacy fallback

**Decision**: The typed overloads in `GStreamerMSEUtils.h` change their return type from `void`
to `bool`. They return `false` when the supplied capabilities container is empty (no HFP YAML
loaded) and `true` after successfully registering caps. Both
`RialtoGStreamerMSEAudioSink.cpp` and `RialtoGStreamerMSEVideoSink.cpp` are updated to:
1. Call `getSupportedAudioCapabilities()` / `getSupportedVideoCapabilities()` first and pass
   the result to the typed overload.
2. If the typed overload returns `false` (empty capabilities — no HFP config present), fall back
   to the existing `getSupportedMimeTypes()` + legacy `std::vector<std::string>` overload.

**Rationale**: The `bool` return allows callers to detect the "no HFP config" case without
inspecting the struct internals. The fallback preserves backward compatibility on RDK-V and any
other platform where the HFP YAML files are absent, while enabling accurate per-codec cap
registration on HFP-capable platforms. Keeping the legacy path as a fallback avoids any startup
regression when the YAML is not deployed.

**Alternatives considered**:
- *Keep `void` return; check `capabilities.empty()` at the call site*: Leaks struct internals
  into every caller; the helper itself is the correct place to signal "nothing registered".
- *Remove the legacy fallback*: Would break all non-HFP platforms.

### D2: Split dolbyAc3 / dolbyEac3 into independent registration paths

**Decision**: The existing `if (audioCapability.dolbyAc3)` block that pushed both `audio/x-ac3`
AND `audio/x-eac3` must be split into:
- `if (audioCapability.dolbyAc3)` → push `audio/x-ac3` only
- `if (audioCapability.dolbyEac3)` → push `audio/x-eac3` only

**Rationale**: The old combined check incorrectly registered EAC3 on any device that supported
standard AC3, even without `DOLBY_EAC3` capability. The new schema separates them as distinct
codecs; the registration logic must reflect this.

**Alternatives considered**:
- *Keep combined check, add dolbyEac3 as alias*: Would still register EAC3 on AC3-only devices.

### D3: Use `has_value()` guard for video codec optional fields

**Decision**: Replace `!videoCapability.codecCapabilities.mpeg2Profiles.empty()` (and similar for
h264, h265, vp9, av1) with `videoCapability.codecCapabilities.mpeg2.has_value()`.

**Rationale**: The new `VideoCodecCapabilities` fields are `std::optional<*CodecCapability>`.
`has_value()` correctly expresses "codec is present in the HFP config". Checking `.empty()` on a
profiles vector is no longer possible since the field type changed.

## Risks / Trade-offs

- **Compile-only change**: No runtime behaviour change on any device that was previously working
  because the old combined `dolbyAc3` check only registered `audio/x-eac3` when a device also had
  `dolbyAc3`. On any device with both codecs the behaviour is the same. On a device with AC3 but
  no EAC3, the new code correctly withholds `audio/x-eac3` registration.
- **Test fixture breakage**: Unit tests construct `AudioDecoderCapability` and
  `VideoCodecCapabilities` with the old struct syntax; they will not compile until updated.

## Migration Plan

1. Update `GStreamerMSEUtils.h`:
   - Change return type of both typed overloads from `void` to `bool`
2. Update `GStreamerMSEUtils.cpp` audio section:
   - Return `false` when `audioCapabilities.capabilities.empty()`
   - Split `dolbyAc3` → `audio/x-ac3` only; add `dolbyEac3` → `audio/x-eac3`
   - Remove `dolbyMat` and `wma` blocks
3. Update `GStreamerMSEUtils.cpp` video section:
   - Return `false` when `videoCapabilities.capabilities.empty()`
   - Replace `.mpeg2Profiles.empty()` checks with `.mpeg2.has_value()` (and h264/h265/vp9/av1)
4. Update `RialtoGStreamerMSEAudioSink.cpp`:
   - Replace direct `getSupportedMimeTypes()` call with `getSupportedAudioCapabilities()` +
     typed overload; fall back to legacy mime-type path if typed overload returns `false`
5. Update `RialtoGStreamerMSEVideoSink.cpp`:
   - Same pattern: `getSupportedVideoCapabilities()` + typed overload with fallback
6. Update `GStreamerMSEUtilsTests.cpp`:
   - Reconstruct `AudioDecoderCapability` without `DolbyMatCapability`/`WmaCapability`; use new
     struct constructors
   - Reconstruct `VideoCodecCapabilities` using `std::optional<*CodecCapability>` fields
   - Add `dolbyEac3` test case; add negative tests for `wma`/`dolbyMat` caps

**Rollback**: `git revert source/GStreamerMSEUtils.h source/GStreamerMSEUtils.cpp source/RialtoGStreamerMSEAudioSink.cpp source/RialtoGStreamerMSEVideoSink.cpp tests/ut/GStreamerMSEUtilsTests.cpp`.
If rialto is rolled back, these files must also be reverted to avoid struct mismatch compile errors.
