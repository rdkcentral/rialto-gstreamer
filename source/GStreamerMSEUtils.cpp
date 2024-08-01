/*
 * Copyright (C) 2022 Sky UK
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

#include "GStreamerMSEUtils.h"
#include "GstreamerCatLog.h"
#include <unordered_map>
#include <unordered_set>

#define GST_CAT_DEFAULT rialtoGStreamerCat
void rialto_mse_sink_setup_supported_caps(GstElementClass *elementClass,
                                          const std::vector<std::string> &supportedMimeTypes)
{
    static const std::unordered_map<std::string, std::vector<std::string>> kMimeToCaps =
        {{"audio/mp4", {"audio/mpeg, mpegversion=1", "audio/mpeg, mpegversion=2", "audio/mpeg, mpegversion=4"}},
         {"audio/aac", {"audio/mpeg, mpegversion=2", "audio/mpeg, mpegversion=4"}},
         {"audio/x-eac3", {"audio/x-ac3", "audio/x-eac3"}},
         {"audio/x-opus", {"audio/x-opus"}},
         {"audio/b-wav", {"audio/b-wav"}},
         {"video/h264", {"video/x-h264"}},
         {"video/h265", {"video/x-h265"}},
         {"video/x-av1", {"video/x-av1"}},
         {"video/x-vp9", {"video/x-vp9"}}};

    std::unordered_set<std::string> addedCaps; // keep track what caps were added to avoid duplicates
    GstCaps *caps = gst_caps_new_empty();
    for (const std::string &mime : supportedMimeTypes)
    {
        auto mimeToCapsIt = kMimeToCaps.find(mime);
        if (mimeToCapsIt != kMimeToCaps.end())
        {
            for (const std::string &capsStr : mimeToCapsIt->second)
            {
                if (addedCaps.find(capsStr) == addedCaps.end())
                {
                    GST_INFO("Caps '%s' is supported", capsStr.c_str());
                    GstCaps *newCaps = gst_caps_from_string(capsStr.c_str());
                    gst_caps_append(caps, newCaps);
                    addedCaps.insert(capsStr);
                }
            }
        }
        else
        {
            GST_WARNING("Mime '%s' is not supported", mime.c_str());
        }
    }

    GstPadTemplate *sinktempl = gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps);
    gst_element_class_add_pad_template(elementClass, sinktempl);
    gst_caps_unref(caps);
}

std::optional<firebolt::rialto::Layout> rialto_mse_sink_convert_layout(const gchar *layoutStr)
{
    if (g_strcmp0(layoutStr, "interleaved") == 0)
    {
        return firebolt::rialto::Layout::INTERLEAVED;
    }
    if (g_strcmp0(layoutStr, "non-interleaved") == 0)
    {
        return firebolt::rialto::Layout::NON_INTERLEAVED;
    }
    return std::nullopt;
}

std::optional<firebolt::rialto::Format> rialto_mse_sink_convert_format(const gchar *formatStr)
{
    if (g_strcmp0(formatStr, "S8") == 0)
    {
        return firebolt::rialto::Format::S8;
    }
    if (g_strcmp0(formatStr, "U8") == 0)
    {
        return firebolt::rialto::Format::U8;
    }
    if (g_strcmp0(formatStr, "S16LE") == 0)
    {
        return firebolt::rialto::Format::S16LE;
    }
    if (g_strcmp0(formatStr, "S16BE") == 0)
    {
        return firebolt::rialto::Format::S16BE;
    }
    if (g_strcmp0(formatStr, "U16LE") == 0)
    {
        return firebolt::rialto::Format::U16LE;
    }
    if (g_strcmp0(formatStr, "U16BE") == 0)
    {
        return firebolt::rialto::Format::U16BE;
    }
    if (g_strcmp0(formatStr, "S24_32LE") == 0)
    {
        return firebolt::rialto::Format::S24_32LE;
    }
    if (g_strcmp0(formatStr, "S24_32BE") == 0)
    {
        return firebolt::rialto::Format::S24_32BE;
    }
    if (g_strcmp0(formatStr, "U24_32LE") == 0)
    {
        return firebolt::rialto::Format::U24_32LE;
    }
    if (g_strcmp0(formatStr, "U24_32BE") == 0)
    {
        return firebolt::rialto::Format::U24_32BE;
    }
    if (g_strcmp0(formatStr, "S32LE") == 0)
    {
        return firebolt::rialto::Format::S32LE;
    }
    if (g_strcmp0(formatStr, "S32BE") == 0)
    {
        return firebolt::rialto::Format::S32BE;
    }
    if (g_strcmp0(formatStr, "U32LE") == 0)
    {
        return firebolt::rialto::Format::U32LE;
    }
    if (g_strcmp0(formatStr, "U32BE") == 0)
    {
        return firebolt::rialto::Format::U32BE;
    }
    if (g_strcmp0(formatStr, "S24LE") == 0)
    {
        return firebolt::rialto::Format::S24LE;
    }
    if (g_strcmp0(formatStr, "S24BE") == 0)
    {
        return firebolt::rialto::Format::S24BE;
    }
    if (g_strcmp0(formatStr, "U24LE") == 0)
    {
        return firebolt::rialto::Format::U24LE;
    }
    if (g_strcmp0(formatStr, "U24BE") == 0)
    {
        return firebolt::rialto::Format::U24BE;
    }
    if (g_strcmp0(formatStr, "S20LE") == 0)
    {
        return firebolt::rialto::Format::S20LE;
    }
    if (g_strcmp0(formatStr, "S20BE") == 0)
    {
        return firebolt::rialto::Format::S20BE;
    }
    if (g_strcmp0(formatStr, "U20LE") == 0)
    {
        return firebolt::rialto::Format::U20LE;
    }
    if (g_strcmp0(formatStr, "U20BE") == 0)
    {
        return firebolt::rialto::Format::U20BE;
    }
    if (g_strcmp0(formatStr, "S18LE") == 0)
    {
        return firebolt::rialto::Format::S18LE;
    }
    if (g_strcmp0(formatStr, "S18BE") == 0)
    {
        return firebolt::rialto::Format::S18BE;
    }
    if (g_strcmp0(formatStr, "U18LE") == 0)
    {
        return firebolt::rialto::Format::U18LE;
    }
    if (g_strcmp0(formatStr, "U18BE") == 0)
    {
        return firebolt::rialto::Format::U18BE;
    }
    if (g_strcmp0(formatStr, "F32LE") == 0)
    {
        return firebolt::rialto::Format::F32LE;
    }
    if (g_strcmp0(formatStr, "F32BE") == 0)
    {
        return firebolt::rialto::Format::F32BE;
    }
    if (g_strcmp0(formatStr, "F64LE") == 0)
    {
        return firebolt::rialto::Format::F64LE;
    }
    if (g_strcmp0(formatStr, "F64BE") == 0)
    {
        return firebolt::rialto::Format::F64BE;
    }
    return std::nullopt;
}
