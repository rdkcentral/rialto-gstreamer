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

std::mutex MediaPlayerManager::m_mediaPlayerClientsMutex;
std::map<const GstObject *gstBinParent, MediaPlayerClientInfo> m_mediaPlayerClientsInfo;

MediaPlayerManager::MediaPlayerManager() : m_currentGstBinParent(nullptr)
{
}

MediaPlayerManager::~MediaPlayerManager()
{
    releaseMediaPlayerClient();
}

std::shared_ptr<GStreamerMSEMediaPlayerClient> MediaPlayerManager::getMediaPlayerClient(const GstObject *gstBinParent)
{
    if (nullptr == m_client.lock())
    {
        createMediaPlayerClient(gstBinParent);
        return m_client.lock();
    }
    else
    {
        if (gstBinParent != m_currentGstBinParent)
        {
            // New parent gst bin, release old client and create new
            releaseMediaPlayerClient();
            createMediaPlayerClient(gstBinParent);
        }
        return m_client.lock();
    }
}

bool MediaPlayerManager::hasControl(const GstObject *gstBinParent)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    auto it = m_mediaPlayerClientsInfo.find(gstBinParent);
    if (it != m_mediaPlayerClientsInfo.end())
    {
        if (it->second.controller == this)
        {
            return true;
        }
        else
        { // in case there's no controller anymore
            return acquireControl(*it);
        }
    }
}

void MediaPlayerManager::releaseMediaPlayerClient(const GstObject *gstBinParent)
{
    if (nullptr != m_client.lock())
    {
        std::lock_guard<std::mutex> guard(m_mediaPlayerClientsMutex);

        auto it = m_mediaPlayerClientsInfo.find(gstBinParent);
        if (it != m_mediaPlayerClientsInfo.end())
        {
            it->second.refCount--;
            if (it->second.refCount == 0)
            {
                it->second.client->stopStreaming();
                it->second.client->destroyClientBackend();
                m_mediaPlayerClientsInfo.erase(it);
            }
            else
            {
                if (it->second.controller == this)
                    it->second.controller = nullptr;
            }
            m_client = nullptr;
        }
    }
}

bool MediaPlayerManager::acquireControl(MediaPlayerClientInfo& mediaPlayerClientInfo)
{
    if (mediaPlayerClientInfo.controller == nullptr)
    {
        mediaPlayerClientInfo.controller = this;
        return true;
    }

    return false;
}

void MediaPlayerManager::createMediaPlayerClient(const GstObject *gstBinParent)
{
    std::lock_guard<std::mutex> guard(m_mediaPlayerClientsMutex);

    auto it = mediaPlayerClients.find(gstBinParent);
    if (it != mediaPlayerClients.end())
    {
        it->second.refCount++;
        m_client = it->second.client;
    }
    else
    {
        std::shared_ptr<firebolt::rialto::client::ClientBackendInterface> clientBackend =
            std::make_shared<firebolt::rialto::client::ClientBackend>();
        client = std::make_shared<GStreamerMSEMediaPlayerClient>(clientBackend);

        if (client->createBackend())
        {
            // Store the new client in global map
            MediaPlayerClientInfo newClientInfo;
            newClientInfo->client = client;
            newClientInfo->controller = this;
            newClientInfo->refCount = 1;
            m_mediaPlayerClientsInfo.insert(std::pair<const GstObject *gstBinParent, MediaPlayerClientInfo>(gstBinParent, newClientInfo));

            // Store a weak pointer to the client locally
            m_client = client;
            m_currentGstBinParent = gstBinParent;
        }
        else
        {
            // Failed to create backend
            return;
        }
    }
}
