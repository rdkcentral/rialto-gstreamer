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

#include "RialtoGStreamerMSEVideoSink.h"
#include "GStreamerEMEUtils.h"
#include "GStreamerMSEUtils.h"
#include "RialtoGStreamerMSEBaseSinkPrivate.h"
#include "RialtoGStreamerMSEVideoSinkPrivate.h"
#include <IMediaPipelineCapabilities.h>
#include <gst/gst.h>
#include <inttypes.h>
#include <stdint.h>

using namespace firebolt::rialto::client;

GST_DEBUG_CATEGORY_STATIC(RialtoMSEVideoSinkDebug);
#define GST_CAT_DEFAULT RialtoMSEVideoSinkDebug

#define rialto_mse_video_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE(RialtoMSEVideoSink, rialto_mse_video_sink, RIALTO_TYPE_MSE_BASE_SINK,
                        G_ADD_PRIVATE(RialtoMSEVideoSink)
                            GST_DEBUG_CATEGORY_INIT(RialtoMSEVideoSinkDebug, "rialtomsevideosink", 0,
                                                    "rialto mse video sink"));

enum
{
    PROP_0,
    PROP_WINDOW_SET,
    PROP_MAX_VIDEO_WIDTH,
    PROP_MAX_VIDEO_HEIGHT,
    PROP_LAST
};

static GstStateChangeReturn rialto_mse_video_sink_change_state(GstElement *element, GstStateChange transition)
{
    RialtoMSEVideoSink *sink = RIALTO_MSE_VIDEO_SINK(element);
    RialtoMSEVideoSinkPrivate *priv = sink->priv;
    RialtoMSEBaseSinkPrivate *basePriv = sink->parent.priv;

    switch (transition)
    {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    {
        // Attach the media player client to media player manager.
        // maxWidth and maxHeight are used to set the video capabilities of the MediaPlayer.
        // If the mediaPlayer has already been created (ie. an audio sink on the same parent bus changed state first)
        // the video capabilities will NOT be set.
        GstObject *parentObject = rialto_mse_base_get_oldest_gst_bin_parent(element);
        if (!basePriv->m_mediaPlayerManager.attachMediaPlayerClient(parentObject,
                                                                    priv->maxWidth, priv->maxHeight))
        {
            GST_ERROR_OBJECT(sink, "Cannot attach the MediaPlayerClient");
            return GST_STATE_CHANGE_FAILURE;
        }
        GST_INFO_OBJECT(element, "Attached media player client with parent %s(%p)", gst_object_get_name(parentObject), parentObject);

        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = basePriv->m_mediaPlayerManager.getMediaPlayerClient();
        firebolt::rialto::IMediaPipeline::MediaSource vsource(-1, firebolt::rialto::MediaSourceType::VIDEO, "");
        if ((!client) || (!client->attachSource(vsource, RIALTO_MSE_BASE_SINK(sink))))
        {
            GST_ERROR_OBJECT(sink, "Failed to attach video source");
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

static firebolt::rialto::IMediaPipeline::MediaSource rialto_mse_video_sink_create_media_source(RialtoMSEBaseSink *sink,
                                                                                               GstCaps *caps)
{
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    const gchar *strct_name = gst_structure_get_name(structure);

    firebolt::rialto::SegmentAlignment alignment = rialto_mse_base_sink_get_segment_alignment(sink, structure);
    std::vector<uint8_t> codecData = rialto_mse_base_sink_get_codec_data(sink, structure);
    firebolt::rialto::StreamFormat format = rialto_mse_base_sink_get_stream_format(sink, structure);
    if (strct_name)
    {
        if (g_str_has_prefix(strct_name, "video/x-h264"))
        {
            firebolt::rialto::IMediaPipeline::MediaSource viddat(-1, firebolt::rialto::MediaSourceType::VIDEO,
                                                                 "video/h264", alignment, format, codecData);
            return viddat;
        }
        else if (g_str_has_prefix(strct_name, "video/x-h265"))
        {
            return firebolt::rialto::IMediaPipeline::MediaSource(-1, firebolt::rialto::MediaSourceType::VIDEO,
                                                                 "video/h265", alignment, format, codecData);
        }
        else
        {
            GST_INFO_OBJECT(sink, "%s video media source created", strct_name);
            return firebolt::rialto::IMediaPipeline::MediaSource(-1, firebolt::rialto::MediaSourceType::VIDEO,
                                                                 strct_name, alignment, format, codecData);
        }
    }
    else
    {
        GST_ERROR_OBJECT(sink,
                         "Empty caps' structure name! Failed to set mime type when constructing video media source");
        return firebolt::rialto::IMediaPipeline::MediaSource(-1, firebolt::rialto::MediaSourceType::VIDEO, "", alignment);
    }
}

static gboolean rialto_mse_video_sink_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
    RialtoMSEBaseSink *sink = RIALTO_MSE_BASE_SINK(parent);
    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_CAPS:
    {
        GstCaps *caps;
        gst_event_parse_caps(event, &caps);
        gchar *capsStr = gst_caps_to_string(caps);

        GST_INFO_OBJECT(sink, "Attaching VIDEO source with caps %s", capsStr);
        g_free(capsStr);
        firebolt::rialto::IMediaPipeline::MediaSource vsource = rialto_mse_video_sink_create_media_source(sink, caps);

        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = sink->priv->m_mediaPlayerManager.getMediaPlayerClient();
        if ((!client) || (!client->attachSource(vsource, sink)))
        {
            GST_ERROR_OBJECT(sink, "Failed to attach VIDEO source");
        }

        break;
    }
    default:
        break;
    }

    return rialto_mse_base_sink_event(pad, parent, event);
}

static void rialto_mse_video_sink_get_property(GObject *object, guint propId, GValue *value, GParamSpec *pspec)
{
    RialtoMSEVideoSink *sink = RIALTO_MSE_VIDEO_SINK(object);
    RialtoMSEVideoSinkPrivate *priv = sink->priv;
    RialtoMSEBaseSinkPrivate *basePriv = sink->parent.priv;
    std::shared_ptr<GStreamerMSEMediaPlayerClient> client;

    switch (propId)
    {
    case PROP_WINDOW_SET:
        if (!sink || !basePriv || !(client = basePriv->m_mediaPlayerManager.getMediaPlayerClient()))
        {
            GST_WARNING_OBJECT(object, "missing media player client");
        }
        else
        {
            g_value_set_string(value, client->getVideoRectangle().c_str());
        }
        break;
    case PROP_MAX_VIDEO_WIDTH:
        if (!sink || !priv)
        {
            GST_WARNING_OBJECT(object, "Sink not initalised");
        }
        else
        {
            // maxWidth should only be set for video only streams.
            g_value_set_uint(value, priv->maxWidth);
        }
        break;
    case PROP_MAX_VIDEO_HEIGHT:
        if (!sink || !priv)
        {
            GST_WARNING_OBJECT(object, "Sink not initalised");
        }
        else
        {
            // maxHeight should only be set for video only streams.
            g_value_set_uint(value, priv->maxHeight);
        }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
}

static void rialto_mse_video_sink_set_property(GObject *object, guint propId, const GValue *value, GParamSpec *pspec)
{
    RialtoMSEVideoSink *sink = RIALTO_MSE_VIDEO_SINK(object);
    RialtoMSEVideoSinkPrivate *priv = sink->priv;
    RialtoMSEBaseSinkPrivate *basePriv = sink->parent.priv;
    std::shared_ptr<GStreamerMSEMediaPlayerClient> client;

    switch (propId)
    {
    case PROP_WINDOW_SET:
        if (!basePriv || !(client = basePriv->m_mediaPlayerManager.getMediaPlayerClient()))
        {
            GST_WARNING_OBJECT(object, "missing media player client");
        }
        else
        {
            const gchar *rectangle = g_value_get_string(value);
            if (rectangle)
            {
                client->setVideoRectangle(std::string(rectangle));
            }
        }
        break;
    case PROP_MAX_VIDEO_WIDTH:
        if (!sink || !priv)
        {
            GST_WARNING_OBJECT(object, "Sink not initalised");
        }
        else
        {
            priv->maxWidth = g_value_get_uint(value);
        }
        break;
    case PROP_MAX_VIDEO_HEIGHT:
        if (!sink || !priv)
        {
            GST_WARNING_OBJECT(object, "Sink not initalised");
        }
        else
        {
            priv->maxHeight = g_value_get_uint(value);
        }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
}

static void rialto_mse_video_sink_qos_handle(GstElement *element, uint64_t processed, uint64_t dropped)
{
    GstBus *bus = gst_element_get_bus(element);
    /* Hardcode isLive to FALSE and set invalid timestamps */
    GstMessage *message = gst_message_new_qos(GST_OBJECT(element), FALSE, GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE,
                                              GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE);

    gst_message_set_qos_stats(message, GST_FORMAT_BUFFERS, processed, dropped);
    gst_bus_post(bus, message);
}

static void rialto_mse_video_sink_init(RialtoMSEVideoSink *sink)
{
    RialtoMSEBaseSinkPrivate *basePriv = sink->parent.priv;

    sink->priv = static_cast<RialtoMSEVideoSinkPrivate *>(rialto_mse_video_sink_get_instance_private(sink));
    new (sink->priv) RialtoMSEVideoSinkPrivate();

    if (!rialto_mse_base_sink_initialise_sinkpad(RIALTO_MSE_BASE_SINK(sink)))
    {
        GST_ERROR_OBJECT(sink, "Failed to initialise VIDEO sink. Sink pad initialisation failed.");
        return;
    }

    gst_pad_set_chain_function(basePriv->mSinkPad, rialto_mse_base_sink_chain);
    gst_pad_set_event_function(basePriv->mSinkPad, rialto_mse_video_sink_event);

    basePriv->mCallbacks.qosCallback = std::bind(rialto_mse_video_sink_qos_handle, GST_ELEMENT_CAST(sink),
                                                 std::placeholders::_1, std::placeholders::_2);
}

static void rialto_mse_video_sink_finalize(GObject *object)
{
    RialtoMSEVideoSink *sink = RIALTO_MSE_VIDEO_SINK(object);
    RialtoMSEVideoSinkPrivate *priv = sink->priv;

    priv->~RialtoMSEVideoSinkPrivate();
}

static void rialto_mse_video_sink_class_init(RialtoMSEVideoSinkClass *klass)
{
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    GstElementClass *elementClass = GST_ELEMENT_CLASS(klass);
    gobjectClass->finalize = rialto_mse_video_sink_finalize;
    gobjectClass->get_property = rialto_mse_video_sink_get_property;
    gobjectClass->set_property = rialto_mse_video_sink_set_property;
    elementClass->change_state = rialto_mse_video_sink_change_state;

    g_object_class_install_property(gobjectClass, PROP_WINDOW_SET,
                                    g_param_spec_string("rectangle", "rectangle", "Window Set Format: x,y,width,height",
                                                        nullptr, GParamFlags(G_PARAM_WRITABLE)));

    g_object_class_install_property(gobjectClass, PROP_MAX_VIDEO_WIDTH,
                                    g_param_spec_uint("maxVideoWidth", "maxVideoWidth",
                                                      "Maximum width of video frames to be decoded", 0, 3840,
                                                      DEFAULT_MAX_VIDEO_WIDTH, GParamFlags(G_PARAM_READWRITE)));

    g_object_class_install_property(gobjectClass, PROP_MAX_VIDEO_HEIGHT,
                                    g_param_spec_uint("maxVideoHeight", "maxVideoHeight",
                                                      "Maximum height of video frames to be decoded", 0, 2160,
                                                      DEFAULT_MAX_VIDEO_HEIGHT, GParamFlags(G_PARAM_READWRITE)));

    std::unique_ptr<firebolt::rialto::IMediaPipelineCapabilities> mediaPlayerCapabilities =
        firebolt::rialto::IMediaPipelineCapabilitiesFactory::createFactory()->createMediaPipelineCapabilities();
    if (mediaPlayerCapabilities)
    {
        std::vector<std::string> supportedMimeTypes =
            mediaPlayerCapabilities->getSupportedMimeTypes(firebolt::rialto::MediaSourceType::VIDEO);

        rialto_mse_sink_setup_supported_caps(elementClass, supportedMimeTypes);
    }
    else
    {
        GST_ERROR("Failed to get supported mime types for VIDEO");
    }

    gst_element_class_set_details_simple(elementClass, "Rialto Video Sink", "Decoder/Video/Sink/Video",
                                         "Communicates with Rialto Server", "Sky");
}
