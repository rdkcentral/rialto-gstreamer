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

#pragma once

/**
 * @file RialtoVideoFrameCaptureHandle.h
 *
 * The value type of the Rialto MSE video sink's "video-frame-capture" property. A handle holds a shared reference
 * to a Rialto video frame capture; copying a GValue that holds one copies the reference, and freeing it drops it.
 */

#include <IVideoFrameCapture.h>
#include <glib-object.h>

#include <memory>

struct RialtoVideoFrameCaptureHandle
{
    std::shared_ptr<firebolt::rialto::IVideoFrameCapture> capture;
};

/** The boxed GType of RialtoVideoFrameCaptureHandle. */
GType rialto_video_frame_capture_handle_get_type();
#define RIALTO_TYPE_VIDEO_FRAME_CAPTURE_HANDLE (rialto_video_frame_capture_handle_get_type())

/**
 * @brief Creates a handle holding a reference to \a capture, for g_object_set().
 *
 * The caller frees it with g_boxed_free() once the property is set; the sink keeps its own copy.
 */
RialtoVideoFrameCaptureHandle *
rialto_video_frame_capture_handle_new(const std::shared_ptr<firebolt::rialto::IVideoFrameCapture> &capture);

/** The capture a handle refers to; null for a null handle. */
std::shared_ptr<firebolt::rialto::IVideoFrameCapture>
rialto_video_frame_capture_handle_get(const RialtoVideoFrameCaptureHandle *handle);
