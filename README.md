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
or non-zero counts. While playback continues it prints the counters again every
15 seconds, and once more at EOS (`Final Frame counters:`); these later reads are
informational and never change the exit code. Use `--help` to display
the command syntax. `--fault-scenario NAME` selects a GStreamer-level fault:
`frame-drop`, `frame-repeat`, `buffer-underflow`, or `decode-error`. Unknown
names are argument errors. The fault is injected by a bounded buffer probe on
`rialtomsevideosink`'s sink pad, installed only after playback is running and
removed when the run ends (or after `--fault-window-seconds`, default 10, counted
from the first buffer the fault affects). A fault is applied only once the sink
has rendered at least 3 frames (read from `frame-counters`), so there is always
a preceding frame on screen. The
optional positive-integer tuning options `--fault-buffers` (default 10),
`--fault-delay-frames` (default 10), `--fault-stall-ms` (default 5000),
`--fault-drop-shift-ms` (default 500), and
`--fault-window-seconds` require `--fault-scenario`. The scenarios themselves
are implemented individually. `frame-drop` is implemented: once the gate above
is open, it moves the PTS (and DTS, where set) of
`--fault-buffers` consecutive video buffers back by `--fault-drop-shift-ms`
(clamped at time zero), so each frame reaches the renderer already late by about
that much and misses its presentation deadline. The shift is relative to the
neighbouring buffers, not to the playback position, because Rialto queues video
well ahead of playback; no seek, flush or EOS is involved. The run fails if no buffer was affected. `frame-repeat` is also implemented: once the gate is open,
it holds a single video buffer for the queued lead plus `--fault-delay-frames` frame durations
(taken from the caps framerate, or the buffer duration), so the renderer
misses new frames for that many frame durations and repeats the preceding frame; it fails if the frame duration is unknown or the
delay does not fit in `--fault-window-seconds`. `buffer-underflow` is also implemented: once the gate is open,
it blocks the video sink pad for the queued lead plus `--fault-stall-ms` (the video stream
only; audio keeps flowing), then releases it so the starvation ends; it fails
if the stall does not fit in `--fault-window-seconds`. The queued lead is how far the
buffer's timestamp is ahead of the playback position when the fault starts; Rialto
queues video well ahead of playback, so the hold has to outlast that queue before the
renderer is actually starved. The hold therefore takes about lead + requested time, so
`--timeout` must cover it. `decode-error` is also implemented for H.264 and H.265 (byte-stream and
length-prefixed): once the gate is open, it inverts all but the first 8 bytes
(or first eighth, if larger) of the slice data in `--fault-buffers` non-key video buffers, leaving
parameter sets, NAL headers and the start of each slice header intact. `--decode-error-mode`
selects the damage: `tail` (the default, as described), `payload` (the whole slice data after the
NAL header) or `header` (only the first 8 bytes of the slice header, which the decoder cannot
parse). `payload` and `header` are harsher; a decoder may drop the frame or lose sync instead of
reporting an error, so use them when `tail` produces no decode-error metric. Buffers
containing an IDR/IRAP slice are skipped. It fails for encrypted media and
unsupported codecs. Corruption can propagate to later frames until the next key
frame, so the decoder may report more errors than the buffers affected. All four
scenarios are now implemented. Because the probe sits on the client side of the
sink, queues between the client and the server can hide a short stall or delay;
increase `--fault-stall-ms` or `--fault-delay-frames`, or use longer, higher-bitrate
media, if no effect is seen. At the end of every fault run the CLI prints one
`Fault injected:` line with the scenario, buffers affected, and the values
actually applied (`pts-shift-ns`, `delay-ns`, `stall-ms-applied`, or
`slices-corrupted`), `lead-ms` (queued lead added to a repeat or underflow hold), followed by diagnostics (`buffers-probed`, `probed-pts-ms`,
`last-rendered`, `last-position-ms`, `not-settled-skips`, and for decode-error
`no-eligible-slice`) that show why a fault did or did not apply, so a run can be matched with the metric messages observed.

### Metric messages

`--log-bus` prints every message read from the playbin bus as a `Bus message:`
line (type, source element and, where present, the structure). It also reads
the bus unfiltered, so it shows whether metric messages reach the CLI at all.

For the whole run the CLI watches the playbin bus for `GST_MESSAGE_APPLICATION`
messages and selects those whose structure is named `media-pipeline-metric`
(other application messages are ignored). Each one is validated and printed as a
`Metric message:` line. Required fields are `metric`, `observation-kind` and
`source_id` as `G_TYPE_UINT`, with `observation-kind` 1 (occurrence), 2
(episode-started) or 3 (episode-resolved). Optional fields must have the right
type: `session_id` `G_TYPE_UINT`, `pts-ns` `G_TYPE_INT64`, and `duration-ns` and
`count` `G_TYPE_UINT64`; occurrence messages must omit `duration-ns` and `count`.
A message that fails validation is reported as `Invalid metric message:` and is
not counted as valid. The `metric` value is printed raw and its numeric category
is not validated. At the end the CLI prints `Metric messages observed: valid=N,
invalid=M`.

Without `--fault-scenario`, observed metric messages are only reported and never
change the exit code. With `--fault-scenario`, at least one valid message is
required: if the fault was applied but no valid `media-pipeline-metric` message
has arrived by the end of playback, the CLI keeps waiting until `--timeout`
expires (the metric can arrive over IPC after EOS) and then exits with code `1`.
Any valid message satisfies the requirement, and further messages are reported
but not treated as failures, because the scenarios overlap (for example a
delayed buffer can cause a frame drop as well as a repeat). Invalid messages are
reported but never count towards the requirement. If the fault itself could not
be applied, the run fails for that reason instead.

Exit codes are deterministic: `0` means playback reached EOS (and, for a fault
run, a valid metric message was observed) or help was shown, `1` means
playback/setup failed, timed out, a requested fault scenario failed, or a
required metric message was not received, `2` means invalid or missing
command-line arguments, and `3` means GStreamer initialization failed.

## Main (Rialto) project webpage:
https://rdkcentral.github.io/rialto/
