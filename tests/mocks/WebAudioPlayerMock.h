/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 Sky UK
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FIREBOLT_RIALTO_WEB_AUDIO_PLAYER_MOCK_H_
#define FIREBOLT_RIALTO_WEB_AUDIO_PLAYER_MOCK_H_

#include "IWebAudioPlayer.h"
#include <gmock/gmock.h>

namespace firebolt::rialto
{
class WebAudioPlayerFactoryMock : public IWebAudioPlayerFactory
{
public:
    MOCK_METHOD(std::unique_ptr<IWebAudioPlayer>, createWebAudioPlayer,
                (std::weak_ptr<IWebAudioPlayerClient> client, const std::string &audioMimeType, const uint32_t priority,
                 const WebAudioConfig *config),
                (const, override));
};

class WebAudioPlayerMock : public IWebAudioPlayer
{
public:
    MOCK_METHOD(bool, play, (), (override));
    MOCK_METHOD(bool, pause, (), (override));
    MOCK_METHOD(bool, setEos, (), (override));
    MOCK_METHOD(bool, getBufferAvailable,
                (uint32_t & availableFrames, std::shared_ptr<WebAudioShmInfo> &webAudioShmInfo), (override));
    MOCK_METHOD(bool, getBufferDelay, (uint32_t & delayFrames), (override));
    MOCK_METHOD(bool, writeBuffer, (const uint32_t numberOfFrames, void *data), (override));
    MOCK_METHOD(bool, getDeviceInfo, (uint32_t & preferredFrames, uint32_t &maximumFrames, bool &supportDeferredPlay),
                (override));
    MOCK_METHOD(bool, setVolume, (double volume), (override));
    MOCK_METHOD(bool, getVolume, (double &volume), (override));
    MOCK_METHOD(std::weak_ptr<IWebAudioPlayerClient>, getClient, (), (override));
};
} // namespace firebolt::rialto

#endif // FIREBOLT_RIALTO_WEB_AUDIO_PLAYER_MOCK_H_
