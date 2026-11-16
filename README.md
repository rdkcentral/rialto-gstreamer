# rialto-gstreamer

## Project webpage:
https://rdkcentral.github.io/rialto-gstreamer/

## Coding Guidelines:
https://wiki.rdkcentral.com/display/ASP/Rialto+Coding+Guidelines

## Building and Running Unit Tests:
https://wiki.rdkcentral.com/display/ASP/Unit+Tests

## Building Rialto using Yocto:
https://wiki.rdkcentral.com/display/ASP/Building+Rialto+using+Yocto

## Running Rialto:
https://wiki.rdkcentral.com/display/ASP/Running+YouTube+with+Rialto

## GStreamer metrics test CLI

The standalone `rialto-gstreamer-metrics-test` executable accepts a media URI,
an optional timeout in seconds, and an optional fault-scenario identifier:

```sh
cmake -S . -B build
cmake --build build --target rialto-gstreamer-metrics-test
./build/tests/metrics-cli/rialto-gstreamer-metrics-test \
    --uri https://example.com/video.m3u8 \
    --timeout 30
```

`--uri` is required and must be a valid absolute GStreamer URI. `--timeout`
must be a positive integer and defaults to 30 seconds. Playback ends
successfully on EOS and fails on a pipeline error or timeout. The CLI explicitly
uses `rialtomsevideosink` as `playbin`'s video sink; the Rialto plugin and a
working Rialto client/server are required at runtime. Once the video source is
active, the CLI queries `frame-counters` and fails if the returned structure
does not contain `rendered`, `dropped`, `corrupted`, and `timestamp` as
`G_TYPE_UINT64` fields. It prints the returned values without requiring fixed
or non-zero counts. Use `--help` to display
the command syntax. `--fault-scenario NAME` is reserved for configured
GStreamer-level scenarios; until those scenarios are implemented, requesting
one fails rather than silently running without the fault.

Exit codes are deterministic: `0` means playback reached EOS or help was
shown, `1` means playback/setup failed, timed out, or a requested fault
scenario is not implemented, `2` means invalid or missing command-line
arguments, and `3` means GStreamer initialization failed.

## Main (Rialto) project webpage:
https://rdkcentral.github.io/rialto/
