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

#include "GStreamerMSEUtils.h"
#include "RialtoGstTest.h"
#include <gtest/gtest.h>

class GStreamerMSEUtilsTests : public RialtoGstTest
{
};

TEST_F(GStreamerMSEUtilsTests, shouldConvertLayout)
{
    EXPECT_EQ(rialto_mse_sink_convert_layout(""), std::nullopt);
    EXPECT_EQ(rialto_mse_sink_convert_layout("interleaved"), firebolt::rialto::Layout::INTERLEAVED);
    EXPECT_EQ(rialto_mse_sink_convert_layout("non-interleaved"), firebolt::rialto::Layout::NON_INTERLEAVED);
}

TEST_F(GStreamerMSEUtilsTests, shouldConvertFormat)
{
    EXPECT_EQ(rialto_mse_sink_convert_format(""), std::nullopt);
    EXPECT_EQ(rialto_mse_sink_convert_format("S8"), firebolt::rialto::Format::S8);
    EXPECT_EQ(rialto_mse_sink_convert_format("U8"), firebolt::rialto::Format::U8);
    EXPECT_EQ(rialto_mse_sink_convert_format("S16LE"), firebolt::rialto::Format::S16LE);
    EXPECT_EQ(rialto_mse_sink_convert_format("S16BE"), firebolt::rialto::Format::S16BE);
    EXPECT_EQ(rialto_mse_sink_convert_format("U16LE"), firebolt::rialto::Format::U16LE);
    EXPECT_EQ(rialto_mse_sink_convert_format("U16BE"), firebolt::rialto::Format::U16BE);
    EXPECT_EQ(rialto_mse_sink_convert_format("S24_32LE"), firebolt::rialto::Format::S24_32LE);
    EXPECT_EQ(rialto_mse_sink_convert_format("S24_32BE"), firebolt::rialto::Format::S24_32BE);
    EXPECT_EQ(rialto_mse_sink_convert_format("U24_32LE"), firebolt::rialto::Format::U24_32LE);
    EXPECT_EQ(rialto_mse_sink_convert_format("U24_32BE"), firebolt::rialto::Format::U24_32BE);
    EXPECT_EQ(rialto_mse_sink_convert_format("S32LE"), firebolt::rialto::Format::S32LE);
    EXPECT_EQ(rialto_mse_sink_convert_format("S32BE"), firebolt::rialto::Format::S32BE);
    EXPECT_EQ(rialto_mse_sink_convert_format("U32LE"), firebolt::rialto::Format::U32LE);
    EXPECT_EQ(rialto_mse_sink_convert_format("U32BE"), firebolt::rialto::Format::U32BE);
    EXPECT_EQ(rialto_mse_sink_convert_format("S24LE"), firebolt::rialto::Format::S24LE);
    EXPECT_EQ(rialto_mse_sink_convert_format("S24BE"), firebolt::rialto::Format::S24BE);
    EXPECT_EQ(rialto_mse_sink_convert_format("U24LE"), firebolt::rialto::Format::U24LE);
    EXPECT_EQ(rialto_mse_sink_convert_format("U24BE"), firebolt::rialto::Format::U24BE);
    EXPECT_EQ(rialto_mse_sink_convert_format("S20LE"), firebolt::rialto::Format::S20LE);
    EXPECT_EQ(rialto_mse_sink_convert_format("S20BE"), firebolt::rialto::Format::S20BE);
    EXPECT_EQ(rialto_mse_sink_convert_format("U20LE"), firebolt::rialto::Format::U20LE);
    EXPECT_EQ(rialto_mse_sink_convert_format("U20BE"), firebolt::rialto::Format::U20BE);
    EXPECT_EQ(rialto_mse_sink_convert_format("S18LE"), firebolt::rialto::Format::S18LE);
    EXPECT_EQ(rialto_mse_sink_convert_format("S18BE"), firebolt::rialto::Format::S18BE);
    EXPECT_EQ(rialto_mse_sink_convert_format("U18LE"), firebolt::rialto::Format::U18LE);
    EXPECT_EQ(rialto_mse_sink_convert_format("U18BE"), firebolt::rialto::Format::U18BE);
    EXPECT_EQ(rialto_mse_sink_convert_format("F32LE"), firebolt::rialto::Format::F32LE);
    EXPECT_EQ(rialto_mse_sink_convert_format("F32BE"), firebolt::rialto::Format::F32BE);
    EXPECT_EQ(rialto_mse_sink_convert_format("F64LE"), firebolt::rialto::Format::F64LE);
    EXPECT_EQ(rialto_mse_sink_convert_format("F64BE"), firebolt::rialto::Format::F64BE);
}

TEST_F(GStreamerMSEUtilsTests, shouldFillAudioDecoderCapabilities)
{
    // Expected caps
    std::vector<GstCaps *> expectedCaps{gst_caps_from_string("audio/x-raw"),
                                        gst_caps_from_string("audio/b-wav"),
                                        gst_caps_from_string("audio/mpeg, mpegversion=(int)2"),
                                        gst_caps_from_string("audio/mpeg, mpegversion=(int)4"),
                                        gst_caps_from_string("audio/mpeg, mpegversion=(int)1"),
                                        gst_caps_from_string("audio/x-alac"),
                                        gst_caps_from_string("audio/x-sbc"),
                                        gst_caps_from_string("audio/x-ac3"),
                                        gst_caps_from_string("audio/x-eac3"),
                                        gst_caps_from_string("audio/x-ac4"),
                                        gst_caps_from_string("audio/ac4"),
                                        gst_caps_from_string("audio/x-true-hd"),
                                        gst_caps_from_string("audio/x-flac"),
                                        gst_caps_from_string("audio/x-vorbis"),
                                        gst_caps_from_string("audio/x-opus"),
                                        gst_caps_from_string("audio/x-wma"),
                                        gst_caps_from_string("audio/x-pn-realaudio"),
                                        gst_caps_from_string("audio/x-dts"),
                                        gst_caps_from_string("audio/x-private1-dts"),
                                        gst_caps_from_string("audio/x-avs")};

    // Supported capabilities
    const firebolt::rialto::AudioDecoderCapability
        capability{firebolt::rialto::PcmCapability{},       firebolt::rialto::AacCapability{},
                   firebolt::rialto::MpegAudioCapability{}, firebolt::rialto::Mp3Capability{},
                   firebolt::rialto::AlacCapability{},      firebolt::rialto::SbcCapability{},
                   firebolt::rialto::DolbyAc3Capability{},  firebolt::rialto::DolbyAc4Capability{},
                   firebolt::rialto::DolbyMatCapability{},  firebolt::rialto::DolbyTruehdCapability{},
                   firebolt::rialto::FlacCapability{},      firebolt::rialto::VorbisCapability{},
                   firebolt::rialto::OpusCapability{},      firebolt::rialto::WmaCapability{},
                   firebolt::rialto::RealAudioCapability{}, firebolt::rialto::UsacCapability{},
                   firebolt::rialto::DtsCapability{},       firebolt::rialto::AvsCapability{}};
    const firebolt::rialto::AudioDecoderCapabilities audioDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, audioDecoderCapabilities));
    GstPadTemplate *sinkPadTemplate{gst_element_class_get_pad_template(elementClass, "sink")};
    GstCaps *caps{gst_pad_template_get_caps(sinkPadTemplate)};
    for (GstCaps *expectedCap : expectedCaps)
    {
        EXPECT_TRUE(gst_caps_is_subset(expectedCap, caps));
    }
    gst_caps_unref(caps);
    gst_object_unref(sink);

    for (GstCaps *expectedCap : expectedCaps)
    {
        gst_caps_unref(expectedCap);
    }
}

TEST_F(GStreamerMSEUtilsTests, shouldFillVideoDecoderCapabilities)
{
    // Expected caps
    std::vector<GstCaps *> expectedCaps{
        gst_caps_from_string("video/mpeg, mpegversion=2"),
        gst_caps_from_string("video/x-h264"),
        gst_caps_from_string("video/x-h265"),
        gst_caps_from_string("video/x-vp9"),
        gst_caps_from_string("video/x-av1"),
    };

    // Supported capabilities
    const firebolt::rialto::VideoCodecCapabilities codecCapabilities{{firebolt::rialto::Mpeg2Profile{}},
                                                                     {firebolt::rialto::H264Profile{}},
                                                                     {firebolt::rialto::H265Profile{}},
                                                                     {firebolt::rialto::Vp9Profile{}},
                                                                     {firebolt::rialto::Av1Profile{}}};
    const firebolt::rialto::VideoDecoderCapability capability{codecCapabilities, {}};
    const firebolt::rialto::VideoDecoderCapabilities videoDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, videoDecoderCapabilities));
    GstPadTemplate *sinkPadTemplate{gst_element_class_get_pad_template(elementClass, "sink")};
    GstCaps *caps{gst_pad_template_get_caps(sinkPadTemplate)};
    for (GstCaps *expectedCap : expectedCaps)
    {
        EXPECT_TRUE(gst_caps_is_subset(expectedCap, caps));
    }
    gst_caps_unref(caps);
    gst_object_unref(sink);

    for (GstCaps *expectedCap : expectedCaps)
    {
        gst_caps_unref(expectedCap);
    }
}