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

#include "GStreamerEMEUtils.h"
#include "RialtoGStreamerEMEProtectionMetadata.h"
#include <gst/base/gstbytewriter.h>
#include <gst/gst.h>
#include <gtest/gtest.h>

// Most of functionality tested in BufferParser Tests. Mostly corner cases here.
class GStreamerEmeUtilsTests : public testing::Test
{
public:
    BufferProtectionMetadata m_metadata;
};

TEST_F(GStreamerEmeUtilsTests, ShouldNotProcessNullBuffer)
{
    ProcessProtectionMetadata(nullptr, m_metadata);
}

TEST_F(GStreamerEmeUtilsTests, ShouldProcessSubsamples)
{
    constexpr uint16_t kClearBytes = 7;
    constexpr uint32_t kEncryptedBytes{12};
    constexpr unsigned int kSubsampleCount{1};
    constexpr unsigned int kSubsampleSize = kSubsampleCount * (sizeof(uint32_t) + sizeof(uint16_t));
    std::vector<uint8_t> dataVec(kSubsampleSize, 0);
    GstByteWriter *byteWriter = gst_byte_writer_new_with_data(dataVec.data(), kSubsampleSize, FALSE);
    gst_byte_writer_put_uint16_be(byteWriter, kClearBytes);
    gst_byte_writer_put_uint32_be(byteWriter, kEncryptedBytes);
    GstBuffer *subsamplesBuffer{gst_buffer_new_allocate(nullptr, dataVec.size(), nullptr)};
    gst_buffer_fill(subsamplesBuffer, 0, dataVec.data(), dataVec.size());
    GstBuffer *buffer = gst_buffer_new();
    GstStructure *info = gst_structure_new("application/x-cenc", "encrypted", G_TYPE_BOOLEAN, TRUE, "subsample_count",
                                           G_TYPE_UINT, kSubsampleCount, "subsamples", GST_TYPE_BUFFER,
                                           subsamplesBuffer, NULL);
    rialto_mse_add_protection_metadata(buffer, info);

    ProcessProtectionMetadata(buffer, m_metadata);

    gst_byte_writer_free(byteWriter);
    gst_buffer_unref(subsamplesBuffer);
    gst_buffer_unref(buffer);

    EXPECT_EQ(m_metadata.subsamples.size(), kSubsampleCount);
    for (const auto &subsample : m_metadata.subsamples)
    {
        EXPECT_EQ(subsample.first, kClearBytes);
        EXPECT_EQ(subsample.second, kEncryptedBytes);
    }
}

TEST_F(GStreamerEmeUtilsTests, ShouldProcessCbcsEncryptionScheme)
{
    const std::string kEncryptionScheme{"cbcs"};
    GstBuffer *buffer = gst_buffer_new();
    GstStructure *info = gst_structure_new("application/x-cenc", "encrypted", G_TYPE_BOOLEAN, TRUE, "cipher-mode",
                                           G_TYPE_STRING, kEncryptionScheme.c_str(), NULL);
    rialto_mse_add_protection_metadata(buffer, info);

    ProcessProtectionMetadata(buffer, m_metadata);

    gst_buffer_unref(buffer);

    EXPECT_EQ(m_metadata.cipherMode, firebolt::rialto::CipherMode::CBCS);
}

TEST_F(GStreamerEmeUtilsTests, ShouldProcessCencEncryptionScheme)
{
    const std::string kEncryptionScheme{"cenc"};
    GstBuffer *buffer = gst_buffer_new();
    GstStructure *info = gst_structure_new("application/x-cenc", "encrypted", G_TYPE_BOOLEAN, TRUE, "cipher-mode",
                                           G_TYPE_STRING, kEncryptionScheme.c_str(), NULL);
    rialto_mse_add_protection_metadata(buffer, info);

    ProcessProtectionMetadata(buffer, m_metadata);

    gst_buffer_unref(buffer);

    EXPECT_EQ(m_metadata.cipherMode, firebolt::rialto::CipherMode::CENC);
}

TEST_F(GStreamerEmeUtilsTests, ShouldProcessCbc1EncryptionScheme)
{
    const std::string kEncryptionScheme{"cbc1"};
    GstBuffer *buffer = gst_buffer_new();
    GstStructure *info = gst_structure_new("application/x-cenc", "encrypted", G_TYPE_BOOLEAN, TRUE, "cipher-mode",
                                           G_TYPE_STRING, kEncryptionScheme.c_str(), NULL);
    rialto_mse_add_protection_metadata(buffer, info);

    ProcessProtectionMetadata(buffer, m_metadata);

    gst_buffer_unref(buffer);

    EXPECT_EQ(m_metadata.cipherMode, firebolt::rialto::CipherMode::CBC1);
}

TEST_F(GStreamerEmeUtilsTests, ShouldProcessCensEncryptionScheme)
{
    const std::string kEncryptionScheme{"cens"};
    GstBuffer *buffer = gst_buffer_new();
    GstStructure *info = gst_structure_new("application/x-cenc", "encrypted", G_TYPE_BOOLEAN, TRUE, "cipher-mode",
                                           G_TYPE_STRING, kEncryptionScheme.c_str(), NULL);
    rialto_mse_add_protection_metadata(buffer, info);

    ProcessProtectionMetadata(buffer, m_metadata);

    gst_buffer_unref(buffer);

    EXPECT_EQ(m_metadata.cipherMode, firebolt::rialto::CipherMode::CENS);
}

TEST_F(GStreamerEmeUtilsTests, ShouldProcessUnknownEncryptionScheme)
{
    const std::string kEncryptionScheme{"surprise"};
    GstBuffer *buffer = gst_buffer_new();
    GstStructure *info = gst_structure_new("application/x-cenc", "encrypted", G_TYPE_BOOLEAN, TRUE, "cipher-mode",
                                           G_TYPE_STRING, kEncryptionScheme.c_str(), NULL);
    rialto_mse_add_protection_metadata(buffer, info);

    ProcessProtectionMetadata(buffer, m_metadata);

    gst_buffer_unref(buffer);

    EXPECT_EQ(m_metadata.cipherMode, firebolt::rialto::CipherMode::UNKNOWN);
}

TEST_F(GStreamerEmeUtilsTests, ShouldFailToReadEncryptionPatternWhenSkipByteBlockIsNotFound)
{
    constexpr unsigned int kCryptByteBlock{7};
    GstBuffer *buffer = gst_buffer_new();
    GstStructure *info = gst_structure_new("application/x-cenc", "encrypted", G_TYPE_BOOLEAN, TRUE, "crypt_byte_block",
                                           G_TYPE_UINT, kCryptByteBlock, NULL);
    rialto_mse_add_protection_metadata(buffer, info);

    ProcessProtectionMetadata(buffer, m_metadata);

    gst_buffer_unref(buffer);

    EXPECT_FALSE(m_metadata.encryptionPatternSet);
}
