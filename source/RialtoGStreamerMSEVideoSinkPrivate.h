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

#pragma once

#include <gst/gst.h>
#include <mutex>
#include <string>

G_BEGIN_DECLS

struct _RialtoMSEVideoSinkPrivate
{
    uint32_t maxWidth = 0;
    uint32_t maxHeight = 0;
    bool stepOnPrerollEnabled = false;

    std::mutex propertyMutex;
    // START of variables locked by propertyMutex
    // rectangle properties
    std::string videoRectangle = "0,0,1920,1080";
    bool rectangleSettingQueued = false;
    // immediate output properties
    bool immediateOutput{false};
    bool immediateOutputQueued{false};
    bool syncmodeStreaming{false};
    bool syncmodeStreamingQueued{false};
    bool showVideoWindow{true};
    bool showVideoWindowQueued{false};
    // END of variables locked by propertyMutex
};

G_END_DECLS
