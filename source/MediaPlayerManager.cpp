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

#include "MediaPlayerManager.h"
#include "ClientBackend.h"

unsigned MediaPlayerManager::m_refCount = 0;
void *MediaPlayerManager::m_controller = nullptr;
std::shared_ptr<GStreamerMSEMediaPlayerClient> MediaPlayerManager::m_mseClient = nullptr;
std::mutex MediaPlayerManager::m_mutex;

MediaPlayerManager::MediaPlayerManager() : m_isReleaseNeeded(true)
{
    createMediaPlayerClient();
}

MediaPlayerManager::~MediaPlayerManager()
{
    releaseMediaPlayerClient();
}

std::shared_ptr<GStreamerMSEMediaPlayerClient> MediaPlayerManager::getMediaPlayerClient()
{
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_mseClient;
}

bool MediaPlayerManager::hasControl()
{
    std::lock_guard<std::mutex> guard(m_mutex);
    if (m_controller == this)
    {
        return true;
    }
    else
    { // in case there's no controller anymore
        return acquireControl();
    }
}

void MediaPlayerManager::releaseMediaPlayerClient()
{
    if (m_isReleaseNeeded)
    {
        std::lock_guard<std::mutex> guard(m_mutex);

        m_isReleaseNeeded = false;
        if (m_refCount > 0)
        {
            m_refCount--;
            if (m_refCount == 0)
            {
                m_mseClient->stopStreaming();
                m_mseClient->destroyClientBackend();
                m_mseClient.reset();
            }

            if (m_controller == this)
                m_controller = nullptr;
        }
    }
}

bool MediaPlayerManager::acquireControl()
{
    if (m_controller == nullptr)
    {
        m_controller = this;
        return true;
    }

    return false;
}

void MediaPlayerManager::createMediaPlayerClient()
{
    std::lock_guard<std::mutex> guard(m_mutex);

    if (m_refCount == 0)
    {
        std::shared_ptr<firebolt::rialto::client::ClientBackendInterface> clientBackend =
            std::make_shared<firebolt::rialto::client::ClientBackend>();
        m_mseClient = std::make_shared<GStreamerMSEMediaPlayerClient>(clientBackend);

        if (m_mseClient->createBackend())
        {
            m_controller = this;
        }
        else
        {
            m_mseClient.reset();
            return;
        }
    }
    m_refCount++;
}
