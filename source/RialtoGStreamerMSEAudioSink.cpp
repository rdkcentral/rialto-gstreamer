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
        firebolt::rialto::IMediaPipeline::MediaSource vsource(-1, firebolt::rialto::MediaSourceType::AUDIO, "");
        if (!priv->m_mediaPlayerManager.getMediaPlayerClient()->attachSource(vsource, sink))
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
        firebolt::rialto::IMediaPipeline::MediaSource vsource(-1, firebolt::rialto::MediaSourceType::AUDIO, capsStr);
        g_free(capsStr);

        if (!sink->priv->m_mediaPlayerManager.getMediaPlayerClient()->attachSource(vsource, sink))
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

    if (!priv->m_mediaPlayerManager.getMediaPlayerClient())
    {
        GST_ERROR_OBJECT(sink, "Failed to initialise AUDIO sink. There's no media player client.");
        return;
    }

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
