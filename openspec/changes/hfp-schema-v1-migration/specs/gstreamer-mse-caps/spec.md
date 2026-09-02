## ADDED Requirements

### Requirement: dolbyEac3 registers audio/x-eac3 independently
When `AudioDecoderCapability.dolbyEac3` is present, the system SHALL register
`audio/x-eac3` GStreamer caps on the audio sink pad. This registration SHALL be
independent of `dolbyAc3` — a device with only AC3 SHALL NOT get EAC3 registered.

#### Scenario: dolbyEac3 present — eac3 cap registered
- **WHEN** `rialto_mse_sink_setup_supported_caps` is called with an `AudioDecoderCapabilities`
  whose capability entry has `dolbyEac3` set
- **THEN** the resulting sink pad caps SHALL contain `audio/x-eac3`

#### Scenario: dolbyAc3 only — eac3 cap NOT registered
- **WHEN** `rialto_mse_sink_setup_supported_caps` is called with `dolbyAc3` set but `dolbyEac3`
  absent
- **THEN** the resulting sink pad caps SHALL NOT contain `audio/x-eac3`

#### Scenario: neither dolbyAc3 nor dolbyEac3 — no ac3/eac3 caps
- **WHEN** both `dolbyAc3` and `dolbyEac3` are absent from the capability entry
- **THEN** the resulting sink pad caps SHALL contain neither `audio/x-ac3` nor `audio/x-eac3`

### Requirement: dolbyAc3 registers audio/x-ac3 only
When `AudioDecoderCapability.dolbyAc3` is present, the system SHALL register
`audio/x-ac3` only. It SHALL NOT register `audio/x-eac3` based solely on `dolbyAc3`.

#### Scenario: dolbyAc3 present — only ac3 cap registered
- **WHEN** `rialto_mse_sink_setup_supported_caps` is called with `dolbyAc3` set and `dolbyEac3`
  absent
- **THEN** the resulting sink pad caps SHALL contain `audio/x-ac3` and SHALL NOT contain
  `audio/x-eac3`

## REMOVED Requirements

### Requirement: dolbyMat registers audio/x-raw
**Reason**: `DOLBY_MAT` has been removed from the HFP schema v1.0.0 capability set.
**Migration**: No replacement — `audio/x-raw` from PCM capability is the correct path for raw
audio registration.

#### Scenario: dolbyMat absent — audio/x-raw not registered via dolbyMat
- **WHEN** `rialto_mse_sink_setup_supported_caps` is called and `dolbyMat` is absent from the
  capability entry
- **THEN** `audio/x-raw` SHALL only be registered if `pcm` capability is present, not via
  `dolbyMat`

### Requirement: wma registers audio/x-wma
**Reason**: `WMA` has been removed from the HFP schema v1.0.0 capability set.
**Migration**: No replacement — `audio/x-wma` is no longer a supported capability.

#### Scenario: wma removed — audio/x-wma not registered
- **WHEN** `rialto_mse_sink_setup_supported_caps` is called with any valid `AudioDecoderCapabilities`
- **THEN** the resulting sink pad caps SHALL NOT contain `audio/x-wma`

## MODIFIED Requirements

### Requirement: Video codec caps use optional field presence check
The video overload of `rialto_mse_sink_setup_supported_caps` SHALL check each of the five
video codec fields (`mpeg2`, `h264`, `h265`, `vp9`, `av1`) using `has_value()` on the
`std::optional<*CodecCapability>` field before registering the corresponding GStreamer caps.

#### Scenario: mpeg2 optional present — video/mpeg registered
- **WHEN** `videoCapability.codecCapabilities.mpeg2.has_value()` is true
- **THEN** `video/mpeg, mpegversion=2` SHALL be registered on the video sink pad

#### Scenario: mpeg2 optional absent — video/mpeg not registered
- **WHEN** `videoCapability.codecCapabilities.mpeg2` is `std::nullopt`
- **THEN** `video/mpeg, mpegversion=2` SHALL NOT be registered

#### Scenario: all five video codecs present — all caps registered
- **WHEN** all five codec optional fields (`mpeg2`, `h264`, `h265`, `vp9`, `av1`) have values
- **THEN** all five corresponding GStreamer caps SHALL be registered on the video sink pad
