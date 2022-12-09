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

#define USE_GLIB 1

#include "RialtoGStreamerMSEBaseSink.h"
#include "GStreamerUtils.h"
#include "RialtoControlClientBackend.h"
#include "RialtoGStreamerMSEBaseSinkPrivate.h"
#include <IMediaPipeline.h>
#include <gst/gst.h>

GST_DEBUG_CATEGORY_STATIC(RialtoMSEBaseSinkDebug);
#define GST_CAT_DEFAULT RialtoMSEBaseSinkDebug

#define rialto_mse_base_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE(RialtoMSEBaseSink, rialto_mse_base_sink, GST_TYPE_ELEMENT,
                        G_ADD_PRIVATE(RialtoMSEBaseSink)
                            GST_DEBUG_CATEGORY_INIT(RialtoMSEBaseSinkDebug, "rialtomsebasesink", 0,
                                                    "rialto mse base sink"));

enum
{
    Prop0,
    PropSync,
    PropLast
};

enum
{
    PROP_0,
    PROP_LOCATION,
    PROP_HANDLE_RESET_TIME_MESSAGE,
    PROP_LAST
};

static void rialto_mse_base_sink_eos_handler(RialtoMSEBaseSink *sink)
{
    gst_element_post_message(GST_ELEMENT_CAST(sink), gst_message_new_eos(GST_OBJECT_CAST(sink)));
}

static void rialto_mse_base_sink_rialto_state_changed_handler(RialtoMSEBaseSink *sink,
                                                              firebolt::rialto::PlaybackState state)
{
    GstState current = GST_STATE(sink);
    GstState next = GST_STATE_NEXT(sink);
    GstState pending = GST_STATE_PENDING(sink);
    GstState postNext = next == pending ? GST_STATE_VOID_PENDING : pending;

    GST_DEBUG_OBJECT(sink,
                     "Received server's state change to %u. Sink's states are: current state: %s next state: %s "
                     "pending state: %s, last return state %s",
                     static_cast<uint32_t>(state), gst_element_state_get_name(current),
                     gst_element_state_get_name(next), gst_element_state_get_name(pending),
                     gst_element_state_change_return_get_name(GST_STATE_RETURN(sink)));

    if ((state == firebolt::rialto::PlaybackState::PAUSED && next == GST_STATE_PAUSED) ||
        (state == firebolt::rialto::PlaybackState::PLAYING && next == GST_STATE_PLAYING))
    {
        GST_STATE(sink) = next;
        GST_STATE_NEXT(sink) = postNext;
        GST_STATE_PENDING(sink) = GST_STATE_VOID_PENDING;
        GST_STATE_RETURN(sink) = GST_STATE_CHANGE_SUCCESS;

        GST_INFO_OBJECT(sink, "Async state transition to state %s done", gst_element_state_get_name(next));

        gst_element_post_message(GST_ELEMENT_CAST(sink),
                                 gst_message_new_state_changed(GST_OBJECT_CAST(sink), current, next, pending));
        gst_element_post_message(GST_ELEMENT_CAST(sink),
                                 gst_message_new_async_done(GST_OBJECT_CAST(sink), GST_CLOCK_TIME_NONE));
    }
}

static void rialto_mse_base_sink_seek_completed_handler(RialtoMSEBaseSink *sink)
{
    GST_INFO_OBJECT(sink, "Seek completed");
    std::unique_lock<std::mutex> lock(sink->priv->mSeekMutex);
    sink->priv->mSeekCondVariable.notify_all();
}

static void rialto_mse_base_sink_init(RialtoMSEBaseSink *sink)
{
    GST_INFO_OBJECT(sink, "Init: %" GST_PTR_FORMAT, sink);
    sink->priv = static_cast<RialtoMSEBaseSinkPrivate *>(rialto_mse_base_sink_get_instance_private(sink));
    new (sink->priv) RialtoMSEBaseSinkPrivate();

    sink->priv->m_rialtoControlClient = std::make_unique<firebolt::rialto::client::RialtoControlClientBackend>();

    RialtoGStreamerMSEBaseSinkCallbacks callbacks;
    callbacks.eosCallback = std::bind(rialto_mse_base_sink_eos_handler, sink);
    callbacks.seekCompletedCallback = std::bind(rialto_mse_base_sink_seek_completed_handler, sink);
    callbacks.stateChangedCallback =
        std::bind(rialto_mse_base_sink_rialto_state_changed_handler, sink, std::placeholders::_1);
    sink->priv->mCallbacks = callbacks;
    gst_segment_init(&sink->priv->mLastSegment, GST_FORMAT_TIME);
    GST_OBJECT_FLAG_SET(sink, GST_ELEMENT_FLAG_SINK);
}

static void rialto_mse_base_sink_finalize(GObject *object)
{
    RialtoMSEBaseSink *sink = RIALTO_MSE_BASE_SINK(object);
    RialtoMSEBaseSinkPrivate *priv = sink->priv;
    GST_INFO_OBJECT(sink, "Finalize: %" GST_PTR_FORMAT " %" GST_PTR_FORMAT, sink, priv);

    priv->~RialtoMSEBaseSinkPrivate();
    GST_CALL_PARENT(G_OBJECT_CLASS, finalize, (object));
}

static void rialto_mse_base_sink_get_property(GObject *object, guint propId, GValue *value, GParamSpec *pspec)
{
    RialtoMSEBaseSink *sink = RIALTO_MSE_BASE_SINK(object);

    switch (propId)
    {
    case PROP_LOCATION:
        g_value_set_string(value, sink->priv->mUri.c_str());
        break;
    case PROP_HANDLE_RESET_TIME_MESSAGE:
        g_value_set_boolean(value, sink->priv->mHandleResetTimeMessage ? TRUE : FALSE);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
}

static void rialto_mse_base_sink_set_property(GObject *object, guint propId, const GValue *value, GParamSpec *pspec)
{
    RialtoMSEBaseSink *sink = RIALTO_MSE_BASE_SINK(object);

    switch (propId)
    {
    case PROP_LOCATION:
        sink->priv->mUri = g_value_get_string(value);
        break;
    case PROP_HANDLE_RESET_TIME_MESSAGE:
        sink->priv->mHandleResetTimeMessage = g_value_get_boolean(value) != FALSE;
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
}

static gboolean rialto_mse_base_sink_query(GstElement *element, GstQuery *query)
{
    RialtoMSEBaseSink *sink = RIALTO_MSE_BASE_SINK(element);
    GST_DEBUG_OBJECT(sink, "handling query '%s'", GST_QUERY_TYPE_NAME(query));
    switch (GST_QUERY_TYPE(query))
    {
    case GST_QUERY_SEEKING:
    {
        GstFormat fmt;
        gst_query_parse_seeking(query, &fmt, NULL, NULL, NULL);
        gst_query_set_seeking(query, fmt, FALSE, 0, -1);
        return TRUE;
    }
    case GST_QUERY_POSITION:
    {
        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = sink->priv->m_mediaPlayerManager.getMediaPlayerClient();
        if ((!client) || (!sink->priv->m_mediaPlayerManager.hasControl()))
        {
            return FALSE;
        }

        GstFormat fmt;
        gst_query_parse_position(query, &fmt, NULL);
        switch (fmt)
        {
        case GST_FORMAT_TIME:
        {
            gint64 position = client->getPosition();
            GST_DEBUG_OBJECT(sink, "Queried position is %" GST_TIME_FORMAT, GST_TIME_ARGS(position));
            if (position < 0)
            {
                return FALSE;
            }
            gst_query_set_position(query, fmt, position);
            break;
        }
        default:
            break;
        }
        return TRUE;
    }
    default:
        break;
    }

    GstElement *parent = GST_ELEMENT(&sink->parent);
    return GST_ELEMENT_CLASS(parent_class)->query(parent, query);
}

static void rialto_mse_base_sink_change_playback_rate(RialtoMSEBaseSink *sink, GstEvent *event)
{
    const GstStructure *structure{gst_event_get_structure(event)};
    gdouble playbackRate{1.0};
    if (gst_structure_get_double(structure, "rate", &playbackRate) == TRUE)
    {
        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = sink->priv->m_mediaPlayerManager.getMediaPlayerClient();
        if ((client) && (sink->priv->m_mediaPlayerManager.hasControl()))
        {
            GST_DEBUG_OBJECT(sink, "Instant playback rate change: %.2f", playbackRate);
            client->setPlaybackRate(playbackRate);
        }
    }
}

static void rialto_mse_base_sink_flush_start(RialtoMSEBaseSink *sink)
{
    std::lock_guard<std::mutex> lock(sink->priv->mSinkMutex);
    if (!sink->priv->mIsFlushOngoing)
    {
        GST_INFO_OBJECT(sink, "Starting flushing");
        sink->priv->mIsEos = false;
        sink->priv->mIsFlushOngoing = true;
        sink->priv->clearBuffersUnlocked();
    }
}

static void rialto_mse_base_sink_flush_stop(RialtoMSEBaseSink *sink, bool resetTime)
{
    GST_INFO_OBJECT(sink, "Stopping flushing");
    std::lock_guard<std::mutex> lock(sink->priv->mSinkMutex);
    sink->priv->mIsFlushOngoing = false;

    if (resetTime)
    {
        GST_DEBUG_OBJECT(sink, "sending reset_time message");
        gst_element_post_message(GST_ELEMENT_CAST(sink), gst_message_new_reset_time(GST_OBJECT_CAST(sink), 0));
    }
}

static void rialto_mse_base_sink_seek(RialtoMSEBaseSink *sink)
{
    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = sink->priv->m_mediaPlayerManager.getMediaPlayerClient();
    if (!client)
    {
        GST_ERROR_OBJECT(sink, "Could not get the media player client");
        return;
    }

    client->notifySourceStartedSeeking(sink->priv->mSourceId);

    if (sink->priv->m_mediaPlayerManager.hasControl())
    {
        // this will force sink's async transition to paused state and make that pipeline will need to
        // wait for RialtoServer's preroll after seek
        gst_element_lost_state(GST_ELEMENT_CAST(sink));

        std::unique_lock<std::mutex> lock(sink->priv->mSeekMutex);
        GST_INFO_OBJECT(sink, "Seeking to position %" GST_TIME_FORMAT, GST_TIME_ARGS(sink->priv->mLastSegment.start));
        client->seek(sink->priv->mLastSegment.start);
        sink->priv->mSeekCondVariable.wait(lock);
    }
}

static gboolean rialto_mse_base_sink_send_event(GstElement *element, GstEvent *event)
{
    RialtoMSEBaseSink *sink = RIALTO_MSE_BASE_SINK(element);
    GST_DEBUG_OBJECT(sink, "handling event '%s'", GST_EVENT_TYPE_NAME(event));
    bool shouldForwardUpstream = GST_EVENT_IS_UPSTREAM(event);

    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_SEEK:
    {
        gdouble rate;
        GstFormat seekFormat;
        GstSeekFlags flags;
        GstSeekType startType, stopType;
        gint64 start, stop;
        if (event)
        {
            gst_event_parse_seek(event, &rate, &seekFormat, &flags, &startType, &start, &stopType, &stop);

            if (flags & GST_SEEK_FLAG_FLUSH)
            {
                rialto_mse_base_sink_flush_start(sink);
            }
            else
            {
                GST_ERROR_OBJECT(sink, "Flushless seek is not supported");
                gst_event_unref(event);
                return FALSE;
            }

            if (seekFormat == GST_FORMAT_TIME)
            {
                gint64 seekPosition = -1;
                switch (startType)
                {
                case GST_SEEK_TYPE_SET:
                    seekPosition = start;
                    break;
                case GST_SEEK_TYPE_END:
                    GST_ERROR_OBJECT(sink, "GST_SEEK_TYPE_END seek is not supported");
                    gst_event_unref(event);
                    return FALSE;
                default:
                    break;
                }

                if (seekPosition != -1)
                {
                    std::lock_guard<std::mutex> lock(sink->priv->mSinkMutex);
                    gst_segment_init(&sink->priv->mLastSegment, GST_FORMAT_TIME);
                    sink->priv->mLastSegment.start = seekPosition;
                }
            }
        }
    }
    default:
        break;
    }

    if (shouldForwardUpstream)
    {
        bool result = gst_pad_push_event(sink->priv->mSinkPad, event);
        if (!result)
        {
            GST_DEBUG_OBJECT(sink, "forwarding upstream event '%s' failed", GST_EVENT_TYPE_NAME(event));
        }
        return result;
    }

    gst_event_unref(event);

    return TRUE;
}

static GstStateChangeReturn rialto_mse_base_sink_change_state(GstElement *element, GstStateChange transition)
{
    RialtoMSEBaseSink *sink = RIALTO_MSE_BASE_SINK(element);
    RialtoMSEBaseSinkPrivate *priv = sink->priv;

    GstState current_state = GST_STATE_TRANSITION_CURRENT(transition);
    GstState next_state = GST_STATE_TRANSITION_NEXT(transition);
    GST_INFO_OBJECT(sink, "State change: (%s) -> (%s)", gst_element_state_get_name(current_state),
                    gst_element_state_get_name(next_state));

    GstStateChangeReturn status = GST_STATE_CHANGE_SUCCESS;
    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = sink->priv->m_mediaPlayerManager.getMediaPlayerClient();

    switch (transition)
    {
    case GST_STATE_CHANGE_NULL_TO_READY:
        if (!priv->mSinkPad)
        {
            GST_ERROR_OBJECT(sink, "Cannot start, because there's no sink pad");
            return GST_STATE_CHANGE_FAILURE;
        }
        priv->m_rialtoControlClient->getRialtoControlBackend();
        if (!priv->m_rialtoControlClient->isRialtoControlBackendCreated())
        {
            GST_ERROR_OBJECT(sink, "Cannot get the rialto control object");
            return GST_STATE_CHANGE_FAILURE;
        }

        if (!priv->m_rialtoControlClient->setApplicationState(firebolt::rialto::ApplicationState::RUNNING))
        {
            GST_ERROR_OBJECT(sink, "Cannot set rialto state to running");
            return GST_STATE_CHANGE_FAILURE;
        }
        break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    {
        if (!client)
        {
            GST_ERROR_OBJECT(sink, "Cannot get the media player client object");
            return GST_STATE_CHANGE_FAILURE;
        }

        priv->mIsFlushOngoing = false;
        if (priv->m_mediaPlayerManager.hasControl())
        {
            gst_element_post_message(element, gst_message_new_async_start(GST_OBJECT(element)));
            status = GST_STATE_CHANGE_ASYNC;
            client->pause();
        }
        break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
        if (!client)
        {
            GST_ERROR_OBJECT(sink, "Cannot get the media player client object");
            return GST_STATE_CHANGE_FAILURE;
        }

        if (priv->m_mediaPlayerManager.hasControl())
        {
            gst_element_post_message(element, gst_message_new_async_start(GST_OBJECT(element)));
            status = GST_STATE_CHANGE_ASYNC;
            client->play();
        }
        break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
        if (!client)
        {
            GST_ERROR_OBJECT(sink, "Cannot get the media player client object");
            return GST_STATE_CHANGE_FAILURE;
        }

        if (priv->m_mediaPlayerManager.hasControl())
        {
            gst_element_post_message(element, gst_message_new_async_start(GST_OBJECT(element)));
            status = GST_STATE_CHANGE_ASYNC;
            client->pause();
        }
        break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
        if (!client)
        {
            GST_ERROR_OBJECT(sink, "Cannot get the media player client object");
            return GST_STATE_CHANGE_FAILURE;
        }

        client->removeSource(priv->mSourceId);
        {
            std::lock_guard<std::mutex> lock(sink->priv->mSinkMutex);
            priv->clearBuffersUnlocked();
        }
        if (priv->m_mediaPlayerManager.hasControl())
        {
            client->stop();
        }

        priv->m_mediaPlayerManager.releaseMediaPlayerClient();
        break;
    case GST_STATE_CHANGE_READY_TO_NULL:
        priv->m_rialtoControlClient->removeRialtoControlBackend();
        break;
    default:
        break;
    }

    GstStateChangeReturn result = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
    if (G_UNLIKELY(result == GST_STATE_CHANGE_FAILURE))
    {
        GST_WARNING_OBJECT(sink, "State change failed");
        return result;
    }
    else if (result == GST_STATE_CHANGE_ASYNC)
    {
        return GST_STATE_CHANGE_ASYNC;
    }

    return status;
}

static void rialto_mse_base_sink_class_init(RialtoMSEBaseSinkClass *klass)
{
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    GstElementClass *elementClass = GST_ELEMENT_CLASS(klass);

    gst_element_class_set_metadata(elementClass, "Rialto MSE base sink", "Generic", "A sink for Rialto", "Sky");

    gobjectClass->finalize = rialto_mse_base_sink_finalize;
    gobjectClass->get_property = rialto_mse_base_sink_get_property;
    gobjectClass->set_property = rialto_mse_base_sink_set_property;
    elementClass->query = rialto_mse_base_sink_query;
    elementClass->send_event = rialto_mse_base_sink_send_event;
    elementClass->change_state = rialto_mse_base_sink_change_state;

    g_object_class_install_property(gobjectClass, PROP_LOCATION,
                                    g_param_spec_string("location", "location", "Location to read from", nullptr,
                                                        GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(gobjectClass, PROP_HANDLE_RESET_TIME_MESSAGE,
                                    g_param_spec_boolean("handle-reset-time-message", "Handle Reset Time Message",
                                                         "Handle Reset Time Message", FALSE,
                                                         GParamFlags(G_PARAM_READWRITE)));
}

GstFlowReturn rialto_mse_base_sink_chain(GstPad *pad, GstObject *parent, GstBuffer *buf)
{
    size_t MAX_INTERNAL_BUFFERS_QUEUE_SIZE = 24;
    RialtoMSEBaseSink *sink = RIALTO_MSE_BASE_SINK(parent);
    GST_LOG_OBJECT(sink, "Handling buffer %p with PTS %" GST_TIME_FORMAT, buf, GST_TIME_ARGS(GST_BUFFER_PTS(buf)));

    std::unique_lock<std::mutex> lock(sink->priv->mSinkMutex);

    if (sink->priv->mSamples.size() >= MAX_INTERNAL_BUFFERS_QUEUE_SIZE)
    {
        GST_DEBUG_OBJECT(sink, "Waiting for more space in buffers queue\n");
        sink->priv->mNeedDataCondVariable.wait(lock);
    }

    if (sink->priv->mIsFlushOngoing)
    {
        GST_DEBUG_OBJECT(sink, "Discarding buffer which was received during flushing");
        gst_buffer_unref(buf);
        return GST_FLOW_FLUSHING;
    }

    GstSample *sample = gst_sample_new(buf, sink->priv->mCaps, &sink->priv->mLastSegment, nullptr);
    if (sample)
        sink->priv->mSamples.push(sample);
    else
        GST_ERROR_OBJECT(sink, "Failed to create a sample");

    gst_buffer_unref(buf);

    return GST_FLOW_OK;
}

bool rialto_mse_base_sink_initialise_sinkpad(RialtoMSEBaseSink *sink)
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
    sink->priv->mSinkPad = sinkPad;

    return true;
}

bool rialto_mse_base_sink_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
    RialtoMSEBaseSink *sink = RIALTO_MSE_BASE_SINK(parent);
    GST_DEBUG_OBJECT(sink, "handling event %" GST_PTR_FORMAT, event);
    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_SEGMENT:
    {
        std::lock_guard<std::mutex> lock(sink->priv->mSinkMutex);
        gst_event_copy_segment(event, &sink->priv->mLastSegment);
        break;
    }
    case GST_EVENT_EOS:
    {
        std::lock_guard<std::mutex> lock(sink->priv->mSinkMutex);
        sink->priv->mIsEos = true;
        break;
    }
    case GST_EVENT_CAPS:
    {
        GstCaps *caps;
        gst_event_parse_caps(event, &caps);
        {
            std::lock_guard<std::mutex> lock(sink->priv->mSinkMutex);
            if (sink->priv->mCaps)
            {
                if (!gst_caps_is_equal(caps, sink->priv->mCaps))
                {
                    gst_caps_unref(sink->priv->mCaps);
                    sink->priv->mCaps = gst_caps_copy(caps);
                }
            }
            else
            {
                sink->priv->mCaps = gst_caps_copy(caps);
            }
        }
        break;
    }
    case GST_EVENT_SINK_MESSAGE:
    {
        GstMessage *message = nullptr;
        gst_event_parse_sink_message(event, &message);

        if (message)
        {
            gst_element_post_message(GST_ELEMENT_CAST(sink), message);
        }

        break;
    }
    case GST_EVENT_CUSTOM_DOWNSTREAM:
    case GST_EVENT_CUSTOM_DOWNSTREAM_OOB:
    {
        if (gst_event_has_name(event, "custom-instant-rate-change"))
        {
            GST_DEBUG_OBJECT(sink, "Change rate event received");
            rialto_mse_base_sink_change_playback_rate(sink, event);
        }
        break;
    }
    case GST_EVENT_FLUSH_START:
    {
        rialto_mse_base_sink_flush_start(sink);
        break;
    }
    case GST_EVENT_FLUSH_STOP:
    {
        gboolean reset_time;
        gst_event_parse_flush_stop(event, &reset_time);

        rialto_mse_base_sink_seek(sink);
        rialto_mse_base_sink_flush_stop(sink, reset_time);
        break;
    }
    default:
        break;
    }

    gst_event_unref(event);

    return TRUE;
}

GstSample *rialto_mse_base_sink_get_front_sample(RialtoMSEBaseSink *sink)
{
    std::lock_guard<std::mutex> lock(sink->priv->mSinkMutex);
    if (!sink->priv->mSamples.empty())
    {
        GstSample *sample = sink->priv->mSamples.front();
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GST_LOG_OBJECT(sink, "Pulling buffer %p with PTS %" GST_TIME_FORMAT, buffer,
                       GST_TIME_ARGS(GST_BUFFER_PTS(buffer)));

        return sample;
    }

    return nullptr;
}

void rialto_mse_base_sink_pop_sample(RialtoMSEBaseSink *sink)
{
    std::lock_guard<std::mutex> lock(sink->priv->mSinkMutex);
    sink->priv->mNeedDataCondVariable.notify_all();
    if (!sink->priv->mSamples.empty())
    {
        gst_sample_unref(sink->priv->mSamples.front());
        sink->priv->mSamples.pop();
    }
}

bool rialto_mse_base_sink_is_eos(RialtoMSEBaseSink *sink)
{
    std::lock_guard<std::mutex> lock(sink->priv->mSinkMutex);
    return sink->priv->mSamples.empty() && sink->priv->mIsEos;
}

void rialto_mse_base_handle_rialto_server_state_changed(RialtoMSEBaseSink *sink, firebolt::rialto::PlaybackState state)
{
    if (sink->priv->mCallbacks.stateChangedCallback)
    {
        sink->priv->mCallbacks.stateChangedCallback(state);
    }
}

void rialto_mse_base_handle_rialto_server_eos(RialtoMSEBaseSink *sink)
{
    if (sink->priv->mCallbacks.eosCallback)
    {
        sink->priv->mCallbacks.eosCallback();
    }
}

void rialto_mse_base_handle_rialto_server_completed_seek(RialtoMSEBaseSink *sink)
{
    if (sink->priv->mCallbacks.seekCompletedCallback)
    {
        sink->priv->mCallbacks.seekCompletedCallback();
    }
}

void rialto_mse_base_handle_rialto_server_sent_qos(RialtoMSEBaseSink *sink, uint64_t processed, uint64_t dropped)
{
    if (sink->priv->mCallbacks.qosCallback)
    {
        sink->priv->mCallbacks.qosCallback(processed, dropped);
    }
}

GstObject *rialto_mse_base_get_oldest_gst_bin_parent(GstElement *element)
{
    GstObject *parent = gst_object_get_parent(GST_OBJECT_CAST(element));
    GstObject *result = GST_OBJECT_CAST(element);
    if (parent)
    {
        if (GST_IS_BIN(parent))
        {
            result = rialto_mse_base_get_oldest_gst_bin_parent(GST_ELEMENT_CAST(parent));
        }
        gst_object_unref(parent);
    }

    return result;
}

std::vector<uint8_t> rialto_mse_base_sink_get_codec_data(RialtoMSEBaseSink *sink, const GstStructure *structure)
{
    std::vector<uint8_t> codecData;

    const GValue *codec_data;
    codec_data = gst_structure_get_value(structure, "codec_data");
    if (codec_data)
    {
        GstBuffer *buf = gst_value_get_buffer(codec_data);
        if (buf)
        {
            GstMappedBuffer mappedBuf(buf, GST_MAP_READ);
            if (mappedBuf)
            {
                codecData.assign(mappedBuf.data(), mappedBuf.data() + mappedBuf.size());
            }
            else
            {
                GST_ERROR_OBJECT(sink, "Failed to read codec_data");
            }
        }
    }

    return codecData;
}

firebolt::rialto::StreamFormat rialto_mse_base_sink_get_stream_format(RialtoMSEBaseSink *sink,
                                                                      const GstStructure *structure)
{
    const gchar *streamFormat = gst_structure_get_string(structure, "stream-format");
    firebolt::rialto::StreamFormat format = firebolt::rialto::StreamFormat::UNDEFINED;
    if (streamFormat)
    {
        static const std::unordered_map<std::string, firebolt::rialto::StreamFormat> stringToStreamFormatMap =
            {{"raw", firebolt::rialto::StreamFormat::RAW},
             {"avc", firebolt::rialto::StreamFormat::AVC},
             {"byte-stream", firebolt::rialto::StreamFormat::BYTE_STREAM}};

        auto strToStreamFormatIt = stringToStreamFormatMap.find(streamFormat);
        if (strToStreamFormatIt != stringToStreamFormatMap.end())
        {
            format = strToStreamFormatIt->second;
        }
    }

    return format;
}

firebolt::rialto::SegmentAlignment rialto_mse_base_sink_get_segment_alignment(RialtoMSEBaseSink *sink,
                                                                              const GstStructure *s)
{
    const gchar *alignment = gst_structure_get_string(s, "alignment");
    if (alignment)
    {
        GST_DEBUG_OBJECT(sink, "Alignment found %s", alignment);
        if (strcmp(alignment, "au") == 0)
        {
            return firebolt::rialto::SegmentAlignment::AU;
        }
        else if (strcmp(alignment, "nal") == 0)
        {
            return firebolt::rialto::SegmentAlignment::NAL;
        }
    }

    return firebolt::rialto::SegmentAlignment::UNDEFINED;
}
