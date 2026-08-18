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
                                        gst_caps_from_string("audio/x-pn-realaudio"),
                                        gst_caps_from_string("audio/x-dts"),
                                        gst_caps_from_string("audio/x-private1-dts"),
                                        gst_caps_from_string("audio/x-avs")};

    // Supported capabilities
    const firebolt::rialto::common::AudioDecoderCapability capability{
        firebolt::rialto::common::PcmCapability{},       firebolt::rialto::common::AacCapability{},
        firebolt::rialto::common::MpegAudioCapability{}, firebolt::rialto::common::Mp3Capability{},
        firebolt::rialto::common::AlacCapability{},      firebolt::rialto::common::SbcCapability{},
        firebolt::rialto::common::DolbyAc3Capability{},  firebolt::rialto::common::DolbyAc4Capability{},
        firebolt::rialto::common::DolbyEac3Capability{}, firebolt::rialto::common::DolbyTruehdCapability{},
        firebolt::rialto::common::FlacCapability{},      firebolt::rialto::common::VorbisCapability{},
        firebolt::rialto::common::OpusCapability{},      firebolt::rialto::common::RealAudioCapability{},
        firebolt::rialto::common::UsacCapability{},      firebolt::rialto::common::DtsCapability{},
        firebolt::rialto::common::AvsCapability{}};
    const firebolt::rialto::common::AudioDecoderCapabilities audioDecoderCapabilities{"1.0", "1.1", {capability}};

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
    const firebolt::rialto::common::VideoCodecCapabilities codecCapabilities{
        firebolt::rialto::common::Mpeg2CodecCapability{}, firebolt::rialto::common::H264CodecCapability{},
        firebolt::rialto::common::H265CodecCapability{}, firebolt::rialto::common::Vp9CodecCapability{},
        firebolt::rialto::common::Av1CodecCapability{}};
    const firebolt::rialto::common::VideoDecoderCapability capability{codecCapabilities};
    const firebolt::rialto::common::VideoDecoderCapabilities videoDecoderCapabilities{"1.0", "1.1", {capability}};

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

TEST_F(GStreamerMSEUtilsTests, shouldRegisterEac3WhenDolbyEac3Present)
{
    const firebolt::rialto::common::AudioDecoderCapability capability{.dolbyEac3 = firebolt::rialto::common::DolbyEac3Capability{}};
    const firebolt::rialto::common::AudioDecoderCapabilities audioDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, audioDecoderCapabilities));
    GstPadTemplate *sinkPadTemplate{gst_element_class_get_pad_template(elementClass, "sink")};
    GstCaps *caps{gst_pad_template_get_caps(sinkPadTemplate)};
    GstCaps *eac3Caps = gst_caps_from_string("audio/x-eac3");
    EXPECT_TRUE(gst_caps_is_subset(eac3Caps, caps));
    gst_caps_unref(eac3Caps);
    gst_caps_unref(caps);
    gst_object_unref(sink);
}

TEST_F(GStreamerMSEUtilsTests, shouldNotRegisterEac3WhenOnlyDolbyAc3Present)
{
    const firebolt::rialto::common::AudioDecoderCapability capability{.dolbyAc3 = firebolt::rialto::common::DolbyAc3Capability{}};
    const firebolt::rialto::common::AudioDecoderCapabilities audioDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, audioDecoderCapabilities));
    GstPadTemplate *sinkPadTemplate{gst_element_class_get_pad_template(elementClass, "sink")};
    GstCaps *caps{gst_pad_template_get_caps(sinkPadTemplate)};
    GstCaps *ac3Caps = gst_caps_from_string("audio/x-ac3");
    GstCaps *eac3Caps = gst_caps_from_string("audio/x-eac3");
    EXPECT_TRUE(gst_caps_is_subset(ac3Caps, caps));
    EXPECT_FALSE(gst_caps_is_subset(eac3Caps, caps));
    gst_caps_unref(ac3Caps);
    gst_caps_unref(eac3Caps);
    gst_caps_unref(caps);
    gst_object_unref(sink);
}

TEST_F(GStreamerMSEUtilsTests, shouldNotRegisterWma)
{
    const firebolt::rialto::common::AudioDecoderCapability capability{.pcm = firebolt::rialto::common::PcmCapability{}};
    const firebolt::rialto::common::AudioDecoderCapabilities audioDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, audioDecoderCapabilities));
    GstPadTemplate *sinkPadTemplate{gst_element_class_get_pad_template(elementClass, "sink")};
    GstCaps *caps{gst_pad_template_get_caps(sinkPadTemplate)};
    GstCaps *wmaCaps = gst_caps_from_string("audio/x-wma");
    EXPECT_FALSE(gst_caps_is_subset(wmaCaps, caps));
    gst_caps_unref(wmaCaps);
    gst_caps_unref(caps);
    gst_object_unref(sink);
}

TEST_F(GStreamerMSEUtilsTests, shouldNotRegisterDolbyMatRaw)
{
    // No pcm and no dolbyMat — audio/x-raw must not appear
    const firebolt::rialto::common::AudioDecoderCapability capability{.aac = firebolt::rialto::common::AacCapability{}};
    const firebolt::rialto::common::AudioDecoderCapabilities audioDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, audioDecoderCapabilities));
    GstPadTemplate *sinkPadTemplate{gst_element_class_get_pad_template(elementClass, "sink")};
    GstCaps *caps{gst_pad_template_get_caps(sinkPadTemplate)};
    GstCaps *rawCaps = gst_caps_from_string("audio/x-raw");
    EXPECT_FALSE(gst_caps_is_subset(rawCaps, caps));
    gst_caps_unref(rawCaps);
    gst_caps_unref(caps);
    gst_object_unref(sink);
}

TEST_F(GStreamerMSEUtilsTests, shouldNotRegisterVideoCodecWhenOptionalIsNullopt)
{
    // Only h264 present — mpeg2/h265/vp9/av1 must not be registered
    const firebolt::rialto::common::VideoCodecCapabilities codecCapabilities{
        std::nullopt, firebolt::rialto::common::H264CodecCapability{}, std::nullopt, std::nullopt, std::nullopt};
    const firebolt::rialto::common::VideoDecoderCapability capability{codecCapabilities};
    const firebolt::rialto::common::VideoDecoderCapabilities videoDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, videoDecoderCapabilities));
    GstPadTemplate *sinkPadTemplate{gst_element_class_get_pad_template(elementClass, "sink")};
    GstCaps *caps{gst_pad_template_get_caps(sinkPadTemplate)};
    GstCaps *h264Caps = gst_caps_from_string("video/x-h264");
    GstCaps *mpeg2Caps = gst_caps_from_string("video/mpeg, mpegversion=2");
    GstCaps *vp9Caps = gst_caps_from_string("video/x-vp9");
    EXPECT_TRUE(gst_caps_is_subset(h264Caps, caps));
    EXPECT_FALSE(gst_caps_is_subset(mpeg2Caps, caps));
    EXPECT_FALSE(gst_caps_is_subset(vp9Caps, caps));
    gst_caps_unref(h264Caps);
    gst_caps_unref(mpeg2Caps);
    gst_caps_unref(vp9Caps);
    gst_caps_unref(caps);
    gst_object_unref(sink);
}
