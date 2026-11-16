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

#include <string>

namespace metricscli
{
struct FrameCounters
{
    guint64 rendered{0};
    guint64 dropped{0};
    guint64 corrupted{0};
    guint64 timestamp{0};
};

inline bool parseFrameCounters(const GstStructure *structure, FrameCounters &counters, std::string &error)
{
    if (!structure)
    {
        error = "Failed to query frame-counters from rialtomsevideosink.";
        return false;
    }

    if (g_strcmp0(gst_structure_get_name(structure), "frame-counters") != 0)
    {
        error = "Invalid frame-counters structure name.";
        return false;
    }

    constexpr const char *requiredFields[]{"rendered", "dropped", "corrupted", "timestamp"};
    for (const char *field : requiredFields)
    {
        if (gst_structure_get_field_type(structure, field) != G_TYPE_UINT64)
        {
            error = std::string{"Invalid or missing frame-counters field '"} + field + "' (expected G_TYPE_UINT64).";
            return false;
        }
    }

    FrameCounters parsed;
    if (!gst_structure_get_uint64(structure, "rendered", &parsed.rendered) ||
        !gst_structure_get_uint64(structure, "dropped", &parsed.dropped) ||
        !gst_structure_get_uint64(structure, "corrupted", &parsed.corrupted) ||
        !gst_structure_get_uint64(structure, "timestamp", &parsed.timestamp))
    {
        error = "Failed to read the required values from frame-counters.";
        return false;
    }

    counters = parsed;
    error.clear();
    return true;
}
} // namespace metricscli
