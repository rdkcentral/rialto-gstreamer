/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 Sky UK
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "BufferParser.h"
#include "RialtoGStreamerEMEProtectionMetadata.h"
#include <gst/gst.h>
#include <gtest/gtest.h>

namespace
{
constexpr int64_t kTimestamp{1234};
constexpr int64_t kDuration{4321};
constexpr int kRate{12};
constexpr int kChannels{9};
constexpr int kStreamId{1};
constexpr unsigned int kCryptByteBlock{7};
constexpr unsigned int kSkipByteBlock{3};
constexpr int kWidth{1024};
constexpr int kHeight{768};
constexpr firebolt::rialto::Fraction kFrameRate{25, 1};
const std::string kCodecDataStr{"CodecData"};
const std::vector<uint8_t> kCodecDataVec{kCodecDataStr.begin(), kCodecDataStr.end()};
} // namespace

class BufferParserTests : public testing::Test
{
public:
    BufferParserTests()
    {
        gst_init(nullptr, nullptr);
        buildBuffer();
        buildMapInfo();
    }

    ~BufferParserTests() override
    {
        gst_sample_unref(m_sample);
        gst_buffer_unref(m_buffer);
    }

    void buildSample(GstCaps *caps) { m_sample = gst_sample_new(m_buffer, caps, nullptr, nullptr); }

    std::vector<uint8_t> m_bufferData{1, 2, 3, 4};
    GstBuffer *m_buffer;
    GstSample *m_sample;
    GstMapInfo m_mapInfo;
    GstBuffer *m_bufferCodecData;

private:
    void buildBuffer()
    {
        m_buffer = gst_buffer_new_allocate(nullptr, m_bufferData.size(), nullptr);
        GST_BUFFER_PTS(m_buffer) = kTimestamp;
        GST_BUFFER_DURATION(m_buffer) = kDuration;
        GstStructure *info = gst_structure_new("application/x-cenc", "encrypted", G_TYPE_BOOLEAN, TRUE,
                                               "crypt_byte_block", G_TYPE_UINT, kCryptByteBlock, "skip_byte_block",
                                               G_TYPE_UINT, kSkipByteBlock, NULL);
        rialto_mse_add_protection_metadata(m_buffer, info);
    }

    void buildMapInfo()
    {
        m_mapInfo.data = m_bufferData.data();
        m_mapInfo.size = m_bufferData.size();
    }
};

TEST_F(BufferParserTests, ShouldParseAudioBufferCenc)
{
    AudioBufferParser parser;
    GstCaps *caps = gst_caps_new_simple("application/x-cenc", "rate", G_TYPE_INT, kRate, "channels", G_TYPE_INT,
                                        kChannels, nullptr);
    buildSample(caps);
    auto segment = parser.parseBuffer(m_sample, m_buffer, m_mapInfo, kStreamId);
    ASSERT_TRUE(segment);
    EXPECT_EQ(segment->getId(), kStreamId);
    EXPECT_EQ(segment->getType(), firebolt::rialto::MediaSourceType::AUDIO);
    EXPECT_EQ(segment->getTimeStamp(), kTimestamp);
    EXPECT_EQ(segment->getDuration(), kDuration);
    EXPECT_TRUE(segment->isEncrypted());
    firebolt::rialto::IMediaPipeline::MediaSegmentAudio *audioSegment{
        dynamic_cast<firebolt::rialto::IMediaPipeline::MediaSegmentAudio *>(segment.get())};
    ASSERT_TRUE(audioSegment);
    EXPECT_EQ(audioSegment->getSampleRate(), kRate);
    EXPECT_EQ(audioSegment->getNumberOfChannels(), kChannels);
    gst_caps_unref(caps);
}

TEST_F(BufferParserTests, ShouldParseAudioBufferWebm)
{
    AudioBufferParser parser;
    GstCaps *caps = gst_caps_new_simple("application/x-webm-enc", "rate", G_TYPE_INT, kRate, "channels", G_TYPE_INT,
                                        kChannels, nullptr);
    buildSample(caps);
    auto segment = parser.parseBuffer(m_sample, m_buffer, m_mapInfo, kStreamId);
    ASSERT_TRUE(segment);
    EXPECT_EQ(segment->getId(), kStreamId);
    EXPECT_EQ(segment->getType(), firebolt::rialto::MediaSourceType::AUDIO);
    EXPECT_EQ(segment->getTimeStamp(), kTimestamp);
    EXPECT_EQ(segment->getDuration(), kDuration);
    EXPECT_TRUE(segment->isEncrypted());
    firebolt::rialto::IMediaPipeline::MediaSegmentAudio *audioSegment{
        dynamic_cast<firebolt::rialto::IMediaPipeline::MediaSegmentAudio *>(segment.get())};
    ASSERT_TRUE(audioSegment);
    EXPECT_EQ(audioSegment->getSampleRate(), kRate);
    EXPECT_EQ(audioSegment->getNumberOfChannels(), kChannels);
    gst_caps_unref(caps);
}

TEST_F(BufferParserTests, ShouldParseAudioBufferBufferCodecData)
{
    AudioBufferParser parser;
    GstBuffer *codecDataBuf{gst_buffer_new_allocate(nullptr, kCodecDataVec.size(), nullptr)};
    gst_buffer_fill(codecDataBuf, 0, kCodecDataVec.data(), kCodecDataVec.size());
    GstCaps *caps = gst_caps_new_simple("application/x-webm-enc", "rate", G_TYPE_INT, kRate, "channels", G_TYPE_INT,
                                        kChannels, "codec_data", GST_TYPE_BUFFER, codecDataBuf, nullptr);
    buildSample(caps);
    auto segment = parser.parseBuffer(m_sample, m_buffer, m_mapInfo, kStreamId);
    ASSERT_TRUE(segment);
    ASSERT_TRUE(segment->getCodecData());
    EXPECT_EQ(segment->getCodecData()->type, firebolt::rialto::CodecDataType::BUFFER);
    EXPECT_EQ(segment->getCodecData()->data, kCodecDataVec);
    gst_caps_unref(caps);
    gst_buffer_unref(codecDataBuf);
}

TEST_F(BufferParserTests, ShouldParseAudioBufferInvalidBufferCodecData)
{
    AudioBufferParser parser;
    GstCaps *caps = gst_caps_new_simple("application/x-webm-enc", "rate", G_TYPE_INT, kRate, "channels", G_TYPE_INT,
                                        kChannels, "codec_data", GST_TYPE_BUFFER, kCodecDataStr.c_str(), nullptr);
    buildSample(caps);
    auto segment = parser.parseBuffer(m_sample, m_buffer, m_mapInfo, kStreamId);
    ASSERT_TRUE(segment);
    EXPECT_FALSE(segment->getCodecData());
    gst_caps_unref(caps);
}

TEST_F(BufferParserTests, ShouldParseAudioBufferStringCodecData)
{
    AudioBufferParser parser;
    GstCaps *caps = gst_caps_new_simple("application/x-webm-enc", "rate", G_TYPE_INT, kRate, "channels", G_TYPE_INT,
                                        kChannels, "codec_data", G_TYPE_STRING, kCodecDataStr.c_str(), nullptr);
    buildSample(caps);
    auto segment = parser.parseBuffer(m_sample, m_buffer, m_mapInfo, kStreamId);
    ASSERT_TRUE(segment);
    ASSERT_TRUE(segment->getCodecData());
    EXPECT_EQ(segment->getCodecData()->type, firebolt::rialto::CodecDataType::STRING);
    EXPECT_EQ(segment->getCodecData()->data, kCodecDataVec);
    gst_caps_unref(caps);
}

TEST_F(BufferParserTests, ShouldParseVideoBuffer)
{
    VideoBufferParser parser;
    GstCaps *caps = gst_caps_new_simple("application/x-cenc", "width", G_TYPE_INT, kWidth, "height", G_TYPE_INT,
                                        kHeight, "framerate", GST_TYPE_FRACTION, kFrameRate.numerator,
                                        kFrameRate.denominator, nullptr);
    buildSample(caps);
    auto segment = parser.parseBuffer(m_sample, m_buffer, m_mapInfo, kStreamId);
    ASSERT_TRUE(segment);
    EXPECT_EQ(segment->getId(), kStreamId);
    EXPECT_EQ(segment->getType(), firebolt::rialto::MediaSourceType::VIDEO);
    EXPECT_EQ(segment->getTimeStamp(), kTimestamp);
    EXPECT_EQ(segment->getDuration(), kDuration);
    EXPECT_TRUE(segment->isEncrypted());
    firebolt::rialto::IMediaPipeline::MediaSegmentVideo *videoSegment{
        dynamic_cast<firebolt::rialto::IMediaPipeline::MediaSegmentVideo *>(segment.get())};
    ASSERT_TRUE(videoSegment);
    EXPECT_EQ(videoSegment->getWidth(), kWidth);
    EXPECT_EQ(videoSegment->getHeight(), kHeight);
    EXPECT_EQ(videoSegment->getFrameRate().numerator, kFrameRate.numerator);
    EXPECT_EQ(videoSegment->getFrameRate().denominator, kFrameRate.denominator);
    gst_caps_unref(caps);
}
