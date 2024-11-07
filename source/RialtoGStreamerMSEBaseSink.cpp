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

#include <cstring>
#include <limits>

#include <gst/gst.h>

#include "ControlBackend.h"
#include "GStreamerUtils.h"
#include "IClientLogControl.h"
#include "IMediaPipeline.h"
#include "LogToGstHandler.h"
#include "RialtoGStreamerMSEBaseSink.h"
#include "RialtoGStreamerMSEBaseSinkPrivate.h"

GST_DEBUG_CATEGORY_STATIC(RialtoMSEBaseSinkDebug);
#define GST_CAT_DEFAULT RialtoMSEBaseSinkDebug

#define rialto_mse_base_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE(RialtoMSEBaseSink, rialto_mse_base_sink, GST_TYPE_ELEMENT,
                        G_ADD_PRIVATE(RialtoMSEBaseSink)
                            GST_DEBUG_CATEGORY_INIT(RialtoMSEBaseSinkDebug, "rialtomsebasesink", 0,
                                                    "rialto mse base sink"));

enum
{
    PROP_0,
    PROP_IS_SINGLE_PATH_STREAM,
    PROP_N_STREAMS,
    PROP_HAS_DRM,
    PROP_STATS,
    PROP_LAST
};

enum
{
    SIGNAL_UNDERFLOW,
    SIGNAL_LAST
};

static guint g_signals[SIGNAL_LAST] = {0};

static unsigned rialto_mse_base_sink_get_gst_play_flag(const char *nick)
{
    GFlagsClass *flagsClass = static_cast<GFlagsClass *>(g_type_class_ref(g_type_from_name("GstPlayFlags")));
    GFlagsValue *flag = g_flags_get_value_by_nick(flagsClass, nick);
    return flag ? flag->value : 0;
}

void rialto_mse_base_async_start(RialtoMSEBaseSink *sink)
{
    sink->priv->m_isStateCommitNeeded = true;
    gst_element_post_message(GST_ELEMENT_CAST(sink), gst_message_new_async_start(GST_OBJECT(sink)));
}

static void rialto_mse_base_async_done(RialtoMSEBaseSink *sink)
{
    sink->priv->m_isStateCommitNeeded = false;
    gst_element_post_message(GST_ELEMENT_CAST(sink),
                             gst_message_new_async_done(GST_OBJECT_CAST(sink), GST_CLOCK_TIME_NONE));
}

static void rialto_mse_base_sink_eos_handler(RialtoMSEBaseSink *sink)
{
    GstState currentState = GST_STATE(sink);
    if ((currentState != GST_STATE_PAUSED) && (currentState != GST_STATE_PLAYING))
    {
        GST_ERROR_OBJECT(sink, "Sink cannot post a EOS message in state '%s', posting an error instead",
                         gst_element_state_get_name(currentState));

        const char *errMessage = "Rialto sinks received EOS in non-playing state";
        GError *gError{g_error_new_literal(GST_STREAM_ERROR, 0, errMessage)};
        gst_element_post_message(GST_ELEMENT_CAST(sink),
                                 gst_message_new_error(GST_OBJECT_CAST(sink), gError, errMessage));
        g_error_free(gError);
    }
    else
    {
        gst_element_post_message(GST_ELEMENT_CAST(sink), gst_message_new_eos(GST_OBJECT_CAST(sink)));
    }
}

static void rialto_mse_base_sink_error_handler(RialtoMSEBaseSink *sink, firebolt::rialto::PlaybackError error)
{
    GError *gError = nullptr;
    std::string message;
    switch (error)
    {
    case firebolt::rialto::PlaybackError::DECRYPTION:
    {
        message = "Rialto dropped a frame that failed to decrypt";
        gError = g_error_new_literal(GST_STREAM_ERROR, GST_STREAM_ERROR_DECRYPT, message.c_str());
        break;
    }
    case firebolt::rialto::PlaybackError::UNKNOWN:
    default:
    {
        message = "Rialto server playback failed";
        gError = g_error_new_literal(GST_STREAM_ERROR, 0, message.c_str());
        break;
    }
    }
    gst_element_post_message(GST_ELEMENT_CAST(sink),
                             gst_message_new_error(GST_OBJECT_CAST(sink), gError, message.c_str()));
    g_error_free(gError);
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
        if (!priv->m_sinkPad)
        {
            GST_ERROR_OBJECT(sink, "Cannot start, because there's no sink pad");
            return GST_STATE_CHANGE_FAILURE;
        }
        if (!priv->m_rialtoControlClient->waitForRunning())
        {
            GST_ERROR_OBJECT(sink, "Control: Rialto client cannot reach running state");
            return GST_STATE_CHANGE_FAILURE;
        }
        GST_INFO_OBJECT(sink, "Control: Rialto client reached running state");
        break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    {
        if (!client)
        {
            GST_ERROR_OBJECT(sink, "Cannot get the media player client object");
            return GST_STATE_CHANGE_FAILURE;
        }

        priv->m_isFlushOngoing = false;

        StateChangeResult result = client->pause(priv->m_sourceId);
        if (result == StateChangeResult::SUCCESS_ASYNC || result == StateChangeResult::NOT_ATTACHED)
        {
            // NOT_ATTACHED is not a problem here, because source will be attached later when GST_EVENT_CAPS is received
            if (result == StateChangeResult::NOT_ATTACHED)
            {
                rialto_mse_base_async_start(sink);
            }
            status = GST_STATE_CHANGE_ASYNC;
        }

        break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
    {
        if (!client)
        {
            GST_ERROR_OBJECT(sink, "Cannot get the media player client object");
            return GST_STATE_CHANGE_FAILURE;
        }

        StateChangeResult result = client->play(priv->m_sourceId);
        if (result == StateChangeResult::SUCCESS_ASYNC)
        {
            status = GST_STATE_CHANGE_ASYNC;
        }
        else if (result == StateChangeResult::NOT_ATTACHED)
        {
            GST_ERROR_OBJECT(sink, "Failed to change state to playing");
            return GST_STATE_CHANGE_FAILURE;
        }

        break;
    }
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
    {
        if (!client)
        {
            GST_ERROR_OBJECT(sink, "Cannot get the media player client object");
            return GST_STATE_CHANGE_FAILURE;
        }

        StateChangeResult result = client->pause(priv->m_sourceId);
        if (result == StateChangeResult::SUCCESS_ASYNC)
        {
            status = GST_STATE_CHANGE_ASYNC;
        }
        else if (result == StateChangeResult::NOT_ATTACHED)
        {
            GST_ERROR_OBJECT(sink, "Failed to change state to paused");
            return GST_STATE_CHANGE_FAILURE;
        }

        break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_READY:
        if (!client)
        {
            GST_ERROR_OBJECT(sink, "Cannot get the media player client object");
            return GST_STATE_CHANGE_FAILURE;
        }

        if (priv->m_isStateCommitNeeded)
        {
            GST_DEBUG_OBJECT(sink, "Sending async_done in PAUSED->READY transition");
            rialto_mse_base_async_done(sink);
        }

        client->removeSource(priv->m_sourceId);
        {
            std::lock_guard<std::mutex> lock(sink->priv->m_sinkMutex);
            priv->clearBuffersUnlocked();
            priv->m_sourceAttached = false;
        }
        break;
    case GST_STATE_CHANGE_READY_TO_NULL:
        // Playback will be stopped once all sources are finished and ref count
        // of the media pipeline object reaches 0
        priv->m_mediaPlayerManager.releaseMediaPlayerClient();
        priv->m_rialtoControlClient->removeControlBackend();
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

    if (sink->priv->m_isStateCommitNeeded)
    {
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
            rialto_mse_base_async_done(sink);
        }
        /* Immediately transition to PLAYING when prerolled and PLAY is requested */
        else if (state == firebolt::rialto::PlaybackState::PAUSED && current == GST_STATE_PAUSED &&
                 next == GST_STATE_PLAYING)
        {
            GST_INFO_OBJECT(sink, "Async state transition to PAUSED done. Transitioning to PLAYING");
            rialto_mse_base_sink_change_state(GST_ELEMENT(sink), GST_STATE_CHANGE_PAUSED_TO_PLAYING);
        }
    }
}

static void rialto_mse_base_sink_flush_completed_handler(RialtoMSEBaseSink *sink)
{
    GST_INFO_OBJECT(sink, "Flush completed");
    std::unique_lock<std::mutex> lock(sink->priv->m_flushMutex);
    sink->priv->m_flushCondVariable.notify_all();
}

static void rialto_mse_base_sink_init(RialtoMSEBaseSink *sink)
{
    GST_INFO_OBJECT(sink, "Init: %" GST_PTR_FORMAT, sink);
    sink->priv = static_cast<RialtoMSEBaseSinkPrivate *>(rialto_mse_base_sink_get_instance_private(sink));
    new (sink->priv) RialtoMSEBaseSinkPrivate();

    sink->priv->m_rialtoControlClient = std::make_unique<firebolt::rialto::client::ControlBackend>();

    RialtoGStreamerMSEBaseSinkCallbacks callbacks;
    callbacks.eosCallback = std::bind(rialto_mse_base_sink_eos_handler, sink);
    callbacks.flushCompletedCallback = std::bind(rialto_mse_base_sink_flush_completed_handler, sink);
    callbacks.stateChangedCallback =
        std::bind(rialto_mse_base_sink_rialto_state_changed_handler, sink, std::placeholders::_1);
    callbacks.errorCallback = std::bind(rialto_mse_base_sink_error_handler, sink, std::placeholders::_1);
    sink->priv->m_callbacks = callbacks;
    gst_segment_init(&sink->priv->m_lastSegment, GST_FORMAT_TIME);
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

    std::lock_guard<std::mutex> lock(sink->priv->m_sinkMutex);
    switch (propId)
    {
    case PROP_IS_SINGLE_PATH_STREAM:
        g_value_set_boolean(value, sink->priv->m_isSinglePathStream ? TRUE : FALSE);
        break;
    case PROP_N_STREAMS:
        g_value_set_int(value, sink->priv->m_numOfStreams);
        break;
    case PROP_HAS_DRM:
        g_value_set_boolean(value, sink->priv->m_hasDrm);
        break;
    case PROP_STATS:
    {
        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = sink->priv->m_mediaPlayerManager.getMediaPlayerClient();
        if (!client)
        {
            GST_ERROR_OBJECT(sink, "Could not get the media player client");
            return;
        }

        guint64 totalVideoFrames;
        guint64 droppedVideoFrames;
        if (client->getStats(sink->priv->m_sourceId, totalVideoFrames, droppedVideoFrames))
        {
            GstStructure *stats{gst_structure_new("stats", "rendered", G_TYPE_UINT64, totalVideoFrames, "dropped",
                                                  G_TYPE_UINT64, droppedVideoFrames, nullptr)};
            g_value_set_pointer(value, stats);
        }
        else
        {
            GST_ERROR_OBJECT(sink, "No stats returned from client");
        }
    }
    break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
}

static void rialto_mse_base_sink_set_property(GObject *object, guint propId, const GValue *value, GParamSpec *pspec)
{
    RialtoMSEBaseSink *sink = RIALTO_MSE_BASE_SINK(object);

    std::lock_guard<std::mutex> lock(sink->priv->m_sinkMutex);
    switch (propId)
    {
    case PROP_IS_SINGLE_PATH_STREAM:
        sink->priv->m_isSinglePathStream = g_value_get_boolean(value) != FALSE;
        break;
    case PROP_N_STREAMS:
        sink->priv->m_numOfStreams = g_value_get_int(value);
        break;
    case PROP_HAS_DRM:
        sink->priv->m_hasDrm = g_value_get_boolean(value) != FALSE;
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
        if (!client)
        {
            return FALSE;
        }

        GstFormat fmt;
        gst_query_parse_position(query, &fmt, NULL);
        switch (fmt)
        {
        case GST_FORMAT_TIME:
        {
            gint64 position = client->getPosition(sink->priv->m_sourceId);
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

static void rialto_mse_base_sink_flush_server(RialtoMSEBaseSink *sink, bool resetTime)
{
    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = sink->priv->m_mediaPlayerManager.getMediaPlayerClient();
    if (!client)
    {
        GST_ERROR_OBJECT(sink, "Could not get the media player client");
        return;
    }

    std::unique_lock<std::mutex> lock(sink->priv->m_flushMutex);
    GST_INFO_OBJECT(sink, "Flushing sink with sourceId %d", sink->priv->m_sourceId.load());
    client->flush(sink->priv->m_sourceId, resetTime);
    if (sink->priv->m_sourceAttached)
    {
        sink->priv->m_flushCondVariable.wait(lock);
    }
    else
    {
        GST_DEBUG_OBJECT(sink, "Skip waiting for flush finish - source not attached yet.");
    }
}

static void rialto_mse_base_sink_flush_start(RialtoMSEBaseSink *sink)
{
    std::lock_guard<std::mutex> lock(sink->priv->m_sinkMutex);
    if (!sink->priv->m_isFlushOngoing)
    {
        GST_INFO_OBJECT(sink, "Starting flushing");
        if (sink->priv->m_isEos)
        {
            GST_INFO_OBJECT(sink, "Rialto Client is in EOS state, request pause to reset state");
            std::shared_ptr<GStreamerMSEMediaPlayerClient> client =
                sink->priv->m_mediaPlayerManager.getMediaPlayerClient();
            if (client)
            {
                client->pause(sink->priv->m_sourceId);
            }
            else
            {
                GST_ERROR_OBJECT(sink, "Could not get the media player client");
            }
            sink->priv->m_isEos = false;
        }
        sink->priv->m_isFlushOngoing = true;
        sink->priv->clearBuffersUnlocked();
    }
}

static void rialto_mse_base_sink_flush_stop(RialtoMSEBaseSink *sink, bool resetTime)
{
    GST_INFO_OBJECT(sink, "Stopping flushing");
    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = sink->priv->m_mediaPlayerManager.getMediaPlayerClient();
    if (client)
    {
        client->notifyLostState(sink->priv->m_sourceId);
    }
    else
    {
        GST_ERROR_OBJECT(sink, "Could not get the media player client");
    }
    rialto_mse_base_sink_flush_server(sink, resetTime);
    std::lock_guard<std::mutex> lock(sink->priv->m_sinkMutex);
    sink->priv->m_isFlushOngoing = false;

    if (resetTime)
    {
        GST_DEBUG_OBJECT(sink, "sending reset_time message");
        gst_element_post_message(GST_ELEMENT_CAST(sink), gst_message_new_reset_time(GST_OBJECT_CAST(sink), 0));
    }
}

static void rialto_mse_base_sink_set_segment(RialtoMSEBaseSink *sink)
{
    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = sink->priv->m_mediaPlayerManager.getMediaPlayerClient();
    if (!client)
    {
        GST_ERROR_OBJECT(sink, "Could not get the media player client");
        return;
    }
    const bool kResetTime{sink->priv->m_lastSegment.flags == GST_SEGMENT_FLAG_RESET};
    client->setSourcePosition(sink->priv->m_sourceId, sink->priv->m_lastSegment.start, kResetTime,
                              sink->priv->m_lastSegment.applied_rate, sink->priv->m_lastSegment.stop);
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
        gdouble rate{1.0};
        GstFormat seekFormat;
        GstSeekFlags flags{GST_SEEK_FLAG_NONE};
        GstSeekType startType, stopType;
        gint64 start, stop;
        if (event)
        {
            gst_event_parse_seek(event, &rate, &seekFormat, &flags, &startType, &start, &stopType, &stop);

            if (flags & GST_SEEK_FLAG_FLUSH)
            {
                if (seekFormat == GST_FORMAT_TIME && startType == GST_SEEK_TYPE_END)
                {
                    GST_ERROR_OBJECT(sink, "GST_SEEK_TYPE_END seek is not supported");
                    gst_event_unref(event);
                    return FALSE;
                }
            }
#if GST_CHECK_VERSION(1, 18, 0)
            else if (flags & GST_SEEK_FLAG_INSTANT_RATE_CHANGE)
            {
                std::shared_ptr<GStreamerMSEMediaPlayerClient> client =
                    sink->priv->m_mediaPlayerManager.getMediaPlayerClient();
                if ((client) && (sink->priv->m_mediaPlayerManager.hasControl()))
                {
                    GST_DEBUG_OBJECT(sink, "Instant playback rate change: %.2f", rate);
                    client->setPlaybackRate(rate);
                }

                gdouble rateMultiplier = rate / sink->priv->m_lastSegment.rate;
                GstEvent *rateChangeEvent = gst_event_new_instant_rate_change(rateMultiplier, (GstSegmentFlags)flags);
                gst_event_set_seqnum(rateChangeEvent, gst_event_get_seqnum(event));
                gst_event_unref(event);
                if (gst_pad_send_event(sink->priv->m_sinkPad, rateChangeEvent) != TRUE)
                {
                    GST_ERROR_OBJECT(sink, "Sending instant rate change failed.");
                    return FALSE;
                }
                return TRUE;
            }
#endif
            else
            {
                GST_WARNING_OBJECT(sink, "Seek with flags 0x%X is not supported", flags);
                gst_event_unref(event);
                return FALSE;
            }
        }
    }
    default:
        break;
    }

    if (shouldForwardUpstream)
    {
        bool result = gst_pad_push_event(sink->priv->m_sinkPad, event);
        if (!result)
        {
            GST_DEBUG_OBJECT(sink, "forwarding upstream event '%s' failed", GST_EVENT_TYPE_NAME(event));
        }

        return result;
    }

    gst_event_unref(event);
    return TRUE;
}

static void rialto_mse_base_sink_copy_segment(RialtoMSEBaseSink *sink, GstEvent *event)
{
    std::lock_guard<std::mutex> lock(sink->priv->m_sinkMutex);
    gst_event_copy_segment(event, &sink->priv->m_lastSegment);
}

static void rialto_mse_base_sink_class_init(RialtoMSEBaseSinkClass *klass)
{
    std::shared_ptr<firebolt::rialto::IClientLogHandler> logToGstHandler =
        std::make_shared<firebolt::rialto::LogToGstHandler>();

    if (!firebolt::rialto::IClientLogControlFactory::createFactory()->createClientLogControl().registerLogHandler(logToGstHandler,
                                                                                                                  true))
    {
        GST_ERROR("Unable to preRegister log handler");
    }

    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    GstElementClass *elementClass = GST_ELEMENT_CLASS(klass);

    gst_element_class_set_metadata(elementClass, "Rialto MSE base sink", "Generic", "A sink for Rialto", "Sky");

    gobjectClass->finalize = rialto_mse_base_sink_finalize;
    gobjectClass->get_property = rialto_mse_base_sink_get_property;
    gobjectClass->set_property = rialto_mse_base_sink_set_property;
    elementClass->query = rialto_mse_base_sink_query;
    elementClass->send_event = rialto_mse_base_sink_send_event;
    elementClass->change_state = rialto_mse_base_sink_change_state;

    g_signals[SIGNAL_UNDERFLOW] = g_signal_new("buffer-underflow-callback", G_TYPE_FROM_CLASS(klass),
                                               (GSignalFlags)(G_SIGNAL_RUN_LAST), 0, nullptr, nullptr,
                                               g_cclosure_marshal_VOID__UINT_POINTER, G_TYPE_NONE, 2, G_TYPE_UINT,
                                               G_TYPE_POINTER);

    g_object_class_install_property(gobjectClass, PROP_IS_SINGLE_PATH_STREAM,
                                    g_param_spec_boolean("single-path-stream", "single path stream",
                                                         "is single path stream", FALSE, GParamFlags(G_PARAM_READWRITE)));

    g_object_class_install_property(gobjectClass, PROP_N_STREAMS,
                                    g_param_spec_int("streams-number", "streams number", "streams number", 1, G_MAXINT,
                                                     1, GParamFlags(G_PARAM_READWRITE)));

    g_object_class_install_property(gobjectClass, PROP_HAS_DRM,
                                    g_param_spec_boolean("has-drm", "has drm", "has drm", TRUE,
                                                         GParamFlags(G_PARAM_READWRITE)));
    g_object_class_install_property(gobjectClass, PROP_STATS,
                                    g_param_spec_pointer("stats", NULL, "pointer to a gst_structure",
                                                         GParamFlags(G_PARAM_READABLE)));
}

GstFlowReturn rialto_mse_base_sink_chain(GstPad *pad, GstObject *parent, GstBuffer *buf)
{
    size_t MAX_INTERNAL_BUFFERS_QUEUE_SIZE = 24;
    RialtoMSEBaseSink *sink = RIALTO_MSE_BASE_SINK(parent);
    GST_LOG_OBJECT(sink, "Handling buffer %p with PTS %" GST_TIME_FORMAT, buf, GST_TIME_ARGS(GST_BUFFER_PTS(buf)));

    std::unique_lock<std::mutex> lock(sink->priv->m_sinkMutex);

    if (sink->priv->m_samples.size() >= MAX_INTERNAL_BUFFERS_QUEUE_SIZE)
    {
        GST_DEBUG_OBJECT(sink, "Waiting for more space in buffers queue\n");
        sink->priv->m_needDataCondVariable.wait(lock);
    }

    if (sink->priv->m_isFlushOngoing)
    {
        GST_DEBUG_OBJECT(sink, "Discarding buffer which was received during flushing");
        gst_buffer_unref(buf);
        return GST_FLOW_FLUSHING;
    }

    GstSample *sample = gst_sample_new(buf, sink->priv->m_caps, &sink->priv->m_lastSegment, nullptr);
    if (sample)
        sink->priv->m_samples.push(sample);
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
    sink->priv->m_sinkPad = sinkPad;

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
        rialto_mse_base_sink_copy_segment(sink, event);
        rialto_mse_base_sink_set_segment(sink);
        break;
    }
    case GST_EVENT_EOS:
    {
        std::lock_guard<std::mutex> lock(sink->priv->m_sinkMutex);
        sink->priv->m_isEos = true;
        break;
    }
    case GST_EVENT_CAPS:
    {
        GstCaps *caps;
        gst_event_parse_caps(event, &caps);
        {
            std::lock_guard<std::mutex> lock(sink->priv->m_sinkMutex);
            if (sink->priv->m_caps)
            {
                if (!gst_caps_is_equal(caps, sink->priv->m_caps))
                {
                    gst_caps_unref(sink->priv->m_caps);
                    sink->priv->m_caps = gst_caps_copy(caps);
                }
            }
            else
            {
                sink->priv->m_caps = gst_caps_copy(caps);
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

        rialto_mse_base_sink_flush_stop(sink, reset_time);
        break;
    }
    case GST_EVENT_STREAM_COLLECTION:
    {
        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = sink->priv->m_mediaPlayerManager.getMediaPlayerClient();
        if (!client)
        {
            gst_event_unref(event);
            return FALSE;
        }
        int32_t videoStreams{0}, audioStreams{0}, textStreams{0};
        GstStreamCollection *streamCollection;
        gst_event_parse_stream_collection(event, &streamCollection);
        guint streamsSize = gst_stream_collection_get_size(streamCollection);
        for (guint i = 0; i < streamsSize; ++i)
        {
            auto *stream = gst_stream_collection_get_stream(streamCollection, i);
            auto type = gst_stream_get_stream_type(stream);
            if (type & GST_STREAM_TYPE_AUDIO)
            {
                ++audioStreams;
            }
            else if (type & GST_STREAM_TYPE_VIDEO)
            {
                ++videoStreams;
            }
            else if (type & GST_STREAM_TYPE_TEXT)
            {
                ++textStreams;
            }
        }
        gst_object_unref(streamCollection);
        client->handleStreamCollection(audioStreams, videoStreams, textStreams);
        client->sendAllSourcesAttachedIfPossible();
        break;
    }
    default:
        break;
    }

    gst_event_unref(event);

    return TRUE;
}

GstRefSample rialto_mse_base_sink_get_front_sample(RialtoMSEBaseSink *sink)
{
    std::lock_guard<std::mutex> lock(sink->priv->m_sinkMutex);
    if (!sink->priv->m_samples.empty())
    {
        GstSample *sample = sink->priv->m_samples.front();
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GST_LOG_OBJECT(sink, "Pulling buffer %p with PTS %" GST_TIME_FORMAT, buffer,
                       GST_TIME_ARGS(GST_BUFFER_PTS(buffer)));

        return GstRefSample{sample};
    }

    return GstRefSample{};
}

void rialto_mse_base_sink_pop_sample(RialtoMSEBaseSink *sink)
{
    std::lock_guard<std::mutex> lock(sink->priv->m_sinkMutex);
    sink->priv->m_needDataCondVariable.notify_all();
    if (!sink->priv->m_samples.empty())
    {
        gst_sample_unref(sink->priv->m_samples.front());
        sink->priv->m_samples.pop();
    }
}

bool rialto_mse_base_sink_is_eos(RialtoMSEBaseSink *sink)
{
    std::lock_guard<std::mutex> lock(sink->priv->m_sinkMutex);
    return sink->priv->m_samples.empty() && sink->priv->m_isEos;
}

void rialto_mse_base_handle_rialto_server_state_changed(RialtoMSEBaseSink *sink, firebolt::rialto::PlaybackState state)
{
    if (sink->priv->m_callbacks.stateChangedCallback)
    {
        sink->priv->m_callbacks.stateChangedCallback(state);
    }
}

void rialto_mse_base_handle_rialto_server_eos(RialtoMSEBaseSink *sink)
{
    if (sink->priv->m_callbacks.eosCallback)
    {
        sink->priv->m_callbacks.eosCallback();
    }
}

void rialto_mse_base_handle_rialto_server_completed_flush(RialtoMSEBaseSink *sink)
{
    if (sink->priv->m_callbacks.flushCompletedCallback)
    {
        sink->priv->m_callbacks.flushCompletedCallback();
    }
}

void rialto_mse_base_handle_rialto_server_sent_qos(RialtoMSEBaseSink *sink, uint64_t processed, uint64_t dropped)
{
    if (sink->priv->m_callbacks.qosCallback)
    {
        sink->priv->m_callbacks.qosCallback(processed, dropped);
    }
}

void rialto_mse_base_handle_rialto_server_error(RialtoMSEBaseSink *sink, firebolt::rialto::PlaybackError error)
{
    if (sink->priv->m_callbacks.errorCallback)
    {
        sink->priv->m_callbacks.errorCallback(error);
    }
}

void rialto_mse_base_handle_rialto_server_sent_buffer_underflow(RialtoMSEBaseSink *sink)
{
    GST_WARNING_OBJECT(sink, "Sending underflow signal");
    // send 2 last parameters just to be compatible with RDK's buffer-underflow-callback signal signature
    g_signal_emit(G_OBJECT(sink), g_signals[SIGNAL_UNDERFLOW], 0, 0, nullptr);
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

std::shared_ptr<firebolt::rialto::CodecData> rialto_mse_base_sink_get_codec_data(RialtoMSEBaseSink *sink,
                                                                                 const GstStructure *structure)
{
    const GValue *codec_data = gst_structure_get_value(structure, "codec_data");
    if (codec_data)
    {
        GstBuffer *buf = gst_value_get_buffer(codec_data);
        if (buf)
        {
            GstMappedBuffer mappedBuf(buf, GST_MAP_READ);
            if (mappedBuf)
            {
                auto codecData = std::make_shared<firebolt::rialto::CodecData>();
                codecData->data = std::vector<std::uint8_t>(mappedBuf.data(), mappedBuf.data() + mappedBuf.size());
                codecData->type = firebolt::rialto::CodecDataType::BUFFER;
                return codecData;
            }
            else
            {
                GST_ERROR_OBJECT(sink, "Failed to read codec_data");
                return nullptr;
            }
        }
        const gchar *str = g_value_get_string(codec_data);
        if (str)
        {
            auto codecData = std::make_shared<firebolt::rialto::CodecData>();
            codecData->data = std::vector<std::uint8_t>(str, str + std::strlen(str));
            codecData->type = firebolt::rialto::CodecDataType::STRING;
            return codecData;
        }
    }

    return nullptr;
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
             {"byte-stream", firebolt::rialto::StreamFormat::BYTE_STREAM},
             {"hvc1", firebolt::rialto::StreamFormat::HVC1},
             {"hev1", firebolt::rialto::StreamFormat::HEV1}};

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

bool rialto_mse_base_sink_get_dv_profile(RialtoMSEBaseSink *sink, const GstStructure *s, uint32_t &dvProfile)
{
    gboolean isDolbyVisionEnabled = false;
    if (gst_structure_get_boolean(s, "dovi-stream", &isDolbyVisionEnabled) && isDolbyVisionEnabled)
    {
        if (gst_structure_get_uint(s, "dv_profile", &dvProfile))
        {
            return true;
        }
    }
    return false;
}

void rialto_mse_base_sink_lost_state(RialtoMSEBaseSink *sink)
{
    sink->priv->m_isStateCommitNeeded = true;
    gst_element_lost_state(GST_ELEMENT_CAST(sink));
}

static bool rialto_mse_base_sink_get_n_streams_from_parent(GstObject *parentObject, gint &n_video, gint &n_audio,
                                                           gint &n_text)
{
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(parentObject), "n-video") &&
        g_object_class_find_property(G_OBJECT_GET_CLASS(parentObject), "n-audio") &&
        g_object_class_find_property(G_OBJECT_GET_CLASS(parentObject), "n-text"))
    {
        g_object_get(parentObject, "n-video", &n_video, "n-audio", &n_audio, "n-text", &n_text, nullptr);

        if (g_object_class_find_property(G_OBJECT_GET_CLASS(parentObject), "flags"))
        {
            guint flags = 0;
            g_object_get(parentObject, "flags", &flags, nullptr);
            n_video = flags & rialto_mse_base_sink_get_gst_play_flag("video") ? n_video : 0;
            n_audio = flags & rialto_mse_base_sink_get_gst_play_flag("audio") ? n_audio : 0;
            n_text = flags & rialto_mse_base_sink_get_gst_play_flag("text") ? n_text : 0;
        }

        return true;
    }

    return false;
}

static bool rialto_mse_base_sink_set_streams_number(RialtoMSEBaseSink *sink, GstObject *parentObject)
{
    RialtoMSEBaseSinkPrivate *priv = sink->priv;
    int32_t videoStreams{-1}, audioStreams{-1}, subtitleStreams{-1};

    GstContext *context = gst_element_get_context(GST_ELEMENT(sink), "streams-info");
    if (context)
    {
        GST_DEBUG_OBJECT(sink, "Getting number of streams from \"streams-info\" context");

        guint n_video{0}, n_audio{0}, n_text{0};

        const GstStructure *streamsInfoStructure = gst_context_get_structure(context);
        gst_structure_get_uint(streamsInfoStructure, "video-streams", &n_video);
        gst_structure_get_uint(streamsInfoStructure, "audio-streams", &n_audio);
        gst_structure_get_uint(streamsInfoStructure, "text-streams", &n_text);

        if (n_video > std::numeric_limits<int32_t>::max() || n_audio > std::numeric_limits<int32_t>::max() ||
            n_text > std::numeric_limits<int32_t>::max())
        {
            GST_ERROR_OBJECT(sink, "Number of streams is too big, video=%u, audio=%u, text=%u", n_video, n_audio, n_text);
            gst_context_unref(context);
            return false;
        }

        videoStreams = n_video;
        audioStreams = n_audio;
        subtitleStreams = n_text;

        gst_context_unref(context);
    }
    else if (rialto_mse_base_sink_get_n_streams_from_parent(parentObject, videoStreams, audioStreams, subtitleStreams))
    {
        GST_DEBUG_OBJECT(sink, "Got number of streams from playbin2 properties");
    }
    else
    {
        // The default value of streams is V:1, A:1, S:0
        // Changing the default setting via properties is considered as DEPRECATED
        subtitleStreams = 0;
        std::lock_guard<std::mutex> lock(priv->m_sinkMutex);
        if (priv->m_mediaSourceType == firebolt::rialto::MediaSourceType::VIDEO)
        {
            videoStreams = priv->m_numOfStreams;
            if (priv->m_isSinglePathStream)
            {
                audioStreams = 0;
                subtitleStreams = 0;
            }
        }
        else if (priv->m_mediaSourceType == firebolt::rialto::MediaSourceType::AUDIO)
        {
            audioStreams = priv->m_numOfStreams;
            if (priv->m_isSinglePathStream)
            {
                videoStreams = 0;
                subtitleStreams = 0;
            }
        }
        else if (priv->m_mediaSourceType == firebolt::rialto::MediaSourceType::SUBTITLE)
        {
            subtitleStreams = priv->m_numOfStreams;
            if (priv->m_isSinglePathStream)
            {
                videoStreams = 0;
                audioStreams = 0;
            }
        }
    }

    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = sink->priv->m_mediaPlayerManager.getMediaPlayerClient();
    if (!client)
    {
        GST_ERROR_OBJECT(sink, "MediaPlayerClient is nullptr");
        return false;
    }

    client->handleStreamCollection(audioStreams, videoStreams, subtitleStreams);

    return true;
}

bool rialto_mse_base_sink_attach_to_media_client_and_set_streams_number(GstElement *element, const uint32_t maxVideoWidth,
                                                                        const uint32_t maxVideoHeight)
{
    RialtoMSEBaseSink *sink = RIALTO_MSE_BASE_SINK(element);
    RialtoMSEBaseSinkPrivate *priv = sink->priv;

    GstObject *parentObject = rialto_mse_base_get_oldest_gst_bin_parent(element);
    if (!priv->m_mediaPlayerManager.attachMediaPlayerClient(parentObject, maxVideoWidth, maxVideoHeight))
    {
        GST_ERROR_OBJECT(sink, "Cannot attach the MediaPlayerClient");
        return false;
    }

    gchar *parentObjectName = gst_object_get_name(parentObject);
    GST_INFO_OBJECT(element, "Attached media player client with parent %s(%p)", parentObjectName, parentObject);
    g_free(parentObjectName);

    return rialto_mse_base_sink_set_streams_number(sink, parentObject);
}
