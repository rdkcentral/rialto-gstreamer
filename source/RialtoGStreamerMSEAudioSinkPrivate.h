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

#include "Constants.h"
#include <atomic>
#include <gst/gst.h>
#include <mutex>

G_BEGIN_DECLS

struct AudioFadeConfig
{
    double volume;
    uint32_t duration;
    firebolt::rialto::EaseType easeType;
};

struct _RialtoMSEAudioSinkPrivate
{
    std::atomic<double> targetVolume = kDefaultVolume;
    std::atomic_bool mute = kDefaultMute;
    std::atomic_bool isVolumeQueued = false;
    std::atomic_bool isMuteQueued = false;
    std::atomic_bool lowLatency = kDefaultLowLatency;
    std::atomic_bool isLowLatencyQueued = false;
    std::atomic_bool sync = kDefaultSync;
    std::atomic_bool isSyncQueued = false;
    std::atomic_bool syncOff = kDefaultSyncOff;
    std::atomic_bool isSyncOffQueued = false;
    std::atomic<int32_t> streamSyncMode = kDefaultStreamSyncMode;
    std::atomic_bool isStreamSyncModeQueued = false;
    AudioFadeConfig audioFadeConfig;
    std::mutex audioFadeConfigMutex;
    std::atomic_bool isAudioFadeQueued = false;
};

G_END_DECLS
