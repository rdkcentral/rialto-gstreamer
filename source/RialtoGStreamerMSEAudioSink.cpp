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
#include <gst/audio/audio.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <inttypes.h>
#include <stdint.h>

#include "Constants.h"
#include "GStreamerEMEUtils.h"
#include "GStreamerMSEUtils.h"
#include "IMediaPipelineCapabilities.h"
#include "RialtoGStreamerMSEAudioSink.h"
#include "RialtoGStreamerMSEAudioSinkPrivate.h"
#include "RialtoGStreamerMSEBaseSinkPrivate.h"

using namespace firebolt::rialto::client;

GST_DEBUG_CATEGORY_STATIC(RialtoMSEAudioSinkDebug);
#define GST_CAT_DEFAULT RialtoMSEAudioSinkDebug

#define rialto_mse_audio_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE(RialtoMSEAudioSink, rialto_mse_audio_sink, RIALTO_TYPE_MSE_BASE_SINK,
                        G_ADD_PRIVATE(RialtoMSEAudioSink) G_IMPLEMENT_INTERFACE(GST_TYPE_STREAM_VOLUME, NULL)
                            GST_DEBUG_CATEGORY_INIT(RialtoMSEAudioSinkDebug, "rialtomseaudiosink", 0,
                                                    "rialto mse audio sink"));

enum
{
    PROP_0,
    PROP_VOLUME,
    PROP_MUTE,
    PROP_GAP,
    PROP_LOW_LATENCY,
    PROP_SYNC,
    PROP_SYNC_OFF,
    PROP_STREAM_SYNC_MODE,
    PROP_LAST
};

static GstStateChangeReturn rialto_mse_audio_sink_change_state(GstElement *element, GstStateChange transition)
{
    RialtoMSEAudioSink *sink = RIALTO_MSE_AUDIO_SINK(element);
    RialtoMSEBaseSinkPrivate *basePriv = sink->parent.priv;
    RialtoMSEAudioSinkPrivate *priv = sink->priv;

    switch (transition)
    {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    {
        if (!rialto_mse_base_sink_attach_to_media_client_and_set_streams_number(element))
        {
            return GST_STATE_CHANGE_FAILURE;
        }

        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = basePriv->m_mediaPlayerManager.getMediaPlayerClient();
        if (!client)
        {
            GST_ERROR_OBJECT(sink, "MediaPlayerClient is nullptr");
            return GST_STATE_CHANGE_FAILURE;
        }
        if (priv->isVolumeQueued)
        {
            client->setVolume(priv->volume);
            priv->isVolumeQueued = false;
        }
        if (priv->isLowLatencyQueued)
        {
            if (!client->setLowLatency(priv->lowLatency))
            {
                GST_ERROR_OBJECT(sink, "Could not set queued low-latency");
            }
            priv->isLowLatencyQueued = false;
        }
        if (priv->isSyncQueued)
        {
            if (!client->setSync(priv->sync))
            {
                GST_ERROR_OBJECT(sink, "Could not set queued sync");
            }
            priv->isSyncQueued = false;
        }
        if (priv->isSyncOffQueued)
        {
            if (!client->setSyncOff(priv->syncOff))
            {
                GST_ERROR_OBJECT(sink, "Could not set queued sync-off");
            }
            priv->isSyncOffQueued = false;
        }
        if (priv->isStreamSyncModeQueued)
        {
            if (!client->setStreamSyncMode(priv->streamSyncMode))
            {
                GST_ERROR_OBJECT(sink, "Could not set queued stream-sync-mode");
            }
            priv->isStreamSyncModeQueued = false;
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
            guint16 pre_skip = 0;
            gint16 gain = 0;
            if (gst_codec_utils_opus_parse_caps(caps, &sample_rate, &number_of_channels, &channel_mapping_family,
                                                &streams, &stereo_streams, channel_mapping))
            {
                GstBuffer *id_header;
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
        else if (g_str_has_prefix(strct_name, "audio/b-wav") || g_str_has_prefix(strct_name, "audio/x-raw"))
        {
            gint sample_rate = 0;
            gint number_of_channels = 0;
            std::optional<uint64_t> channelMask;
            gst_structure_get_int(structure, "rate", &sample_rate);
            gst_structure_get_int(structure, "channels", &number_of_channels);
            std::optional<firebolt::rialto::Layout> layout =
                rialto_mse_sink_convert_layout(gst_structure_get_string(structure, "layout"));
            std::optional<firebolt::rialto::Format> format =
                rialto_mse_sink_convert_format(gst_structure_get_string(structure, "format"));
            const GValue *channelMaskValue = gst_structure_get_value(structure, "channel-mask");
            if (channelMaskValue)
            {
                channelMask = gst_value_get_bitmask(channelMaskValue);
            }

            if (g_str_has_prefix(strct_name, "audio/b-wav"))
            {
                mimeType = "audio/b-wav";
            }
            else
            {
                mimeType = "audio/x-raw";
            }

            audioConfig = firebolt::rialto::AudioConfig{static_cast<uint32_t>(number_of_channels),
                                                        static_cast<uint32_t>(sample_rate),
                                                        {},
                                                        format,
                                                        layout,
                                                        channelMask};
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
    RialtoMSEAudioSink *audioSink = RIALTO_MSE_AUDIO_SINK(parent);
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

                if (audioSink->priv->isMuteQueued)
                {
                    client->setMute(audioSink->priv->mute, basePriv->m_sourceId);
                    audioSink->priv->isMuteQueued = false;
                }

                // check if READY -> PAUSED was requested before source was attached
                if (GST_STATE_NEXT(sink) == GST_STATE_PAUSED)
                {
                    client->pause(sink->priv->m_sourceId);
                }
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
    if (!sink)
    {
        GST_ERROR_OBJECT(object, "Sink not initalised");
        return;
    }
    RialtoMSEBaseSinkPrivate *basePriv = sink->parent.priv;
    RialtoMSEAudioSinkPrivate *priv = sink->priv;
    if (!basePriv || !priv)
    {
        GST_ERROR_OBJECT(object, "Private Sink not initalised");
        return;
    }

    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = basePriv->m_mediaPlayerManager.getMediaPlayerClient();

    switch (propId)
    {
    case PROP_VOLUME:
    {
        if (!client)
        {
            g_value_set_double(value, priv->volume);
            return;
        }
        g_value_set_double(value, client->getVolume());
        break;
    }
    case PROP_MUTE:
    {
        if (!client)
        {
            g_value_set_boolean(value, priv->mute);
            return;
        }
        g_value_set_boolean(value, client->getMute(basePriv->m_sourceId));
        break;
    }
    case PROP_SYNC:
    {
        if (!client)
        {
            g_value_set_boolean(value, priv->sync);
            return;
        }

        bool sync{kDefaultSync};
        if (!client->getSync(sync))
        {
            GST_ERROR_OBJECT(sink, "Could not get sync");
        }
        g_value_set_boolean(value, sync);
        break;
    }
    case PROP_STREAM_SYNC_MODE:
    {
        if (!client)
        {
            g_value_set_int(value, priv->streamSyncMode);
            return;
        }

        int32_t streamSyncMode{kDefaultStreamSyncMode};
        if (!client->getStreamSyncMode(streamSyncMode))
        {
            GST_ERROR_OBJECT(sink, "Could not get stream-sync-mode");
        }
        g_value_set_int(value, streamSyncMode);
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
    if (!sink)
    {
        GST_ERROR_OBJECT(object, "Sink not initalised");
        return;
    }
    RialtoMSEBaseSinkPrivate *basePriv = sink->parent.priv;
    RialtoMSEAudioSinkPrivate *priv = sink->priv;
    if (!basePriv || !priv)
    {
        GST_ERROR_OBJECT(object, "Private Sink not initalised");
        return;
    }

    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = basePriv->m_mediaPlayerManager.getMediaPlayerClient();

    switch (propId)
    {
    case PROP_VOLUME:
    {
        priv->volume = g_value_get_double(value);
        if (!client)
        {
            GST_DEBUG_OBJECT(object, "Enqueue volume setting");
            priv->isVolumeQueued = true;
            return;
        }
        client->setVolume(priv->volume);
        break;
    }
    case PROP_MUTE:
    {
        priv->mute = g_value_get_boolean(value);
        if (!client || !basePriv->m_sourceAttached)
        {
            GST_DEBUG_OBJECT(object, "Enqueue mute setting");
            priv->isMuteQueued = true;
            return;
        }
        client->setMute(priv->mute, basePriv->m_sourceId);
        break;
    }
    case PROP_GAP:
    {
        gint64 position{0}, discontinuityGap{0};
        guint duration{0};
        gboolean audioAac{FALSE};

        GstStructure *gapData = GST_STRUCTURE_CAST(g_value_get_boxed(value));
        if (!gst_structure_get_int64(gapData, "position", &position))
        {
            GST_WARNING_OBJECT(object, "Set gap: position is missing!");
        }
        if (!gst_structure_get_uint(gapData, "duration", &duration))
        {
            GST_WARNING_OBJECT(object, "Set gap: duration is missing!");
        }
        if (!gst_structure_get_int64(gapData, "discontinuity-gap", &discontinuityGap))
        {
            GST_WARNING_OBJECT(object, "Set gap: discontinuity gap is missing!");
        }
        if (!gst_structure_get_boolean(gapData, "audio-aac", &audioAac))
        {
            GST_WARNING_OBJECT(object, "Set gap: audio aac is missing!");
        }

        GST_DEBUG_OBJECT(object, "Processing audio gap.");
        client->processAudioGap(position, duration, discontinuityGap, audioAac);
        break;
    }
    case PROP_LOW_LATENCY:
    {
        priv->lowLatency = g_value_get_boolean(value);
        if (!client)
        {
            GST_DEBUG_OBJECT(object, "Enqueue low latency setting");
            priv->isLowLatencyQueued = true;
            return;
        }

        if (!client->setLowLatency(priv->lowLatency))
        {
            GST_ERROR_OBJECT(sink, "Could not set low-latency");
        }
        break;
    }
    case PROP_SYNC:
    {
        priv->sync = g_value_get_boolean(value);
        if (!client)
        {
            GST_DEBUG_OBJECT(object, "Enqueue sync setting");
            priv->isSyncQueued = true;
            return;
        }

        if (!client->setSync(priv->sync))
        {
            GST_ERROR_OBJECT(sink, "Could not set sync");
        }
        break;
    }
    case PROP_SYNC_OFF:
    {
        priv->syncOff = g_value_get_boolean(value);
        if (!client)
        {
            GST_DEBUG_OBJECT(object, "Enqueue sync off setting");
            priv->isSyncOffQueued = true;
            return;
        }

        if (!client->setSyncOff(priv->syncOff))
        {
            GST_ERROR_OBJECT(sink, "Could not set sync-off");
        }
        break;
    }
    case PROP_STREAM_SYNC_MODE:
    {
        priv->streamSyncMode = g_value_get_int(value);
        if (!client)
        {
            GST_DEBUG_OBJECT(object, "Enqueue stream sync mode setting");
            priv->isStreamSyncModeQueued = true;
            return;
        }

        if (!client->setStreamSyncMode(priv->streamSyncMode))
        {
            GST_ERROR_OBJECT(sink, "Could not set stream-sync-mode");
        }
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
    gst_object_unref(bus);
}

static void rialto_mse_audio_sink_init(RialtoMSEAudioSink *sink)
{
    RialtoMSEBaseSinkPrivate *priv = sink->parent.priv;

    sink->priv = static_cast<RialtoMSEAudioSinkPrivate *>(rialto_mse_audio_sink_get_instance_private(sink));
    new (sink->priv) RialtoMSEAudioSinkPrivate();

    if (!rialto_mse_base_sink_initialise_sinkpad(RIALTO_MSE_BASE_SINK(sink)))
    {
        GST_ERROR_OBJECT(sink, "Failed to initialise AUDIO sink. Sink pad initialisation failed.");
        return;
    }

    priv->m_mediaSourceType = firebolt::rialto::MediaSourceType::AUDIO;
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
                                    g_param_spec_double("volume", "Volume", "Volume of this stream", 0, 1.0,
                                                        kDefaultVolume,
                                                        GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(gobjectClass, PROP_MUTE,
                                    g_param_spec_boolean("mute", "Mute", "Mute status of this stream", kDefaultMute,
                                                         GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(gobjectClass, PROP_GAP,
                                    g_param_spec_boxed("gap", "Gap", "Audio Gap", GST_TYPE_STRUCTURE,
                                                       (GParamFlags)(G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS)));

    std::unique_ptr<firebolt::rialto::IMediaPipelineCapabilities> mediaPlayerCapabilities =
        firebolt::rialto::IMediaPipelineCapabilitiesFactory::createFactory()->createMediaPipelineCapabilities();
    if (mediaPlayerCapabilities)
    {
        std::vector<std::string> supportedMimeTypes =
            mediaPlayerCapabilities->getSupportedMimeTypes(firebolt::rialto::MediaSourceType::AUDIO);

        rialto_mse_sink_setup_supported_caps(elementClass, supportedMimeTypes);

        const std::string kLowLatencyPropertyName{"low-latency"};
        const std::string kSyncPropertyName{"sync"};
        const std::string kSyncOffPropertyName{"sync-off"};
        const std::string kStreamSyncModePropertyName{"stream-sync-mode"};
        const std::vector<std::string> kPropertyNamesToSearch{kLowLatencyPropertyName, kSyncPropertyName,
                                                              kSyncOffPropertyName, kStreamSyncModePropertyName};
        std::vector<std::string> supportedProperties{
            mediaPlayerCapabilities->getSupportedProperties(firebolt::rialto::MediaSourceType::AUDIO,
                                                            kPropertyNamesToSearch)};

        for (auto it = supportedProperties.begin(); it != supportedProperties.end(); ++it)
        {
            if (kLowLatencyPropertyName == *it)
            {
                g_object_class_install_property(gobjectClass, PROP_LOW_LATENCY,
                                                g_param_spec_boolean(kLowLatencyPropertyName.c_str(),
                                                                     "low latency", "Turn on low latency mode, for use with gaming (no audio decoding, no a/v sync)",
                                                                     kDefaultLowLatency, GParamFlags(G_PARAM_WRITABLE)));
            }
            else if (kSyncPropertyName == *it)
            {
                g_object_class_install_property(gobjectClass, PROP_SYNC,
                                                g_param_spec_boolean(kSyncPropertyName.c_str(), "sync", "Clock sync",
                                                                     kDefaultSync, GParamFlags(G_PARAM_READWRITE)));
            }
            else if (kSyncOffPropertyName == *it)
            {
                g_object_class_install_property(gobjectClass, PROP_SYNC_OFF,
                                                g_param_spec_boolean(kSyncOffPropertyName.c_str(),
                                                                     "sync off", "Turn on free running audio. Must be set before pipeline is PLAYING state.",
                                                                     kDefaultSyncOff, GParamFlags(G_PARAM_WRITABLE)));
            }
            else if (kStreamSyncModePropertyName == *it)
            {
                g_object_class_install_property(gobjectClass, PROP_STREAM_SYNC_MODE,
                                                g_param_spec_int(kStreamSyncModePropertyName.c_str(),
                                                                 "stream sync mode", "1 - Frame to decode frame will immediately proceed next frame sync, 0 - Frame decoded with no frame sync",
                                                                 0, G_MAXINT, kDefaultStreamSyncMode,
                                                                 GParamFlags(G_PARAM_READWRITE)));
            }
            else
            {
                GST_ERROR("Unexpected property %s returned from rialto", it->c_str());
            }
        }
    }
    else
    {
        GST_ERROR("Failed to get supported mime types for AUDIO");
    }

    gst_element_class_set_details_simple(elementClass, "Rialto Audio Sink", "Decoder/Audio/Sink/Audio",
                                         "Communicates with Rialto Server", "Sky");
}
