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

#include "RialtoGStreamerMSEAudioSink.h"
#include "GStreamerEMEUtils.h"
#include "GStreamerMSEUtils.h"
#include "RialtoGStreamerMSEBaseSinkPrivate.h"
#include <IMediaPipelineCapabilities.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <inttypes.h>
#include <stdint.h>

using namespace firebolt::rialto::client;

GST_DEBUG_CATEGORY_STATIC(RialtoMSEAudioSinkDebug);
#define GST_CAT_DEFAULT RialtoMSEAudioSinkDebug

#define rialto_mse_audio_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE(RialtoMSEAudioSink, rialto_mse_audio_sink, RIALTO_TYPE_MSE_BASE_SINK,
                        GST_DEBUG_CATEGORY_INIT(RialtoMSEAudioSinkDebug, "rialtomseaudiosink", 0,
                                                "rialto mse audio sink"));

static GstStateChangeReturn rialto_mse_audio_sink_change_state(GstElement *element, GstStateChange transition)
{
    RialtoMSEBaseSink *sink = RIALTO_MSE_BASE_SINK(element);
    RialtoMSEBaseSinkPrivate *priv = sink->priv;

    switch (transition)
    {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    {
        // Attach the media player client to media player manager
        GstObject *parentObject = rialto_mse_base_get_oldest_gst_bin_parent(element);
        if (!priv->m_mediaPlayerManager.attachMediaPlayerClient(parentObject))
        {
            GST_ERROR_OBJECT(sink, "Cannot attach the MediaPlayerClient");
            return GST_STATE_CHANGE_FAILURE;
        }
        GST_INFO_OBJECT(element, "Attached media player client with parent %s(%p)", gst_object_get_name(parentObject), parentObject);

        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = priv->m_mediaPlayerManager.getMediaPlayerClient();
        firebolt::rialto::IMediaPipeline::MediaSource vsource(-1, firebolt::rialto::MediaSourceType::AUDIO, "");
        if ((!client) || (!client->attachSource(vsource, sink)))
        {
            GST_ERROR_OBJECT(sink, "Failed to attach audio source");
            return GST_STATE_CHANGE_FAILURE;
        }
        break;
    }
    default:
        break;
    }

    GstStateChangeReturn result = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
    if (G_UNLIKELY(result == GST_STATE_CHANGE_FAILURE))
    {
        GST_WARNING_OBJECT(sink, "State change failed");
        return result;
    }

    return result;
}

static firebolt::rialto::IMediaPipeline::MediaSource rialto_mse_audio_sink_create_media_source(RialtoMSEBaseSink *sink,
                                                                                               GstCaps *caps)
{
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    const gchar *strct_name = gst_structure_get_name(structure);

    firebolt::rialto::SegmentAlignment alignment = rialto_mse_base_sink_get_segment_alignment(sink, structure);
    std::vector<uint8_t> codecData = rialto_mse_base_sink_get_codec_data(sink, structure);
    firebolt::rialto::StreamFormat format = rialto_mse_base_sink_get_stream_format(sink, structure);

    if (strct_name)
    {
        if (g_str_has_prefix(strct_name, "audio/mpeg") || g_str_has_prefix(strct_name, "audio/x-eac3") ||
            g_str_has_prefix(strct_name, "audio/x-ac3"))
        {
            gint sample_rate = 0;
            gint number_of_channels = 0;
            gst_structure_get_int(structure, "rate", &sample_rate);
            gst_structure_get_int(structure, "channels", &number_of_channels);

            firebolt::rialto::AudioConfig audioConfig{static_cast<uint32_t>(number_of_channels),
                                                      static_cast<uint32_t>(sample_rate),
                                                      {}};
            if (g_str_has_prefix(strct_name, "audio/mpeg"))
            {
                return firebolt::rialto::IMediaPipeline::MediaSource(-1, "audio/mp4", audioConfig, alignment, format,
                                                                     codecData);
            }
            else
            {
                return firebolt::rialto::IMediaPipeline::MediaSource(-1, "audio/x-eac3", audioConfig, alignment, format,
                                                                     codecData);
            }
        }
        else if (g_str_has_prefix(strct_name, "audio/x-opus"))
        {
            guint32 sample_rate = 48000;
            guint8 number_of_channels, streams, stereo_streams, channel_mapping_family;
            guint8 channel_mapping[256];
            GstBuffer *id_header;
            guint16 pre_skip = 0;
            gint16 gain = 0;
            if (gst_codec_utils_opus_parse_caps(caps, &sample_rate, &number_of_channels, &channel_mapping_family,
                                                &streams, &stereo_streams, channel_mapping))
            {
                id_header = gst_codec_utils_opus_create_header(sample_rate, number_of_channels, channel_mapping_family,
                                                               streams, stereo_streams, channel_mapping, pre_skip, gain);
                std::vector<uint8_t> codec_specific_config;
                GstMapInfo lsMap;
                if (gst_buffer_map(id_header, &lsMap, GST_MAP_READ))
                {
                    codec_specific_config.assign(lsMap.data, lsMap.data + lsMap.size);
                    gst_buffer_unmap(id_header, &lsMap);
                }
                else
                {
                    GST_ERROR_OBJECT(sink, "Failed to read opus header details from a GstBuffer!");
                }
                gst_buffer_unref(id_header);

                firebolt::rialto::AudioConfig audioConfig{number_of_channels, sample_rate, codec_specific_config};
                return firebolt::rialto::IMediaPipeline::MediaSource(-1, "audio/x-opus", audioConfig, alignment, format,
                                                                     codecData);
            }
            else
            {
                GST_ERROR("Failed to parse opus caps!");
            }
        }
        else
        {
            GST_INFO_OBJECT(sink, "%s audio media source created", strct_name);
            return firebolt::rialto::IMediaPipeline::MediaSource(-1, firebolt::rialto::MediaSourceType::AUDIO,
                                                                 strct_name, alignment, format, codecData);
        }
    }
    GST_ERROR_OBJECT(sink, "Empty caps' structure name! Failed to set mime type for audio media source.");
    return firebolt::rialto::IMediaPipeline::MediaSource(-1, firebolt::rialto::MediaSourceType::AUDIO, "", alignment);
}

static gboolean rialto_mse_audio_sink_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
    RialtoMSEBaseSink *sink = RIALTO_MSE_BASE_SINK(parent);
    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_CAPS:
    {
        GstCaps *caps;
        gst_event_parse_caps(event, &caps);
        gchar *capsStr = gst_caps_to_string(caps);

        GST_INFO_OBJECT(sink, "Attaching AUDIO source with caps %s", capsStr);
        g_free(capsStr);
        firebolt::rialto::IMediaPipeline::MediaSource asource = rialto_mse_audio_sink_create_media_source(sink, caps);

        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = sink->priv->m_mediaPlayerManager.getMediaPlayerClient();
        if ((!client) || (!client->attachSource(asource, sink)))
        {
            GST_ERROR_OBJECT(sink, "Failed to attach AUDIO source");
        }
        break;
    }
    default:
        break;
    }

    return rialto_mse_base_sink_event(pad, parent, event);
}

static void rialto_mse_audio_sink_qos_handle(GstElement *element, uint64_t processed, uint64_t dropped)
{
    GstBus *bus = gst_element_get_bus(element);
    /* Hardcode isLive to FALSE and set invalid timestamps */
    GstMessage *message = gst_message_new_qos(GST_OBJECT(element), FALSE, GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE,
                                              GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE);
    gst_message_set_qos_stats(message, GST_FORMAT_DEFAULT, processed, dropped);
    gst_bus_post(bus, message);
}

static void rialto_mse_audio_sink_init(RialtoMSEAudioSink *sink)
{
    RialtoMSEBaseSinkPrivate *priv = sink->parent.priv;

    if (!rialto_mse_base_sink_initialise_sinkpad(RIALTO_MSE_BASE_SINK(sink)))
    {
        GST_ERROR_OBJECT(sink, "Failed to initialise AUDIO sink. Sink pad initialisation failed.");
        return;
    }

    gst_pad_set_chain_function(priv->mSinkPad, rialto_mse_base_sink_chain);
    gst_pad_set_event_function(priv->mSinkPad, rialto_mse_audio_sink_event);

    priv->mCallbacks.qosCallback = std::bind(rialto_mse_audio_sink_qos_handle, GST_ELEMENT_CAST(sink),
                                             std::placeholders::_1, std::placeholders::_2);
}

static void rialto_mse_audio_sink_class_init(RialtoMSEAudioSinkClass *klass)
{
    GstElementClass *elementClass = GST_ELEMENT_CLASS(klass);
    elementClass->change_state = rialto_mse_audio_sink_change_state;

    std::unique_ptr<firebolt::rialto::IMediaPipelineCapabilities> mediaPlayerCapabilities =
        firebolt::rialto::IMediaPipelineCapabilitiesFactory::createFactory()->createMediaPipelineCapabilities();
    if (mediaPlayerCapabilities)
    {
        std::vector<std::string> supportedMimeTypes =
            mediaPlayerCapabilities->getSupportedMimeTypes(firebolt::rialto::MediaSourceType::AUDIO);

        rialto_mse_sink_setup_supported_caps(elementClass, supportedMimeTypes);
    }
    else
    {
        GST_ERROR("Failed to get supported mime types for AUDIO");
    }

    gst_element_class_set_details_simple(elementClass, "Rialto Audio Sink", "Decoder/Audio/Sink/Audio",
                                         "Communicates with Rialto Server", "Sky");
}
