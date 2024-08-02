/*
 * Copyright (C) 2024 Sky UK
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

#include "GStreamerMSEMediaPlayerClient.h"
#include "RialtoGStreamerMSEBaseSink.h"
#include <gst/base/gstbasesink.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define RIALTO_TYPE_MSE_SUBTITLE_SINK (rialto_mse_subtitle_sink_get_type())
#define RIALTO_MSE_SUBTITLE_SINK(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), RIALTO_TYPE_MSE_SUBTITLE_SINK, RialtoMSESubtitleSink))
#define RIALTO_MSE_SUBTITLE_SINK_CLASS(klass)                                                                             \
    (G_TYPE_CHECK_CLASS_CAST((klass), RIALTO_TYPE_MSE_SUBTITLE_SINK, RialtoMSESubtitleSinkClass))
#define RIALTO_IS_MSE_SUBTITLE_SINK(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), RIALTO_TYPE_MSE_SUBTITLE_SINK))
#define RIALTO_IS_MSE_SUBTITLE_SINK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), RIALTO_TYPE_MSE_SUBTITLE_SINK))



typedef struct _RialtoMSESubtitleSink RialtoMSESubtitleSink;
typedef struct _RialtoMSESubtitleSinkClass RialtoMSESubtitleSinkClass;
typedef struct _RialtoMSESubtitleSinkPrivate RialtoMSESubtitleSinkPrivate;

//TODO-klops: move to separate file 
struct _RialtoMSESubtitleSinkPrivate
{
    std::string m_textTrackIdentifier;
    uint32_t m_videoId = 0;
};

struct _RialtoMSESubtitleSink
{
    RialtoMSEBaseSink parent;
    RialtoMSESubtitleSinkPrivate *priv;
};

struct _RialtoMSESubtitleSinkClass
{
    RialtoMSEBaseSinkClass parent_class;
};

GType rialto_mse_subtitle_sink_get_type(void);

void rialto_mse_subtitle_sink_set_client_backend(GstElement *sink,
                                              const std::shared_ptr<GStreamerMSEMediaPlayerClient> &mediaPlayerClient);
G_END_DECLS
