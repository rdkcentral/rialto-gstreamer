/*
 * Copyright (C) 2025 Sky UK
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

#include "PullModePlaybackDelegate.h"
#include "ControlBackend.h"
#include "GstreamerCatLog.h"
#include "RialtoGStreamerMSEBaseSink.h"
#include "RialtoGStreamerMSEBaseSinkPrivate.h"

#define GST_CAT_DEFAULT rialtoGStreamerCat

namespace
{
GstObject *getOldestGstBinParent(GstElement *element)
{
    GstObject *parent = gst_object_get_parent(GST_OBJECT_CAST(element));
    GstObject *result = GST_OBJECT_CAST(element);
    if (parent)
    {
        if (GST_IS_BIN(parent))
        {
            result = getOldestGstBinParent(GST_ELEMENT_CAST(parent));
        }
        gst_object_unref(parent);
    }

    return result;
}

unsigned getGstPlayFlag(const char *nick)
{
    GFlagsClass *flagsClass = static_cast<GFlagsClass *>(g_type_class_ref(g_type_from_name("GstPlayFlags")));
    GFlagsValue *flag = g_flags_get_value_by_nick(flagsClass, nick);
    const unsigned kResult{flag ? flag->value : 0};
    g_type_class_unref(flagsClass);
    return kResult;
}

bool getNStreamsFromParent(GstObject *parentObject, gint &n_video, gint &n_audio, gint &n_text)
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
            n_video = (flags & getGstPlayFlag("video")) ? n_video : 0;
            n_audio = (flags & getGstPlayFlag("audio")) ? n_audio : 0;
            n_text = (flags & getGstPlayFlag("text")) ? n_text : 0;
        }

        return true;
    }

    return false;
}
} // namespace

PullModePlaybackDelegate::PullModePlaybackDelegate(GstElement *sink) : m_sink{sink}
{
    m_sinkPad = RIALTO_MSE_BASE_SINK(sink)->priv->m_sinkPad;
    gst_segment_init(&m_lastSegment, GST_FORMAT_TIME);
}

void PullModePlaybackDelegate::createControlBackend()
{
    if (!m_rialtoControlClient)
    {
        m_rialtoControlClient = std::make_unique<firebolt::rialto::client::ControlBackend>(shared_from_this());
    }
}

PullModePlaybackDelegate::~PullModePlaybackDelegate()
{
    if (m_caps)
        gst_caps_unref(m_caps);
    clearBuffersUnlocked();
}

void PullModePlaybackDelegate::clearBuffersUnlocked()
{
    m_isSinkFlushOngoing = true;
    m_needDataCondVariable.notify_all();
    while (!m_samples.empty())
    {
        GstSample *sample = m_samples.front();
        m_samples.pop();
        gst_sample_unref(sample);
    }
    setLastBuffer(nullptr);
}

void PullModePlaybackDelegate::setSourceId(int32_t sourceId)
{
    m_sourceId = sourceId;
}

void PullModePlaybackDelegate::handleEos()
{
    GstState currentState = GST_STATE(m_sink);
    if ((currentState != GST_STATE_PAUSED) && (currentState != GST_STATE_PLAYING))
    {
        GST_ERROR_OBJECT(m_sink, "Sink cannot post a EOS message in state '%s', posting an error instead",
                         gst_element_state_get_name(currentState));

        const char *errMessage = "Rialto sinks received EOS in non-playing state";
        GError *gError{g_error_new_literal(GST_STREAM_ERROR, 0, errMessage)};
        gst_element_post_message(m_sink, gst_message_new_error(GST_OBJECT_CAST(m_sink), gError, errMessage));
        g_error_free(gError);
    }
    else
    {
        std::unique_lock lock{m_sinkMutex};
        if (!m_isSinkFlushOngoing && !m_isServerFlushOngoing)
        {
            gst_element_post_message(m_sink, gst_message_new_eos(GST_OBJECT_CAST(m_sink)));
        }
        else
        {
            GST_WARNING_OBJECT(m_sink, "Skip sending eos message - flush is ongoing...");
        }
    }
}

void PullModePlaybackDelegate::handleFlushCompleted()
{
    GST_INFO_OBJECT(m_sink, "Flush completed");
    std::unique_lock<std::mutex> lock(m_sinkMutex);
    m_isServerFlushOngoing = false;
    m_isTimeResetOngoing = false;
}

void PullModePlaybackDelegate::handleStateChanged(firebolt::rialto::PlaybackState state)
{
    GstState current = GST_STATE(m_sink);
    GstState next = GST_STATE_NEXT(m_sink);
    GstState pending = GST_STATE_PENDING(m_sink);
    GstState postNext = next == pending ? GST_STATE_VOID_PENDING : pending;

    GST_DEBUG_OBJECT(m_sink,
                     "Received server's state change to %u. Sink's states are: current state: %s next state: %s "
                     "pending state: %s, last return state %s",
                     static_cast<uint32_t>(state), gst_element_state_get_name(current),
                     gst_element_state_get_name(next), gst_element_state_get_name(pending),
                     gst_element_state_change_return_get_name(GST_STATE_RETURN(m_sink)));

    if (m_isStateCommitNeeded)
    {
        if ((state == firebolt::rialto::PlaybackState::PAUSED && next == GST_STATE_PAUSED) ||
            (state == firebolt::rialto::PlaybackState::PLAYING && next == GST_STATE_PLAYING))
        {
            GST_STATE(m_sink) = next;
            GST_STATE_NEXT(m_sink) = postNext;
            GST_STATE_PENDING(m_sink) = GST_STATE_VOID_PENDING;
            GST_STATE_RETURN(m_sink) = GST_STATE_CHANGE_SUCCESS;

            GST_INFO_OBJECT(m_sink, "Async state transition to state %s done", gst_element_state_get_name(next));

            gst_element_post_message(m_sink,
                                     gst_message_new_state_changed(GST_OBJECT_CAST(m_sink), current, next, pending));
            postAsyncDone();
        }
        /* Immediately transition to PLAYING when prerolled and PLAY is requested */
        else if (state == firebolt::rialto::PlaybackState::PAUSED && current == GST_STATE_PAUSED &&
                 next == GST_STATE_PLAYING)
        {
            GST_INFO_OBJECT(m_sink, "Async state transition to PAUSED done. Transitioning to PLAYING");
            changeState(GST_STATE_CHANGE_PAUSED_TO_PLAYING);
        }
    }
}

GstStateChangeReturn PullModePlaybackDelegate::changeState(GstStateChange transition)
{
    GstState current_state = GST_STATE_TRANSITION_CURRENT(transition);
    GstState next_state = GST_STATE_TRANSITION_NEXT(transition);
    GST_INFO_OBJECT(m_sink, "State change: (%s) -> (%s)", gst_element_state_get_name(current_state),
                    gst_element_state_get_name(next_state));

    GstStateChangeReturn status = GST_STATE_CHANGE_SUCCESS;
    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();

    switch (transition)
    {
    case GST_STATE_CHANGE_NULL_TO_READY:
        if (!m_sinkPad)
        {
            GST_ERROR_OBJECT(m_sink, "Cannot start, because there's no sink pad");
            return GST_STATE_CHANGE_FAILURE;
        }
        if (!m_rialtoControlClient->waitForRunning())
        {
            GST_ERROR_OBJECT(m_sink, "Control: Rialto client cannot reach running state");
            return GST_STATE_CHANGE_FAILURE;
        }
        GST_INFO_OBJECT(m_sink, "Control: Rialto client reached running state");
        break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    {
        if (!client)
        {
            GST_ERROR_OBJECT(m_sink, "Cannot get the media player client object");
            return GST_STATE_CHANGE_FAILURE;
        }

        m_isSinkFlushOngoing = false;

        StateChangeResult result = client->pause(m_sourceId);
        if (result == StateChangeResult::SUCCESS_ASYNC || result == StateChangeResult::NOT_ATTACHED)
        {
            // NOT_ATTACHED is not a problem here, because source will be attached later when GST_EVENT_CAPS is received
            if (result == StateChangeResult::NOT_ATTACHED)
            {
                postAsyncStart();
            }
            status = GST_STATE_CHANGE_ASYNC;
        }

        break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
    {
        if (!client)
        {
            GST_ERROR_OBJECT(m_sink, "Cannot get the media player client object");
            return GST_STATE_CHANGE_FAILURE;
        }

        StateChangeResult result = client->play(m_sourceId);
        if (result == StateChangeResult::SUCCESS_ASYNC)
        {
            status = GST_STATE_CHANGE_ASYNC;
        }
        else if (result == StateChangeResult::NOT_ATTACHED)
        {
            GST_ERROR_OBJECT(m_sink, "Failed to change state to playing");
            return GST_STATE_CHANGE_FAILURE;
        }

        break;
    }
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
    {
        if (!client)
        {
            GST_ERROR_OBJECT(m_sink, "Cannot get the media player client object");
            return GST_STATE_CHANGE_FAILURE;
        }

        StateChangeResult result = client->pause(m_sourceId);
        if (result == StateChangeResult::SUCCESS_ASYNC)
        {
            status = GST_STATE_CHANGE_ASYNC;
        }
        else if (result == StateChangeResult::NOT_ATTACHED)
        {
            GST_ERROR_OBJECT(m_sink, "Failed to change state to paused");
            return GST_STATE_CHANGE_FAILURE;
        }

        break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_READY:
        if (!client)
        {
            GST_ERROR_OBJECT(m_sink, "Cannot get the media player client object");
            return GST_STATE_CHANGE_FAILURE;
        }

        if (m_isStateCommitNeeded)
        {
            GST_DEBUG_OBJECT(m_sink, "Sending async_done in PAUSED->READY transition");
            postAsyncDone();
        }

        client->removeSource(m_sourceId);
        {
            std::lock_guard<std::mutex> lock(m_sinkMutex);
            clearBuffersUnlocked();
            m_sourceAttached = false;
        }
        break;
    case GST_STATE_CHANGE_READY_TO_NULL:
        // Playback will be stopped once all sources are finished and ref count
        // of the media pipeline object reaches 0
        m_mediaPlayerManager.releaseMediaPlayerClient();
        m_rialtoControlClient->removeControlBackend();
        break;
    default:
        break;
    }

    return status;
}

void PullModePlaybackDelegate::handleError(const std::string &message, gint code)
{
    GError *gError{g_error_new_literal(GST_STREAM_ERROR, code, message.c_str())};
    gst_element_post_message(GST_ELEMENT_CAST(m_sink),
                             gst_message_new_error(GST_OBJECT_CAST(m_sink), gError, message.c_str()));
    g_error_free(gError);
}

void PullModePlaybackDelegate::notifyApplicationState(firebolt::rialto::ApplicationState state)
{
    if (state == firebolt::rialto::ApplicationState::UNKNOWN)
    {
        GST_WARNING_OBJECT(m_sink, "Rialto control sent unknown application state");
        handleError("Rialto client reached unknown application state");
    }
}

void PullModePlaybackDelegate::postAsyncStart()
{
    m_isStateCommitNeeded = true;
    gst_element_post_message(GST_ELEMENT_CAST(m_sink), gst_message_new_async_start(GST_OBJECT(m_sink)));
}

void PullModePlaybackDelegate::postAsyncDone()
{
    m_isStateCommitNeeded = false;
    gst_element_post_message(m_sink, gst_message_new_async_done(GST_OBJECT_CAST(m_sink), GST_CLOCK_TIME_NONE));
}

void PullModePlaybackDelegate::setProperty(const Property &type, const GValue *value)
{
    switch (type)
    {
    case Property::IsSinglePathStream:
    {
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        m_isSinglePathStream = g_value_get_boolean(value) != FALSE;
        break;
    }
    case Property::NumberOfStreams:
    {
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        m_numOfStreams = g_value_get_int(value);
        break;
    }
    case Property::HasDrm:
    {
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        m_hasDrm = g_value_get_boolean(value) != FALSE;
        break;
    }
    case Property::EnableLastSample:
    {
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        m_enableLastSample = g_value_get_boolean(value) != FALSE;
        if (!m_enableLastSample)
        {
            if (m_lastBuffer)
            {
                gst_buffer_unref(m_lastBuffer);
                m_lastBuffer = nullptr;
            }
        }
        break;
    }
    default:
    {
        break;
    }
    }
}

void PullModePlaybackDelegate::getProperty(const Property &type, GValue *value)
{
    switch (type)
    {
    case Property::IsSinglePathStream:
    {
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        g_value_set_boolean(value, m_isSinglePathStream ? TRUE : FALSE);
        break;
    }
    case Property::NumberOfStreams:
    {
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        g_value_set_int(value, m_numOfStreams);
        break;
    }
    case Property::HasDrm:
    {
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        g_value_set_boolean(value, m_hasDrm ? TRUE : FALSE);
        break;
    }
    case Property::Stats:
    {
        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
        if (!client)
        {
            GST_ERROR_OBJECT(m_sink, "Could not get the media player client");
            break;
        }

        guint64 totalVideoFrames{0};
        guint64 droppedVideoFrames{0};
        if (client->getStats(m_sourceId, totalVideoFrames, droppedVideoFrames))
        {
            GstStructure *stats{gst_structure_new("stats", "rendered", G_TYPE_UINT64, totalVideoFrames, "dropped",
                                                  G_TYPE_UINT64, droppedVideoFrames, nullptr)};
            g_value_set_pointer(value, stats);
        }
        else
        {
            GST_ERROR_OBJECT(m_sink, "No stats returned from client");
        }
        break;
    }
    case Property::EnableLastSample:
    {
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        g_value_set_boolean(value, m_enableLastSample ? TRUE : FALSE);
        break;
    }
    case Property::LastSample:
    {
        // Mutex inside getLastSample function
        gst_value_take_sample(value, getLastSample());
        break;
    }
    default:
    {
        break;
    }
    }
}

std::optional<gboolean> PullModePlaybackDelegate::handleQuery(GstQuery *query) const
{
    GST_DEBUG_OBJECT(m_sink, "handling query '%s'", GST_QUERY_TYPE_NAME(query));
    switch (GST_QUERY_TYPE(query))
    {
    case GST_QUERY_SEEKING:
    {
        GstFormat fmt;
        gst_query_parse_seeking(query, &fmt, NULL, NULL, NULL);
        // [SEEK-FIX] Only advertise TIME-seekable for the audio-only progressive
        // (BYTES-segment) pull-mode case. For MSE A/V the audio and video sinks use TIME
        // segments and must keep the original (non-seekable) behaviour so WebKit's MSE seek
        // path is unchanged (otherwise AV audio drops out on seek).
        bool isProgressiveAudio = false;
        {
            std::lock_guard<std::mutex> lock(m_sinkMutex);
            isProgressiveAudio = (m_lastSegment.format == GST_FORMAT_BYTES);
        }
        if (fmt == GST_FORMAT_TIME && isProgressiveAudio)
        {
            // [SEEK-FIX] Report seekable=TRUE for TIME format so WebKit allows seek.
            // Query duration from upstream first; fall back to cached tag duration
            // (ID3 / parsed MP3) so audio-only pull-mode pipelines still get a range.
            gint64 duration = -1;
            GstQuery *durationQuery = gst_query_new_duration(GST_FORMAT_TIME);
            if (gst_pad_peer_query(m_sinkPad, durationQuery))
            {
                gst_query_parse_duration(durationQuery, NULL, &duration);
            }
            else
            {
                gint64 cached = m_cachedDuration.load();
                if (cached > 0)
                {
                    duration = cached;
                    GST_INFO_OBJECT(m_sink, "[SEEK-FIX] GST_QUERY_SEEKING: using cached tag duration=%" G_GINT64_FORMAT
                                    " ns (%.3f sec)", cached, (double)cached / GST_SECOND);
                }
            }
            gst_query_unref(durationQuery);

            GST_INFO_OBJECT(m_sink, "[SEEK-FIX] GST_QUERY_SEEKING: format=TIME, seekable=TRUE, duration=%" G_GINT64_FORMAT,
                            duration);
            gst_query_set_seeking(query, fmt, TRUE, 0, duration);
            return TRUE;
        }
        // MSE A/V or non-TIME query: keep the original (baseline) behaviour - not seekable here.
        GST_DEBUG_OBJECT(m_sink, "[SEEK-FIX] GST_QUERY_SEEKING: format=%d not progressive-audio, seekable=FALSE", fmt);
        gst_query_set_seeking(query, fmt, FALSE, 0, -1);
        return TRUE;
    }
    case GST_QUERY_POSITION:
    {
        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
        if (!client)
        {
            return FALSE;
        }
        bool isProgressiveAudio = false;
        {
            std::unique_lock<std::mutex> lock(m_sinkMutex);
            if (m_isServerFlushOngoing && m_isTimeResetOngoing)
            {
                GST_WARNING_OBJECT(m_sink, "Position query during server flush and time reset, returning FALSE");
                return FALSE;
            }
            isProgressiveAudio = (m_lastSegment.format == GST_FORMAT_BYTES);
        }

        GstFormat fmt;
        gst_query_parse_position(query, &fmt, NULL);
        switch (fmt)
        {
        case GST_FORMAT_TIME:
        {
            gint64 position = client->getPosition(m_sourceId);
            GST_DEBUG_OBJECT(m_sink, "Queried position is %" GST_TIME_FORMAT, GST_TIME_ARGS(position));

            // [SEEK-FIX — Fix B] Hold the last seek target as a floor while server
            // preroll briefly reports -1 or a value below the seek target. This converts
            // the visible scrub-bar "glitch back to 0:00" into a smooth hold.
            // Restricted to the audio-only progressive (BYTES) path so MSE A/V position is
            // reported verbatim (the clamp must not leak onto the video + AAC sinks).
            gint64 seekPos = isProgressiveAudio ? m_lastSeekPositionNs.load() : -1;
            if (seekPos > 0)
            {
                if (position < 0)
                {
                    GST_DEBUG_OBJECT(m_sink, "[SEEK-FIX] Position unknown, using seek target %" GST_TIME_FORMAT
                                     " as fallback", GST_TIME_ARGS(seekPos));
                    position = seekPos;
                }
                else if (position < seekPos)
                {
                    GST_DEBUG_OBJECT(m_sink, "[SEEK-FIX] Position %" GST_TIME_FORMAT
                                     " below seek target %" GST_TIME_FORMAT ", clamping to floor",
                                     GST_TIME_ARGS(position), GST_TIME_ARGS(seekPos));
                    position = seekPos;
                }
            }

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
    case GST_QUERY_SEGMENT:
    {
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        GstFormat format{m_lastSegment.format};
        gint64 start{static_cast<gint64>(gst_segment_to_stream_time(&m_lastSegment, format, m_lastSegment.start))};
        gint64 stop{0};
        if (m_lastSegment.stop == GST_CLOCK_TIME_NONE)
        {
            stop = m_lastSegment.duration;
        }
        else
        {
            stop = gst_segment_to_stream_time(&m_lastSegment, format, m_lastSegment.stop);
        }
        gst_query_set_segment(query, m_lastSegment.rate, format, start, stop);
        return TRUE;
    }
    case GST_QUERY_DURATION:
    {
        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
        if (!client)
        {
            return FALSE;
        }
        GstFormat fmt;
        int64_t duration{-1};
        gst_query_parse_duration(query, &fmt, NULL);
        if (fmt == GST_FORMAT_TIME)
        {
            if (client->getDuration(duration) && duration > 0)
            {
                gst_query_set_duration(query, fmt, duration);
                return TRUE;
            }
            // [SEEK-FIX] Try upstream (peer pad) before giving up
            if (gst_pad_peer_query(m_sinkPad, query))
            {
                return TRUE;
            }
            // [SEEK-FIX] Fall back to cached ID3 / parsed MP3 duration
            gint64 cached = m_cachedDuration.load();
            if (cached > 0)
            {
                gst_query_set_duration(query, GST_FORMAT_TIME, cached);
                GST_INFO_OBJECT(m_sink, "[SEEK-FIX] GST_QUERY_DURATION: using cached tag duration=%" G_GINT64_FORMAT
                                " ns (%.3f sec)", cached, (double)cached / GST_SECOND);
                return TRUE;
            }
            return FALSE;
        }
        if (fmt == GST_FORMAT_BYTES && gst_pad_peer_query(m_sinkPad, query))
        {
            return TRUE;
        }
        return FALSE;
    }
    default:
        break;
    }
    return std::nullopt;
}

gboolean PullModePlaybackDelegate::handleSendEvent(GstEvent *event)
{
    GST_DEBUG_OBJECT(m_sink, "handling event '%s'", GST_EVENT_TYPE_NAME(event));
    bool shouldForwardUpstream = GST_EVENT_IS_UPSTREAM(event);

    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_SEEK:
    {
        gdouble rate{1.0};
        GstFormat seekFormat{GST_FORMAT_UNDEFINED};
        GstSeekFlags flags{GST_SEEK_FLAG_NONE};
        GstSeekType startType{GST_SEEK_TYPE_NONE}, stopType{GST_SEEK_TYPE_NONE};
        gint64 start{0}, stop{0};
        gst_event_parse_seek(event, &rate, &seekFormat, &flags, &startType, &start, &stopType, &stop);
        GST_INFO_OBJECT(m_sink, "[SEEK-FIX] handleSendEvent: GST_EVENT_SEEK received! "
                        "format=%s, flags=0x%x, rate=%.2f, start=%" G_GINT64_FORMAT " (%.3f sec), "
                        "stop=%" G_GINT64_FORMAT ", startType=%d, stopType=%d",
                        (seekFormat == GST_FORMAT_TIME) ? "TIME" :
                        (seekFormat == GST_FORMAT_BYTES) ? "BYTES" : "OTHER",
                        flags, rate, start,
                        (seekFormat == GST_FORMAT_TIME) ? (double)start / GST_SECOND : (double)start,
                        stop, startType, stopType);

        if (flags & GST_SEEK_FLAG_FLUSH)
        {
            if (seekFormat == GST_FORMAT_TIME && startType == GST_SEEK_TYPE_END)
            {
                GST_ERROR_OBJECT(m_sink, "GST_SEEK_TYPE_END seek is not supported");
                gst_event_unref(event);
                return FALSE;
            }
            // [SEEK-FIX Layer 4] Handle TIME seek when segment is in BYTES format.
            // This happens in audio-only pull-mode pipelines (httpsrc -> id3demux -> sink)
            // where upstream doesn't understand TIME seeks. We convert TIME -> BYTES locally
            // when we have both a cached duration and the file size. Otherwise we forward
            // the TIME seek upstream as-is and let the pipeline (id3demux/mpegaudioparse)
            // perform the conversion.
            if (seekFormat == GST_FORMAT_TIME && m_lastSegment.format == GST_FORMAT_BYTES)
            {
                gint64 cachedDur = m_cachedDuration.load();
                guint64 fileSize = m_lastSegment.duration;

                if (fileSize == 0 || fileSize == (guint64)(-1))
                {
                    gint64 peerBytes = 0;
                    if (gst_pad_peer_query_duration(m_sinkPad, GST_FORMAT_BYTES, &peerBytes) && peerBytes > 0)
                    {
                        fileSize = (guint64)peerBytes;
                        GST_INFO_OBJECT(m_sink, "[SEEK-FIX] Got file size from upstream query: %" G_GUINT64_FORMAT " bytes",
                                        fileSize);
                    }
                }

                if (cachedDur > 0 && fileSize > 0 && fileSize != (guint64)(-1))
                {
                    gint64 byteStart = (gint64)((gdouble)start / (gdouble)cachedDur * (gdouble)fileSize);
                    gint64 byteStop = (stop == -1 || stop == (gint64)GST_CLOCK_TIME_NONE)
                                      ? -1
                                      : (gint64)((gdouble)stop / (gdouble)cachedDur * (gdouble)fileSize);

                    GST_INFO_OBJECT(m_sink, "[SEEK-FIX] Converting TIME seek to BYTES: "
                                    "time=%" G_GINT64_FORMAT " ns (%.3f sec) -> byte=%" G_GINT64_FORMAT
                                    " (fileSize=%" G_GUINT64_FORMAT ", duration=%" G_GINT64_FORMAT " ns)",
                                    start, (gdouble)start / GST_SECOND, byteStart, fileSize, cachedDur);

                    // Store the pending seek time for setSegment() to use when new segment arrives
                    m_pendingSeekTime.store(start);
                    // Store persistently for position query floor (Fix B)
                    m_lastSeekPositionNs.store(start);

                    guint32 seqnum = gst_event_get_seqnum(event);
                    gst_event_unref(event);

                    // Forward BYTES seek upstream. The source handles FLUSH_START/FLUSH_STOP
                    // and pushes a new SEGMENT downstream which lands in our handleEvent.
                    GstEvent *bytesSeek = gst_event_new_seek(rate, GST_FORMAT_BYTES, flags,
                                                             startType, byteStart, stopType, byteStop);
                    gst_event_set_seqnum(bytesSeek, seqnum);
                    bool seekResult = gst_pad_push_event(m_sinkPad, bytesSeek);

                    if (!seekResult)
                    {
                        GST_ERROR_OBJECT(m_sink, "[SEEK-FIX] BYTES seek upstream failed");
                        m_pendingSeekTime.store(-1);
                        return FALSE;
                    }

                    GST_INFO_OBJECT(m_sink, "[SEEK-FIX] TIME->BYTES seek forwarded successfully, "
                                    "pendingSeekTime=%" G_GINT64_FORMAT " ns (%.3f sec)",
                                    start, (gdouble)start / GST_SECOND);
                    return TRUE;
                }
                else
                {
                    // Cannot convert locally; forward the TIME seek upstream as-is.
                    GST_INFO_OBJECT(m_sink, "[SEEK-FIX] Cannot convert TIME seek to BYTES locally "
                                    "(cachedDuration=%" G_GINT64_FORMAT " ns, fileSize=%" G_GUINT64_FORMAT "), "
                                    "forwarding TIME seek upstream",
                                    cachedDur, fileSize);
                    m_pendingSeekTime.store(start);
                    m_lastSeekPositionNs.store(start);
                    // Fall through to forward upstream via shouldForwardUpstream
                }
            }
            // Update last segment (normal TIME-format segment case)
            else if (seekFormat == GST_FORMAT_TIME)
            {
                gboolean update{FALSE};
                std::lock_guard<std::mutex> lock(m_sinkMutex);
                gst_segment_do_seek(&m_lastSegment, rate, seekFormat, flags, startType, start, stopType, stop, &update);
            }
        }
#if GST_CHECK_VERSION(1, 18, 0)
        else if (flags & GST_SEEK_FLAG_INSTANT_RATE_CHANGE)
        {
            gdouble rateMultiplier = rate / m_lastSegment.rate;
            GstEvent *rateChangeEvent = gst_event_new_instant_rate_change(rateMultiplier, (GstSegmentFlags)flags);
            gst_event_set_seqnum(rateChangeEvent, gst_event_get_seqnum(event));
            gst_event_unref(event);
            if (gst_pad_send_event(m_sinkPad, rateChangeEvent) != TRUE)
            {
                GST_ERROR_OBJECT(m_sink, "Sending instant rate change failed.");
                return FALSE;
            }
            return TRUE;
        }
#endif
        else
        {
            GST_WARNING_OBJECT(m_sink, "Seek with flags 0x%X is not supported", flags);
            gst_event_unref(event);
            return FALSE;
        }
        break;
    }
#if GST_CHECK_VERSION(1, 18, 0)
    case GST_EVENT_INSTANT_RATE_SYNC_TIME:
    {
        double rate{0.0};
        GstClockTime runningTime{GST_CLOCK_TIME_NONE}, upstreamRunningTime{GST_CLOCK_TIME_NONE};
        guint32 seqnum = gst_event_get_seqnum(event);
        gst_event_parse_instant_rate_sync_time(event, &rate, &runningTime, &upstreamRunningTime);

        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
        if ((client) && (m_mediaPlayerManager.hasControl()))
        {
            GST_DEBUG_OBJECT(m_sink, "Instant playback rate change: %.2f", rate);
            m_currentInstantRateChangeSeqnum = seqnum;
            client->setPlaybackRate(rate);
        }
        break;
    }
#endif
    default:
        break;
    }

    if (shouldForwardUpstream)
    {
        bool result = gst_pad_push_event(m_sinkPad, event);
        if (!result)
        {
            GST_DEBUG_OBJECT(m_sink, "forwarding upstream event '%s' failed", GST_EVENT_TYPE_NAME(event));
        }

        return result;
    }

    gst_event_unref(event);
    return TRUE;
}

gboolean PullModePlaybackDelegate::handleEvent(GstPad *pad, GstObject *parent, GstEvent *event)
{
    GST_DEBUG_OBJECT(m_sink, "handling event %" GST_PTR_FORMAT, event);
    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_SEGMENT:
    {
        copySegment(event);
        setSegment();
        break;
    }
    case GST_EVENT_EOS:
    {
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        m_isEos = true;
        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
        if (client)
        {
            client->getFlushAndDataSynchronizer().notifyDataReceived(m_sourceId);
        }
        break;
    }
    case GST_EVENT_CAPS:
    {
        GstCaps *caps;
        gst_event_parse_caps(event, &caps);
        {
            std::lock_guard<std::mutex> lock(m_sinkMutex);
            if (m_caps)
            {
                if (!gst_caps_is_equal(caps, m_caps))
                {
                    gst_caps_unref(m_caps);
                    m_caps = gst_caps_copy(caps);
                }
            }
            else
            {
                m_caps = gst_caps_copy(caps);
            }
        }
        break;
    }
    case GST_EVENT_TAG:
    {
        // [SEEK-FIX] Cache GST_TAG_DURATION from ID3 metadata so QUERY_SEEKING/QUERY_DURATION
        // can answer when upstream and the server cannot.
        GstTagList *tagList = nullptr;
        gst_event_parse_tag(event, &tagList);
        if (tagList)
        {
            guint64 duration = 0;
            if (gst_tag_list_get_uint64(tagList, GST_TAG_DURATION, &duration) && duration > 0)
            {
                m_cachedDuration.store(static_cast<gint64>(duration));
                GST_INFO_OBJECT(m_sink, "[SEEK-FIX] GST_EVENT_TAG: cached duration=%" G_GUINT64_FORMAT
                                " ns (%.3f sec)", duration, (double)duration / GST_SECOND);
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
            gst_element_post_message(m_sink, message);
        }

        break;
    }
    case GST_EVENT_CUSTOM_DOWNSTREAM:
    case GST_EVENT_CUSTOM_DOWNSTREAM_OOB:
    {
        if (gst_event_has_name(event, "custom-instant-rate-change"))
        {
            GST_DEBUG_OBJECT(m_sink, "Change rate event received");
            changePlaybackRate(event);
        }
        break;
    }
    case GST_EVENT_FLUSH_START:
    {
        startFlushing();
        break;
    }
    case GST_EVENT_FLUSH_STOP:
    {
        gboolean resetTime{FALSE};
        gst_event_parse_flush_stop(event, &resetTime);

        stopFlushing(resetTime);
        break;
    }
    case GST_EVENT_STREAM_COLLECTION:
    {
        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
        if (!client)
        {
            gst_event_unref(event);
            return FALSE;
        }
        int32_t videoStreams{0}, audioStreams{0}, textStreams{0};
        GstStreamCollection *streamCollection{nullptr};
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
#if GST_CHECK_VERSION(1, 18, 0)
    case GST_EVENT_INSTANT_RATE_CHANGE:
    {
        guint32 seqnum = gst_event_get_seqnum(event);
        if (m_lastInstantRateChangeSeqnum == seqnum || m_currentInstantRateChangeSeqnum.load() == seqnum)
        {
            /* Ignore if we already received the instant-rate-sync-time event from the pipeline */
            GST_DEBUG_OBJECT(m_sink, "Instant rate change event with seqnum %u already handled. Ignoring...", seqnum);
            break;
        }

        m_lastInstantRateChangeSeqnum = seqnum;
        gdouble rate{0.0};
        GstSegmentFlags flags{GST_SEGMENT_FLAG_NONE};
        gst_event_parse_instant_rate_change(event, &rate, &flags);
        GstMessage *msg = gst_message_new_instant_rate_request(GST_OBJECT_CAST(m_sink), rate);
        gst_message_set_seqnum(msg, seqnum);
        gst_element_post_message(m_sink, msg);
        break;
    }
#endif
    default:
        break;
    }

    gst_event_unref(event);

    return TRUE;
}

void PullModePlaybackDelegate::copySegment(GstEvent *event)
{
    std::lock_guard<std::mutex> lock(m_sinkMutex);
    gst_event_copy_segment(event, &m_lastSegment);
}

void PullModePlaybackDelegate::setSegment()
{
    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
    if (!client)
    {
        GST_ERROR_OBJECT(m_sink, "[SEEK-FIX] Could not get the media player client");
        return;
    }
    const bool kResetTime{m_lastSegment.flags == GST_SEGMENT_FLAG_RESET};

    int64_t position;
    if (m_lastSegment.format == GST_FORMAT_TIME)
    {
        // Segment is in TIME format (normal video case) - use start directly.
        position = static_cast<int64_t>(m_lastSegment.start);
        GST_INFO_OBJECT(m_sink, "[SEEK-FIX] setSegment: TIME format, position=%" G_GINT64_FORMAT
                        " ns (%.3f sec)", position, static_cast<double>(position) / GST_SECOND);
    }
    else if (m_lastSegment.format == GST_FORMAT_BYTES)
    {
        // Audio-only pull-mode: segment.start is a byte offset, not a time.
        // If a pending seek time was stashed by handleSendEvent's TIME->BYTES conversion,
        // use that as the source position; otherwise fall back to segment.time.
        gint64 pendingTime = m_pendingSeekTime.exchange(-1);
        if (pendingTime >= 0)
        {
            position = pendingTime;
            GST_INFO_OBJECT(m_sink, "[SEEK-FIX] setSegment: BYTES + pendingSeekTime=%" G_GINT64_FORMAT
                            " ns (%.3f sec) from TIME->BYTES conversion",
                            position, static_cast<double>(position) / GST_SECOND);
        }
        else
        {
            position = static_cast<int64_t>(m_lastSegment.time);
            GST_WARNING_OBJECT(m_sink, "[SEEK-FIX] setSegment: BYTES format, start=%" G_GUINT64_FORMAT
                               " bytes, using segment.time=%" G_GINT64_FORMAT " ns (%.3f sec) as position",
                               m_lastSegment.start, position, static_cast<double>(position) / GST_SECOND);
        }
    }
    else
    {
        // Unknown format - fall back to segment.time.
        position = static_cast<int64_t>(m_lastSegment.time);
        GST_WARNING_OBJECT(m_sink, "[SEEK-FIX] setSegment: unknown format=%d, fallback time=%" G_GINT64_FORMAT,
                           m_lastSegment.format, position);
    }

    GST_INFO_OBJECT(m_sink, "[SEEK-FIX] setSegment: setSourcePosition(sourceId=%d, position=%" G_GINT64_FORMAT
                    " ns, resetTime=%d, appliedRate=%.2f, stop=%" G_GUINT64_FORMAT ")",
                    m_sourceId.load(), position, kResetTime, m_lastSegment.applied_rate, m_lastSegment.stop);

    client->setSourcePosition(m_sourceId, position, kResetTime, m_lastSegment.applied_rate, m_lastSegment.stop);

    // [SEEK-FIX] Persist the position used so GST_QUERY_POSITION can floor to it (Fix B).
    // Restrict to the audio-only progressive (BYTES) path: MSE A/V uses TIME segments and must
    // not have its reported position floored (that leaked the audio-only fix onto the video +
    // AAC sinks and caused AV audio to drop out on seek).
    if (position >= 0 && m_lastSegment.format == GST_FORMAT_BYTES)
    {
        m_lastSeekPositionNs.store(position);
    }

    m_segmentSet = true;
}

void PullModePlaybackDelegate::tryParseMp3Duration(GstBuffer *buffer)
{
    m_durationParseAttempted.store(true);

    guint64 fileSize = m_lastSegment.duration;
    if (fileSize == 0 || fileSize == static_cast<guint64>(-1))
    {
        GST_DEBUG_OBJECT(m_sink, "[SEEK-FIX] tryParseMp3Duration: no valid file size in segment");
        return;
    }

    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ))
    {
        GST_WARNING_OBJECT(m_sink, "[SEEK-FIX] tryParseMp3Duration: failed to map buffer");
        return;
    }

    if (map.size < 4)
    {
        gst_buffer_unmap(buffer, &map);
        return;
    }

    // Parse MP3 frame header (4 bytes)
    guint32 header = (map.data[0] << 24) | (map.data[1] << 16) | (map.data[2] << 8) | map.data[3];

    // Check sync word (11 bits of 1s)
    if ((header & 0xFFE00000) != 0xFFE00000)
    {
        GST_DEBUG_OBJECT(m_sink, "[SEEK-FIX] tryParseMp3Duration: no MP3 sync word in first buffer");
        gst_buffer_unmap(buffer, &map);
        return;
    }

    guint version = (header >> 19) & 0x3;    // 00=2.5, 01=reserved, 10=2, 11=1
    guint layer = (header >> 17) & 0x3;       // 01=III, 10=II, 11=I
    guint bitrateIdx = (header >> 12) & 0xF;
    guint srateIdx = (header >> 10) & 0x3;

    if (version == 0x1 || layer == 0x0 || bitrateIdx == 0 || bitrateIdx == 0xF || srateIdx == 0x3)
    {
        GST_DEBUG_OBJECT(m_sink, "[SEEK-FIX] tryParseMp3Duration: invalid MP3 header fields");
        gst_buffer_unmap(buffer, &map);
        return;
    }

    static const int kMpeg1Layer3Bitrates[] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0};
    static const int kMpeg2Layer3Bitrates[] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0};
    static const int kSampleRates[4][3] = {
        {11025, 12000, 8000},   // MPEG 2.5
        {0, 0, 0},              // reserved
        {22050, 24000, 16000},  // MPEG 2
        {44100, 48000, 32000}   // MPEG 1
    };

    int sampleRate = kSampleRates[version][srateIdx];
    int bitrate = 0;
    if (version == 3 && layer == 1) // MPEG1 Layer III
        bitrate = kMpeg1Layer3Bitrates[bitrateIdx];
    else if (layer == 1) // MPEG2/2.5 Layer III
        bitrate = kMpeg2Layer3Bitrates[bitrateIdx];
    else
    {
        GST_DEBUG_OBJECT(m_sink, "[SEEK-FIX] tryParseMp3Duration: unsupported MPEG version/layer combo");
        gst_buffer_unmap(buffer, &map);
        return;
    }

    if (bitrate == 0 || sampleRate == 0)
    {
        gst_buffer_unmap(buffer, &map);
        return;
    }

    // Try to find XING/Info header for more accurate duration.
    guint channelMode = (header >> 6) & 0x3; // 0=stereo, 1=joint, 2=dual, 3=mono
    guint xingOffset = 0;
    if (version == 3) // MPEG1
        xingOffset = (channelMode == 3) ? 17 : 32; // mono: 17, stereo: 32
    else // MPEG2/2.5
        xingOffset = (channelMode == 3) ? 9 : 17;
    xingOffset += 4; // skip frame header

    gint64 duration = -1;
    int samplesPerFrame = (version == 3) ? 1152 : 576; // MPEG1 vs MPEG2/2.5

    if (map.size >= xingOffset + 12)
    {
        const guint8 *xing = map.data + xingOffset;
        if ((xing[0] == 'X' && xing[1] == 'i' && xing[2] == 'n' && xing[3] == 'g') ||
            (xing[0] == 'I' && xing[1] == 'n' && xing[2] == 'f' && xing[3] == 'o'))
        {
            guint32 flags = (xing[4] << 24) | (xing[5] << 16) | (xing[6] << 8) | xing[7];
            if (flags & 0x1) // Frames field present
            {
                guint32 totalFrames = (xing[8] << 24) | (xing[9] << 16) | (xing[10] << 8) | xing[11];
                if (totalFrames > 0)
                {
                    duration = (gint64)totalFrames * samplesPerFrame * GST_SECOND / sampleRate;
                    GST_INFO_OBJECT(m_sink, "[SEEK-FIX] tryParseMp3Duration: XING header found! "
                                    "totalFrames=%u, samplesPerFrame=%d, sampleRate=%d -> "
                                    "duration=%" G_GINT64_FORMAT " ns (%.3f sec)",
                                    totalFrames, samplesPerFrame, sampleRate,
                                    duration, (double)duration / GST_SECOND);
                }
            }
        }
    }

    // Fallback: estimate from CBR bitrate.
    if (duration < 0)
    {
        guint64 audioSize = fileSize;
        if (m_lastSegment.position > 0 && m_lastSegment.position < fileSize)
            audioSize = fileSize - m_lastSegment.position;

        duration = (gint64)((gdouble)audioSize * 8.0 / ((gdouble)bitrate * 1000.0) * GST_SECOND);
        GST_INFO_OBJECT(m_sink, "[SEEK-FIX] tryParseMp3Duration: CBR estimate - "
                        "bitrate=%d kbps, audioSize=%" G_GUINT64_FORMAT ", sampleRate=%d -> "
                        "duration=%" G_GINT64_FORMAT " ns (%.3f sec)",
                        bitrate, audioSize, sampleRate, duration, (double)duration / GST_SECOND);
    }

    gst_buffer_unmap(buffer, &map);

    if (duration > 0)
    {
        m_cachedDuration.store(duration);
        GST_INFO_OBJECT(m_sink, "[SEEK-FIX] tryParseMp3Duration: cached duration=%" G_GINT64_FORMAT
                        " ns (%.3f sec)", duration, (double)duration / GST_SECOND);
    }
}

void PullModePlaybackDelegate::changePlaybackRate(GstEvent *event)
{
    const GstStructure *structure{gst_event_get_structure(event)};
    gdouble playbackRate{1.0};
    if (gst_structure_get_double(structure, "rate", &playbackRate) == TRUE)
    {
        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
        if (client && m_mediaPlayerManager.hasControl())
        {
            GST_DEBUG_OBJECT(m_sink, "Instant playback rate change: %.2f", playbackRate);
            client->setPlaybackRate(playbackRate);
        }
    }
}

void PullModePlaybackDelegate::startFlushing()
{
    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
    if (client)
    {
        client->getFlushAndDataSynchronizer().waitIfRequired(m_sourceId);
    }
    std::lock_guard<std::mutex> lock(m_sinkMutex);
    if (!m_isSinkFlushOngoing)
    {
        GST_INFO_OBJECT(m_sink, "Starting flushing");
        if (m_isEos)
        {
            GST_DEBUG_OBJECT(m_sink, "Flush will clear EOS state.");
            m_isEos = false;
        }
        m_isSinkFlushOngoing = true;
        m_segmentSet = false;
        clearBuffersUnlocked();
    }
}

void PullModePlaybackDelegate::stopFlushing(bool resetTime)
{
    GST_INFO_OBJECT(m_sink, "Stopping flushing");
    flushServer(resetTime);
    std::lock_guard<std::mutex> lock(m_sinkMutex);
    m_isSinkFlushOngoing = false;

    if (resetTime)
    {
        GST_DEBUG_OBJECT(m_sink, "sending reset_time message");
        gst_element_post_message(m_sink, gst_message_new_reset_time(GST_OBJECT_CAST(m_sink), 0));
    }
}

void PullModePlaybackDelegate::flushServer(bool resetTime)
{
    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
    if (!client)
    {
        GST_ERROR_OBJECT(m_sink, "Could not get the media player client");
        return;
    }

    {
        std::unique_lock<std::mutex> lock(m_sinkMutex);
        m_isServerFlushOngoing = true;
        m_isTimeResetOngoing = true;
    }
    client->flush(m_sourceId, resetTime);
}

GstFlowReturn PullModePlaybackDelegate::handleBuffer(GstBuffer *buffer)
{
    constexpr size_t kMaxInternalBuffersQueueSize = 24;
    GST_LOG_OBJECT(m_sink, "Handling buffer %p with PTS %" GST_TIME_FORMAT, buffer,
                   GST_TIME_ARGS(GST_BUFFER_PTS(buffer)));

    // [SEEK-FIX] Parse MP3 frame header for duration on the first BYTES buffer so
    // QUERY_SEEKING/QUERY_DURATION can answer in audio-only pull-mode pipelines.
    if (m_cachedDuration.load() < 0 && !m_durationParseAttempted.load() &&
        m_lastSegment.format == GST_FORMAT_BYTES)
    {
        tryParseMp3Duration(buffer);
    }

    std::unique_lock<std::mutex> lock(m_sinkMutex);

    if (m_samples.size() >= kMaxInternalBuffersQueueSize)
    {
        GST_DEBUG_OBJECT(m_sink, "Waiting for more space in buffers queue\n");
        m_needDataCondVariable.wait(lock);
    }

    if (m_isSinkFlushOngoing)
    {
        GST_DEBUG_OBJECT(m_sink, "Discarding buffer which was received during flushing");
        gst_buffer_unref(buffer);
        return GST_FLOW_FLUSHING;
    }

    GstSample *sample = gst_sample_new(buffer, m_caps, &m_lastSegment, nullptr);
    if (sample)
        m_samples.push(sample);
    else
        GST_ERROR_OBJECT(m_sink, "Failed to create a sample");

    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
    if (client)
    {
        client->getFlushAndDataSynchronizer().notifyDataReceived(m_sourceId);
    }

    setLastBuffer(buffer);

    gst_buffer_unref(buffer);

    return GST_FLOW_OK;
}

GstRefSample PullModePlaybackDelegate::getFrontSample() const
{
    std::lock_guard<std::mutex> lock(m_sinkMutex);
    if (m_isServerFlushOngoing)
    {
        GST_WARNING_OBJECT(m_sink, "Skip pulling buffer - flush is ongoing on server side...");
        return GstRefSample{};
    }
    if (!m_samples.empty())
    {
        GstSample *sample = m_samples.front();
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GST_LOG_OBJECT(m_sink, "Pulling buffer %p with PTS %" GST_TIME_FORMAT, buffer,
                       GST_TIME_ARGS(GST_BUFFER_PTS(buffer)));

        return GstRefSample{sample};
    }

    return GstRefSample{};
}

void PullModePlaybackDelegate::popSample()
{
    std::lock_guard<std::mutex> lock(m_sinkMutex);
    if (!m_samples.empty())
    {
        gst_sample_unref(m_samples.front());
        m_samples.pop();
    }
    m_needDataCondVariable.notify_all();
}

bool PullModePlaybackDelegate::isEos() const
{
    std::lock_guard<std::mutex> lock(m_sinkMutex);
    return m_samples.empty() && m_isEos;
}

bool PullModePlaybackDelegate::isReadyToSendData() const
{
    std::lock_guard<std::mutex> lock(m_sinkMutex);
    return m_isEos || m_segmentSet;
}

void PullModePlaybackDelegate::lostState()
{
    m_isStateCommitNeeded = true;
    gst_element_lost_state(m_sink);
}

bool PullModePlaybackDelegate::attachToMediaClientAndSetStreamsNumber(const uint32_t maxVideoWidth,
                                                                      const uint32_t maxVideoHeight)
{
    GstObject *parentObject = getOldestGstBinParent(m_sink);
    if (!m_mediaPlayerManager.attachMediaPlayerClient(parentObject, maxVideoWidth, maxVideoHeight, isLiveLatencyEnabled()))
    {
        GST_ERROR_OBJECT(m_sink, "Cannot attach the MediaPlayerClient");
        return false;
    }

    gchar *parentObjectName = gst_object_get_name(parentObject);
    GST_INFO_OBJECT(m_sink, "Attached media player client with parent %s(%p)", parentObjectName, parentObject);
    g_free(parentObjectName);

    return setStreamsNumber(parentObject);
}

bool PullModePlaybackDelegate::setStreamsNumber(GstObject *parentObject)
{
    int32_t videoStreams{-1}, audioStreams{-1}, subtitleStreams{-1};

    GstContext *context = gst_element_get_context(m_sink, "streams-info");
    if (context)
    {
        GST_DEBUG_OBJECT(m_sink, "Getting number of streams from \"streams-info\" context");

        guint n_video{0}, n_audio{0}, n_text{0};

        const GstStructure *streamsInfoStructure = gst_context_get_structure(context);
        gst_structure_get_uint(streamsInfoStructure, "video-streams", &n_video);
        gst_structure_get_uint(streamsInfoStructure, "audio-streams", &n_audio);
        gst_structure_get_uint(streamsInfoStructure, "text-streams", &n_text);

        if (n_video > std::numeric_limits<int32_t>::max() || n_audio > std::numeric_limits<int32_t>::max() ||
            n_text > std::numeric_limits<int32_t>::max())
        {
            GST_ERROR_OBJECT(m_sink, "Number of streams is too big, video=%u, audio=%u, text=%u", n_video, n_audio,
                             n_text);
            gst_context_unref(context);
            return false;
        }

        videoStreams = n_video;
        audioStreams = n_audio;
        subtitleStreams = n_text;

        gst_context_unref(context);
    }
    else if (getNStreamsFromParent(parentObject, videoStreams, audioStreams, subtitleStreams))
    {
        GST_DEBUG_OBJECT(m_sink, "Got number of streams from playbin2 properties");
    }
    else
    {
        // The default value of streams is V:1, A:1, S:0
        // Changing the default setting via properties is considered as DEPRECATED
        subtitleStreams = 0;
        std::lock_guard<std::mutex> lock{m_sinkMutex};
        if (m_mediaSourceType == firebolt::rialto::MediaSourceType::VIDEO)
        {
            videoStreams = m_numOfStreams;
            if (m_isSinglePathStream)
            {
                audioStreams = 0;
                subtitleStreams = 0;
            }
        }
        else if (m_mediaSourceType == firebolt::rialto::MediaSourceType::AUDIO)
        {
            audioStreams = m_numOfStreams;
            if (m_isSinglePathStream)
            {
                videoStreams = 0;
                subtitleStreams = 0;
            }
        }
        else if (m_mediaSourceType == firebolt::rialto::MediaSourceType::SUBTITLE)
        {
            subtitleStreams = m_numOfStreams;
            if (m_isSinglePathStream)
            {
                videoStreams = 0;
                audioStreams = 0;
            }
        }
    }

    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
    if (!client)
    {
        GST_ERROR_OBJECT(m_sink, "MediaPlayerClient is nullptr");
        return false;
    }

    client->handleStreamCollection(audioStreams, videoStreams, subtitleStreams);

    return true;
}

bool PullModePlaybackDelegate::isLiveLatencyEnabled() const
{
    GstContext *context = gst_element_get_context(m_sink, "streams-info");
    if (context)
    {
        GST_DEBUG_OBJECT(m_sink, "Checking if live latency is enabled from \"streams-info\" context");

        gboolean isEnabled{FALSE};

        const GstStructure *streamsInfoStructure = gst_context_get_structure(context);
        gst_structure_get_boolean(streamsInfoStructure, "enable-live-latency", &isEnabled);

        gst_context_unref(context);
        return isEnabled != FALSE;
    }
    return false;
}

GstSample *PullModePlaybackDelegate::getLastSample() const
{
    std::lock_guard<std::mutex> lock(m_sinkMutex);
    if (m_enableLastSample && m_lastBuffer)
    {
        return gst_sample_new(m_lastBuffer, m_caps, &m_lastSegment, nullptr);
    }
    return nullptr;
}

void PullModePlaybackDelegate::setLastBuffer(GstBuffer *buffer)
{
    if (m_enableLastSample)
    {
        if (m_lastBuffer)
        {
            gst_buffer_unref(m_lastBuffer);
        }
        if (buffer)
        {
            m_lastBuffer = gst_buffer_ref(buffer);
        }
        else
        {
            m_lastBuffer = nullptr;
        }
    }
}
