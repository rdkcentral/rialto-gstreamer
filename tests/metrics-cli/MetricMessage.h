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

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

namespace metricscli
{
constexpr guint kObservationOccurrence{1};
constexpr guint kObservationEpisodeStarted{2};
constexpr guint kObservationEpisodeResolved{3};
constexpr const char *kMetricMessageName{"media-pipeline-metric"};

struct MetricObservation
{
    guint metric{0}; // Opaque: the numeric category is deliberately not interpreted or validated
    guint observationKind{0};
    guint sourceId{0};
    std::optional<guint> sessionId;
    std::optional<gint64> ptsNs;
    std::optional<guint64> durationNs;
    std::optional<guint64> count;
};

inline bool isMetricMessage(const GstStructure *structure)
{
    return structure && g_strcmp0(gst_structure_get_name(structure), kMetricMessageName) == 0;
}

inline const char *observationKindName(guint kind)
{
    switch (kind)
    {
    case kObservationOccurrence:
        return "occurrence";
    case kObservationEpisodeStarted:
        return "episode-started";
    case kObservationEpisodeResolved:
        return "episode-resolved";
    default:
        return "invalid";
    }
}

// Validates the client-side envelope of a media-pipeline-metric structure. Required: metric,
// observation-kind and source_id as G_TYPE_UINT, with observation-kind 1..3. Optional: session_id
// (G_TYPE_UINT), pts-ns (G_TYPE_INT64), duration-ns and count (G_TYPE_UINT64). Occurrences must
// omit duration-ns and count. The metric category itself is not validated.
inline bool parseMetricMessage(const GstStructure *structure, MetricObservation &observation, std::string &error)
{
    if (!isMetricMessage(structure))
    {
        error = std::string{"Structure is not a "} + kMetricMessageName + " message.";
        return false;
    }

    constexpr const char *requiredFields[]{"metric", "observation-kind", "source_id"};
    for (const char *field : requiredFields)
    {
        if (!gst_structure_has_field(structure, field))
        {
            error = std::string{"Missing required field '"} + field + "'.";
            return false;
        }
        if (gst_structure_get_field_type(structure, field) != G_TYPE_UINT)
        {
            error = std::string{"Required field '"} + field + "' must be G_TYPE_UINT.";
            return false;
        }
    }

    struct OptionalField
    {
        const char *name;
        GType type;
        const char *typeName;
    };
    constexpr OptionalField optionalFields[]{{"session_id", G_TYPE_UINT, "G_TYPE_UINT"},
                                             {"pts-ns", G_TYPE_INT64, "G_TYPE_INT64"},
                                             {"duration-ns", G_TYPE_UINT64, "G_TYPE_UINT64"},
                                             {"count", G_TYPE_UINT64, "G_TYPE_UINT64"}};
    for (const OptionalField &field : optionalFields)
    {
        if (gst_structure_has_field(structure, field.name) &&
            gst_structure_get_field_type(structure, field.name) != field.type)
        {
            error = std::string{"Optional field '"} + field.name + "' must be " + field.typeName + ".";
            return false;
        }
    }

    MetricObservation parsed;
    gst_structure_get_uint(structure, "metric", &parsed.metric);
    gst_structure_get_uint(structure, "observation-kind", &parsed.observationKind);
    gst_structure_get_uint(structure, "source_id", &parsed.sourceId);

    if (parsed.observationKind < kObservationOccurrence || parsed.observationKind > kObservationEpisodeResolved)
    {
        error = "Invalid observation-kind " + std::to_string(parsed.observationKind) +
                " (expected 1 occurrence, 2 episode-started or 3 episode-resolved).";
        return false;
    }

    guint sessionId{0};
    if (gst_structure_get_uint(structure, "session_id", &sessionId))
    {
        parsed.sessionId = sessionId;
    }
    gint64 ptsNs{0};
    if (gst_structure_get_int64(structure, "pts-ns", &ptsNs))
    {
        parsed.ptsNs = ptsNs;
    }
    guint64 durationNs{0};
    if (gst_structure_get_uint64(structure, "duration-ns", &durationNs))
    {
        parsed.durationNs = durationNs;
    }
    guint64 count{0};
    if (gst_structure_get_uint64(structure, "count", &count))
    {
        parsed.count = count;
    }

    if (parsed.observationKind == kObservationOccurrence && (parsed.durationNs || parsed.count))
    {
        error = "Occurrence messages must omit duration-ns and count.";
        return false;
    }

    observation = parsed;
    error.clear();
    return true;
}

inline std::string describeMetricObservation(const MetricObservation &observation)
{
    std::string text{"metric=" + std::to_string(observation.metric) + " (category not validated)" +
                     ", observation-kind=" + observationKindName(observation.observationKind) + " (" +
                     std::to_string(observation.observationKind) + ")" +
                     ", source_id=" + std::to_string(observation.sourceId)};
    if (observation.sessionId)
    {
        text += ", session_id=" + std::to_string(*observation.sessionId);
    }
    if (observation.ptsNs)
    {
        text += ", pts-ns=" + std::to_string(*observation.ptsNs);
    }
    if (observation.durationNs)
    {
        text += ", duration-ns=" + std::to_string(*observation.durationNs);
    }
    if (observation.count)
    {
        text += ", count=" + std::to_string(*observation.count);
    }
    return text;
}

// Collects media-pipeline-metric application messages from the pipeline bus. Application messages
// with other structure names are not metric messages and are ignored.
class MetricObserver
{
public:
    void handle(GstMessage *message)
    {
        if (!message || GST_MESSAGE_TYPE(message) != GST_MESSAGE_APPLICATION)
        {
            return;
        }
        const GstStructure *structure{gst_message_get_structure(message)};
        if (!isMetricMessage(structure))
        {
            return;
        }

        MetricObservation observation;
        std::string error;
        if (parseMetricMessage(structure, observation, error))
        {
            ++m_valid;
            std::cout << "Metric message: " << describeMetricObservation(observation) << '\n';
            return;
        }

        ++m_invalid;
        gchar *text{gst_structure_to_string(structure)};
        std::cerr << "Invalid metric message: " << error << " (" << (text ? text : "") << ")\n";
        g_free(text);
    }

    std::uint32_t validCount() const { return m_valid; }
    std::uint32_t invalidCount() const { return m_invalid; }

private:
    std::uint32_t m_valid{0};
    std::uint32_t m_invalid{0};
};
} // namespace metricscli
