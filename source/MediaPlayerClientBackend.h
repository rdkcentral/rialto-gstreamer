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

#include "MediaPlayerClientBackendInterface.h"
#include <IMediaPipeline.h>

#include <memory>

namespace firebolt::rialto::client
{
class MediaPlayerClientBackend final : public MediaPlayerClientBackendInterface
{
public:
    MediaPlayerClientBackend() : mMediaPlayerBackend(nullptr) {}
    ~MediaPlayerClientBackend() final { mMediaPlayerBackend.reset(); }

    void createMediaPlayerBackend(std::weak_ptr<IMediaPipelineClient> client, uint32_t maxWidth, uint32_t maxHeight) override
    {
        firebolt::rialto::VideoRequirements videoRequirements;
        videoRequirements.maxWidth = maxWidth;
        videoRequirements.maxHeight = maxHeight;

        mMediaPlayerBackend =
            firebolt::rialto::IMediaPipelineFactory::createFactory()->createMediaPipeline(client, videoRequirements);

        if (!mMediaPlayerBackend)
        {
            GST_ERROR("Could not create media player backend");
            return;
        }
    }

    bool isMediaPlayerBackendCreated() const override { return static_cast<bool>(mMediaPlayerBackend); }

    bool attachSource(std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> &source) override
    {
        return mMediaPlayerBackend->attachSource(source);
    }

    bool removeSource(int32_t id) override { return mMediaPlayerBackend->removeSource(id); }

    bool allSourcesAttached() override { return mMediaPlayerBackend->allSourcesAttached(); }

    bool load(firebolt::rialto::MediaType type, const std::string &mimeType, const std::string &url) override
    {
        return mMediaPlayerBackend->load(type, mimeType, url);
    }

    bool play() override { return mMediaPlayerBackend->play(); }
    bool pause() override { return mMediaPlayerBackend->pause(); }
    bool stop() override { return mMediaPlayerBackend->stop(); }
    bool haveData(firebolt::rialto::MediaSourceStatus status, unsigned int needDataRequestId) override
    {
        return mMediaPlayerBackend->haveData(status, needDataRequestId);
    }
    bool seek(int64_t seekPosition) override { return mMediaPlayerBackend->setPosition(seekPosition); }
    bool setPlaybackRate(double rate) override { return mMediaPlayerBackend->setPlaybackRate(rate); }
    bool setVideoWindow(unsigned int x, unsigned int y, unsigned int width, unsigned int height) override
    {
        return mMediaPlayerBackend->setVideoWindow(x, y, width, height);
    }

    firebolt::rialto::AddSegmentStatus
    addSegment(unsigned int needDataRequestId,
               const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &mediaSegment)
    {
        return mMediaPlayerBackend->addSegment(needDataRequestId, mediaSegment);
    }

    bool getPosition(int64_t &position) override { return mMediaPlayerBackend->getPosition(position); }

    bool renderFrame() override { return mMediaPlayerBackend->renderFrame(); }

    bool setVolume(double volume) { return mMediaPlayerBackend->setVolume(volume); }

    bool getVolume(double &volume) { return mMediaPlayerBackend->getVolume(volume); }

private:
    std::unique_ptr<IMediaPipeline> mMediaPlayerBackend;
};
} // namespace firebolt::rialto::client
