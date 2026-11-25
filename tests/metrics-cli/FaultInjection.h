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

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
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
    DecodeError
};

// How decode-error damages a slice.
enum class DecodeErrorMode
{
    Tail,    // Invert all but the first bytes of the slice data (default)
    Payload, // Invert the whole slice data after the NAL header, including the slice header
    Header   // Invert only the slice header bytes, so the decoder cannot parse the slice
};

inline const char *decodeErrorModeName(DecodeErrorMode mode)
{
    switch (mode)
    {
    case DecodeErrorMode::Tail:
        return "tail";
    case DecodeErrorMode::Payload:
        return "payload";
    case DecodeErrorMode::Header:
        return "header";
    }
    return "tail";
}

inline std::optional<DecodeErrorMode> findDecodeErrorMode(std::string_view name)
{
    for (const DecodeErrorMode mode : {DecodeErrorMode::Tail, DecodeErrorMode::Payload, DecodeErrorMode::Header})
    {
        if (name == decodeErrorModeName(mode))
        {
            return mode;
        }
    }
    return std::nullopt;
}

struct FaultParameters
{
    std::uint32_t bufferCount{10};     // Buffers affected by frame-drop and decode-error
    std::uint32_t delayFrames{10};     // Frame durations a buffer is held by frame-repeat
    std::uint32_t stallMs{5000};       // Time video buffers are blocked by buffer-underflow
    std::uint32_t windowSeconds{10};   // How long the probe stays installed after a fault is first applied
    std::uint32_t dropShiftMs{500};    // How far frame-drop moves buffer timestamps back
    DecodeErrorMode decodeErrorMode{DecodeErrorMode::Tail};
};

// State shared between the CLI and a scenario handler running on the streaming thread.
class FaultContext
{
public:
    // Returns the playback position in nanoseconds, or nullopt if it cannot be obtained.
    using PositionQuery = std::function<std::optional<gint64>()>;
    // Returns the number of video frames rendered so far, or nullopt if it cannot be obtained.
    using RenderedQuery = std::function<std::optional<std::uint64_t>()>;

    FaultContext(const FaultParameters &parameters, GstElement *pipeline, PositionQuery positionQuery = nullptr,
                 RenderedQuery renderedQuery = nullptr)
        : parameters{parameters}, pipeline{pipeline}, m_positionQuery{std::move(positionQuery)},
          m_renderedQuery{std::move(renderedQuery)}
    {
    }

    // The sink whose frame-counters property is read when no RenderedQuery was supplied.
    void setVideoSink(GstElement *sink) { m_videoSink = sink; }
    bool hasRenderedSource() const { return m_renderedQuery || m_videoSink; }

    // Called from the streaming thread only. Reads of the sink property are limited to one per 50 ms.
    std::optional<std::uint64_t> queryRenderedFrames()
    {
        if (m_renderedQuery)
        {
            return m_renderedQuery();
        }
        const gint64 now{g_get_monotonic_time()};
        if (m_lastRenderedQueryUs != 0 && now - m_lastRenderedQueryUs < 50000)
        {
            return m_cachedRendered;
        }
        GstStructure *counters{nullptr};
        g_object_get(m_videoSink, "frame-counters", &counters, nullptr);
        std::optional<std::uint64_t> rendered;
        guint64 value{0};
        if (counters && gst_structure_get_uint64(counters, "rendered", &value))
        {
            rendered = value;
        }
        if (counters)
        {
            gst_structure_free(counters);
        }
        m_lastRenderedQueryUs = now;
        m_cachedRendered = rendered;
        return rendered;
    }

    std::optional<gint64> queryPosition() const
    {
        if (m_positionQuery)
        {
            return m_positionQuery();
        }
        gint64 position{0};
        if (!pipeline || !gst_element_query_position(pipeline, GST_FORMAT_TIME, &position) || position < 0)
        {
            return std::nullopt;
        }
        return position;
    }

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

    // Set by frame-drop: how far buffer timestamps are moved back, in nanoseconds.
    std::optional<GstClockTime> ptsShift;

    // Set by frame-repeat: how long its buffer was held, in nanoseconds.
    std::optional<std::uint64_t> appliedDelay;

    // Set by buffer-underflow: how long video buffers were blocked, in milliseconds.
    std::optional<std::uint32_t> appliedStallMs;

    // Set by frame-repeat and buffer-underflow: queued lead added to the hold, in nanoseconds.
    std::optional<std::uint64_t> appliedLeadNs;

    // Incremented by decode-error: how many slice NAL units were corrupted.
    std::atomic<std::uint32_t> corruptedSlices{0};

    // Diagnostics for runs where a fault could not be applied.
    std::atomic<gint64> lastPositionNs{-1};     // Last playback position seen by a handler
    std::atomic<std::int64_t> lastRendered{-1};    // Last rendered frame count seen by a handler
    std::atomic<std::uint32_t> notSettledSkips{0}; // Buffers passed because playback had not settled
    std::atomic<std::uint32_t> noEligibleSlice{0}; // decode-error buffers with no corruptible slice

private:
    mutable std::mutex m_mutex;
    PositionQuery m_positionQuery;
    RenderedQuery m_renderedQuery;
    GstElement *m_videoSink{nullptr};
    gint64 m_lastRenderedQueryUs{0};
    std::optional<std::uint64_t> m_cachedRendered;
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
// Faults are applied once the sink has rendered this many frames, so there is a preceding frame on
// screen and the pipeline is demonstrably running.
constexpr std::uint64_t kSettleRenderedFrames{3};
constexpr gulong kStallSliceUs{50000};
constexpr GstClockTime kMaxLead{120 * GST_SECOND}; // Upper bound on the extra hold added for queued data

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

// Reports a failure on the context if the rendered frame count cannot be read.
inline PlaybackState checkPlaybackSettled(FaultContext &context, const char *scenario)
{
    bool settled{true};
    if (context.hasRenderedSource())
    {
        const std::optional<std::uint64_t> rendered{context.queryRenderedFrames()};
        if (!rendered)
        {
            context.fail(std::string{scenario} + ": failed to read the rendered frame count from frame-counters.");
            return PlaybackState::Failed;
        }
        context.lastRendered = static_cast<std::int64_t>(*rendered);
        settled = *rendered >= kSettleRenderedFrames;
    }
    if (!settled)
    {
        ++context.notSettledSkips;
        return PlaybackState::NotSettled;
    }
    return PlaybackState::Settled;
}

// frame-drop (late frames): moves the PTS (and DTS, where set) of consecutive video buffers back by
// a small fixed amount (dropShiftMs) relative to their neighbours. When such a buffer reaches the
// renderer its presentation time has already passed by about that amount, so it misses its deadline.
// The shift is relative, not measured against the playback position, because Rialto queues video well
// ahead of playback. No seek, flush or EOS is involved, since those are excluded from the drop
// definition.
inline GstPadProbeReturn frameDropHandler(GstPad *, GstPadProbeInfo *info, FaultContext &context,
                                          std::uint32_t, bool &applied)
{
    GstBuffer *buffer{GST_PAD_PROBE_INFO_BUFFER(info)};
    if (!buffer || !GST_BUFFER_PTS_IS_VALID(buffer))
    {
        return GST_PAD_PROBE_PASS;
    }

    switch (checkPlaybackSettled(context, "frame-drop"))
    {
    case PlaybackState::Failed:
    case PlaybackState::NotSettled:
        return GST_PAD_PROBE_PASS;
    case PlaybackState::Settled:
        break;
    }

    const GstClockTime shift{static_cast<GstClockTime>(context.parameters.dropShiftMs) * GST_MSECOND};
    context.ptsShift = shift;
    buffer = gst_buffer_make_writable(buffer);
    info->data = buffer;
    GST_BUFFER_PTS(buffer) = subtractClamped(GST_BUFFER_PTS(buffer), shift);
    if (GST_BUFFER_DTS_IS_VALID(buffer))
    {
        GST_BUFFER_DTS(buffer) = subtractClamped(GST_BUFFER_DTS(buffer), shift);
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

// How far ahead of the current playback position a buffer is. Rialto queues video well ahead of
// playback, so holding a buffer only starves the renderer once the hold exceeds this lead. Returns
// zero when the buffer has no timestamp; sets ok to false (and fails the context) if the position
// cannot be queried.
inline GstClockTime bufferLead(GstPad *pad, GstBuffer *buffer, FaultContext &context, const char *scenario, bool &ok)
{
    ok = true;
    if (!GST_BUFFER_PTS_IS_VALID(buffer))
    {
        return 0;
    }
    const std::optional<gint64> position{context.queryPosition()};
    if (!position)
    {
        context.fail(std::string{scenario} + ": failed to query the current playback position.");
        ok = false;
        return 0;
    }
    context.lastPositionNs = *position;

    guint64 streamTime{GST_BUFFER_PTS(buffer)};
    if (GstEvent *segmentEvent{gst_pad_get_sticky_event(pad, GST_EVENT_SEGMENT, 0)})
    {
        const GstSegment *eventSegment{nullptr};
        gst_event_parse_segment(segmentEvent, &eventSegment);
        if (eventSegment->format == GST_FORMAT_TIME)
        {
            const guint64 converted{gst_segment_to_stream_time(eventSegment, GST_FORMAT_TIME, GST_BUFFER_PTS(buffer))};
            if (converted != GST_CLOCK_TIME_NONE)
            {
                streamTime = converted;
            }
        }
        gst_event_unref(segmentEvent);
    }

    GstClockTime lead{streamTime > static_cast<guint64>(*position) ? streamTime - static_cast<guint64>(*position) : 0};
    lead = std::min(lead, kMaxLead);
    context.appliedLeadNs = lead;
    return lead;
}

// Holds the streaming thread, in slices so a cancelled run is not held up by a long hold.
inline void holdPad(const FaultContext &context, gint64 durationUs)
{
    const gint64 endUs{g_get_monotonic_time() + durationUs};
    while (!context.cancelled())
    {
        const gint64 remainingUs{endUs - g_get_monotonic_time()};
        if (remainingUs <= 0)
        {
            break;
        }
        g_usleep(static_cast<gulong>(std::min<gint64>(remainingUs, kStallSliceUs)));
    }
}

// frame-repeat (single delayed buffer): holds one video buffer so the renderer has no new frame at
// the expected time and repeats the preceding one. The hold is the lead (the time the queues keep the
// renderer fed) plus delayFrames frame durations, so the renderer misses new frames for about
// delayFrames frame durations. The requested delay must fit in the fault window.
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

    bool leadKnown{false};
    const GstClockTime lead{bufferLead(pad, buffer, context, "frame-repeat", leadKnown)};
    if (!leadKnown)
    {
        return GST_PAD_PROBE_PASS;
    }

    context.appliedDelay = delayNs;
    applied = true;
    holdPad(context, static_cast<gint64>((delayNs + lead) / 1000));
    return GST_PAD_PROBE_OK;
}

// buffer-underflow (decoder input starvation): blocks the video sink pad by holding the streaming
// thread inside the probe, so no video buffer reaches the sink. The hold is the lead (how far the
// queues run ahead of playback) plus stallMs, so the decoder starves for about stallMs once the
// queues drain; releasing the buffer lets input resume and the starvation end.
// Only the video pad is held, so audio keeps flowing. Independent of need-data flow control.
inline GstPadProbeReturn bufferUnderflowHandler(GstPad *pad, GstPadProbeInfo *info, FaultContext &context,
                                                std::uint32_t, bool &applied)
{
    GstBuffer *buffer{GST_PAD_PROBE_INFO_BUFFER(info)};
    if (!buffer)
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

    bool leadKnown{false};
    const GstClockTime lead{bufferLead(pad, buffer, context, "buffer-underflow", leadKnown)};
    if (!leadKnown)
    {
        return GST_PAD_PROBE_PASS;
    }

    context.appliedStallMs = context.parameters.stallMs;
    applied = true;
    holdPad(context, static_cast<gint64>(context.parameters.stallMs) * 1000 + static_cast<gint64>(lead / 1000));
    return GST_PAD_PROBE_OK;
}

// Selects the slice NAL units of a buffer that decode-error may corrupt. Parameter sets, SEI and other
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
constexpr std::size_t kSliceHeaderGuard{8}; // Bytes after the NAL header that are never corrupted

inline bool getCodecInfo(GstPad *pad, CodecInfo &codec, std::string &error)
{
    GstCaps *caps{gst_pad_get_current_caps(pad)};
    if (!caps || gst_caps_get_size(caps) == 0)
    {
        if (caps)
        {
            gst_caps_unref(caps);
        }
        error = "decode-error: no caps negotiated on the video sink pad.";
        return false;
    }

    const GstStructure *structure{gst_caps_get_structure(caps, 0)};
    const std::string name{gst_structure_get_name(structure)};
    const gchar *streamFormat{gst_structure_get_string(structure, "stream-format")};
    const std::string format{streamFormat ? streamFormat : "byte-stream"};
    bool supported{true};

    if (name.rfind("application/x-", 0) == 0)
    {
        error = "decode-error: encrypted media (" + name + ") cannot be corrupted.";
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
        error = "decode-error: unsupported video codec '" + name + "' (supported: video/x-h264, video/x-h265).";
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

// Corrupts each eligible slice according to the mode, always leaving the NAL header intact.
// Tail (default) keeps the start of the slice header so the decoder reports the damage per frame;
// Payload and Header are harsher and may make the decoder drop the slice or lose sync instead.
// Returns the number of slices corrupted.
inline std::uint32_t corruptSlices(guint8 *data, std::size_t size, const CodecInfo &codec,
                                   DecodeErrorMode mode = DecodeErrorMode::Tail)
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
        const std::size_t payloadSize{slice.size - headerSize};
        std::size_t corruptFrom{payloadStart + std::max(kSliceHeaderGuard, payloadSize / 8)};
        std::size_t corruptEnd{slice.offset + slice.size};
        if (mode == DecodeErrorMode::Payload)
        {
            corruptFrom = payloadStart;
        }
        else if (mode == DecodeErrorMode::Header)
        {
            corruptFrom = payloadStart;
            corruptEnd = payloadStart + kSliceHeaderGuard;
        }
        for (std::size_t i = corruptFrom; i < corruptEnd; ++i)
        {
            data[i] ^= 0xFF;
        }
    }
    return static_cast<std::uint32_t>(slices.size());
}

// decode-error (frame-level corruption): corrupts the slice payload of bufferCount non-key video
// buffers, one affected frame per buffer. Fails for encrypted media and unsupported codecs.
inline GstPadProbeReturn decodeErrorHandler(GstPad *pad, GstPadProbeInfo *info, FaultContext &context,
                                             std::uint32_t, bool &applied)
{
    GstBuffer *buffer{GST_PAD_PROBE_INFO_BUFFER(info)};
    if (!buffer)
    {
        return GST_PAD_PROBE_PASS;
    }

    switch (checkPlaybackSettled(context, "decode-error"))
    {
    case PlaybackState::Failed:
    case PlaybackState::NotSettled:
        return GST_PAD_PROBE_PASS;
    case PlaybackState::Settled:
        break;
    }

    if (gst_buffer_get_protection_meta(buffer))
    {
        context.fail("decode-error: encrypted media (protection metadata) cannot be corrupted.");
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
        context.fail("decode-error: the video buffer cannot be mapped writable.");
        return GST_PAD_PROBE_PASS;
    }
    const std::uint32_t corrupted{corruptSlices(map.data, map.size, codec, context.parameters.decodeErrorMode)};
    gst_buffer_unmap(buffer, &map);

    if (corrupted == 0)
    {
        ++context.noEligibleSlice;
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
        {FaultScenario::DecodeError, "decode-error", decodeErrorHandler, bufferCountLimit},
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
// limit is reached or the time window (counted from the first affected buffer) has passed, and is
// always removed by stop().
class FaultInjector
{
public:
    FaultInjector(const FaultScenarioInfo &info, const FaultParameters &parameters, GstElement *pipeline,
                  FaultContext::PositionQuery positionQuery = nullptr,
                  FaultContext::RenderedQuery renderedQuery = nullptr)
        : m_info{info}, m_context{parameters, pipeline, std::move(positionQuery), std::move(renderedQuery)}
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
        m_deadlineUs = 0; // The window starts when the first buffer is affected
        m_context.setVideoSink(sink);

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

    // One-line summary of what was injected. Call after stop(), once the streaming thread can no
    // longer update the values.
    std::string summary() const
    {
        const FaultParameters &parameters{m_context.parameters};
        std::string text{std::string{"scenario="} + m_info.name +
                         ", buffers-affected=" + std::to_string(m_seen.load())};
        switch (m_info.scenario)
        {
        case FaultScenario::FrameDrop:
            text += ", buffers-requested=" + std::to_string(parameters.bufferCount);
            text += ", pts-shift-ns=" + (m_context.ptsShift ? std::to_string(*m_context.ptsShift) : "none");
            break;
        case FaultScenario::FrameRepeat:
            text += ", delay-frames=" + std::to_string(parameters.delayFrames);
            text += ", delay-ns=" + (m_context.appliedDelay ? std::to_string(*m_context.appliedDelay) : "none");
            break;
        case FaultScenario::BufferUnderflow:
            text += ", stall-ms-requested=" + std::to_string(parameters.stallMs);
            text += ", stall-ms-applied=" +
                    (m_context.appliedStallMs ? std::to_string(*m_context.appliedStallMs) : "none");
            break;
        case FaultScenario::DecodeError:
            text += ", buffers-requested=" + std::to_string(parameters.bufferCount);
            text += ", slices-corrupted=" + std::to_string(m_context.corruptedSlices.load());
            text += std::string{", decode-error-mode="} + decodeErrorModeName(parameters.decodeErrorMode);
            break;
        }
        text += ", window-seconds=" + std::to_string(parameters.windowSeconds);
        if (m_context.appliedLeadNs)
        {
            text += ", lead-ms=" + std::to_string(*m_context.appliedLeadNs / GST_MSECOND);
        }
        text += ", buffers-probed=" + std::to_string(m_probed.load());
        if (m_probed.load() > 0)
        {
            text += ", probed-pts-ms=" + std::to_string(m_firstPtsMs.load()) + ".." + std::to_string(m_lastPtsMs.load());
        }
        text += ", last-rendered=" +
                (m_context.lastRendered.load() >= 0 ? std::to_string(m_context.lastRendered.load())
                                                    : std::string{"unknown"});
        text += ", last-position-ms=" +
                (m_context.lastPositionNs.load() >= 0 ? std::to_string(m_context.lastPositionNs.load() / GST_MSECOND)
                                                      : std::string{"unknown"});
        text += ", not-settled-skips=" + std::to_string(m_context.notSettledSkips.load());
        if (m_info.scenario == FaultScenario::DecodeError)
        {
            text += ", no-eligible-slice=" + std::to_string(m_context.noEligibleSlice.load());
        }
        return text;
    }

private:
    static GstPadProbeReturn probeCallback(GstPad *pad, GstPadProbeInfo *info, gpointer userData)
    {
        return static_cast<FaultInjector *>(userData)->onBuffer(pad, info);
    }

    GstPadProbeReturn onBuffer(GstPad *pad, GstPadProbeInfo *info)
    {
        if (GstBuffer *probed{GST_PAD_PROBE_INFO_BUFFER(info)}; probed && GST_BUFFER_PTS_IS_VALID(probed))
        {
            const gint64 ptsMs{static_cast<gint64>(GST_BUFFER_PTS(probed) / GST_MSECOND)};
            if (m_probed.fetch_add(1) == 0)
            {
                m_firstPtsMs = ptsMs;
            }
            m_lastPtsMs = ptsMs;
        }
        else
        {
            ++m_probed;
        }
        const gint64 deadlineUs{m_deadlineUs.load()};
        if (m_context.failed() || m_seen.load() >= m_limit || (deadlineUs != 0 && g_get_monotonic_time() >= deadlineUs))
        {
            // Only the side that clears the id may remove the probe, avoiding a double removal.
            return m_probeId.exchange(0) != 0 ? GST_PAD_PROBE_REMOVE : GST_PAD_PROBE_PASS;
        }
        bool applied{false};
        const GstPadProbeReturn result{m_info.handler(pad, info, m_context, m_seen.load(), applied)};
        if (applied)
        {
            ++m_seen;
            gint64 notStarted{0};
            m_deadlineUs.compare_exchange_strong(
                notStarted, g_get_monotonic_time() +
                                static_cast<gint64>(m_context.parameters.windowSeconds) * G_USEC_PER_SEC);
        }
        return result;
    }

    const FaultScenarioInfo &m_info;
    FaultContext m_context;
    GstPad *m_pad{nullptr};
    std::atomic<gulong> m_probeId{0};
    std::atomic<std::uint32_t> m_seen{0};
    std::atomic<std::uint32_t> m_probed{0};
    std::atomic<gint64> m_firstPtsMs{0};
    std::atomic<gint64> m_lastPtsMs{0};
    std::uint32_t m_limit{0};
    std::atomic<gint64> m_deadlineUs{0};
};
} // namespace metricscli
