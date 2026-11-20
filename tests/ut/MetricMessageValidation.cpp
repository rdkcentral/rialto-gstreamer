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

#include "MetricMessageValidation.h"

#include "MediaCommon.h"

#include <algorithm>
#include <array>

bool validateMediaPipelineMetricMessage(const GstStructure *structure)
{
    if (structure == nullptr)
    {
        return false;
    }

    if (g_strcmp0(gst_structure_get_name(structure), "media-pipeline-metric") != 0)
    {
        return false;
    }

    const std::array<const char *, 3> requiredFields = {"metric", "observation-kind", "source_id"};
    const std::array<GType, 3> requiredTypes = {G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT};

    for (size_t i = 0; i < requiredFields.size(); ++i)
    {
        if (!gst_structure_has_field(structure, requiredFields[i]))
        {
            return false;
        }
        if (gst_structure_get_field_type(structure, requiredFields[i]) != requiredTypes[i])
        {
            return false;
        }
    }

    if (gst_structure_has_field(structure, "session_id") &&
        gst_structure_get_field_type(structure, "session_id") != G_TYPE_UINT)
    {
        return false;
    }

    const guint metric = g_value_get_uint(gst_structure_get_value(structure, "metric"));
    const std::array<guint, 7> validMetrics = {static_cast<guint>(firebolt::rialto::MetricType::VIDEO_FRAME_DROP),
                                               static_cast<guint>(firebolt::rialto::MetricType::AUDIO_FRAME_DROP),
                                               static_cast<guint>(firebolt::rialto::MetricType::VIDEO_DECODE_ERROR),
                                               static_cast<guint>(firebolt::rialto::MetricType::AUDIO_DECODE_ERROR),
                                               static_cast<guint>(firebolt::rialto::MetricType::VIDEO_FRAME_REPEAT),
                                               static_cast<guint>(firebolt::rialto::MetricType::AUDIO_GAP),
                                               static_cast<guint>(firebolt::rialto::MetricType::BUFFER_UNDERFLOW)};
    if (std::find(validMetrics.begin(), validMetrics.end(), metric) == validMetrics.end())
    {
        return false;
    }

    const guint observationKind = g_value_get_uint(gst_structure_get_value(structure, "observation-kind"));
    const std::array<guint, 3> validObservationKinds = {
        static_cast<guint>(firebolt::rialto::MetricPhase::OCCURRENCE),
        static_cast<guint>(firebolt::rialto::MetricPhase::EPISODE_STARTED),
        static_cast<guint>(firebolt::rialto::MetricPhase::EPISODE_RESOLVED)};
    if (std::find(validObservationKinds.begin(), validObservationKinds.end(), observationKind) ==
        validObservationKinds.end())
    {
        return false;
    }

    const guint sourceId = g_value_get_uint(gst_structure_get_value(structure, "source_id"));
    if (sourceId == 0)
    {
        return false;
    }

    const std::array<const char *, 3> optionalFields = {"pts-ns", "duration-ns", "count"};
    const std::array<GType, 3> optionalTypes = {G_TYPE_INT64, G_TYPE_UINT64, G_TYPE_UINT64};

    for (size_t i = 0; i < optionalFields.size(); ++i)
    {
        if (gst_structure_has_field(structure, optionalFields[i]) &&
            gst_structure_get_field_type(structure, optionalFields[i]) != optionalTypes[i])
        {
            return false;
        }
    }

    const bool isVideoFrameRepeat = (metric == static_cast<guint>(firebolt::rialto::MetricType::VIDEO_FRAME_REPEAT));
    const bool isAudioGap = (metric == static_cast<guint>(firebolt::rialto::MetricType::AUDIO_GAP));
    const bool isBufferUnderflow = (metric == static_cast<guint>(firebolt::rialto::MetricType::BUFFER_UNDERFLOW));
    const bool isOccurrence = (observationKind == static_cast<guint>(firebolt::rialto::MetricPhase::OCCURRENCE));
    const bool isEpisodeStarted =
        (observationKind == static_cast<guint>(firebolt::rialto::MetricPhase::EPISODE_STARTED));
    const bool isEpisodeResolved =
        (observationKind == static_cast<guint>(firebolt::rialto::MetricPhase::EPISODE_RESOLVED));

    const bool hasCount = gst_structure_has_field(structure, "count");
    const bool hasDurationNs = gst_structure_has_field(structure, "duration-ns");
    const bool isEpisodeMetric = isVideoFrameRepeat || isAudioGap || isBufferUnderflow;

    if ((isEpisodeMetric && isOccurrence) || (!isEpisodeMetric && !isOccurrence))
    {
        return false;
    }

    if (isOccurrence && (hasCount || hasDurationNs))
    {
        return false;
    }

    if (isEpisodeStarted && (hasCount || hasDurationNs))
    {
        return false;
    }

    if (isEpisodeResolved)
    {
        if (!hasDurationNs)
        {
            return false;
        }
        if (isBufferUnderflow && hasCount)
        {
            return false;
        }
        if ((isVideoFrameRepeat || isAudioGap) && !hasCount)
        {
            return false;
        }
    }

    return true;
}
