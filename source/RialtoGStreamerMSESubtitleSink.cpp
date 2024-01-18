/*
 * Copyright (C) 2024 Sky UK
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

#include "RialtoGStreamerMSESubtitleSink.h"
#include "RialtoGStreamerMSEBaseSinkPrivate.h"
#include "GStreamerMSEUtils.h"

//todo:
// it might be worth to create a new base class MSEBaseAVSnik for audio and video and SubtitleSink would inherit directly from BaseSnik
// probably the same thing needs to be done with MediaSource
// BaseSink   ->   BaseAVSink   ->  AudioSink
//    |                |
// SubtitleSink     VideoSink
using namespace firebolt::rialto::client;

GST_DEBUG_CATEGORY_STATIC(RialtoMSESubtitleSinkDebug);
#define GST_CAT_DEFAULT RialtoMSESubtitleSinkDebug

#define rialto_mse_subtitle_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE(RialtoMSESubtitleSink, rialto_mse_subtitle_sink, RIALTO_TYPE_MSE_BASE_SINK,
                            GST_DEBUG_CATEGORY_INIT(RialtoMSESubtitleSinkDebug, "rialtomsesubtitlesink", 0,
                                                    "rialto mse subtitle sink"));

enum
{
    PROP_0,
    PROP_MUTE,
    PROP_LAST
};


static GstStateChangeReturn rialto_mse_subtitle_sink_change_state(GstElement *element, GstStateChange transition)
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

        int32_t textStreams = 0;
        bool isTextOnly = false;

        gint n_video = 0;
        gint n_audio = 0;
        gint n_text = 1; //change to take from priv if not playbin
        if (rialto_mse_base_sink_get_n_streams_from_parent(parentObject, n_video, n_audio, n_text))
        {
            textStreams = n_text;
            // isAudioOnly = n_video == 0;
            // GST_INFO_OBJECT(element, "There are %u audio streams and isAudioOnly value is %s", n_audio,
            //                 isAudioOnly ? "'true'" : "'false'");
        }
        else
        {
            // std::lock_guard<std::mutex> lock(priv->m_sinkMutex);
            // audioStreams = priv->m_numOfStreams;
            // isAudioOnly = priv->m_isSinglePathStream;
        }

        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = priv->m_mediaPlayerManager.getMediaPlayerClient();
        if (client)
        {
            client->setTextStreamsInfo(textStreams, isTextOnly);
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
rialto_mse_subtitle_sink_create_media_source(RialtoMSEBaseSink *sink, GstCaps *caps)
{
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    const gchar *strct_name = gst_structure_get_name(structure);

    std::string mimeType;

    if (strct_name)
    {
        if (g_str_has_prefix(strct_name, "text/vtt"))
        {
            mimeType = "text/vtt";
        }
        else
        {
            mimeType = "application/ttml+xml";
        }

///using namespace firebolt::rialto::IMediaPipeline?
        return std::make_unique<firebolt::rialto::IMediaPipeline::MediaSourceSubtitle>(mimeType);
    }

    GST_ERROR_OBJECT(sink, "Empty caps' structure name! Failed to set mime type for text media source.");
    return nullptr;
}

static gboolean rialto_mse_subtitle_sink_event(GstPad *pad, GstObject *parent, GstEvent *event)
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
            rialto_mse_subtitle_sink_create_media_source(sink, caps);
        if (asource)
        {
            std::shared_ptr<GStreamerMSEMediaPlayerClient> client =
                sink->priv->m_mediaPlayerManager.getMediaPlayerClient();
            if ((!client) || (!client->attachSource(asource, sink)))
            {
                GST_ERROR_OBJECT(sink, "Failed to attach TEXT source");
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

static void rialto_mse_subtitle_sink_get_property(GObject *object, guint propId, GValue *value, GParamSpec *pspec)
{
    RialtoMSESubtitleSink *sink = RIALTO_MSE_SUBTITLE_SINK(object);
    if (!sink)
    {
        GST_ERROR_OBJECT(object, "Sink not initalised");
        return;
    }
    RialtoMSEBaseSinkPrivate *basePriv = sink->parent.priv;
    if (!basePriv)
    {
        GST_ERROR_OBJECT(object, "RialtoMSEBaseSinkPrivate not initalised");
        return;
    }

    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = basePriv->m_mediaPlayerManager.getMediaPlayerClient();

    switch (propId)
    {
    case PROP_MUTE:
    {
        if (!client)
        {
            GST_WARNING_OBJECT(object, "missing media player client");
            return;
        }
        g_value_set_boolean(value, true/*client->getMute()*/);
        break;
    }
    default:
    {
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
    }
}

static void rialto_mse_subtitle_sink_set_property(GObject *object, guint propId, const GValue *value, GParamSpec *pspec)
{
    RialtoMSESubtitleSink *sink = RIALTO_MSE_SUBTITLE_SINK(object);
    if (!sink)
    {
        GST_ERROR_OBJECT(object, "Sink not initalised");
        return;
    }
    RialtoMSEBaseSinkPrivate *basePriv = sink->parent.priv;
    if (!basePriv)
    {
        GST_ERROR_OBJECT(object, "RialtoMSEBaseSinkPrivate not initalised");
        return;
    }

    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = basePriv->m_mediaPlayerManager.getMediaPlayerClient();

    switch (propId)
    {
    case PROP_MUTE:
    {
        if (!client)
        {
            GST_WARNING_OBJECT(object, "missing media player client");
            return;
        }
        //client->setMute(g_value_get_boolean(value));
        break;
    }
    default:
    {
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
    }
}

// static void rialto_mse_subtitle_sink_qos_handle(GstElement *element, uint64_t processed, uint64_t dropped)
// {
//     GstBus *bus = gst_element_get_bus(element);
//     /* Hardcode isLive to FALSE and set invalid timestamps */
//     GstMessage *message = gst_message_new_qos(GST_OBJECT(element), FALSE, GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE,
//                                               GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE);
//     gst_message_set_qos_stats(message, GST_FORMAT_DEFAULT, processed, dropped);
//     gst_bus_post(bus, message);
//     gst_object_unref(bus);
// }

static void rialto_mse_subtitle_sink_init(RialtoMSESubtitleSink *sink)
{
    RialtoMSEBaseSinkPrivate *priv = sink->parent.priv;

    if (!rialto_mse_base_sink_initialise_sinkpad(RIALTO_MSE_BASE_SINK(sink)))
    {
        GST_ERROR_OBJECT(sink, "Failed to initialise TEXT sink. Sink pad initialisation failed.");
        return;
    }

    gst_pad_set_chain_function(priv->m_sinkPad, rialto_mse_base_sink_chain);
    gst_pad_set_event_function(priv->m_sinkPad, rialto_mse_subtitle_sink_event);
}

static void rialto_mse_subtitle_sink_class_init(RialtoMSESubtitleSinkClass *klass)
{
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    GstElementClass *elementClass = GST_ELEMENT_CLASS(klass);
    gobjectClass->get_property = rialto_mse_subtitle_sink_get_property;
    gobjectClass->set_property = rialto_mse_subtitle_sink_set_property;
    elementClass->change_state = rialto_mse_subtitle_sink_change_state;

    g_object_class_install_property(gobjectClass, PROP_MUTE,
                                    g_param_spec_boolean("mute", "Mute", "Mute status of this stream", FALSE,
                                                         GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    // std::unique_ptr<firebolt::rialto::IMediaPipelineCapabilities> mediaPlayerCapabilities =
    //     firebolt::rialto::IMediaPipelineCapabilitiesFactory::createFactory()->createMediaPipelineCapabilities();
    // if (mediaPlayerCapabilities)
    // {
    //     std::vector<std::string> supportedMimeTypes =
    //         mediaPlayerCapabilities->getSupportedMimeTypes(firebolt::rialto::MediaSourceType::AUDIO);

    //     rialto_mse_sink_setup_supported_caps(elementClass, supportedMimeTypes);
    // }
    // else
    // {
    //     GST_ERROR("Failed to get supported mime types for AUDIO");
    // }

    std::vector<std::string> supportedMimeTypes = {"application/ttml+xml", "text/vtt"};
    rialto_mse_sink_setup_supported_caps(elementClass, supportedMimeTypes);

    gst_element_class_set_details_simple(elementClass, "Rialto Subtitle Sink", "Sink/Parser/Subtitle",
                                         "Communicates with Rialto Server", "Sky");
}
