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

#ifndef FIREBOLT_RIALTO_CLIENT_WEB_AUDIO_CLIENT_BACKEND_MOCK_H_
#define FIREBOLT_RIALTO_CLIENT_WEB_AUDIO_CLIENT_BACKEND_MOCK_H_

#include "WebAudioClientBackendInterface.h"
#include <gmock/gmock.h>

namespace firebolt::rialto::client
{
class WebAudioClientBackendMock : public WebAudioClientBackendInterface
{
public:
    MOCK_METHOD(bool, createWebAudioBackend,
                (std::weak_ptr<IWebAudioPlayerClient> client, const std::string &audioMimeType, const uint32_t priority,
                 const WebAudioConfig *config),
                (override));
    MOCK_METHOD(void, destroyWebAudioBackend, (), (override));
    MOCK_METHOD(bool, play, (), (override));
    MOCK_METHOD(bool, pause, (), (override));
    MOCK_METHOD(bool, setEos, (), (override));
    MOCK_METHOD(bool, getBufferAvailable, (uint32_t & availableFrames), (override));
    MOCK_METHOD(bool, getBufferDelay, (uint32_t & delayFrames), (override));
    MOCK_METHOD(bool, writeBuffer, (const uint32_t numberOfFrames, void *data), (override));
    MOCK_METHOD(bool, getDeviceInfo, (uint32_t & preferredFrames, uint32_t &maximumFrames, bool &supportDeferredPlay),
                (override));
    MOCK_METHOD(bool, setVolume, (double volume), (override));
    MOCK_METHOD(bool, getVolume, (double &volume), (override));
};
} // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_WEB_AUDIO_CLIENT_BACKEND_MOCK_H_
