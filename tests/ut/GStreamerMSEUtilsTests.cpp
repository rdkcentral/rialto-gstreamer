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
#include <cstring>
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
    const firebolt::rialto::common::AudioDecoderCapability
        capability{firebolt::rialto::common::PcmCapability{},       firebolt::rialto::common::AacCapability{},
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
    const firebolt::rialto::common::VideoCodecCapabilities
        codecCapabilities{firebolt::rialto::common::Mpeg2CodecCapability{},
                          firebolt::rialto::common::H264CodecCapability{},
                          firebolt::rialto::common::H265CodecCapability{},
                          firebolt::rialto::common::Vp9CodecCapability{}, firebolt::rialto::common::Av1CodecCapability{}};
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
    firebolt::rialto::common::AudioDecoderCapability capability{};
    capability.dolbyEac3 = firebolt::rialto::common::DolbyEac3Capability{};
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
    firebolt::rialto::common::AudioDecoderCapability capability{};
    capability.dolbyAc3 = firebolt::rialto::common::DolbyAc3Capability{};
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
    firebolt::rialto::common::AudioDecoderCapability capability{};
    capability.pcm = firebolt::rialto::common::PcmCapability{};
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
    firebolt::rialto::common::AudioDecoderCapability capability{};
    capability.aac = firebolt::rialto::common::AacCapability{};
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
    const firebolt::rialto::common::VideoCodecCapabilities codecCapabilities{std::nullopt,
                                                                             firebolt::rialto::common::H264CodecCapability{},
                                                                             std::nullopt, std::nullopt, std::nullopt};
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

TEST_F(GStreamerMSEUtilsTests, shouldFillSupportedCapsFromMimeTypes)
{
    std::vector<std::string> supportedMimeTypes{"audio/mp4", "video/h264", "text/vtt"};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, supportedMimeTypes));
    GstPadTemplate *sinkPadTemplate{gst_element_class_get_pad_template(elementClass, "sink")};
    GstCaps *caps{gst_pad_template_get_caps(sinkPadTemplate)};

    // Verify expected caps for each mime type
    GstCaps *mp4Caps = gst_caps_from_string("audio/mpeg, mpegversion=4");
    GstCaps *h264Caps = gst_caps_from_string("video/x-h264");
    GstCaps *vttCaps = gst_caps_from_string("text/vtt");

    EXPECT_TRUE(gst_caps_is_subset(mp4Caps, caps));
    EXPECT_TRUE(gst_caps_is_subset(h264Caps, caps));
    EXPECT_TRUE(gst_caps_is_subset(vttCaps, caps));

    gst_caps_unref(mp4Caps);
    gst_caps_unref(h264Caps);
    gst_caps_unref(vttCaps);
    gst_caps_unref(caps);
    gst_object_unref(sink);
}

TEST_F(GStreamerMSEUtilsTests, shouldHandleUnsupportedMimeType)
{
    std::vector<std::string> supportedMimeTypes{"unsupported/mime"};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, supportedMimeTypes));
    gst_object_unref(sink);
}

TEST_F(GStreamerMSEUtilsTests, shouldReturnFalseForEmptyAudioCapabilities)
{
    const firebolt::rialto::common::AudioDecoderCapabilities audioDecoderCapabilities{"1.0", "1.1", {}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_FALSE(rialto_mse_sink_setup_supported_caps(elementClass, audioDecoderCapabilities));
    gst_object_unref(sink);
}

TEST_F(GStreamerMSEUtilsTests, shouldReturnFalseForEmptyVideoCapabilities)
{
    const firebolt::rialto::common::VideoDecoderCapabilities videoDecoderCapabilities{"1.0", "1.1", {}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_FALSE(rialto_mse_sink_setup_supported_caps(elementClass, videoDecoderCapabilities));
    gst_object_unref(sink);
}

TEST_F(GStreamerMSEUtilsTests, shouldGetSegmentAlignment)
{
    GstStructure *structAu = gst_structure_new_empty("test");
    gst_structure_set(structAu, "alignment", G_TYPE_STRING, "au", nullptr);
    EXPECT_EQ(get_segment_alignment(structAu), firebolt::rialto::SegmentAlignment::AU);
    gst_structure_free(structAu);

    GstStructure *structNal = gst_structure_new_empty("test");
    gst_structure_set(structNal, "alignment", G_TYPE_STRING, "nal", nullptr);
    EXPECT_EQ(get_segment_alignment(structNal), firebolt::rialto::SegmentAlignment::NAL);
    gst_structure_free(structNal);

    GstStructure *structUndef = gst_structure_new_empty("test");
    gst_structure_set(structUndef, "alignment", G_TYPE_STRING, "unknown", nullptr);
    EXPECT_EQ(get_segment_alignment(structUndef), firebolt::rialto::SegmentAlignment::UNDEFINED);
    gst_structure_free(structUndef);

    GstStructure *structNoAlignment = gst_structure_new_empty("test");
    EXPECT_EQ(get_segment_alignment(structNoAlignment), firebolt::rialto::SegmentAlignment::UNDEFINED);
    gst_structure_free(structNoAlignment);
}

TEST_F(GStreamerMSEUtilsTests, shouldGetDvProfile)
{
    GstStructure *structWithDv = gst_structure_new_empty("test");
    gst_structure_set(structWithDv, "dovi-stream", G_TYPE_BOOLEAN, true, "dv_profile", G_TYPE_UINT, 10u, nullptr);
    uint32_t dvProfile = 0;
    EXPECT_TRUE(get_dv_profile(structWithDv, dvProfile));
    EXPECT_EQ(dvProfile, 10u);
    gst_structure_free(structWithDv);

    GstStructure *structWithoutDv = gst_structure_new_empty("test");
    gst_structure_set(structWithoutDv, "dovi-stream", G_TYPE_BOOLEAN, false, nullptr);
    dvProfile = 0;
    EXPECT_FALSE(get_dv_profile(structWithoutDv, dvProfile));
    gst_structure_free(structWithoutDv);

    GstStructure *structNoDviStream = gst_structure_new_empty("test");
    dvProfile = 0;
    EXPECT_FALSE(get_dv_profile(structNoDviStream, dvProfile));
    gst_structure_free(structNoDviStream);
}

TEST_F(GStreamerMSEUtilsTests, shouldGetStreamFormat)
{
    GstStructure *structRaw = gst_structure_new_empty("test");
    gst_structure_set(structRaw, "stream-format", G_TYPE_STRING, "raw", nullptr);
    EXPECT_EQ(get_stream_format(structRaw), firebolt::rialto::StreamFormat::RAW);
    gst_structure_free(structRaw);

    GstStructure *structAvc = gst_structure_new_empty("test");
    gst_structure_set(structAvc, "stream-format", G_TYPE_STRING, "avc", nullptr);
    EXPECT_EQ(get_stream_format(structAvc), firebolt::rialto::StreamFormat::AVC);
    gst_structure_free(structAvc);

    GstStructure *structByteStream = gst_structure_new_empty("test");
    gst_structure_set(structByteStream, "stream-format", G_TYPE_STRING, "byte-stream", nullptr);
    EXPECT_EQ(get_stream_format(structByteStream), firebolt::rialto::StreamFormat::BYTE_STREAM);
    gst_structure_free(structByteStream);

    GstStructure *structHvc1 = gst_structure_new_empty("test");
    gst_structure_set(structHvc1, "stream-format", G_TYPE_STRING, "hvc1", nullptr);
    EXPECT_EQ(get_stream_format(structHvc1), firebolt::rialto::StreamFormat::HVC1);
    gst_structure_free(structHvc1);

    GstStructure *structHev1 = gst_structure_new_empty("test");
    gst_structure_set(structHev1, "stream-format", G_TYPE_STRING, "hev1", nullptr);
    EXPECT_EQ(get_stream_format(structHev1), firebolt::rialto::StreamFormat::HEV1);
    gst_structure_free(structHev1);

    GstStructure *structUnknown = gst_structure_new_empty("test");
    gst_structure_set(structUnknown, "stream-format", G_TYPE_STRING, "unknown", nullptr);
    EXPECT_EQ(get_stream_format(structUnknown), firebolt::rialto::StreamFormat::UNDEFINED);
    gst_structure_free(structUnknown);

    GstStructure *structNoFormat = gst_structure_new_empty("test");
    EXPECT_EQ(get_stream_format(structNoFormat), firebolt::rialto::StreamFormat::UNDEFINED);
    gst_structure_free(structNoFormat);
}

TEST_F(GStreamerMSEUtilsTests, shouldGetCodecDataFromBuffer)
{
    GstBuffer *buffer = gst_buffer_new_allocate(nullptr, 10, nullptr);
    GstMapInfo map;
    gst_buffer_map(buffer, &map, GST_MAP_WRITE);
    for (int i = 0; i < 10; i++)
    {
        map.data[i] = i;
    }
    gst_buffer_unmap(buffer, &map);

    GstStructure *structure = gst_structure_new_empty("test");
    GValue value = G_VALUE_INIT;
    g_value_init(&value, GST_TYPE_BUFFER);
    gst_value_set_buffer(&value, buffer);
    gst_structure_set_value(structure, "codec_data", &value);
    g_value_unset(&value);

    auto codecData = get_codec_data(structure);
    EXPECT_TRUE(codecData != nullptr);
    EXPECT_EQ(codecData->type, firebolt::rialto::CodecDataType::BUFFER);
    EXPECT_EQ(codecData->data.size(), 10u);
    for (int i = 0; i < 10; i++)
    {
        EXPECT_EQ(codecData->data[i], i);
    }

    gst_structure_free(structure);
    gst_buffer_unref(buffer);
}

TEST_F(GStreamerMSEUtilsTests, shouldGetCodecDataFromString)
{
    GstStructure *structure = gst_structure_new("test", "codec_data", G_TYPE_STRING, "test_codec_data", nullptr);

    auto codecData = get_codec_data(structure);
    EXPECT_TRUE(codecData != nullptr);
    EXPECT_EQ(codecData->type, firebolt::rialto::CodecDataType::STRING);
    EXPECT_EQ(codecData->data.size(), strlen("test_codec_data"));

    gst_structure_free(structure);
}

TEST_F(GStreamerMSEUtilsTests, shouldReturnNullptrWhenNoCodecData)
{
    GstStructure *structure = gst_structure_new_empty("test");
    auto codecData = get_codec_data(structure);
    EXPECT_TRUE(codecData == nullptr);
    gst_structure_free(structure);
}

// Additional granular tests for individual audio codecs
TEST_F(GStreamerMSEUtilsTests, shouldRegisterPcmAudio)
{
    firebolt::rialto::common::AudioDecoderCapability capability{};
    capability.pcm = firebolt::rialto::common::PcmCapability{};
    const firebolt::rialto::common::AudioDecoderCapabilities audioDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, audioDecoderCapabilities));
    gst_object_unref(sink);
}

TEST_F(GStreamerMSEUtilsTests, shouldRegisterAacAudio)
{
    firebolt::rialto::common::AudioDecoderCapability capability{};
    capability.aac = firebolt::rialto::common::AacCapability{};
    const firebolt::rialto::common::AudioDecoderCapabilities audioDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, audioDecoderCapabilities));
    gst_object_unref(sink);
}

TEST_F(GStreamerMSEUtilsTests, shouldRegisterMpegAudio)
{
    firebolt::rialto::common::AudioDecoderCapability capability{};
    capability.mpegAudio = firebolt::rialto::common::MpegAudioCapability{};
    const firebolt::rialto::common::AudioDecoderCapabilities audioDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, audioDecoderCapabilities));
    gst_object_unref(sink);
}

TEST_F(GStreamerMSEUtilsTests, shouldRegisterMp3Audio)
{
    firebolt::rialto::common::AudioDecoderCapability capability{};
    capability.mp3 = firebolt::rialto::common::Mp3Capability{};
    const firebolt::rialto::common::AudioDecoderCapabilities audioDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, audioDecoderCapabilities));
    gst_object_unref(sink);
}

TEST_F(GStreamerMSEUtilsTests, shouldRegisterAlacAudio)
{
    firebolt::rialto::common::AudioDecoderCapability capability{};
    capability.alac = firebolt::rialto::common::AlacCapability{};
    const firebolt::rialto::common::AudioDecoderCapabilities audioDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, audioDecoderCapabilities));
    gst_object_unref(sink);
}

TEST_F(GStreamerMSEUtilsTests, shouldRegisterSbcAudio)
{
    firebolt::rialto::common::AudioDecoderCapability capability{};
    capability.sbc = firebolt::rialto::common::SbcCapability{};
    const firebolt::rialto::common::AudioDecoderCapabilities audioDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, audioDecoderCapabilities));
    gst_object_unref(sink);
}

TEST_F(GStreamerMSEUtilsTests, shouldRegisterFlacAudio)
{
    firebolt::rialto::common::AudioDecoderCapability capability{};
    capability.flac = firebolt::rialto::common::FlacCapability{};
    const firebolt::rialto::common::AudioDecoderCapabilities audioDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, audioDecoderCapabilities));
    gst_object_unref(sink);
}

TEST_F(GStreamerMSEUtilsTests, shouldRegisterVorbisAudio)
{
    firebolt::rialto::common::AudioDecoderCapability capability{};
    capability.vorbis = firebolt::rialto::common::VorbisCapability{};
    const firebolt::rialto::common::AudioDecoderCapabilities audioDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, audioDecoderCapabilities));
    gst_object_unref(sink);
}

TEST_F(GStreamerMSEUtilsTests, shouldRegisterOpusAudio)
{
    firebolt::rialto::common::AudioDecoderCapability capability{};
    capability.opus = firebolt::rialto::common::OpusCapability{};
    const firebolt::rialto::common::AudioDecoderCapabilities audioDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, audioDecoderCapabilities));
    gst_object_unref(sink);
}

TEST_F(GStreamerMSEUtilsTests, shouldRegisterRealAudioAudio)
{
    firebolt::rialto::common::AudioDecoderCapability capability{};
    capability.realAudio = firebolt::rialto::common::RealAudioCapability{};
    const firebolt::rialto::common::AudioDecoderCapabilities audioDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, audioDecoderCapabilities));
    gst_object_unref(sink);
}

TEST_F(GStreamerMSEUtilsTests, shouldRegisterUsacAudio)
{
    firebolt::rialto::common::AudioDecoderCapability capability{};
    capability.usac = firebolt::rialto::common::UsacCapability{};
    const firebolt::rialto::common::AudioDecoderCapabilities audioDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, audioDecoderCapabilities));
    gst_object_unref(sink);
}

TEST_F(GStreamerMSEUtilsTests, shouldRegisterDtsAudio)
{
    firebolt::rialto::common::AudioDecoderCapability capability{};
    capability.dts = firebolt::rialto::common::DtsCapability{};
    const firebolt::rialto::common::AudioDecoderCapabilities audioDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, audioDecoderCapabilities));
    gst_object_unref(sink);
}

TEST_F(GStreamerMSEUtilsTests, shouldRegisterAvsAudio)
{
    firebolt::rialto::common::AudioDecoderCapability capability{};
    capability.avs = firebolt::rialto::common::AvsCapability{};
    const firebolt::rialto::common::AudioDecoderCapabilities audioDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, audioDecoderCapabilities));
    gst_object_unref(sink);
}

TEST_F(GStreamerMSEUtilsTests, shouldRegisterDolbyAc4Audio)
{
    firebolt::rialto::common::AudioDecoderCapability capability{};
    capability.dolbyAc4 = firebolt::rialto::common::DolbyAc4Capability{};
    const firebolt::rialto::common::AudioDecoderCapabilities audioDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, audioDecoderCapabilities));
    gst_object_unref(sink);
}

TEST_F(GStreamerMSEUtilsTests, shouldRegisterDolbyTruehdAudio)
{
    firebolt::rialto::common::AudioDecoderCapability capability{};
    capability.dolbyTruehd = firebolt::rialto::common::DolbyTruehdCapability{};
    const firebolt::rialto::common::AudioDecoderCapabilities audioDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, audioDecoderCapabilities));
    gst_object_unref(sink);
}

// Additional granular tests for individual video codecs
TEST_F(GStreamerMSEUtilsTests, shouldRegisterMpeg2Video)
{
    const firebolt::rialto::common::VideoCodecCapabilities codecCapabilities{firebolt::rialto::common::Mpeg2CodecCapability{},
                                                                             std::nullopt, std::nullopt, std::nullopt,
                                                                             std::nullopt};
    const firebolt::rialto::common::VideoDecoderCapability capability{codecCapabilities};
    const firebolt::rialto::common::VideoDecoderCapabilities videoDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, videoDecoderCapabilities));
    gst_object_unref(sink);
}

TEST_F(GStreamerMSEUtilsTests, shouldRegisterH265Video)
{
    const firebolt::rialto::common::VideoCodecCapabilities codecCapabilities{std::nullopt, std::nullopt,
                                                                             firebolt::rialto::common::H265CodecCapability{},
                                                                             std::nullopt, std::nullopt};
    const firebolt::rialto::common::VideoDecoderCapability capability{codecCapabilities};
    const firebolt::rialto::common::VideoDecoderCapabilities videoDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, videoDecoderCapabilities));
    gst_object_unref(sink);
}

TEST_F(GStreamerMSEUtilsTests, shouldRegisterVp9Video)
{
    const firebolt::rialto::common::VideoCodecCapabilities codecCapabilities{std::nullopt, std::nullopt, std::nullopt,
                                                                             firebolt::rialto::common::Vp9CodecCapability{},
                                                                             std::nullopt};
    const firebolt::rialto::common::VideoDecoderCapability capability{codecCapabilities};
    const firebolt::rialto::common::VideoDecoderCapabilities videoDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, videoDecoderCapabilities));
    gst_object_unref(sink);
}

TEST_F(GStreamerMSEUtilsTests, shouldRegisterAv1Video)
{
    const firebolt::rialto::common::VideoCodecCapabilities codecCapabilities{std::nullopt, std::nullopt, std::nullopt,
                                                                             std::nullopt,
                                                                             firebolt::rialto::common::Av1CodecCapability{}};
    const firebolt::rialto::common::VideoDecoderCapability capability{codecCapabilities};
    const firebolt::rialto::common::VideoDecoderCapabilities videoDecoderCapabilities{"1.0", "1.1", {capability}};

    GstElement *sink = gst_element_factory_make("fakesink", "test_sink");
    GstElementClass *elementClass{GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink))};
    EXPECT_TRUE(rialto_mse_sink_setup_supported_caps(elementClass, videoDecoderCapabilities));
    gst_object_unref(sink);
}

// Tests to cover static map initializations and error paths
TEST_F(GStreamerMSEUtilsTests, shouldConvertAllAudioFormats)
{
    // Test all format conversions to trigger kStringToFormat map initialization
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

TEST_F(GStreamerMSEUtilsTests, shouldConvertAllStreamFormats)
{
    // Test all stream format conversions to trigger stringToStreamFormatMap initialization
    GstStructure *structRaw = gst_structure_new_empty("test");
    gst_structure_set(structRaw, "stream-format", G_TYPE_STRING, "raw", nullptr);
    EXPECT_EQ(get_stream_format(structRaw), firebolt::rialto::StreamFormat::RAW);
    gst_structure_free(structRaw);

    GstStructure *structAvc = gst_structure_new_empty("test");
    gst_structure_set(structAvc, "stream-format", G_TYPE_STRING, "avc", nullptr);
    EXPECT_EQ(get_stream_format(structAvc), firebolt::rialto::StreamFormat::AVC);
    gst_structure_free(structAvc);

    GstStructure *structByteStream = gst_structure_new_empty("test");
    gst_structure_set(structByteStream, "stream-format", G_TYPE_STRING, "byte-stream", nullptr);
    EXPECT_EQ(get_stream_format(structByteStream), firebolt::rialto::StreamFormat::BYTE_STREAM);
    gst_structure_free(structByteStream);

    GstStructure *structHvc1 = gst_structure_new_empty("test");
    gst_structure_set(structHvc1, "stream-format", G_TYPE_STRING, "hvc1", nullptr);
    EXPECT_EQ(get_stream_format(structHvc1), firebolt::rialto::StreamFormat::HVC1);
    gst_structure_free(structHvc1);

    GstStructure *structHev1 = gst_structure_new_empty("test");
    gst_structure_set(structHev1, "stream-format", G_TYPE_STRING, "hev1", nullptr);
    EXPECT_EQ(get_stream_format(structHev1), firebolt::rialto::StreamFormat::HEV1);
    gst_structure_free(structHev1);
}

TEST_F(GStreamerMSEUtilsTests, shouldHandleCodecDataReadError)
{
    // Test handling of empty buffer codec_data
    GstStructure *structure = gst_structure_new_empty("test");

    // Create an empty buffer and set as codec_data
    GstBuffer *buffer = gst_buffer_new();
    gst_structure_set(structure, "codec_data", GST_TYPE_BUFFER, buffer, nullptr);

    // Empty buffer should still return valid CodecData with empty data vector
    auto codecData = get_codec_data(structure);
    EXPECT_TRUE(codecData != nullptr);
    EXPECT_TRUE(codecData->data.empty());
    EXPECT_EQ(codecData->type, firebolt::rialto::CodecDataType::BUFFER);
    gst_structure_free(structure);
}

TEST_F(GStreamerMSEUtilsTests, shouldHandleUnknownStreamFormat)
{
    // Test unknown stream format handling - should not crash
    GstStructure *structure = gst_structure_new_empty("test");
    gst_structure_set(structure, "stream-format", G_TYPE_STRING, "unknown-format", nullptr);
    // Function should handle gracefully without throwing
    auto result = get_stream_format(structure);
    (void)result; // Use the result to avoid unused variable warnings
    gst_structure_free(structure);
}

TEST_F(GStreamerMSEUtilsTests, shouldHandleInvalidFormatString)
{
    // Test invalid format string
    auto result = rialto_mse_sink_convert_format("INVALID_FORMAT");
    EXPECT_EQ(result, std::nullopt);
}
