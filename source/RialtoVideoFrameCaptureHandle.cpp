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

namespace
{
gpointer copyHandle(gpointer handle)
{
    return new RialtoVideoFrameCaptureHandle{*static_cast<RialtoVideoFrameCaptureHandle *>(handle)};
}

void freeHandle(gpointer handle)
{
    delete static_cast<RialtoVideoFrameCaptureHandle *>(handle);
}
} // namespace

GType rialto_video_frame_capture_handle_get_type()
{
    static GType type{0};
    if (g_once_init_enter(&type))
    {
        GType registered = g_boxed_type_register_static("RialtoVideoFrameCaptureHandle", copyHandle, freeHandle);
        g_once_init_leave(&type, registered);
    }
    return type;
}

RialtoVideoFrameCaptureHandle *
rialto_video_frame_capture_handle_new(const std::shared_ptr<firebolt::rialto::IVideoFrameCapture> &capture)
{
    return new RialtoVideoFrameCaptureHandle{capture};
}

std::shared_ptr<firebolt::rialto::IVideoFrameCapture>
rialto_video_frame_capture_handle_get(const RialtoVideoFrameCaptureHandle *handle)
{
    return handle ? handle->capture : nullptr;
}
