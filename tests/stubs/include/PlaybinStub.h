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

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_PLAY_BIN_STUB (gst_play_bin_stub_get_type())
#define GST_PLAY_BIN_STUB(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_PLAY_BIN_STUB, GstPlayBinStub))
#define GST_PLAY_BIN_STUB_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_PLAY_BIN_STUB, GstPlayBinStubClass))
#define GST_IS_PLAY_BIN_STUB(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_PLAY_BIN_STUB))
#define GST_IS_PLAY_BIN_STUB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_PLAY_BIN_STUB))

typedef struct _GstPlayBinStub GstPlayBinStub;
typedef struct _GstPlayBinStubClass GstPlayBinStubClass;

struct _GstPlayBinStub
{
    GstPipeline parent;

    gint n_video{0};
    gint n_audio{0};
    gint n_text{0};
    guint flags{0};
};

struct _GstPlayBinStubClass
{
    GstPipelineClass parent_class;
};

typedef enum
{
    GST_PLAY_FLAG_VIDEO = (1 << 0),
    GST_PLAY_FLAG_AUDIO = (1 << 1),
    GST_PLAY_FLAG_TEXT = (1 << 2),
} GstPlayFlags;

#define GST_TYPE_PLAY_FLAGS (gst_play_flags_get_type())

bool register_play_bin_stub();

G_END_DECLS