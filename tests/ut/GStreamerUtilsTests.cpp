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

#include "GStreamerUtils.h"
#include <gst/gst.h>
#include <gtest/gtest.h>
#include <vector>

namespace
{
const std::vector<uint8_t> kData{1, 2, 3, 4};
} // namespace

TEST(GstMappedBufferTests, ShouldMapBuffer)
{
    GstBuffer *buffer{gst_buffer_new_allocate(nullptr, kData.size(), nullptr)};
    gst_buffer_fill(buffer, 0, kData.data(), kData.size());
    // Scope introduced here to unmap buffer before unreffing it.
    {
        GstMappedBuffer mappedBuf{buffer, GST_MAP_READ};
        EXPECT_TRUE(mappedBuf);
        EXPECT_EQ(mappedBuf.size(), kData.size());
        EXPECT_TRUE(mappedBuf.data());
    }
    gst_buffer_unref(buffer);
}

TEST(GstMappedBufferTests, ShouldFailToMapBuffer)
{
    GstBuffer buffer{};
    GstMappedBuffer mappedBuf{&buffer, GST_MAP_READ};
    EXPECT_FALSE(mappedBuf);
    EXPECT_EQ(mappedBuf.size(), 0);
    EXPECT_FALSE(mappedBuf.data());
}
