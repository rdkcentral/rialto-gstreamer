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
                 const WebAudioConfig *config, std::weak_ptr<client::IWebAudioPlayerIpcFactory> webAudioPlayerIpcFactory,
                 std::weak_ptr<client::IClientController> clientController),
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
