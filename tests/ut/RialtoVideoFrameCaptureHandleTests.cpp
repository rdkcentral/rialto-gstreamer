/*
 * Copyright (C) 2026 Sky UK
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

#include "RialtoVideoFrameCaptureHandle.h"
#include "VideoFrameCaptureMock.h"

#include <gtest/gtest.h>

using firebolt::rialto::IVideoFrameCapture;
using firebolt::rialto::VideoFrameCaptureMock;
using testing::NiceMock;

TEST(RialtoVideoFrameCaptureHandleTests, ShouldHoldOneReferencePerCopy)
{
    std::shared_ptr<IVideoFrameCapture> capture{std::make_shared<NiceMock<VideoFrameCaptureMock>>()};
    ASSERT_EQ(capture.use_count(), 1);

    RialtoVideoFrameCaptureHandle *handle = rialto_video_frame_capture_handle_new(capture);
    EXPECT_EQ(capture.use_count(), 2);
    EXPECT_EQ(rialto_video_frame_capture_handle_get(handle), capture);

    GValue value = G_VALUE_INIT;
    g_value_init(&value, RIALTO_TYPE_VIDEO_FRAME_CAPTURE_HANDLE);
    g_value_set_boxed(&value, handle); // copies the handle
    EXPECT_EQ(capture.use_count(), 3);

    GValue copy = G_VALUE_INIT;
    g_value_init(&copy, RIALTO_TYPE_VIDEO_FRAME_CAPTURE_HANDLE);
    g_value_copy(&value, &copy);
    EXPECT_EQ(capture.use_count(), 4);
    EXPECT_EQ(rialto_video_frame_capture_handle_get(
                  static_cast<const RialtoVideoFrameCaptureHandle *>(g_value_get_boxed(&copy))),
              capture);

    g_value_unset(&copy);
    EXPECT_EQ(capture.use_count(), 3);
    g_value_unset(&value);
    EXPECT_EQ(capture.use_count(), 2);
    g_boxed_free(RIALTO_TYPE_VIDEO_FRAME_CAPTURE_HANDLE, handle);
    EXPECT_EQ(capture.use_count(), 1);
}

TEST(RialtoVideoFrameCaptureHandleTests, ShouldRegisterOneBoxedType)
{
    EXPECT_TRUE(G_TYPE_IS_BOXED(RIALTO_TYPE_VIDEO_FRAME_CAPTURE_HANDLE));
    EXPECT_EQ(RIALTO_TYPE_VIDEO_FRAME_CAPTURE_HANDLE, rialto_video_frame_capture_handle_get_type());
    EXPECT_STREQ(g_type_name(RIALTO_TYPE_VIDEO_FRAME_CAPTURE_HANDLE), "RialtoVideoFrameCaptureHandle");
}

TEST(RialtoVideoFrameCaptureHandleTests, ShouldGiveNoCaptureForNullHandle)
{
    EXPECT_EQ(rialto_video_frame_capture_handle_get(nullptr), nullptr);
}
