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
enum
{
    PROP_0,
    PROP_TS_OFFSET,
    PROP_LAST
};

static void rialto_web_audio_async_start(RialtoWebAudioSink *sink)
{
    sink->priv->m_isStateCommitNeeded = true;
    gst_element_post_message(GST_ELEMENT_CAST(sink), gst_message_new_async_start(GST_OBJECT(sink)));
}

static void rialto_web_audio_async_done(RialtoWebAudioSink *sink)
{
    sink->priv->m_isStateCommitNeeded = false;
    gst_element_post_message(GST_ELEMENT_CAST(sink),
                             gst_message_new_async_done(GST_OBJECT_CAST(sink), GST_CLOCK_TIME_NONE));
}

static void rialto_web_audio_sink_eos_handler(RialtoWebAudioSink *sink)
{
    GstState currentState = GST_STATE(sink);
    if ((currentState != GST_STATE_PAUSED) && (currentState != GST_STATE_PLAYING))
    {
        GST_ERROR_OBJECT(sink, "Sink cannot post a EOS message in state '%s', posting an error instead",
                         gst_element_state_get_name(currentState));

        const char *errMessage = "Web audio sink received EOS in non-playing state";
        gst_element_post_message(GST_ELEMENT_CAST(sink),
                                 gst_message_new_error(GST_OBJECT_CAST(sink),
                                                       g_error_new_literal(GST_STREAM_ERROR, 0, errMessage), errMessage));
    }
    else
    {
        gst_element_post_message(GST_ELEMENT_CAST(sink), gst_message_new_eos(GST_OBJECT_CAST(sink)));
    }
}

static void rialto_web_audio_sink_error_handler(RialtoWebAudioSink *sink, const char *message)
{
    gst_element_post_message(GST_ELEMENT_CAST(sink),
                             gst_message_new_error(GST_OBJECT_CAST(sink),
                                                   g_error_new_literal(GST_STREAM_ERROR, 0, message), message));
}

static void rialto_web_audio_sink_rialto_state_changed_handler(RialtoWebAudioSink *sink,
                                                               firebolt::rialto::WebAudioPlayerState state)
{
    GstState current = GST_STATE(sink);
    GstState next = GST_STATE_NEXT(sink);
    GstState pending = GST_STATE_PENDING(sink);

    GST_DEBUG_OBJECT(sink,
                     "Received server's state change to %u. Sink's states are: current state: %s next state: %s "
                     "pending state: %s, last return state %s",
                     static_cast<uint32_t>(state), gst_element_state_get_name(current),
                     gst_element_state_get_name(next), gst_element_state_get_name(pending),
                     gst_element_state_change_return_get_name(GST_STATE_RETURN(sink)));

    if (sink->priv->m_isStateCommitNeeded &&
        ((state == firebolt::rialto::WebAudioPlayerState::PAUSED && next == GST_STATE_PAUSED) ||
         (state == firebolt::rialto::WebAudioPlayerState::PLAYING && next == GST_STATE_PLAYING)))
    {
        GstState postNext = next == pending ? GST_STATE_VOID_PENDING : pending;
        GST_STATE(sink) = next;
        GST_STATE_NEXT(sink) = postNext;
        GST_STATE_PENDING(sink) = GST_STATE_VOID_PENDING;
        GST_STATE_RETURN(sink) = GST_STATE_CHANGE_SUCCESS;

        GST_INFO_OBJECT(sink, "Async state transition to state %s done", gst_element_state_get_name(next));

        gst_element_post_message(GST_ELEMENT_CAST(sink),
                                 gst_message_new_state_changed(GST_OBJECT_CAST(sink), current, next, pending));
        rialto_web_audio_async_done(sink);
    }
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

        if (!sink->priv->m_rialtoControlClient->waitForRunning())
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
        if (!sink->priv->m_webAudioClient->isOpen())
        {
            GST_INFO_OBJECT(sink, "Delay playing until the caps are recieved and the player is opened");
            sink->priv->m_isPlayingDelayed = true;
            result = GST_STATE_CHANGE_ASYNC;
            rialto_web_audio_async_start(sink);
        }
        else
        {
            if (!sink->priv->m_webAudioClient->play())
            {
                GST_ERROR_OBJECT(sink, "Failed to play web audio");
                result = GST_STATE_CHANGE_FAILURE;
            }
            else
            {
                result = GST_STATE_CHANGE_ASYNC;
                rialto_web_audio_async_start(sink);
            }
        }
        break;
    }
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
    {
        GST_DEBUG("GST_STATE_CHANGE_PLAYING_TO_PAUSED");
        if (!sink->priv->m_webAudioClient->pause())
        {
            GST_ERROR_OBJECT(sink, "Failed to pause web audio");
            result = GST_STATE_CHANGE_FAILURE;
        }
        else
        {
            result = GST_STATE_CHANGE_ASYNC;
            rialto_web_audio_async_start(sink);
        }
        break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    {
        GST_DEBUG("GST_STATE_CHANGE_PAUSED_TO_READY");
        if (!sink->priv->m_webAudioClient->close())
        {
            GST_ERROR_OBJECT(sink, "Failed to close web audio");
            result = GST_STATE_CHANGE_FAILURE;
        }
        break;
    }
    case GST_STATE_CHANGE_READY_TO_NULL:
    {
        GST_DEBUG("GST_STATE_CHANGE_READY_TO_NULL");

        sink->priv->m_rialtoControlClient->removeControlBackend();
    }
    default:
        break;
    }

    gst_object_unref(sinkPad);

    if (result == GST_STATE_CHANGE_SUCCESS)
    {
        GstStateChangeReturn stateChangeRet = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
        if (G_UNLIKELY(stateChangeRet == GST_STATE_CHANGE_FAILURE))
        {
            GST_WARNING_OBJECT(sink, "State change failed");
            return stateChangeRet;
        }
    }

    return result;
}

static gboolean rialto_web_audio_sink_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
    RialtoWebAudioSink *sink = RIALTO_WEB_AUDIO_SINK(parent);
    bool result = false;
    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_EOS:
    {
        GST_DEBUG("GST_EVENT_EOS");
        result = sink->priv->m_webAudioClient->setEos();
        gst_event_unref(event);
        break;
    }
    case GST_EVENT_CAPS:
    {
        GstCaps *caps;
        gst_event_parse_caps(event, &caps);
        GST_INFO_OBJECT(sink, "Opening WebAudio with caps %" GST_PTR_FORMAT, caps);

        if (!sink->priv->m_webAudioClient->open(caps))
        {
            GST_ERROR_OBJECT(sink, "Failed to open web audio");
        }
        else if (sink->priv->m_isPlayingDelayed)
        {
            if (!sink->priv->m_webAudioClient->play())
            {
                GST_ERROR_OBJECT(sink, "Failed to play web audio");
            }
            else
            {
                sink->priv->m_isPlayingDelayed = false;
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

static void rialto_web_audio_sink_get_property(GObject *object, guint propId, GValue *value, GParamSpec *pspec)
{
    switch (propId)
    {
    case PROP_TS_OFFSET:
    {
        GST_INFO_OBJECT(object, "ts-offset property not supported, RialtoWebAudioSink does not require the "
                                "synchronisation of sources");
        break;
    }

    default:
    {
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
    }
}

static void rialto_web_audio_sink_set_property(GObject *object, guint propId, const GValue *value, GParamSpec *pspec)
{
    switch (propId)
    {
    case PROP_TS_OFFSET:
    {
        GST_INFO_OBJECT(object, "ts-offset property not supported, RialtoWebAudioSink does not require the "
                                "synchronisation of sources");
        break;
    }
    default:
    {
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
    }
}

static GstFlowReturn rialto_web_audio_sink_chain(GstPad *pad, GstObject *parent, GstBuffer *buf)
{
    RialtoWebAudioSink *sink = RIALTO_WEB_AUDIO_SINK(parent);
    bool res = sink->priv->m_webAudioClient->notifyNewSample(buf);
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

static bool rialto_web_audio_sink_initialise_sinkpad(RialtoWebAudioSink *sink)
{
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

    WebAudioSinkCallbacks callbacks;
    callbacks.eosCallback = std::bind(rialto_web_audio_sink_eos_handler, sink);
    callbacks.stateChangedCallback =
        std::bind(rialto_web_audio_sink_rialto_state_changed_handler, sink, std::placeholders::_1);
    callbacks.errorCallback = std::bind(rialto_web_audio_sink_error_handler, sink, std::placeholders::_1);

    sink->priv->m_rialtoControlClient = std::make_unique<firebolt::rialto::client::ControlBackend>();
    sink->priv->m_webAudioClient = std::make_shared<GStreamerWebAudioPlayerClient>(callbacks);

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
    sink->priv->m_webAudioClient = nullptr;
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
    gobjectClass->get_property = rialto_web_audio_sink_get_property;
    gobjectClass->set_property = rialto_web_audio_sink_set_property;

    elementClass->change_state = rialto_web_audio_sink_change_state;
    elementClass->send_event = rialto_web_audio_sink_send_event;

    g_object_class_install_property(gobjectClass, PROP_TS_OFFSET,
                                    g_param_spec_int64("ts-offset",
                                                       "ts-offset", "Not supported, RialtoWebAudioSink does not require the synchronisation of sources",
                                                       G_MININT64, G_MAXINT64, 0,
                                                       GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    rialto_web_audio_sink_setup_supported_caps(elementClass);

    gst_element_class_set_details_simple(elementClass, "Rialto Web Audio Sink", "Decoder/Audio/Sink/Audio",
                                         "Communicates with Rialto Server", "Sky");
}
