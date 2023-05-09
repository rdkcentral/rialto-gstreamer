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
#include <gst/base/gstbasesink.h>
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
    GstElement *mAppSink;
    std::shared_ptr<GStreamerWebAudioPlayerClient> mWebAudioClient;
    std::unique_ptr<firebolt::rialto::client::ControlBackendInterface> mRialtoControlClient;
};

struct _RialtoWebAudioSink
{
    GstBin parent;
    RialtoWebAudioSinkPrivate *priv;
};

struct _RialtoWebAudioSinkClass
{
    GstBinClass parent_class;
};

GType rialto_web_audio_sink_get_type(void);

G_END_DECLS
