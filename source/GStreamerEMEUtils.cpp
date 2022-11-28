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

#include "GStreamerEMEUtils.h"
#include "GStreamerUtils.h"
#include <RialtoGStreamerEMEProtectionMetadata.h>
#include <cstdint>
#include <stdio.h>

void getEncryptedFromProtectionMetadata(GstRialtoProtectionMetadata *protectionMeta, BufferProtectionMetadata &metadata)
{
    gboolean encrypted = FALSE;
    gst_structure_get_boolean(protectionMeta->info, "encrypted", &encrypted);
    metadata.encrypted = encrypted;
}

void getMediaKeySessionIdFromProtectionMetadata(GstRialtoProtectionMetadata *protectionMeta,
                                                BufferProtectionMetadata &metadata)
{
    gint mediaKeySessionId = 0;
    gst_structure_get_int(protectionMeta->info, "mks_id", &mediaKeySessionId);
    metadata.mediaKeySessionId = mediaKeySessionId;
}

void getKIDFromProtectionMetadata(GstRialtoProtectionMetadata *protectionMeta, BufferProtectionMetadata &metadata)
{
    const GValue *value = gst_structure_get_value(protectionMeta->info, "kid");
    if (value)
    {
        GstBuffer *keyIDBuffer = gst_value_get_buffer(value);
        if (keyIDBuffer)
        {
            GstMappedBuffer mappedKeyID(keyIDBuffer, GST_MAP_READ);
            if (mappedKeyID)
            {
                metadata.kid = std::vector<uint8_t>(mappedKeyID.data(), mappedKeyID.data() + mappedKeyID.size());
            }
        }
    }
}

void getIVFromProtectionMetadata(GstRialtoProtectionMetadata *protectionMeta, BufferProtectionMetadata &metadata)
{
    unsigned ivSize = 0;
    gst_structure_get_uint(protectionMeta->info, "iv_size", &ivSize);
    const GValue *value = gst_structure_get_value(protectionMeta->info, "iv");
    if (value)
    {
        GstBuffer *ivBuffer = gst_value_get_buffer(value);
        if (ivBuffer)
        {
            GstMappedBuffer mappedIV(ivBuffer, GST_MAP_READ);
            if (mappedIV && (ivSize == mappedIV.size()))
            {
                metadata.iv = std::vector<uint8_t>(mappedIV.data(), mappedIV.data() + mappedIV.size());
            }
        }
    }
}

void getSubSamplesFromProtectionMetadata(GstRialtoProtectionMetadata *protectionMeta, BufferProtectionMetadata &metadata)
{
    unsigned int subSampleCount = 0;
    gst_structure_get_uint(protectionMeta->info, "subsample_count", &subSampleCount);

    if (subSampleCount)
    {
        const GValue *value = gst_structure_get_value(protectionMeta->info, "subsamples");
        if (value)
        {
            GstBuffer *subSamplesBuffer = gst_value_get_buffer(value);
            if (subSamplesBuffer)
            {
                GstMappedBuffer mappedSubSamples(subSamplesBuffer, GST_MAP_READ);
                if (mappedSubSamples &&
                    ((mappedSubSamples.size() / (sizeof(int16_t) + sizeof(int32_t))) == subSampleCount))
                {
                    std::vector<uint8_t> subSamples(mappedSubSamples.data(),
                                                    mappedSubSamples.data() + mappedSubSamples.size());
                    //'senc' atom
                    // unsigned   int(16)      subsample_count;
                    //{
                    //  unsigned   int(16)      BytesOfClearData;
                    //  unsigned   int(32)      BytesOfEncryptedData;
                    //}[subsample_count]
                    size_t subSampleOffset = 0;
                    for (unsigned int subSampleIdx = 0; subSampleIdx < subSampleCount; ++subSampleIdx)
                    {
                        uint16_t bytesOfClearData = (uint16_t)subSamples[subSampleOffset] << 8 |
                                                    (uint16_t)subSamples[subSampleOffset + 1];
                        uint32_t bytesOfEncryptedData = (uint32_t)subSamples[subSampleOffset + 2] << 24 |
                                                        (uint32_t)subSamples[subSampleOffset + 3] << 16 |
                                                        (uint32_t)subSamples[subSampleOffset + 4] << 8 |
                                                        (uint32_t)subSamples[subSampleOffset + 5];
                        metadata.subsamples.push_back(
                            std::make_pair((uint32_t)bytesOfClearData, (uint32_t)bytesOfEncryptedData));
                        subSampleOffset += sizeof(int16_t) + sizeof(int32_t);
                    }
                }
            }
        }
    }
}

void getInitWithLast15FromProtectionMetadata(GstRialtoProtectionMetadata *protectionMeta,
                                             BufferProtectionMetadata &metadata)
{
    guint initWithLast15 = 0;
    gst_structure_get_uint(protectionMeta->info, "init_with_last_15", &initWithLast15);
    metadata.initWithLast15 = initWithLast15;
}

void ProcessProtectionMetadata(GstBuffer *buffer, BufferProtectionMetadata &metadata)
{
    if (buffer == nullptr)
        return;

    GstRialtoProtectionMetadata *protectionMeta = reinterpret_cast<GstRialtoProtectionMetadata *>(
        gst_buffer_get_meta(buffer, GST_RIALTO_PROTECTION_METADATA_GET_TYPE));
    if (protectionMeta)
    {
        getEncryptedFromProtectionMetadata(protectionMeta, metadata);
        if (metadata.encrypted)
        {
            getMediaKeySessionIdFromProtectionMetadata(protectionMeta, metadata);
            getKIDFromProtectionMetadata(protectionMeta, metadata);
            getIVFromProtectionMetadata(protectionMeta, metadata);
            getSubSamplesFromProtectionMetadata(protectionMeta, metadata);
            getInitWithLast15FromProtectionMetadata(protectionMeta, metadata);
        }
        gst_buffer_remove_meta(buffer, reinterpret_cast<GstMeta *>(protectionMeta));
    }
}
