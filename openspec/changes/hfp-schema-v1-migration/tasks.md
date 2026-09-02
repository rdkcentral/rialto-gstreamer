## 1. GStreamerMSEUtils.cpp — Audio section

- [x] 1.1 Split `dolbyAc3` block: change to push `audio/x-ac3` only (remove `audio/x-eac3` from this block)
- [x] 1.2 Add `dolbyEac3` block: `if (audioCapability.dolbyEac3)` pushing `audio/x-eac3`
- [x] 1.3 Remove `dolbyMat` block (`if (audioCapability.dolbyMat)` that pushes `audio/x-raw`)
- [x] 1.4 Remove `wma` block (`if (audioCapability.wma)` that pushes `audio/x-wma`)

## 2. GStreamerMSEUtils.cpp — Video section

- [x] 2.1 Replace `!videoCapability.codecCapabilities.mpeg2Profiles.empty()` with `videoCapability.codecCapabilities.mpeg2.has_value()`
- [x] 2.2 Replace `!videoCapability.codecCapabilities.h264Profiles.empty()` with `videoCapability.codecCapabilities.h264.has_value()`
- [x] 2.3 Replace `!videoCapability.codecCapabilities.h265Profiles.empty()` with `videoCapability.codecCapabilities.h265.has_value()`
- [x] 2.4 Replace `!videoCapability.codecCapabilities.vp9Profiles.empty()` with `videoCapability.codecCapabilities.vp9.has_value()`
- [x] 2.5 Replace `!videoCapability.codecCapabilities.av1Profiles.empty()` with `videoCapability.codecCapabilities.av1.has_value()`

## 3. GStreamerMSEUtilsTests.cpp — Update audio fixture

- [x] 3.1 Update `shouldFillAudioDecoderCapabilities` fixture: remove `DolbyMatCapability{}` and `WmaCapability{}` from the `AudioDecoderCapability` constructor; add `DolbyEac3Capability{}`
- [x] 3.2 Update expected caps list: remove `audio/x-wma` from `expectedCaps`; keep `audio/x-eac3` (now driven by `dolbyEac3`)
- [x] 3.3 Add test `shouldRegisterEac3WhenDolbyEac3Present`: capability with only `dolbyEac3` → assert `audio/x-eac3` is in caps
- [x] 3.4 Add test `shouldNotRegisterEac3WhenOnlyDolbyAc3Present`: capability with only `dolbyAc3` → assert `audio/x-eac3` is NOT in caps
- [x] 3.5 Add test `shouldNotRegisterWma`: any valid capability → assert `audio/x-wma` is NOT in caps
- [x] 3.6 Add test `shouldNotRegisterDolbyMatRaw`: capability without `pcm` or `dolbyMat` → assert `audio/x-raw` is NOT in caps

## 4. GStreamerMSEUtilsTests.cpp — Update video fixture

- [x] 4.1 Update `shouldFillVideoDecoderCapabilities` fixture: change `VideoCodecCapabilities` construction from flat vector syntax (`{firebolt::rialto::Mpeg2Profile{}}`) to `std::optional<Mpeg2CodecCapability>` fields
- [x] 4.2 Fix `VideoDecoderCapability` constructor: remove the second `{}` arg (old `dynamicRanges` field no longer exists)
- [x] 4.3 Add test `shouldNotRegisterVideoCodecWhenOptionalIsNullopt`: capability with one codec `std::nullopt` → assert its GStreamer cap is NOT registered

## 5. Verify

- [x] 5.1 Build the unit tests and confirm no compile errors
- [x] 5.2 Run `GStreamerMSEUtilsTests` and confirm all tests pass with no regressions
