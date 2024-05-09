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

#pragma once

#include <atomic>
#include <gst/gst.h>

G_BEGIN_DECLS

struct _RialtoMSEAudioSinkPrivate
{
    std::atomic<double> volume = 1.0;
    std::atomic_bool mute = false;
    std::atomic_bool isVolumeQueued = false;
    std::atomic_bool isMuteQueued = false;
};

G_END_DECLS
