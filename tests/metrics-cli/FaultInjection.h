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

// Called on the streaming thread for each buffer inside the fault window. bufferIndex is the
// zero-based count of buffers already handed to the handler since the probe was installed.
using FaultHandler = GstPadProbeReturn (*)(GstPadProbeInfo *info, const FaultParameters &parameters,
                                           std::uint32_t bufferIndex);
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
        {FaultScenario::FrameDrop, "frame-drop", nullptr, bufferCountLimit},
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
    FaultInjector(const FaultScenarioInfo &info, const FaultParameters &parameters)
        : m_info{info}, m_parameters{parameters}
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

        m_limit = m_info.bufferLimit ? m_info.bufferLimit(m_parameters) : m_parameters.bufferCount;
        m_seen = 0;
        m_deadlineUs = g_get_monotonic_time() + static_cast<gint64>(m_parameters.windowSeconds) * G_USEC_PER_SEC;

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
    const FaultParameters &parameters() const { return m_parameters; }

private:
    static GstPadProbeReturn probeCallback(GstPad *, GstPadProbeInfo *info, gpointer userData)
    {
        return static_cast<FaultInjector *>(userData)->onBuffer(info);
    }

    GstPadProbeReturn onBuffer(GstPadProbeInfo *info)
    {
        if (m_seen.load() >= m_limit || g_get_monotonic_time() >= m_deadlineUs)
        {
            // Only the side that clears the id may remove the probe, avoiding a double removal.
            return m_probeId.exchange(0) != 0 ? GST_PAD_PROBE_REMOVE : GST_PAD_PROBE_PASS;
        }
        return m_info.handler(info, m_parameters, m_seen.fetch_add(1));
    }

    const FaultScenarioInfo &m_info;
    FaultParameters m_parameters;
    GstPad *m_pad{nullptr};
    std::atomic<gulong> m_probeId{0};
    std::atomic<std::uint32_t> m_seen{0};
    std::uint32_t m_limit{0};
    gint64 m_deadlineUs{0};
};
} // namespace metricscli
