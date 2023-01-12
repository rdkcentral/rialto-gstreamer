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
#pragma once

#include "WebAudioClientBackendInterface.h"
#include <IWebAudioPlayer.h>
#include <IWebAudioPlayerClient.h>
#include <memory>

namespace firebolt::rialto::client
{
class WebAudioClientBackend final : public WebAudioClientBackendInterface
{
public:
    WebAudioClientBackend() : mWebAudioPlayerBackend(nullptr) {}
    ~WebAudioClientBackend() final { mWebAudioPlayerBackend.reset(); }

    bool createWebAudioBackend(std::weak_ptr<IWebAudioPlayerClient> client, const std::string &audioMimeType,
                               const uint32_t priority, const WebAudioConfig *config) override
    {
        mWebAudioPlayerBackend =
            firebolt::rialto::IWebAudioPlayerFactory::createFactory()->createWebAudioPlayer(client, audioMimeType,
                                                                                            priority, config);

        if (!mWebAudioPlayerBackend)
        {
            GST_ERROR("Could not create web audio backend");
            return false;
        }
        return true;
    }

    bool play() override { return mWebAudioPlayerBackend->play(); }
    bool pause() override { return mWebAudioPlayerBackend->pause(); }
    bool setEos() override { return mWebAudioPlayerBackend->pause(); }
    bool getBufferAvailable(uint32_t &availableFrames) override
    {
        std::shared_ptr<firebolt::rialto::WebAudioShmInfo> webAudioShmInfo;
        return mWebAudioPlayerBackend->getBufferAvailable(availableFrames, webAudioShmInfo);
    }
    bool getBufferDelay(uint32_t &delayFrames) override { return mWebAudioPlayerBackend->getBufferDelay(delayFrames); }
    bool writeBuffer(const uint32_t numberOfFrames, void *data) override
    {
        return mWebAudioPlayerBackend->writeBuffer(numberOfFrames, data);
    }
    bool getDeviceInfo(uint32_t &preferredFrames, uint32_t &maximumFrames, bool &supportDeferredPlay) override
    {
        return mWebAudioPlayerBackend->getDeviceInfo(preferredFrames, maximumFrames, supportDeferredPlay);
    }
    bool setVolume(double volume) { return mWebAudioPlayerBackend->setVolume(volume); }
    bool getVolume(double &volume) { return mWebAudioPlayerBackend->setVolume(volume); }

private:
    std::unique_ptr<IWebAudioPlayer> mWebAudioPlayerBackend;
};
} // namespace firebolt::rialto::client
