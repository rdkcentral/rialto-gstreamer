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
#include <gst/audio/audio.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <inttypes.h>
#include <stdint.h>

using namespace firebolt::rialto::client;

GST_DEBUG_CATEGORY_STATIC(RialtoMSEAudioSinkDebug);
#define GST_CAT_DEFAULT RialtoMSEAudioSinkDebug

#define rialto_mse_audio_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE(RialtoMSEAudioSink, rialto_mse_audio_sink, RIALTO_TYPE_MSE_BASE_SINK,
                        G_IMPLEMENT_INTERFACE(GST_TYPE_STREAM_VOLUME, NULL)
                            GST_DEBUG_CATEGORY_INIT(RialtoMSEAudioSinkDebug, "rialtomseaudiosink", 0,
                                                    "rialto mse audio sink"));

enum
{
    PROP_0,
    PROP_VOLUME,
    PROP_MUTE,
    PROP_LAST
};

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

        gchar *parentObjectName = gst_object_get_name(parentObject);
        GST_INFO_OBJECT(element, "Attached media player client with parent %s(%p)", parentObjectName, parentObject);
        g_free(parentObjectName);

        int32_t audioStreams = 0;
        bool isAudioOnly = false;

        gint n_video = 0;
        gint n_audio = 0;
        if (rialto_mse_base_sink_get_n_streams_from_parent(parentObject, n_video, n_audio))
        {
            audioStreams = n_audio;
            isAudioOnly = n_video == 0;
        }
        else
        {
            std::lock_guard<std::mutex> lock(priv->m_sinkMutex);
            audioStreams = priv->m_numOfStreams;
            isAudioOnly = priv->m_isSinglePathStream;
        }

        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = priv->m_mediaPlayerManager.getMediaPlayerClient();
        if (client)
        {
            client->setAudioStreamsInfo(audioStreams, isAudioOnly);
        }
        else
        {
            GST_ERROR_OBJECT(sink, "MediaPlayerClient is nullptr");
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

static std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource>
rialto_mse_audio_sink_create_media_source(RialtoMSEBaseSink *sink, GstCaps *caps)
{
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    const gchar *strct_name = gst_structure_get_name(structure);

    firebolt::rialto::AudioConfig audioConfig;
    firebolt::rialto::SegmentAlignment alignment = rialto_mse_base_sink_get_segment_alignment(sink, structure);
    std::shared_ptr<firebolt::rialto::CodecData> codecData = rialto_mse_base_sink_get_codec_data(sink, structure);
    firebolt::rialto::StreamFormat format = rialto_mse_base_sink_get_stream_format(sink, structure);
    std::string mimeType;

    if (strct_name)
    {
        if (g_str_has_prefix(strct_name, "audio/mpeg") || g_str_has_prefix(strct_name, "audio/x-eac3") ||
            g_str_has_prefix(strct_name, "audio/x-ac3"))
        {
            gint sample_rate = 0;
            gint number_of_channels = 0;
            gst_structure_get_int(structure, "rate", &sample_rate);
            gst_structure_get_int(structure, "channels", &number_of_channels);

            audioConfig = firebolt::rialto::AudioConfig{static_cast<uint32_t>(number_of_channels),
                                                        static_cast<uint32_t>(sample_rate),
                                                        {}};

            if (g_str_has_prefix(strct_name, "audio/mpeg"))
            {
                mimeType = "audio/mp4";
            }
            else
            {
                mimeType = "audio/x-eac3";
            }
        }
        else if (g_str_has_prefix(strct_name, "audio/x-opus"))
        {
            mimeType = "audio/x-opus";
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

                audioConfig = firebolt::rialto::AudioConfig{number_of_channels, sample_rate, codec_specific_config};
            }
            else
            {
                GST_ERROR("Failed to parse opus caps!");
                return nullptr;
            }
        }
        else
        {
            GST_INFO_OBJECT(sink, "%s audio media source created", strct_name);
            mimeType = strct_name;
        }

        return std::make_unique<firebolt::rialto::IMediaPipeline::MediaSourceAudio>(mimeType, sink->priv->m_hasDrm,
                                                                                    audioConfig, alignment, format,
                                                                                    codecData);
    }

    GST_ERROR_OBJECT(sink, "Empty caps' structure name! Failed to set mime type for audio media source.");
    return nullptr;
}

static gboolean rialto_mse_audio_sink_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
    RialtoMSEBaseSink *sink = RIALTO_MSE_BASE_SINK(parent);
    RialtoMSEBaseSinkPrivate *basePriv = sink->priv;
    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_CAPS:
    {
        GstCaps *caps;
        gst_event_parse_caps(event, &caps);
        if (basePriv->m_sourceAttached)
        {
            GST_INFO_OBJECT(sink, "Source already attached. Skip calling attachSource");
            break;
        }

        GST_INFO_OBJECT(sink, "Attaching AUDIO source with caps %" GST_PTR_FORMAT, caps);

        std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> asource =
            rialto_mse_audio_sink_create_media_source(sink, caps);
        if (asource)
        {
            std::shared_ptr<GStreamerMSEMediaPlayerClient> client =
                sink->priv->m_mediaPlayerManager.getMediaPlayerClient();
            if ((!client) || (!client->attachSource(asource, sink)))
            {
                GST_ERROR_OBJECT(sink, "Failed to attach AUDIO source");
            }
            else
            {
                basePriv->m_sourceAttached = true;
            }
        }
        else
        {
            GST_ERROR_OBJECT(sink, "Failed to create AUDIO source");
        }
        break;
    }
    default:
        break;
    }

    return rialto_mse_base_sink_event(pad, parent, event);
}

static void rialto_mse_audio_sink_get_property(GObject *object, guint propId, GValue *value, GParamSpec *pspec)
{
    RialtoMSEAudioSink *sink = RIALTO_MSE_AUDIO_SINK(object);
    RialtoMSEBaseSinkPrivate *basePriv = sink->parent.priv;
    if (!sink || !basePriv)
    {
        GST_ERROR_OBJECT(object, "Sink not initalised");
        return;
    }

    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = basePriv->m_mediaPlayerManager.getMediaPlayerClient();

    switch (propId)
    {
    case PROP_VOLUME:
    {
        if (!client)
        {
            GST_WARNING_OBJECT(object, "missing media player client");
            return;
        }
        g_value_set_double(value, client->getVolume());
        break;
    }
    case PROP_MUTE:
    {
        if (!client)
        {
            GST_WARNING_OBJECT(object, "missing media player client");
            return;
        }
        g_value_set_boolean(value, client->getMute());
        break;
    }
    default:
    {
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
    }
}

static void rialto_mse_audio_sink_set_property(GObject *object, guint propId, const GValue *value, GParamSpec *pspec)
{
    RialtoMSEAudioSink *sink = RIALTO_MSE_AUDIO_SINK(object);
    RialtoMSEBaseSinkPrivate *basePriv = sink->parent.priv;
    if (!sink || !basePriv)
    {
        GST_ERROR_OBJECT(object, "Sink not initalised");
        return;
    }

    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = basePriv->m_mediaPlayerManager.getMediaPlayerClient();

    switch (propId)
    {
    case PROP_VOLUME:
    {
        if (!client)
        {
            GST_WARNING_OBJECT(object, "missing media player client");
            return;
        }
        client->setVolume(g_value_get_double(value));
        break;
    }
    case PROP_MUTE:
    {
        if (!client)
        {
            GST_WARNING_OBJECT(object, "missing media player client");
            return;
        }
        client->setMute(g_value_get_boolean(value));
        break;
    }
    default:
    {
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
    }
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

    gst_pad_set_chain_function(priv->m_sinkPad, rialto_mse_base_sink_chain);
    gst_pad_set_event_function(priv->m_sinkPad, rialto_mse_audio_sink_event);

    priv->m_callbacks.qosCallback = std::bind(rialto_mse_audio_sink_qos_handle, GST_ELEMENT_CAST(sink),
                                              std::placeholders::_1, std::placeholders::_2);
}

static void rialto_mse_audio_sink_class_init(RialtoMSEAudioSinkClass *klass)
{
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    GstElementClass *elementClass = GST_ELEMENT_CLASS(klass);
    gobjectClass->get_property = rialto_mse_audio_sink_get_property;
    gobjectClass->set_property = rialto_mse_audio_sink_set_property;
    elementClass->change_state = rialto_mse_audio_sink_change_state;

    g_object_class_install_property(gobjectClass, PROP_VOLUME,
                                    g_param_spec_double("volume", "Volume", "Volume of this stream", 0, 1.0, 1.0,
                                                        GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(gobjectClass, PROP_MUTE,
                                    g_param_spec_boolean("mute", "Mute", "Mute status of this stream", FALSE,
                                                         GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

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
