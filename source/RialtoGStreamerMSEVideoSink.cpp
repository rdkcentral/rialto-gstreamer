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

#include <gst/gst.h>
#include <inttypes.h>
#include <stdint.h>

#include "GStreamerEMEUtils.h"
#include "GStreamerMSEUtils.h"
#include "IMediaPipelineCapabilities.h"
#include "RialtoGStreamerMSEBaseSinkPrivate.h"
#include "RialtoGStreamerMSEVideoSink.h"
#include "RialtoGStreamerMSEVideoSinkPrivate.h"

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
    PROP_MAX_VIDEO_WIDTH_DEPRECATED,
    PROP_MAX_VIDEO_HEIGHT_DEPRECATED,
    PROP_FRAME_STEP_ON_PREROLL,
    PROP_IMMEDIATE_OUTPUT,
    PROP_SYNCMODE_STREAMING,
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
        if (!rialto_mse_base_sink_attach_to_media_client_and_set_streams_number(element, priv->maxWidth, priv->maxHeight))
        {
            return GST_STATE_CHANGE_FAILURE;
        }

        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = basePriv->m_mediaPlayerManager.getMediaPlayerClient();
        if (!client)
        {
            GST_ERROR_OBJECT(sink, "MediaPlayerClient is nullptr");
            return GST_STATE_CHANGE_FAILURE;
        }

        std::unique_lock lock{priv->propertyMutex};
        if (priv->rectangleSettingQueued)
        {
            GST_DEBUG_OBJECT(sink, "Set queued video rectangle");
            priv->rectangleSettingQueued = false;
            client->setVideoRectangle(priv->videoRectangle);
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
rialto_mse_video_sink_create_media_source(RialtoMSEBaseSink *sink, GstCaps *caps)
{
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    const gchar *strct_name = gst_structure_get_name(structure);

    firebolt::rialto::SegmentAlignment alignment = rialto_mse_base_sink_get_segment_alignment(sink, structure);
    std::shared_ptr<firebolt::rialto::CodecData> codecData = rialto_mse_base_sink_get_codec_data(sink, structure);
    firebolt::rialto::StreamFormat format = rialto_mse_base_sink_get_stream_format(sink, structure);

    gint width = 0;
    gint height = 0;
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);

    std::string mimeType;
    if (strct_name)
    {
        if (g_str_has_prefix(strct_name, "video/x-h264"))
        {
            mimeType = "video/h264";
        }
        else if (g_str_has_prefix(strct_name, "video/x-h265"))
        {
            mimeType = "video/h265";

            uint32_t dolbyVisionProfile = -1;
            if (rialto_mse_base_sink_get_dv_profile(sink, structure, dolbyVisionProfile))
            {
                return std::make_unique<firebolt::rialto::IMediaPipeline::MediaSourceVideoDolbyVision>(mimeType,
                                                                                                       dolbyVisionProfile,
                                                                                                       sink->priv->m_hasDrm,
                                                                                                       width, height,
                                                                                                       alignment, format,
                                                                                                       codecData);
            }
        }
        else
        {
            mimeType = strct_name;
        }

        GST_INFO_OBJECT(sink, "%s video media source created", mimeType.c_str());
        return std::make_unique<firebolt::rialto::IMediaPipeline::MediaSourceVideo>(mimeType, sink->priv->m_hasDrm,
                                                                                    width, height, alignment, format,
                                                                                    codecData);
    }
    else
    {
        GST_ERROR_OBJECT(sink,
                         "Empty caps' structure name! Failed to set mime type when constructing video media source");
    }

    return nullptr;
}

static gboolean rialto_mse_video_sink_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
    RialtoMSEBaseSink *baseSink = RIALTO_MSE_BASE_SINK(parent);
    RialtoMSEBaseSinkPrivate *basePriv = baseSink->priv;
    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_CAPS:
    {
        GstCaps *caps;
        gst_event_parse_caps(event, &caps);
        if (basePriv->m_sourceAttached)
        {
            GST_INFO_OBJECT(baseSink, "Source already attached. Skip calling attachSource");
            break;
        }

        GST_INFO_OBJECT(baseSink, "Attaching VIDEO source with caps %" GST_PTR_FORMAT, caps);

        std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> vsource =
            rialto_mse_video_sink_create_media_source(baseSink, caps);
        if (vsource)
        {
            std::shared_ptr<GStreamerMSEMediaPlayerClient> client = basePriv->m_mediaPlayerManager.getMediaPlayerClient();
            if ((!client) || (!client->attachSource(vsource, baseSink)))
            {
                GST_ERROR_OBJECT(baseSink, "Failed to attach VIDEO source");
            }
            else
            {
                basePriv->m_sourceAttached = true;

                // check if READY -> PAUSED was requested before source was attached
                if (GST_STATE_NEXT(baseSink) == GST_STATE_PAUSED)
                {
                    client->pause(basePriv->m_sourceId);
                }
                RialtoMSEVideoSink *sink = RIALTO_MSE_VIDEO_SINK(parent);
                RialtoMSEVideoSinkPrivate *priv = sink->priv;
                std::unique_lock lock{priv->propertyMutex};
                if (priv->immediateOutputQueued)
                {
                    GST_DEBUG_OBJECT(sink, "Set queued immediate-output");
                    priv->immediateOutputQueued = false;
                    if (!client->setImmediateOutput(basePriv->m_sourceId, priv->immediateOutput))
                    {
                        GST_ERROR_OBJECT(sink, "Could not set immediate-output");
                    }
                }
                if (priv->syncmodeStreamingQueued)
                {
                    GST_DEBUG_OBJECT(sink, "Set queued syncmode-streaming");
                    priv->syncmodeStreamingQueued = false;
                    if (!client->setStreamSyncMode(basePriv->m_sourceId, priv->syncmodeStreaming))
                    {
                        GST_ERROR_OBJECT(sink, "Could not set syncmode-streaming");
                    }
                }
            }
        }
        else
        {
            GST_ERROR_OBJECT(baseSink, "Failed to create VIDEO source");
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
    if (!sink)
    {
        GST_ERROR_OBJECT(object, "Sink not initalised");
        return;
    }
    RialtoMSEVideoSinkPrivate *priv = sink->priv;
    RialtoMSEBaseSinkPrivate *basePriv = sink->parent.priv;
    if (!priv || !basePriv)
    {
        GST_ERROR_OBJECT(object, "Private Sink not initalised");
        return;
    }

    switch (propId)
    {
    case PROP_WINDOW_SET:
    {
        std::unique_lock lock{priv->propertyMutex};
        auto client = basePriv->m_mediaPlayerManager.getMediaPlayerClient();
        if (!client)
        {
            // Return the default value and
            // queue a setting event (for the default value) so that it will become true when
            // the client connects...
            GST_DEBUG_OBJECT(object, "Return default rectangle setting, and queue an event to set the default upon "
                                     "client connect");
            priv->rectangleSettingQueued = true;
            g_value_set_string(value, priv->videoRectangle.c_str());
        }
        else
        {
            lock.unlock();
            g_value_set_string(value, client->getVideoRectangle().c_str());
        }
        break;
    }
    case PROP_MAX_VIDEO_WIDTH_DEPRECATED:
        GST_WARNING_OBJECT(object, "MaxVideoWidth property is deprecated. Use 'max-video-width' instead");
    case PROP_MAX_VIDEO_WIDTH:
    {
        g_value_set_uint(value, priv->maxWidth);
        break;
    }
    case PROP_MAX_VIDEO_HEIGHT_DEPRECATED:
        GST_WARNING_OBJECT(object, "MaxVideoHeight property is deprecated. Use 'max-video-height' instead");
    case PROP_MAX_VIDEO_HEIGHT:
    {
        g_value_set_uint(value, priv->maxHeight);
        break;
    }
    case PROP_FRAME_STEP_ON_PREROLL:
    {
        g_value_set_boolean(value, priv->stepOnPrerollEnabled);
        break;
    }
    case PROP_IMMEDIATE_OUTPUT:
    {
        std::unique_lock lock{priv->propertyMutex};
        auto client = basePriv->m_mediaPlayerManager.getMediaPlayerClient();
        if (!client)
        {
            // Return the default value and
            // queue a setting event (for the default value) so that it will become true when
            // the client connects...
            GST_DEBUG_OBJECT(object, "Return default immediate-output setting, and queue an event to set the default "
                                     "upon client connect");
            priv->immediateOutputQueued = true;
            g_value_set_boolean(value, priv->immediateOutput);
        }
        else
        {
            bool immediateOutput{priv->immediateOutput};
            lock.unlock();
            if (!client->getImmediateOutput(sink->parent.priv->m_sourceId, immediateOutput))
            {
                GST_ERROR_OBJECT(sink, "Could not get immediate-output");
            }
            g_value_set_boolean(value, immediateOutput);
        }
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
}

static void rialto_mse_video_sink_set_property(GObject *object, guint propId, const GValue *value, GParamSpec *pspec)
{
    RialtoMSEVideoSink *sink = RIALTO_MSE_VIDEO_SINK(object);
    if (!sink)
    {
        GST_ERROR_OBJECT(object, "Sink not initalised");
        return;
    }
    RialtoMSEVideoSinkPrivate *priv = sink->priv;
    RialtoMSEBaseSinkPrivate *basePriv = sink->parent.priv;
    if (!priv || !basePriv)
    {
        GST_ERROR_OBJECT(object, "Private sink not initalised");
        return;
    }

    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = basePriv->m_mediaPlayerManager.getMediaPlayerClient();

    switch (propId)
    {
    case PROP_WINDOW_SET:
    {
        const gchar *rectangle = g_value_get_string(value);
        if (!rectangle)
        {
            GST_WARNING_OBJECT(object, "Rectangle string not valid");
            break;
        }
        std::string videoRectangle{rectangle};
        std::unique_lock lock{priv->propertyMutex};
        priv->videoRectangle = videoRectangle;
        if (!client)
        {
            GST_DEBUG_OBJECT(object, "Rectangle setting enqueued");
            priv->rectangleSettingQueued = true;
        }
        else
        {
            lock.unlock();
            client->setVideoRectangle(videoRectangle);
        }
        break;
    }
    case PROP_MAX_VIDEO_WIDTH:
    case PROP_MAX_VIDEO_WIDTH_DEPRECATED:
        priv->maxWidth = g_value_get_uint(value);
        break;
    case PROP_MAX_VIDEO_HEIGHT:
    case PROP_MAX_VIDEO_HEIGHT_DEPRECATED:
        priv->maxHeight = g_value_get_uint(value);
        break;
    case PROP_FRAME_STEP_ON_PREROLL:
    {
        bool stepOnPrerollEnabled = g_value_get_boolean(value);
        if (client && stepOnPrerollEnabled && !priv->stepOnPrerollEnabled)
        {
            GST_INFO_OBJECT(object, "Frame stepping on preroll");
            client->renderFrame(RIALTO_MSE_BASE_SINK(sink));
        }
        priv->stepOnPrerollEnabled = stepOnPrerollEnabled;
        break;
    }
    case PROP_IMMEDIATE_OUTPUT:
    {
        bool immediateOutput = (g_value_get_boolean(value) != FALSE);
        std::unique_lock lock{priv->propertyMutex};
        priv->immediateOutput = immediateOutput;
        if (!client)
        {
            GST_DEBUG_OBJECT(sink, "Immediate output setting enqueued");
            priv->immediateOutputQueued = true;
        }
        else
        {
            lock.unlock();
            if (!client->setImmediateOutput(basePriv->m_sourceId, immediateOutput))
            {
                GST_ERROR_OBJECT(sink, "Could not set immediate-output");
            }
        }
        break;
    }
    case PROP_SYNCMODE_STREAMING:
    {
        bool syncmodeStreaming = (g_value_get_boolean(value) != FALSE);
        std::unique_lock lock{priv->propertyMutex};
        priv->syncmodeStreaming = syncmodeStreaming;
        if (!client)
        {
            GST_DEBUG_OBJECT(sink, "Syncmode streaming setting enqueued");
            priv->syncmodeStreamingQueued = true;
        }
        else
        {
            lock.unlock();
            if (!client->setStreamSyncMode(basePriv->m_sourceId, syncmodeStreaming))
            {
                GST_ERROR_OBJECT(sink, "Could not set syncmode-streaming");
            }
        }
        break;
    }
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
    gst_object_unref(bus);
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

    basePriv->m_mediaSourceType = firebolt::rialto::MediaSourceType::VIDEO;
    basePriv->m_isAsync = true;
    gst_pad_set_chain_function(basePriv->m_sinkPad, rialto_mse_base_sink_chain);
    gst_pad_set_event_function(basePriv->m_sinkPad, rialto_mse_video_sink_event);

    basePriv->m_callbacks.qosCallback = std::bind(rialto_mse_video_sink_qos_handle, GST_ELEMENT_CAST(sink),
                                                  std::placeholders::_1, std::placeholders::_2);
}

static void rialto_mse_video_sink_class_init(RialtoMSEVideoSinkClass *klass)
{
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    GstElementClass *elementClass = GST_ELEMENT_CLASS(klass);
    gobjectClass->get_property = rialto_mse_video_sink_get_property;
    gobjectClass->set_property = rialto_mse_video_sink_set_property;
    elementClass->change_state = rialto_mse_video_sink_change_state;

    g_object_class_install_property(gobjectClass, PROP_WINDOW_SET,
                                    g_param_spec_string("rectangle", "rectangle", "Window Set Format: x,y,width,height",
                                                        nullptr, GParamFlags(G_PARAM_READWRITE)));

    g_object_class_install_property(gobjectClass, PROP_MAX_VIDEO_WIDTH,
                                    g_param_spec_uint("max-video-width",
                                                      "max video width", "Maximum width of video frames to be decoded. Should only be set for video only streams.",
                                                      0, 3840, DEFAULT_MAX_VIDEO_WIDTH, GParamFlags(G_PARAM_READWRITE)));

    g_object_class_install_property(gobjectClass, PROP_MAX_VIDEO_HEIGHT,
                                    g_param_spec_uint("max-video-height",
                                                      "max video height", "Maximum height of video frames to be decoded. should only be set for video only streams.",
                                                      0, 2160, DEFAULT_MAX_VIDEO_HEIGHT, GParamFlags(G_PARAM_READWRITE)));

    g_object_class_install_property(gobjectClass, PROP_MAX_VIDEO_WIDTH_DEPRECATED,
                                    g_param_spec_uint("maxVideoWidth", "maxVideoWidth", "[DEPRECATED] Use max-video-width",
                                                      0, 3840, DEFAULT_MAX_VIDEO_WIDTH, GParamFlags(G_PARAM_READWRITE)));

    g_object_class_install_property(gobjectClass, PROP_MAX_VIDEO_HEIGHT_DEPRECATED,
                                    g_param_spec_uint("maxVideoHeight", "maxVideoHeight", "[DEPRECATED] max-video-height",
                                                      0, 2160, DEFAULT_MAX_VIDEO_HEIGHT, GParamFlags(G_PARAM_READWRITE)));

    g_object_class_install_property(gobjectClass, PROP_FRAME_STEP_ON_PREROLL,
                                    g_param_spec_boolean("frame-step-on-preroll", "frame step on preroll",
                                                         "allow frame stepping on preroll into pause", FALSE,
                                                         G_PARAM_READWRITE));

    std::unique_ptr<firebolt::rialto::IMediaPipelineCapabilities> mediaPlayerCapabilities =
        firebolt::rialto::IMediaPipelineCapabilitiesFactory::createFactory()->createMediaPipelineCapabilities();
    if (mediaPlayerCapabilities)
    {
        std::vector<std::string> supportedMimeTypes =
            mediaPlayerCapabilities->getSupportedMimeTypes(firebolt::rialto::MediaSourceType::VIDEO);

        rialto_mse_sink_setup_supported_caps(elementClass, supportedMimeTypes);

        const std::string kImmediateOutputPropertyName{"immediate-output"};
        const std::string kSyncmodeStreamingPropertyName{"syncmode-streaming"};
        const std::vector<std::string> kPropertyNamesToSearch{kImmediateOutputPropertyName,
                                                              kSyncmodeStreamingPropertyName};
        std::vector<std::string> supportedProperties{
            mediaPlayerCapabilities->getSupportedProperties(firebolt::rialto::MediaSourceType::VIDEO,
                                                            kPropertyNamesToSearch)};

        for (const auto &propertyName : supportedProperties)
        {
            if (kImmediateOutputPropertyName == propertyName)
            {
                g_object_class_install_property(gobjectClass, PROP_IMMEDIATE_OUTPUT,
                                                g_param_spec_boolean(kImmediateOutputPropertyName.c_str(),
                                                                     "immediate output", "immediate output", TRUE,
                                                                     GParamFlags(G_PARAM_READWRITE)));
            }
            else if (kSyncmodeStreamingPropertyName == propertyName)
            {
                g_object_class_install_property(gobjectClass, PROP_SYNCMODE_STREAMING,
                                                g_param_spec_boolean("syncmode-streaming", "Streaming Sync Mode",
                                                                     "Enable/disable OTT streaming sync mode", FALSE,
                                                                     G_PARAM_WRITABLE));
            }
        }
    }
    else
    {
        GST_ERROR("Failed to get supported mime types for VIDEO");
    }

    gst_element_class_set_details_simple(elementClass, "Rialto Video Sink", "Decoder/Video/Sink/Video",
                                         "Communicates with Rialto Server", "Sky");
}
