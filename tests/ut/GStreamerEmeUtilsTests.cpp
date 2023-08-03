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
