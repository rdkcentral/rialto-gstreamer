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
#include "GstreamerCatLog.h"
#include "IMessageQueue.h"
#include "MediaPlayerClientBackend.h"
#include <algorithm>

std::mutex MediaPlayerManager::m_mediaPlayerClientsMutex;
std::map<const GstObject *, MediaPlayerManager::MediaPlayerClientInfo> MediaPlayerManager::m_mediaPlayerClientsInfo;
#define GST_CAT_DEFAULT rialtoGStreamerCat
MediaPlayerManager::MediaPlayerManager() : m_currentGstBinParent(nullptr) {}

MediaPlayerManager::~MediaPlayerManager()
{
    releaseMediaPlayerClient();
}

bool MediaPlayerManager::attachMediaPlayerClient(const GstObject *gstBinParent, const uint32_t maxVideoWidth,
                                                 const uint32_t maxVideoHeight)
{
    if (!m_client.lock())
    {
        createMediaPlayerClient(gstBinParent, maxVideoWidth, maxVideoHeight);
    }
    else if (gstBinParent != m_currentGstBinParent)
    {
        // New parent gst bin, release old client and create new
        releaseMediaPlayerClient();
        createMediaPlayerClient(gstBinParent, maxVideoWidth, maxVideoHeight);
    }

    if (!m_client.lock())
    {
        GST_ERROR("Failed to attach the media player client");
        return false;
    }
    else
    {
        return true;
    }
}

std::shared_ptr<GStreamerMSEMediaPlayerClient> MediaPlayerManager::getMediaPlayerClient() const
{
    return m_client.lock();
}

bool MediaPlayerManager::hasControl()
{
    if (m_client.lock())
    {
        std::lock_guard<std::mutex> guard(m_mediaPlayerClientsMutex);

        auto it = m_mediaPlayerClientsInfo.find(m_currentGstBinParent);
        if (it != m_mediaPlayerClientsInfo.end())
        {
            if (it->second.controller == this)
            {
                return true;
            }
            else
            { // in case there's no controller anymore
                return acquireControl(it->second);
            }
        }
        else
        {
            GST_WARNING("Could not find the attached media player client");
        }
    }
    else
    {
        GST_WARNING("No media player client attached");
    }

    return false;
}

void MediaPlayerManager::destroyClientEntryUnlocked(std::map<const GstObject *, MediaPlayerClientInfo>::iterator it)
{
    it->second.client->stop();
    it->second.client->stopStreaming();
    it->second.client->destroyClientBackend();
    m_mediaPlayerClientsInfo.erase(it);
}

void MediaPlayerManager::releaseMediaPlayerClient()
{
    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_client.lock();
    if (!client)
    {
        return;
    }

    std::lock_guard<std::mutex> guard(m_mediaPlayerClientsMutex);

    auto it = m_mediaPlayerClientsInfo.find(m_currentGstBinParent);
    if (it == m_mediaPlayerClientsInfo.end())
    {
        // The key we attached with is gone, but we still hold a reference to the client, so the entry
        // must be found and released by value - otherwise the map keeps the client (and with it the
        // RialtoServer session) alive for the lifetime of the process.
        GST_ERROR("Could not find the attached media player client by parent, searching by client");
        it = std::find_if(m_mediaPlayerClientsInfo.begin(), m_mediaPlayerClientsInfo.end(),
                          [&client](const auto &entry) { return entry.second.client == client; });
    }

    if (it != m_mediaPlayerClientsInfo.end())
    {
        it->second.refCount--;
        if (it->second.refCount == 0)
        {
            destroyClientEntryUnlocked(it);
        }
        else
        {
            if (it->second.controller == this)
                it->second.controller = nullptr;
        }
    }
    else
    {
        GST_ERROR("Could not find the attached media player client");
    }

    // Always drop our own reference, even when the entry could not be found. Keeping it would leave
    // this manager permanently attached to a client it can no longer release.
    m_client.reset();
    m_currentGstBinParent = nullptr;
}

bool MediaPlayerManager::acquireControl(MediaPlayerClientInfo &mediaPlayerClientInfo)
{
    if (mediaPlayerClientInfo.controller == nullptr)
    {
        mediaPlayerClientInfo.controller = this;
        return true;
    }

    return false;
}

void MediaPlayerManager::createMediaPlayerClient(const GstObject *gstBinParent, const uint32_t maxVideoWidth,
                                                 const uint32_t maxVideoHeight)
{
    std::lock_guard<std::mutex> guard(m_mediaPlayerClientsMutex);

    auto it = m_mediaPlayerClientsInfo.find(gstBinParent);
    if (it != m_mediaPlayerClientsInfo.end())
    {
        it->second.refCount++;
        m_client = it->second.client;
        m_currentGstBinParent = gstBinParent;
    }
    else
    {
        std::shared_ptr<firebolt::rialto::client::MediaPlayerClientBackendInterface> clientBackend =
            std::make_shared<firebolt::rialto::client::MediaPlayerClientBackend>();
        std::shared_ptr<GStreamerMSEMediaPlayerClient> client =
            std::make_shared<GStreamerMSEMediaPlayerClient>(IMessageQueueFactory::createFactory(), clientBackend,
                                                            maxVideoWidth, maxVideoHeight);

        if (client->createBackend())
        {
            // Store the new client in global map. Note that no reference is taken on the key: the map
            // must not keep the parent bin alive, or the sinks inside it would never be disposed and
            // rialto_mse_base_sink_dispose() could never release this entry.
            MediaPlayerClientInfo newClientInfo;
            newClientInfo.client = client;
            newClientInfo.controller = this;
            newClientInfo.refCount = 1;
            m_mediaPlayerClientsInfo.insert(
                std::pair<const GstObject *, MediaPlayerClientInfo>(gstBinParent, newClientInfo));

            // Store client info in object
            m_client = client;
            m_currentGstBinParent = gstBinParent;
        }
        else
        {
            GST_ERROR("Failed to create the media player client backend");
            return;
        }
    }
}
