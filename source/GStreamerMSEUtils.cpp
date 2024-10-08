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

#if 0
// The x-raw capability is disabled until a workaround is found for the following...
//
// Returning an x-raw capability (which would work) has the side-effect of
// breaking the WebAudio YT cert test.
// When WebAudio in cobalt creates the audio sink, it uses autoaudiosink which is
// selecting the MSE sink instead of webaudio (because it supports x-raw)
//
// When this fix is found, please remove this MACRO from the code
#define RIALTO_ENABLE_X_RAW
#endif

void rialto_mse_sink_setup_supported_caps(GstElementClass *elementClass,
                                          const std::vector<std::string> &supportedMimeTypes)
{
    static const std::unordered_map<std::string, std::vector<std::string>> kMimeToCaps =
        {{"audio/mp4", {"audio/mpeg, mpegversion=1", "audio/mpeg, mpegversion=2", "audio/mpeg, mpegversion=4"}},
         {"audio/aac", {"audio/mpeg, mpegversion=2", "audio/mpeg, mpegversion=4"}},
         {"audio/x-eac3", {"audio/x-ac3", "audio/x-eac3"}},
         {"audio/x-opus", {"audio/x-opus"}},
         {"audio/b-wav", {"audio/b-wav"}},
#ifdef RIALTO_ENABLE_X_RAW
         {"audio/x-raw", {"audio/x-raw"}},
#endif
         {"video/h264", {"video/x-h264"}},
         {"video/h265", {"video/x-h265"}},
         {"video/x-av1", {"video/x-av1"}},
         {"video/x-vp9", {"video/x-vp9"}},
         {"text/vtt", {"text/vtt", "application/x-subtitle-vtt"}},
         {"text/ttml", {"application/ttml+xml"}}};

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
    static const std::unordered_map<std::string, firebolt::rialto::Format>
        kStringToFormat{{"S8", firebolt::rialto::Format::S8},
                        {"U8", firebolt::rialto::Format::U8},
                        {"S16LE", firebolt::rialto::Format::S16LE},
                        {"S16BE", firebolt::rialto::Format::S16BE},
                        {"U16LE", firebolt::rialto::Format::U16LE},
                        {"U16BE", firebolt::rialto::Format::U16BE},
                        {"S24_32LE", firebolt::rialto::Format::S24_32LE},
                        {"S24_32BE", firebolt::rialto::Format::S24_32BE},
                        {"U24_32LE", firebolt::rialto::Format::U24_32LE},
                        {"U24_32BE", firebolt::rialto::Format::U24_32BE},
                        {"S32LE", firebolt::rialto::Format::S32LE},
                        {"S32BE", firebolt::rialto::Format::S32BE},
                        {"U32LE", firebolt::rialto::Format::U32LE},
                        {"U32BE", firebolt::rialto::Format::U32BE},
                        {"S24LE", firebolt::rialto::Format::S24LE},
                        {"S24BE", firebolt::rialto::Format::S24BE},
                        {"U24LE", firebolt::rialto::Format::U24LE},
                        {"U24BE", firebolt::rialto::Format::U24BE},
                        {"S20LE", firebolt::rialto::Format::S20LE},
                        {"S20BE", firebolt::rialto::Format::S20BE},
                        {"U20LE", firebolt::rialto::Format::U20LE},
                        {"U20BE", firebolt::rialto::Format::U20BE},
                        {"S18LE", firebolt::rialto::Format::S18LE},
                        {"S18BE", firebolt::rialto::Format::S18BE},
                        {"U18LE", firebolt::rialto::Format::U18LE},
                        {"U18BE", firebolt::rialto::Format::U18BE},
                        {"F32LE", firebolt::rialto::Format::F32LE},
                        {"F32BE", firebolt::rialto::Format::F32BE},
                        {"F64LE", firebolt::rialto::Format::F64LE},
                        {"F64BE", firebolt::rialto::Format::F64BE}};
    const auto it = kStringToFormat.find(formatStr);
    if (it != kStringToFormat.end())
    {
        return it->second;
    }
    return std::nullopt;
}
