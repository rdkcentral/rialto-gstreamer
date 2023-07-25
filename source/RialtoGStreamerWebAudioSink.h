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

#pragma once

#include "ControlBackendInterface.h"
#include "GStreamerWebAudioPlayerClient.h"
#include <MediaCommon.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define RIALTO_TYPE_WEB_AUDIO_SINK (rialto_web_audio_sink_get_type())
#define RIALTO_WEB_AUDIO_SINK(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), RIALTO_TYPE_WEB_AUDIO_SINK, RialtoWebAudioSink))
#define RIALTO_WEB_AUDIO_SINK_CLASS(klass)                                                                             \
    (G_TYPE_CHECK_CLASS_CAST((klass), RIALTO_TYPE_WEB_AUDIO_SINK, RialtoWebAudioSinkClass))
#define RIALTO_IS_WEB_AUDIO_SINK(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), RIALTO_TYPE_WEB_AUDIO_SINK))
#define RIALTO_IS_WEB_AUDIO_SINK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), RIALTO_TYPE_WEB_AUDIO_SINK))

typedef struct _RialtoWebAudioSink RialtoWebAudioSink;
typedef struct _RialtoWebAudioSinkClass RialtoWebAudioSinkClass;
typedef struct _RialtoWebAudioSinkPrivate RialtoWebAudioSinkPrivate;

struct _RialtoWebAudioSinkPrivate
{
    std::shared_ptr<GStreamerWebAudioPlayerClient> m_webAudioClient;
    std::unique_ptr<firebolt::rialto::client::ControlBackendInterface> m_rialtoControlClient;
    bool m_isPlayingDelayed{false};
    std::atomic<bool> m_isStateCommitNeeded{false};
};

struct _RialtoWebAudioSink
{
    GstElement parent;
    RialtoWebAudioSinkPrivate *priv;
};

struct _RialtoWebAudioSinkClass
{
    GstElementClass parent_class;
};

GType rialto_web_audio_sink_get_type(void);

void rialto_web_audio_handle_rialto_server_state_changed(GstElement *sink, firebolt::rialto::WebAudioPlayerState state);
void rialto_web_audio_handle_rialto_server_eos(GstElement *sink);
void rialto_web_audio_handle_rialto_server_error(GstElement *sink);

G_END_DECLS
