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
#include <unordered_map>
#include <unordered_set>

void rialto_mse_sink_setup_supported_caps(GstElementClass *elementClass,
                                          const std::vector<std::string> &supportedMimeTypes)
{
    static const std::unordered_map<std::string, std::vector<std::string>> kMimeToCaps =
        {{"audio/mp4", {"audio/mpeg, mpegversion=1", "audio/mpeg, mpegversion=2", "audio/mpeg, mpegversion=4"}},
         {"audio/aac", {"audio/mpeg, mpegversion=2", "audio/mpeg, mpegversion=4"}},
         {"audio/x-eac3", {"audio/x-ac3", "audio/x-eac3"}},
         {"audio/x-opus", {"audio/x-opus"}},
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
