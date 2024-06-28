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

#include "PlaybinStub.h"

G_DEFINE_FLAGS_TYPE(GstPlayFlags, gst_play_flags, G_DEFINE_ENUM_VALUE(GST_PLAY_FLAG_VIDEO, "video"),
                    G_DEFINE_ENUM_VALUE(GST_PLAY_FLAG_AUDIO, "audio"), G_DEFINE_ENUM_VALUE(GST_PLAY_FLAG_TEXT, "text"));

enum
{
    PROP_0,
    PROP_FLAGS,
    PROP_N_VIDEO,
    PROP_N_AUDIO,
    PROP_N_TEXT
};

static void gst_play_bin_stub_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *spec);
static void gst_play_bin_stub_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *spec);
static GType gst_play_bin_stub_get_type(void);

GST_DEBUG_CATEGORY_STATIC(gst_play_bin_stub_debug);
#define GST_CAT_DEFAULT gst_play_bin_stub_debug

G_DEFINE_TYPE_WITH_CODE(GstPlayBinStub, gst_play_bin_stub, GST_TYPE_PIPELINE,
                        GST_DEBUG_CATEGORY_INIT(gst_play_bin_stub_debug, "playbinstub", GST_RANK_NONE, "play bin stub"));

void gst_play_bin_stub_class_init(GstPlayBinStubClass *klass)
{
    GObjectClass *gobject_klass;
    GstElementClass *gstelement_klass;

    gobject_klass = (GObjectClass *)klass;
    gstelement_klass = (GstElementClass *)klass;

    gobject_klass->set_property = gst_play_bin_stub_set_property;
    gobject_klass->get_property = gst_play_bin_stub_get_property;

    g_object_class_install_property(gobject_klass, PROP_FLAGS,
                                    g_param_spec_uint("flags", "flags", "flags", 1, G_MAXINT, 1,
                                                      GParamFlags(G_PARAM_READWRITE)));
    g_object_class_install_property(gobject_klass, PROP_N_VIDEO,
                                    g_param_spec_int("n-video", "Number Video", "Total number of video streams", 0,
                                                     G_MAXINT, 0,
                                                     GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(gobject_klass, PROP_N_AUDIO,
                                    g_param_spec_int("n-audio", "Number Audio", "Total number of audio streams", 0,
                                                     G_MAXINT, 0,
                                                     GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(gobject_klass, PROP_N_TEXT,
                                    g_param_spec_int("n-text", "Number Text", "Total number of text streams", 0, G_MAXINT,
                                                     0, GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    gst_element_class_set_static_metadata(gstelement_klass, "Player Bin 2 Stub", "Generic/Bin/Player",
                                          "GstPlayBin2 stub", "Marcin Wojciechowski <marcin.wojciechowski@sky.uk>");

    gst_type_mark_as_plugin_api(GST_TYPE_PLAY_FLAGS, GstPluginAPIFlags(0));
}

static void gst_play_bin_stub_init(GstPlayBinStub *playbin) {}

static void gst_play_bin_stub_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    GstPlayBinStub *playbin = GST_PLAY_BIN_STUB(object);

    switch (prop_id)
    {
    case PROP_FLAGS:
        playbin->flags = g_value_get_uint(value);
        break;
    case PROP_N_VIDEO:
        playbin->n_video = g_value_get_int(value);
        break;
    case PROP_N_AUDIO:
        playbin->n_audio = g_value_get_int(value);
        break;
    case PROP_N_TEXT:
        playbin->n_text = g_value_get_int(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gst_play_bin_stub_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    GstPlayBinStub *playbin = GST_PLAY_BIN_STUB(object);

    switch (prop_id)
    {
    case PROP_FLAGS:
        g_value_set_uint(value, playbin->flags);
        break;
    case PROP_N_VIDEO:
    {
        g_value_set_int(value, playbin->n_video);
        break;
    }
    case PROP_N_AUDIO:
    {
        g_value_set_int(value, playbin->n_audio);
        break;
    }
    case PROP_N_TEXT:
    {
        g_value_set_int(value, playbin->n_text);
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static gboolean play_bin_stub_init(GstPlugin *plugin)
{
    return gst_element_register(plugin, "playbinstub", GST_RANK_NONE, GST_TYPE_PLAY_BIN_STUB);
}

bool register_play_bin_stub()
{
    return gst_plugin_register_static(GST_VERSION_MAJOR, GST_VERSION_MINOR, "playbinstub", "Play bin stub",
                                      play_bin_stub_init, "1.0", "LGPL", PACKAGE, PACKAGE, "http://gstreamer.net/");
}