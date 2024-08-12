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

#include <gst/gst.h>
#include <inttypes.h>
#include <stdint.h>

#include "GStreamerEMEUtils.h"
#include "GStreamerMSEUtils.h"
#include "IMediaPipelineCapabilities.h"
#include "RialtoGStreamerMSEBaseSinkPrivate.h"
#include "RialtoGStreamerMSESubtitleSink.h"

using namespace firebolt::rialto::client;

GST_DEBUG_CATEGORY_STATIC(RialtoMSESubtitleSinkDebug);
#define GST_CAT_DEFAULT RialtoMSESubtitleSinkDebug

#define rialto_mse_subtitle_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE(RialtoMSESubtitleSink, rialto_mse_subtitle_sink, RIALTO_TYPE_MSE_BASE_SINK,
                        G_ADD_PRIVATE(RialtoMSESubtitleSink)
                            GST_DEBUG_CATEGORY_INIT(RialtoMSESubtitleSinkDebug, "rialtomsesubtitlesink", 0,
                                                    "rialto mse subtitle sink"));

enum
{
    PROP_0,
    PROP_MUTE,
    PROP_TEXT_TRACK_IDENTIFIER,
    PROP_WINDOW_ID,
    PROP_ASYNC,
    PROP_LAST
};

static GstStateChangeReturn rialto_mse_subtitle_sink_change_state(GstElement *element, GstStateChange transition)
{
    RialtoMSESubtitleSink *sink = RIALTO_MSE_SUBTITLE_SINK(element);
    RialtoMSEBaseSinkPrivate *basePriv = sink->parent.priv;

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
    RialtoMSESubtitleSink *subSink = RIALTO_MSE_SUBTITLE_SINK(sink);
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    const gchar *mimeName = gst_structure_get_name(structure);

    std::string mimeType;
    if (mimeName)
    {
        if (g_str_has_prefix(mimeName, "text/vtt") || g_str_has_prefix(mimeName, "application/x-subtitle-vtt"))
        {
            mimeType = "text/vtt";
        }
        else if (g_str_has_prefix(mimeName, "application/ttml+xml"))
        {
            mimeType = "text/ttml";
        }
        else
        {
            mimeType = mimeName;
        }

        GST_INFO_OBJECT(sink, "%s subtitle media source created", mimeType.c_str());
        return std::make_unique<firebolt::rialto::IMediaPipeline::MediaSourceSubtitle>(mimeType,
                                                                                       subSink->priv->m_textTrackIdentifier);
    }
    else
    {
        GST_ERROR_OBJECT(sink,
                         "Empty caps' structure name! Failed to set mime type when constructing subtitle media source");
    }

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

        GST_INFO_OBJECT(sink, "Attaching SUBTITLE source with caps %" GST_PTR_FORMAT, caps);

        std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> subtitleSource =
            rialto_mse_subtitle_sink_create_media_source(sink, caps);
        if (subtitleSource)
        {
            std::shared_ptr<GStreamerMSEMediaPlayerClient> client =
                sink->priv->m_mediaPlayerManager.getMediaPlayerClient();
            if ((!client) || (!client->attachSource(subtitleSource, sink)))
            {
                GST_ERROR_OBJECT(sink, "Failed to attach SUBTITLE source");
            }
            else
            {
                basePriv->m_sourceAttached = true;

                // check if READY -> PAUSED was requested before source was attached
                if (GST_STATE_NEXT(sink) == GST_STATE_PAUSED)
                {
                    client->pause(sink->priv->m_sourceId);
                }
            }
        }
        else
        {
            GST_ERROR_OBJECT(sink, "Failed to create SUBTITLE source");
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
    RialtoMSESubtitleSinkPrivate *priv = sink->priv;
    RialtoMSEBaseSinkPrivate *basePriv = sink->parent.priv;
    if (!priv || !basePriv)
    {
        GST_ERROR_OBJECT(object, "Private Sink not initalised");
        return;
    }

    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = basePriv->m_mediaPlayerManager.getMediaPlayerClient();

    switch (propId)
    {
    case PROP_MUTE:
        // if (!client)
        // {
        //     std::unique_lock lock{priv->rectangleMutex};
        //     g_value_set_string(value, priv->videoRectangle.c_str());
        // }
        // else
        // {
        //     g_value_set_string(value, client->getVideoRectangle().c_str());
        // }
        break;
    case PROP_TEXT_TRACK_IDENTIFIER:
    {
        g_value_set_string(value, priv->m_textTrackIdentifier.c_str());
        break;
    }
    case PROP_WINDOW_ID:
    {
        g_value_set_uint(value, priv->m_videoId);
        break;
    }
    case PROP_ASYNC:
    {
        g_value_set_boolean(value, basePriv->m_isAsync);
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
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
    RialtoMSESubtitleSinkPrivate *priv = sink->priv;
    RialtoMSEBaseSinkPrivate *basePriv = sink->parent.priv;
    if (!priv || !basePriv)
    {
        GST_ERROR_OBJECT(object, "Private sink not initalised");
        return;
    }

    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = basePriv->m_mediaPlayerManager.getMediaPlayerClient();

    switch (propId)
    {
    case PROP_MUTE:
        // if (!client)
        // {
        //     std::unique_lock lock{priv->rectangleMutex};
        //     g_value_set_string(value, priv->videoRectangle.c_str());
        // }
        // else
        // {
        //     g_value_set_string(value, client->getVideoRectangle().c_str());
        // }
        break;
    case PROP_TEXT_TRACK_IDENTIFIER:
    {
        const gchar *textTrackIdentifier = g_value_get_string(value);
        if (!textTrackIdentifier)
        {
            GST_WARNING_OBJECT(object, "TextTrackIdentifier string not valid");
            break;
        }
        //std::unique_lock lock{priv->rectangleMutex};
        priv->m_textTrackIdentifier = std::string(textTrackIdentifier);
        break;
    }
    case PROP_WINDOW_ID:
    {
        priv->m_videoId = g_value_get_uint(value);
        break;
    }
    case PROP_ASYNC:
    {
        basePriv->m_isAsync = g_value_get_boolean(value);
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
}

//TODO-klops: needed?
static void rialto_mse_subtitle_sink_qos_handle(GstElement *element, uint64_t processed, uint64_t dropped)
{
    GstBus *bus = gst_element_get_bus(element);
    /* Hardcode isLive to FALSE and set invalid timestamps */
    GstMessage *message = gst_message_new_qos(GST_OBJECT(element), FALSE, GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE,
                                              GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE);

    gst_message_set_qos_stats(message, GST_FORMAT_BUFFERS, processed, dropped);
    gst_bus_post(bus, message);
    gst_object_unref(bus);
}

static void rialto_mse_subtitle_sink_init(RialtoMSESubtitleSink *sink)
{
    RialtoMSEBaseSinkPrivate *basePriv = sink->parent.priv;

    sink->priv = static_cast<RialtoMSESubtitleSinkPrivate *>(rialto_mse_subtitle_sink_get_instance_private(sink));
    new (sink->priv) RialtoMSESubtitleSinkPrivate();

    if (!rialto_mse_base_sink_initialise_sinkpad(RIALTO_MSE_BASE_SINK(sink)))
    {
        GST_ERROR_OBJECT(sink, "Failed to initialise SUBTITLE sink. Sink pad initialisation failed.");
        return;
    }

    basePriv->m_mediaSourceType = firebolt::rialto::MediaSourceType::SUBTITLE;
    basePriv->m_isAsync = false;
    gst_pad_set_chain_function(basePriv->m_sinkPad, rialto_mse_base_sink_chain);
    gst_pad_set_event_function(basePriv->m_sinkPad, rialto_mse_subtitle_sink_event);

    basePriv->m_callbacks.qosCallback = std::bind(rialto_mse_subtitle_sink_qos_handle, GST_ELEMENT_CAST(sink),
                                                  std::placeholders::_1, std::placeholders::_2);
}

static void rialto_mse_subtitle_sink_class_init(RialtoMSESubtitleSinkClass *klass)
{
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    GstElementClass *elementClass = GST_ELEMENT_CLASS(klass);
    gobjectClass->get_property = rialto_mse_subtitle_sink_get_property;
    gobjectClass->set_property = rialto_mse_subtitle_sink_set_property;
    elementClass->change_state = rialto_mse_subtitle_sink_change_state;

    g_object_class_install_property(gobjectClass, PROP_MUTE,
                                    g_param_spec_boolean("mute", "Mute", "Mute subtitles", FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobjectClass, PROP_TEXT_TRACK_IDENTIFIER,
                                    g_param_spec_string("text-track-identifier", "Text Track Identifier",
                                                        "Identifier of text track", nullptr,
                                                        GParamFlags(G_PARAM_READWRITE)));

    g_object_class_install_property(gobjectClass, PROP_WINDOW_ID,
                                    g_param_spec_uint("window-id", "Window ID", "Id of window (placeholder)", 0, 256,
                                                      0, GParamFlags(G_PARAM_READWRITE)));

    g_object_class_install_property(gobjectClass, PROP_ASYNC,
                                    g_param_spec_boolean("async", "Async", "Asynchronous mode", FALSE, G_PARAM_READWRITE));

    std::unique_ptr<firebolt::rialto::IMediaPipelineCapabilities> mediaPlayerCapabilities =
        firebolt::rialto::IMediaPipelineCapabilitiesFactory::createFactory()->createMediaPipelineCapabilities();
    if (mediaPlayerCapabilities)
    {
        std::vector<std::string> supportedMimeTypes =
            mediaPlayerCapabilities->getSupportedMimeTypes(firebolt::rialto::MediaSourceType::SUBTITLE);

        rialto_mse_sink_setup_supported_caps(elementClass, supportedMimeTypes);
    }
    else
    {
        GST_ERROR("Failed to get supported mime types for Subtitle");
    }

    gst_element_class_set_details_simple(elementClass, "Rialto Subtitle Sink", "Parser/Subtitle/Sink/Subtitle",
                                         "Communicates with Rialto Server", "Sky");
}
