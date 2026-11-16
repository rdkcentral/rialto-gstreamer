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
    std::string failure() const
    {
        std::lock_guard<std::mutex> lock{m_mutex};
        return m_failure;
    }

    const FaultParameters parameters;
    GstElement *const pipeline;

    // Set by frame-drop on its first buffer: how far buffer timestamps are moved back, in nanoseconds.
    std::optional<GstClockTime> ptsShift;

private:
    mutable std::mutex m_mutex;
    std::atomic<bool> m_failed{false};
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

inline GstClockTime subtractClamped(GstClockTime value, GstClockTime amount)
{
    return value > amount ? value - amount : 0;
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
        {FaultScenario::FrameRepeat, "frame-repeat", nullptr, singleBufferLimit},
        {FaultScenario::BufferUnderflow, "buffer-underflow", nullptr, singleBufferLimit},
        {FaultScenario::DecoderError, "decoder-error", nullptr, bufferCountLimit},
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
