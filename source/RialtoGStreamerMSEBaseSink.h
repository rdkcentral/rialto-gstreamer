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

#pragma once

#include <IMediaPipeline.h>
#include <MediaCommon.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define RIALTO_TYPE_MSE_BASE_SINK (rialto_mse_base_sink_get_type())
#define RIALTO_MSE_BASE_SINK(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), RIALTO_TYPE_MSE_BASE_SINK, RialtoMSEBaseSink))
#define RIALTO_MSE_BASE_SINK_CLASS(klass)                                                                              \
    (G_TYPE_CHECK_CLASS_CAST((klass), RIALTO_TYPE_MSE_BASE_SINK, RialtoMSEBaseSinkClass))
#define RIALTO_IS_MSE_BASE_SINK(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), RIALTO_TYPE_MSE_BASE_SINK))
#define RIALTO_IS_MSE_BASE_SINK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), RIALTO_TYPE_MSE_BASE_SINK))

typedef struct _RialtoMSEBaseSink RialtoMSEBaseSink;
typedef struct _RialtoMSEBaseSinkClass RialtoMSEBaseSinkClass;
typedef struct _RialtoMSEBaseSinkPrivate RialtoMSEBaseSinkPrivate;

struct _RialtoMSEBaseSink
{
    GstElement parent;
    RialtoMSEBaseSinkPrivate *priv;
};

struct _RialtoMSEBaseSinkClass
{
    GstElementClass parent_class;
};

namespace firebolt::rialto::client
{
class MediaPlayerBackend;
}

GType rialto_mse_base_sink_get_type(void);

GstSample *rialto_mse_base_sink_get_front_sample(RialtoMSEBaseSink *sink);
void rialto_mse_base_sink_pop_sample(RialtoMSEBaseSink *sink);
bool rialto_mse_base_sink_is_eos(RialtoMSEBaseSink *sink);

void rialto_mse_base_handle_rialto_server_state_changed(RialtoMSEBaseSink *sink, firebolt::rialto::PlaybackState state);
void rialto_mse_base_handle_rialto_server_eos(RialtoMSEBaseSink *sink);
void rialto_mse_base_handle_rialto_server_completed_seek(RialtoMSEBaseSink *sink);
void rialto_mse_base_handle_rialto_server_sent_qos(RialtoMSEBaseSink *sink, uint64_t processed, uint64_t dropped);

bool rialto_mse_base_sink_initialise_sinkpad(RialtoMSEBaseSink *sink);
GstFlowReturn rialto_mse_base_sink_chain(GstPad *pad, GstObject *parent, GstBuffer *buf);
bool rialto_mse_base_sink_event(GstPad *pad, GstObject *parent, GstEvent *event);
GstObject *rialto_mse_base_get_oldest_gst_bin_parent(GstElement *element);
firebolt::rialto::SegmentAlignment rialto_mse_base_sink_get_segment_alignment(RialtoMSEBaseSink *sink,
                                                                              const GstStructure *s);
std::vector<uint8_t> rialto_mse_base_sink_get_codec_data(RialtoMSEBaseSink *sink, const GstStructure *structure);
firebolt::rialto::StreamFormat rialto_mse_base_sink_get_stream_format(RialtoMSEBaseSink *sink,
                                                                      const GstStructure *structure);
bool rialto_mse_base_sink_get_dv_profile(RialtoMSEBaseSink *sink, const GstStructure *s, uint32_t &dvProfile);
void rialto_mse_base_sink_lost_state(RialtoMSEBaseSink *sink);
}
;
