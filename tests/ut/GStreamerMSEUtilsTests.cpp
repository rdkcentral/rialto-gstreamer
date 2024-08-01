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
#include <gtest/gtest.h>

TEST(GStreamerMSEUtilsTests, shouldConvertLayout)
{
    EXPECT_EQ(rialto_mse_sink_convert_layout(""), std::nullopt);
    EXPECT_EQ(rialto_mse_sink_convert_layout("interleaved"), firebolt::rialto::Layout::INTERLEAVED);
    EXPECT_EQ(rialto_mse_sink_convert_layout("non-interleaved"), firebolt::rialto::Layout::NON_INTERLEAVED);
}

TEST(GStreamerMSEUtilsTests, shouldConvertFormat)
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
