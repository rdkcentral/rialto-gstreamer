/*
 * Copyright (C) 2026 Sky UK
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include <gst/gst.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace metricscli
{
enum class FaultScenario
{
    FrameDrop,
    FrameRepeat,
    BufferUnderflow,
    DecoderError
};

struct FaultParameters
{
    std::uint32_t bufferCount{3};      // Buffers affected by frame-drop and decoder-error
    std::uint32_t delayFrames{3};      // Frame durations a buffer is held by frame-repeat
    std::uint32_t stallMs{2000};       // Time video buffers are blocked by buffer-underflow
    std::uint32_t windowSeconds{10};   // Upper bound on how long a probe stays installed
};

// State shared between the CLI and a scenario handler running on the streaming thread.
class FaultContext
{
public:
    FaultContext(const FaultParameters &parameters, GstElement *pipeline) : parameters{parameters}, pipeline{pipeline} {}

    void fail(const std::string &message)
    {
        std::lock_guard<std::mutex> lock{m_mutex};
        if (!m_failed.load())
        {
            m_failure = message;
            m_failed = true;
        }
    }
    bool failed() const { return m_failed.load(); }
    void cancel() { m_cancelled = true; }
    bool cancelled() const { return m_cancelled.load(); }
    std::string failure() const
    {
        std::lock_guard<std::mutex> lock{m_mutex};
        return m_failure;
    }

    const FaultParameters parameters;
    GstElement *const pipeline;

    // Set by frame-drop on its first buffer: how far buffer timestamps are moved back, in nanoseconds.
    std::optional<GstClockTime> ptsShift;

    // Set by frame-repeat: how long its buffer was held, in nanoseconds.
    std::optional<std::uint64_t> appliedDelay;

    // Set by buffer-underflow: how long video buffers were blocked, in milliseconds.
    std::optional<std::uint32_t> appliedStallMs;

    // Incremented by decoder-error: how many slice NAL units were corrupted.
    std::atomic<std::uint32_t> corruptedSlices{0};

private:
    mutable std::mutex m_mutex;
    std::atomic<bool> m_failed{false};
    std::atomic<bool> m_cancelled{false};
    std::string m_failure;
};

// Called on the streaming thread for each buffer inside the fault window. bufferIndex is the
// zero-based count of buffers affected so far. A handler sets applied only when it acted on the
// buffer; buffers it did not act on do not count towards the scenario's buffer limit.
using FaultHandler = GstPadProbeReturn (*)(GstPad *pad, GstPadProbeInfo *info, FaultContext &context,
                                           std::uint32_t bufferIndex, bool &applied);
using FaultBufferLimit = std::uint32_t (*)(const FaultParameters &parameters);

struct FaultScenarioInfo
{
    FaultScenario scenario;
    const char *name;
    FaultHandler handler; // nullptr until the scenario itself is implemented
    FaultBufferLimit bufferLimit;
};

namespace detail
{
// Playback must have advanced this far before frame-drop retimes buffers, and buffers are moved to
// this far behind the playback position so they are unambiguously late.
constexpr GstClockTime kFrameDropSettleTime{2 * GST_SECOND};
constexpr GstClockTime kFrameDropLateMargin{1 * GST_SECOND};
constexpr gulong kStallSliceUs{50000};

inline GstClockTime subtractClamped(GstClockTime value, GstClockTime amount)
{
    return value > amount ? value - amount : 0;
}

enum class PlaybackState
{
    Failed,
    NotSettled,
    Settled
};

// Reports a failure on the context if the playback position cannot be queried.
inline PlaybackState checkPlaybackSettled(FaultContext &context, const char *scenario)
{
    gint64 position{0};
    if (!gst_element_query_position(context.pipeline, GST_FORMAT_TIME, &position) || position < 0)
    {
        context.fail(std::string{scenario} + ": failed to query the current playback position.");
        return PlaybackState::Failed;
    }
    return static_cast<GstClockTime>(position) < kFrameDropSettleTime ? PlaybackState::NotSettled
                                                                       : PlaybackState::Settled;
}

// frame-drop (late frames): moves the PTS (and DTS, where set) of consecutive video buffers back
// so each lands behind the current playback position and misses its presentation deadline. No
// seek, flush or EOS is involved, since those are excluded from the drop definition.
inline GstPadProbeReturn frameDropHandler(GstPad *pad, GstPadProbeInfo *info, FaultContext &context,
                                          std::uint32_t, bool &applied)
{
    GstBuffer *buffer{GST_PAD_PROBE_INFO_BUFFER(info)};
    if (!buffer || !GST_BUFFER_PTS_IS_VALID(buffer))
    {
        return GST_PAD_PROBE_PASS;
    }

    if (!context.ptsShift)
    {
        gint64 position{0};
        if (!gst_element_query_position(context.pipeline, GST_FORMAT_TIME, &position) || position < 0)
        {
            context.fail("frame-drop: failed to query the current playback position.");
            return GST_PAD_PROBE_PASS;
        }
        if (static_cast<GstClockTime>(position) < kFrameDropSettleTime)
        {
            return GST_PAD_PROBE_PASS; // Playback has not settled yet; try the next buffer
        }

        GstEvent *segmentEvent{gst_pad_get_sticky_event(pad, GST_EVENT_SEGMENT, 0)};
        if (!segmentEvent)
        {
            context.fail("frame-drop: no segment received on the video sink pad.");
            return GST_PAD_PROBE_PASS;
        }
        const GstSegment *eventSegment{nullptr};
        gst_event_parse_segment(segmentEvent, &eventSegment);
        GstSegment segment;
        gst_segment_init(&segment, GST_FORMAT_UNDEFINED);
        gst_segment_copy_into(eventSegment, &segment);
        gst_event_unref(segmentEvent);

        // Position is a stream time, so compare in the same domain.
        if (segment.format != GST_FORMAT_TIME)
        {
            context.fail("frame-drop: the video sink pad segment is not in time format.");
            return GST_PAD_PROBE_PASS;
        }
        const guint64 streamTime{gst_segment_to_stream_time(&segment, GST_FORMAT_TIME, GST_BUFFER_PTS(buffer))};
        if (streamTime == GST_CLOCK_TIME_NONE)
        {
            context.fail("frame-drop: cannot convert the buffer timestamp to stream time.");
            return GST_PAD_PROBE_PASS;
        }

        const GstClockTime target{static_cast<GstClockTime>(position) - kFrameDropLateMargin};
        if (streamTime <= target)
        {
            context.ptsShift = 0; // Already late enough
        }
        else
        {
            context.ptsShift = streamTime - target;
        }
    }

    buffer = gst_buffer_make_writable(buffer);
    info->data = buffer;
    GST_BUFFER_PTS(buffer) = subtractClamped(GST_BUFFER_PTS(buffer), *context.ptsShift);
    if (GST_BUFFER_DTS_IS_VALID(buffer))
    {
        GST_BUFFER_DTS(buffer) = subtractClamped(GST_BUFFER_DTS(buffer), *context.ptsShift);
    }
    applied = true;
    return GST_PAD_PROBE_OK;
}

// Frame duration from the negotiated framerate, falling back to the buffer duration.
inline GstClockTime frameDuration(GstPad *pad, GstBuffer *buffer)
{
    GstClockTime duration{GST_CLOCK_TIME_NONE};
    if (GstCaps *caps{gst_pad_get_current_caps(pad)})
    {
        gint numerator{0};
        gint denominator{0};
        if (gst_caps_get_size(caps) > 0 &&
            gst_structure_get_fraction(gst_caps_get_structure(caps, 0), "framerate", &numerator, &denominator) &&
            numerator > 0 && denominator > 0)
        {
            duration = gst_util_uint64_scale(GST_SECOND, denominator, numerator);
        }
        gst_caps_unref(caps);
    }
    if (duration == GST_CLOCK_TIME_NONE && GST_BUFFER_DURATION_IS_VALID(buffer) && GST_BUFFER_DURATION(buffer) > 0)
    {
        duration = GST_BUFFER_DURATION(buffer);
    }
    return duration;
}

// frame-repeat (single delayed buffer): holds one video buffer for delayFrames frame durations, so
// the renderer has no new frame at the expected time and repeats the preceding one. The delay is
// kept short (it must fit in the fault window) so that it is not an input starvation.
inline GstPadProbeReturn frameRepeatHandler(GstPad *pad, GstPadProbeInfo *info, FaultContext &context,
                                            std::uint32_t, bool &applied)
{
    GstBuffer *buffer{GST_PAD_PROBE_INFO_BUFFER(info)};
    if (!buffer)
    {
        return GST_PAD_PROBE_PASS;
    }

    // A frame must already have been presented for there to be a preceding frame to repeat.
    switch (checkPlaybackSettled(context, "frame-repeat"))
    {
    case PlaybackState::Failed:
    case PlaybackState::NotSettled:
        return GST_PAD_PROBE_PASS;
    case PlaybackState::Settled:
        break;
    }
    const GstClockTime duration{frameDuration(pad, buffer)};
    if (duration == GST_CLOCK_TIME_NONE)
    {
        context.fail("frame-repeat: the frame duration is unknown (no fixed framerate in the caps and no buffer "
                     "duration).");
        return GST_PAD_PROBE_PASS;
    }

    const std::uint64_t delayNs{static_cast<std::uint64_t>(duration) * context.parameters.delayFrames};
    if (delayNs / 1000 > static_cast<std::uint64_t>(context.parameters.windowSeconds) * G_USEC_PER_SEC)
    {
        context.fail("frame-repeat: the requested delay does not fit in the fault window.");
        return GST_PAD_PROBE_PASS;
    }

    context.appliedDelay = delayNs;
    g_usleep(static_cast<gulong>(delayNs / 1000));
    applied = true;
    return GST_PAD_PROBE_OK;
}

// buffer-underflow (decoder input starvation): blocks the video sink pad for stallMs by holding the
// streaming thread inside the probe, so no video buffer reaches the sink. Once the client and server
// queues drain the decoder starves; releasing the buffer lets input resume and the starvation end.
// Only the video pad is held, so audio keeps flowing. Independent of need-data flow control.
inline GstPadProbeReturn bufferUnderflowHandler(GstPad *, GstPadProbeInfo *info, FaultContext &context,
                                                std::uint32_t, bool &applied)
{
    if (!GST_PAD_PROBE_INFO_BUFFER(info))
    {
        return GST_PAD_PROBE_PASS;
    }

    switch (checkPlaybackSettled(context, "buffer-underflow"))
    {
    case PlaybackState::Failed:
    case PlaybackState::NotSettled:
        return GST_PAD_PROBE_PASS;
    case PlaybackState::Settled:
        break;
    }

    if (static_cast<std::uint64_t>(context.parameters.stallMs) >
        static_cast<std::uint64_t>(context.parameters.windowSeconds) * 1000)
    {
        context.fail("buffer-underflow: the requested stall does not fit in the fault window.");
        return GST_PAD_PROBE_PASS;
    }

    context.appliedStallMs = context.parameters.stallMs;
    applied = true;
    const gint64 endUs{g_get_monotonic_time() + static_cast<gint64>(context.parameters.stallMs) * 1000};
    // Sleep in slices so a cancelled run is not held up by a long stall.
    while (!context.cancelled() && g_get_monotonic_time() < endUs)
    {
        g_usleep(kStallSliceUs);
    }
    return GST_PAD_PROBE_OK;
}

// Selects the slice NAL units of a buffer that decoder-error may corrupt. Parameter sets, SEI and other
// non-slice NAL units are never touched, and neither are buffers containing an IDR/IRAP slice, since
// corrupting those prevents the decoder from (re)starting rather than producing a per-frame error.
struct NalUnit
{
    std::size_t offset{0};
    std::size_t size{0};
};

struct CodecInfo
{
    enum class Type
    {
        H264,
        H265
    } type{Type::H264};
    std::size_t nalLengthSize{0}; // 0 means byte-stream (start code) framing
};

constexpr std::size_t kMinCorruptibleSliceSize{16};

inline bool getCodecInfo(GstPad *pad, CodecInfo &codec, std::string &error)
{
    GstCaps *caps{gst_pad_get_current_caps(pad)};
    if (!caps || gst_caps_get_size(caps) == 0)
    {
        if (caps)
        {
            gst_caps_unref(caps);
        }
        error = "decoder-error: no caps negotiated on the video sink pad.";
        return false;
    }

    const GstStructure *structure{gst_caps_get_structure(caps, 0)};
    const std::string name{gst_structure_get_name(structure)};
    const gchar *streamFormat{gst_structure_get_string(structure, "stream-format")};
    const std::string format{streamFormat ? streamFormat : "byte-stream"};
    bool supported{true};

    if (name.rfind("application/x-", 0) == 0)
    {
        error = "decoder-error: encrypted media (" + name + ") cannot be corrupted.";
        supported = false;
    }
    else if (name == "video/x-h264" || name == "video/x-h265")
    {
        const bool h265{name == "video/x-h265"};
        codec.type = h265 ? CodecInfo::Type::H265 : CodecInfo::Type::H264;
        const bool lengthPrefixed{format == "avc" || format == "avc3" || format == "hvc1" || format == "hev1"};
        codec.nalLengthSize = 0;
        if (lengthPrefixed)
        {
            codec.nalLengthSize = 4;
            const GValue *codecData{gst_structure_get_value(structure, "codec_data")};
            GstBuffer *codecBuffer{codecData && GST_VALUE_HOLDS_BUFFER(codecData) ? gst_value_get_buffer(codecData)
                                                                                  : nullptr};
            GstMapInfo map;
            if (codecBuffer && gst_buffer_map(codecBuffer, &map, GST_MAP_READ))
            {
                const std::size_t lengthByte{h265 ? 21u : 4u};
                if (map.size > lengthByte)
                {
                    codec.nalLengthSize = static_cast<std::size_t>(map.data[lengthByte] & 0x3) + 1;
                }
                gst_buffer_unmap(codecBuffer, &map);
            }
        }
    }
    else
    {
        error = "decoder-error: unsupported video codec '" + name + "' (supported: video/x-h264, video/x-h265).";
        supported = false;
    }

    gst_caps_unref(caps);
    return supported;
}

inline void collectNalUnits(const guint8 *data, std::size_t size, std::size_t lengthSize, std::vector<NalUnit> &nals)
{
    if (lengthSize > 0)
    {
        std::size_t pos{0};
        while (pos + lengthSize <= size)
        {
            std::size_t length{0};
            for (std::size_t i = 0; i < lengthSize; ++i)
            {
                length = (length << 8) | data[pos + i];
            }
            const std::size_t start{pos + lengthSize};
            if (length == 0 || start + length > size)
            {
                return;
            }
            nals.push_back({start, length});
            pos = start + length;
        }
        return;
    }

    // Byte-stream: NAL units follow 00 00 01 start codes and run to the next start code.
    std::vector<std::size_t> starts;
    for (std::size_t i = 0; i + 3 <= size; ++i)
    {
        if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1)
        {
            starts.push_back(i + 3);
            i += 2;
        }
    }
    for (std::size_t n = 0; n < starts.size(); ++n)
    {
        std::size_t end{n + 1 < starts.size() ? starts[n + 1] - 3 : size};
        while (end > starts[n] && data[end - 1] == 0)
        {
            --end; // Strip trailing zero bytes belonging to the next start code
        }
        if (end > starts[n])
        {
            nals.push_back({starts[n], end - starts[n]});
        }
    }
}

// Returns true when nal is a non-IDR slice that can be corrupted; sets keyframe for IDR/IRAP slices.
inline bool classifySlice(const guint8 *data, const NalUnit &nal, CodecInfo::Type type, bool &keyframe)
{
    if (type == CodecInfo::Type::H264)
    {
        const unsigned nalType{data[nal.offset] & 0x1Fu};
        if (nalType == 5)
        {
            keyframe = true;
            return false;
        }
        return nalType == 1;
    }
    const unsigned nalType{(data[nal.offset] >> 1) & 0x3Fu};
    if (nalType >= 16 && nalType <= 21)
    {
        keyframe = true;
        return false;
    }
    return nalType <= 9;
}

// Corrupts the second half of each eligible slice, leaving the NAL header and the start of the slice
// header intact so the decoder reports the damage per frame instead of rejecting the stream.
// Returns the number of slices corrupted.
inline std::uint32_t corruptSlices(guint8 *data, std::size_t size, const CodecInfo &codec)
{
    std::vector<NalUnit> nals;
    collectNalUnits(data, size, codec.nalLengthSize, nals);

    const std::size_t headerSize{codec.type == CodecInfo::Type::H265 ? 2u : 1u};
    std::vector<NalUnit> slices;
    bool keyframe{false};
    for (const NalUnit &nal : nals)
    {
        if (nal.size < headerSize)
        {
            continue;
        }
        bool nalIsKeyframe{false};
        const bool eligible{classifySlice(data, nal, codec.type, nalIsKeyframe)};
        keyframe = keyframe || nalIsKeyframe;
        if (eligible && nal.size >= headerSize + kMinCorruptibleSliceSize)
        {
            slices.push_back(nal);
        }
    }
    if (keyframe)
    {
        return 0;
    }

    for (const NalUnit &slice : slices)
    {
        const std::size_t payloadStart{slice.offset + headerSize};
        const std::size_t corruptFrom{payloadStart + (slice.size - headerSize) / 2};
        for (std::size_t i = corruptFrom; i < slice.offset + slice.size; ++i)
        {
            data[i] ^= 0xFF;
        }
    }
    return static_cast<std::uint32_t>(slices.size());
}

// decoder-error (frame-level corruption): corrupts the slice payload of bufferCount non-key video
// buffers, one affected frame per buffer. Fails for encrypted media and unsupported codecs.
inline GstPadProbeReturn decoderErrorHandler(GstPad *pad, GstPadProbeInfo *info, FaultContext &context,
                                             std::uint32_t, bool &applied)
{
    GstBuffer *buffer{GST_PAD_PROBE_INFO_BUFFER(info)};
    if (!buffer)
    {
        return GST_PAD_PROBE_PASS;
    }

    switch (checkPlaybackSettled(context, "decoder-error"))
    {
    case PlaybackState::Failed:
    case PlaybackState::NotSettled:
        return GST_PAD_PROBE_PASS;
    case PlaybackState::Settled:
        break;
    }

    if (gst_buffer_get_protection_meta(buffer))
    {
        context.fail("decoder-error: encrypted media (protection metadata) cannot be corrupted.");
        return GST_PAD_PROBE_PASS;
    }

    CodecInfo codec;
    std::string error;
    if (!getCodecInfo(pad, codec, error))
    {
        context.fail(error);
        return GST_PAD_PROBE_PASS;
    }

    buffer = gst_buffer_make_writable(buffer);
    info->data = buffer;
    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, static_cast<GstMapFlags>(GST_MAP_READ | GST_MAP_WRITE)))
    {
        context.fail("decoder-error: the video buffer cannot be mapped writable.");
        return GST_PAD_PROBE_PASS;
    }
    const std::uint32_t corrupted{corruptSlices(map.data, map.size, codec)};
    gst_buffer_unmap(buffer, &map);

    if (corrupted == 0)
    {
        return GST_PAD_PROBE_OK; // Key frame or no eligible slice; try the next buffer
    }
    context.corruptedSlices += corrupted;
    applied = true;
    return GST_PAD_PROBE_OK;
}

inline std::uint32_t bufferCountLimit(const FaultParameters &parameters)
{
    return parameters.bufferCount;
}

inline std::uint32_t singleBufferLimit(const FaultParameters &)
{
    return 1;
}

inline const FaultScenarioInfo *scenarioTable(std::size_t &size)
{
    static const FaultScenarioInfo table[]{
        {FaultScenario::FrameDrop, "frame-drop", frameDropHandler, bufferCountLimit},
        {FaultScenario::FrameRepeat, "frame-repeat", frameRepeatHandler, singleBufferLimit},
        {FaultScenario::BufferUnderflow, "buffer-underflow", bufferUnderflowHandler, singleBufferLimit},
        {FaultScenario::DecoderError, "decoder-error", decoderErrorHandler, bufferCountLimit},
    };
    size = sizeof(table) / sizeof(table[0]);
    return table;
}
} // namespace detail

inline const FaultScenarioInfo *findFaultScenario(std::string_view name)
{
    std::size_t size{0};
    const FaultScenarioInfo *table{detail::scenarioTable(size)};
    for (std::size_t i = 0; i < size; ++i)
    {
        if (name == table[i].name)
        {
            return &table[i];
        }
    }
    return nullptr;
}

inline std::string faultScenarioNames()
{
    std::size_t size{0};
    const FaultScenarioInfo *table{detail::scenarioTable(size)};
    std::string names;
    for (std::size_t i = 0; i < size; ++i)
    {
        names += (i ? ", " : "");
        names += table[i].name;
    }
    return names;
}

// Installs a bounded buffer probe on a sink pad. The probe stops affecting buffers once the buffer
// limit or the time window is reached, and is always removed by stop().
class FaultInjector
{
public:
    FaultInjector(const FaultScenarioInfo &info, const FaultParameters &parameters, GstElement *pipeline)
        : m_info{info}, m_context{parameters, pipeline}
    {
    }
    FaultInjector(const FaultInjector &) = delete;
    FaultInjector &operator=(const FaultInjector &) = delete;
    ~FaultInjector() { stop(); }

    bool start(GstElement *sink, std::string &error)
    {
        if (!m_info.handler)
        {
            error = std::string{"Fault scenario '"} + m_info.name + "' is not implemented.";
            return false;
        }
        if (m_pad)
        {
            error = "Fault injection is already running.";
            return false;
        }

        GstPad *pad{gst_element_get_static_pad(sink, "sink")};
        if (!pad)
        {
            error = "Failed to get the sink pad of rialtomsevideosink for fault injection.";
            return false;
        }

        const FaultParameters &parameters{m_context.parameters};
        m_limit = m_info.bufferLimit ? m_info.bufferLimit(parameters) : parameters.bufferCount;
        m_seen = 0;
        m_deadlineUs = g_get_monotonic_time() + static_cast<gint64>(parameters.windowSeconds) * G_USEC_PER_SEC;

        const gulong id{gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, &FaultInjector::probeCallback, this, nullptr)};
        if (id == 0)
        {
            gst_object_unref(pad);
            error = "Failed to install the fault-injection probe.";
            return false;
        }
        m_pad = pad;
        m_probeId = id;
        return true;
    }

    // Safe to call repeatedly and from any thread other than the probe callback.
    void stop()
    {
        if (!m_pad)
        {
            return;
        }
        m_context.cancel();
        const gulong id{m_probeId.exchange(0)};
        if (id != 0)
        {
            gst_pad_remove_probe(m_pad, id);
        }
        gst_object_unref(m_pad);
        m_pad = nullptr;
    }

    std::uint32_t buffersAffected() const { return m_seen.load(); }
    const FaultScenarioInfo &info() const { return m_info; }
    const FaultContext &context() const { return m_context; }

private:
    static GstPadProbeReturn probeCallback(GstPad *pad, GstPadProbeInfo *info, gpointer userData)
    {
        return static_cast<FaultInjector *>(userData)->onBuffer(pad, info);
    }

    GstPadProbeReturn onBuffer(GstPad *pad, GstPadProbeInfo *info)
    {
        if (m_context.failed() || m_seen.load() >= m_limit || g_get_monotonic_time() >= m_deadlineUs)
        {
            // Only the side that clears the id may remove the probe, avoiding a double removal.
            return m_probeId.exchange(0) != 0 ? GST_PAD_PROBE_REMOVE : GST_PAD_PROBE_PASS;
        }
        bool applied{false};
        const GstPadProbeReturn result{m_info.handler(pad, info, m_context, m_seen.load(), applied)};
        if (applied)
        {
            ++m_seen;
        }
        return result;
    }

    const FaultScenarioInfo &m_info;
    FaultContext m_context;
    GstPad *m_pad{nullptr};
    std::atomic<gulong> m_probeId{0};
    std::atomic<std::uint32_t> m_seen{0};
    std::uint32_t m_limit{0};
    gint64 m_deadlineUs{0};
};
} // namespace metricscli
