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
