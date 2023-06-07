/*
 * Copyright (C) 2023 Sky UK
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

#include "RialtoGStreamerWebAudioSink.h"
#include "ControlBackend.h"
#include "GStreamerWebAudioPlayerClient.h"
#include <gst/gst.h>

using namespace firebolt::rialto::client;

GST_DEBUG_CATEGORY_STATIC(RialtoWebAudioSinkDebug);
#define GST_CAT_DEFAULT RialtoWebAudioSinkDebug

#define rialto_web_audio_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE(RialtoWebAudioSink, rialto_web_audio_sink, GST_TYPE_ELEMENT,
                        G_ADD_PRIVATE(RialtoWebAudioSink)
                            GST_DEBUG_CATEGORY_INIT(RialtoWebAudioSinkDebug, "rialtowebaudiosink", 0,
                                                    "rialto web audio sink"));

static void rialto_mse_base_async_start(RialtoWebAudioSink *sink)
{
    sink->priv->mIsStateCommitNeeded = true;
    gst_element_post_message(GST_ELEMENT_CAST(sink), gst_message_new_async_start(GST_OBJECT(sink)));
}

static void rialto_mse_base_async_done(RialtoWebAudioSink *sink)
{
    sink->priv->mIsStateCommitNeeded = false;
    gst_element_post_message(GST_ELEMENT_CAST(sink),
                             gst_message_new_async_done(GST_OBJECT_CAST(sink), GST_CLOCK_TIME_NONE));
}

static void rialto_web_audio_sink_setup_supported_caps(GstElementClass *elementClass)
{
    GstCaps *caps = gst_caps_from_string("audio/x-raw");
    GstPadTemplate *sinktempl = gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps);
    gst_element_class_add_pad_template(elementClass, sinktempl);
    gst_caps_unref(caps);
}

static gboolean rialto_web_audio_sink_send_event(GstElement *element, GstEvent *event)
{
    RialtoWebAudioSink *sink = RIALTO_WEB_AUDIO_SINK(element);
    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_CAPS:
    {
        GstCaps *caps;
        gst_event_parse_caps(event, &caps);
        GST_INFO_OBJECT(sink, "Attaching AUDIO source with caps %" GST_PTR_FORMAT, caps);
    }
    default:
        break;
    }
    GstElement *parent = GST_ELEMENT(&sink->parent);
    return GST_ELEMENT_CLASS(parent_class)->send_event(parent, event);
}

static GstStateChangeReturn rialto_web_audio_sink_change_state(GstElement *element, GstStateChange transition)
{
    RialtoWebAudioSink *sink = RIALTO_WEB_AUDIO_SINK(element);
    GstPad *sinkPad = gst_element_get_static_pad(GST_ELEMENT_CAST(sink), "sink");

    GstState current_state = GST_STATE_TRANSITION_CURRENT(transition);
    GstState next_state = GST_STATE_TRANSITION_NEXT(transition);
    GST_INFO_OBJECT(sink, "State change: (%s) -> (%s)", gst_element_state_get_name(current_state),
                    gst_element_state_get_name(next_state));

    GstStateChangeReturn result = GST_STATE_CHANGE_SUCCESS;
    switch (transition)
    {
    case GST_STATE_CHANGE_NULL_TO_READY:
    {
        GST_DEBUG("GST_STATE_CHANGE_NULL_TO_READY");

        if (!sink->priv->mRialtoControlClient->waitForRunning())
        {
            GST_ERROR_OBJECT(sink, "Rialto client cannot reach running state");
            result = GST_STATE_CHANGE_FAILURE;
        }
        break;
    }
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    {
        GST_DEBUG("GST_STATE_CHANGE_READY_TO_PAUSED");
        break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
    {
        GST_DEBUG("GST_STATE_CHANGE_PAUSED_TO_PLAYING");
        if (!sink->priv->mWebAudioClient->isOpen())
        {
            GST_INFO_OBJECT(sink, "Delay playing until the caps are recieved and the player is opened");
            rialto_mse_base_async_start(sink);
            result = GST_STATE_CHANGE_ASYNC;
        }
        else
        {
            if (!sink->priv->mWebAudioClient->play())
            {
                GST_ERROR_OBJECT(sink, "Failed to play web audio");
                result = GST_STATE_CHANGE_FAILURE;
            }
        }
        break;
    }
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
    {
        GST_DEBUG("GST_STATE_CHANGE_PLAYING_TO_PAUSED");
        if (!sink->priv->mWebAudioClient->pause())
        {
            GST_ERROR_OBJECT(sink, "Failed to pause web audio");
            result = GST_STATE_CHANGE_FAILURE;
        }
        break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    {
        GST_DEBUG("GST_STATE_CHANGE_PAUSED_TO_READY");
        if (!sink->priv->mWebAudioClient->close())
        {
            GST_ERROR_OBJECT(sink, "Failed to close web audio");
            result = GST_STATE_CHANGE_FAILURE;
        }
        break;
    }
    case GST_STATE_CHANGE_READY_TO_NULL:
    {
        GST_DEBUG("GST_STATE_CHANGE_READY_TO_NULL");

        sink->priv->mRialtoControlClient->removeControlBackend();
    }
    default:
        break;
    }

    gst_object_unref(sinkPad);

    if (result == GST_STATE_CHANGE_SUCCESS)
    {
        GstStateChangeReturn result = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
        if (G_UNLIKELY(result == GST_STATE_CHANGE_FAILURE))
        {
            GST_WARNING_OBJECT(sink, "State change failed");
            return result;
        }
    }

    return result;
}

static gboolean rialto_web_audio_sink_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
    RialtoWebAudioSink *sink = RIALTO_WEB_AUDIO_SINK(parent);
    GST_ERROR("lukewill: Event %s", gst_event_type_get_name(GST_EVENT_TYPE(event)));
    bool result = false;
    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_EOS:
    {
        GST_DEBUG("GST_EVENT_EOS");
        result = sink->priv->mWebAudioClient->setEos();
        gst_event_unref(event);
        break;
    }
    case GST_EVENT_CAPS:
    {
        GstCaps *caps;
        gst_event_parse_caps(event, &caps);
        GST_INFO_OBJECT(sink, "Opening WebAudio with caps %" GST_PTR_FORMAT, caps);

        if (!sink->priv->mWebAudioClient->open(caps))
        {
            GST_ERROR_OBJECT(sink, "Failed to open web audio");
        }
        else if (sink->priv->mIsStateCommitNeeded)
        {
            rialto_mse_base_async_done(sink);
            if (!sink->priv->mWebAudioClient->play())
            {
                GST_ERROR_OBJECT(sink, "Failed to play web audio");
            }
            else
            {
                result = true;
            }
        }
        else
        {
            result = true;
        }
        break;
    }
    default:
        result = gst_pad_event_default(pad, parent, event);
        break;
    }
    return result;
}

static GstFlowReturn rialto_web_audio_sink_chain(GstPad *pad, GstObject *parent, GstBuffer *buf)
{
    RialtoWebAudioSink *sink = RIALTO_WEB_AUDIO_SINK(parent);
    bool res = sink->priv->mWebAudioClient->notifyNewSample(buf);
    if (res)
    {
        return GST_FLOW_OK;
    }
    else
    {
        GST_ERROR_OBJECT(sink, "Failed to push sample");
        return GST_FLOW_ERROR;
    }
}

void func(void* data, void* userData)
{
    GST_ERROR("lukewill: %s", GST_PAD_TEMPLATE_NAME_TEMPLATE(GST_PAD_TEMPLATE(data)));
}

static bool rialto_web_audio_sink_initialise_sinkpad(RialtoWebAudioSink *sink)
{
    GList *list = gst_element_class_get_pad_template_list(GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink)));
    g_list_foreach(list, func, nullptr);

    GstPadTemplate *pad_template =
        gst_element_class_get_pad_template(GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink)), "sink");
    if (!pad_template)
    {
        GST_ERROR_OBJECT(sink, "Could not find sink pad template");
        return false;
    }

    GstPad *sinkPad = gst_pad_new_from_template(pad_template, "sink");
    if (!sinkPad)
    {
        GST_ERROR_OBJECT(sink, "Could not create sinkpad");
        return false;
    }

    gst_element_add_pad(GST_ELEMENT_CAST(sink), sinkPad);

    gst_pad_set_event_function(sinkPad, rialto_web_audio_sink_event);
    gst_pad_set_chain_function(sinkPad, rialto_web_audio_sink_chain);

    return true;
}

static void rialto_web_audio_sink_init(RialtoWebAudioSink *sink)
{
    sink->priv = static_cast<RialtoWebAudioSinkPrivate *>(rialto_web_audio_sink_get_instance_private(sink));
    new (sink->priv) RialtoWebAudioSinkPrivate();

    sink->priv->mRialtoControlClient = std::make_unique<firebolt::rialto::client::ControlBackend>();

    sink->priv->mWebAudioClient = std::make_shared<GStreamerWebAudioPlayerClient>(GST_ELEMENT(sink));

    if (!rialto_web_audio_sink_initialise_sinkpad(sink))
    {
        GST_ERROR_OBJECT(sink, "Failed to initialise AUDIO sink. Sink pad initialisation failed.");
        return;
    }
}

static void rialto_web_audio_sink_finalize(GObject *object)
{
    RialtoWebAudioSink *sink = RIALTO_WEB_AUDIO_SINK(object);
    RialtoWebAudioSinkPrivate *priv = sink->priv;
    sink->priv->mWebAudioClient = nullptr;
    GST_INFO_OBJECT(sink, "Finalize: %" GST_PTR_FORMAT " %" GST_PTR_FORMAT, sink, priv);

    priv->~RialtoWebAudioSinkPrivate();

    GST_CALL_PARENT(G_OBJECT_CLASS, finalize, (object));
}

static void rialto_web_audio_sink_class_init(RialtoWebAudioSinkClass *klass)
{
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    GstElementClass *elementClass = GST_ELEMENT_CLASS(klass);

    gst_element_class_set_metadata(elementClass, "Rialto Web Audio sink", "Generic", "A sink for Rialto Web Audio",
                                   "Sky");

    gobjectClass->finalize = rialto_web_audio_sink_finalize;

    elementClass->change_state = rialto_web_audio_sink_change_state;
    elementClass->send_event = rialto_web_audio_sink_send_event;

    rialto_web_audio_sink_setup_supported_caps(elementClass);

    gst_element_class_set_details_simple(elementClass, "Rialto Web Audio Sink", "Decoder/Audio/Sink/Audio",
                                         "Communicates with Rialto Server", "Sky");
}
