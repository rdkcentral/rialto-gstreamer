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

#include <functional>

struct RialtoGStreamerMSEBaseSinkCallbacks
{
    std::function<void(firebolt::rialto::PlaybackError)> errorCallback;
    std::function<void(void)> loadCompletedCallback;
    std::function<void(void)> seekCompletedCallback;
    std::function<void(void)> flushCompletedCallback;
    std::function<void(void)> eosCallback;
    std::function<void(firebolt::rialto::PlaybackState)> stateChangedCallback;
    std::function<void(int percent)> bufferingCallback;
    std::function<void(uint64_t processed, uint64_t dropped)> qosCallback;
};
