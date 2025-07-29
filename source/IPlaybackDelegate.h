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

#pragma once

#include "GStreamerUtils.h"
#include "MediaCommon.h"
#include <gst/gst.h>
#include <memory>

class IPlaybackDelegate
{
public:
    enum class Property
    {
        // PullModePlaybackDelegate Properties
        IsSinglePathStream,
        NumberOfStreams,
        HasDrm,
        Stats,

        // PullModeAudioPlaybackDelegate Properties
        Volume,
        Mute,
        Gap,
        LowLatency,
        Sync,
        SyncOff,
        StreamSyncMode,
        AudioFade,
        FadeVolume,
        LimitBufferingMs,
        UseBuffering,
        Async,

        // PullModeVideoPlaybackDelegate Properties
        WindowSet,
        MaxVideoWidth,
        MaxVideoHeight,
        FrameStepOnPreroll,
        ImmediateOutput,
        SyncmodeStreaming,
        ShowVideoWindow,
        IsMaster,

        // PullModeSubtitlePlaybackDelegate Properties
        TextTrackIdentifier,
        WindowId,

        // PushModeAudioPlaybackDelegate Properties
        TsOffset,
    };

    IPlaybackDelegate() = default;
    virtual ~IPlaybackDelegate() = default;

    IPlaybackDelegate(const IPlaybackDelegate &) = delete;
    IPlaybackDelegate(IPlaybackDelegate &&) = delete;
    IPlaybackDelegate &operator=(const IPlaybackDelegate &) = delete;
    IPlaybackDelegate &operator=(IPlaybackDelegate &&) = delete;

    virtual void setSourceId(int32_t sourceId) = 0;

    virtual void handleEos() = 0;
    virtual void handleFlushCompleted() = 0;
    virtual void handleStateChanged(firebolt::rialto::PlaybackState state) = 0;
    virtual void handleError(const char *message, gint code = 0) = 0;
    virtual void handleQos(uint64_t processed, uint64_t dropped) const = 0;

    virtual GstStateChangeReturn changeState(GstStateChange transition) = 0;
    virtual void postAsyncStart() = 0;
    virtual void setProperty(const Property &type, const GValue *value) = 0;
    virtual void getProperty(const Property &type, GValue *value) = 0;
    virtual std::optional<gboolean> handleQuery(GstQuery *query) const = 0;
    virtual gboolean handleSendEvent(GstEvent *event) = 0;
    virtual gboolean handleEvent(GstPad *pad, GstObject *parent, GstEvent *event) = 0;
    virtual GstFlowReturn handleBuffer(GstBuffer *buffer) = 0;
    virtual GstRefSample getFrontSample() const = 0;
    virtual void popSample() = 0;
    virtual bool isEos() const = 0;
    virtual void lostState() = 0;
    virtual bool attachToMediaClientAndSetStreamsNumber(const uint32_t maxVideoWidth = 0,
                                                        const uint32_t maxVideoHeight = 0) = 0;
};
