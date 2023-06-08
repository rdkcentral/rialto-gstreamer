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

#include <IMediaPipeline.h>

#include <string>

namespace firebolt::rialto::client
{
class MediaPlayerClientBackendInterface
{
public:
    virtual ~MediaPlayerClientBackendInterface() = default;
    virtual void createMediaPlayerBackend(std::weak_ptr<IMediaPipelineClient> client, uint32_t maxWidth,
                                          uint32_t maxHeight) = 0;
    virtual bool isMediaPlayerBackendCreated() const = 0;
    virtual bool attachSource(std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> &source) = 0;
    virtual bool removeSource(int32_t id) = 0;
    virtual bool allSourcesAttached() = 0;
    virtual bool load(firebolt::rialto::MediaType type, const std::string &mimeType, const std::string &url) = 0;
    virtual bool play() = 0;
    virtual bool pause() = 0;
    virtual bool stop() = 0;
    virtual bool haveData(firebolt::rialto::MediaSourceStatus status, unsigned int needDataRequestId) = 0;
    virtual bool seek(int64_t seekPosition) = 0;
    virtual bool setPlaybackRate(double rate) = 0;
    virtual bool setVideoWindow(unsigned int x, unsigned int y, unsigned int width, unsigned int height) = 0;
    virtual firebolt::rialto::AddSegmentStatus
    addSegment(unsigned int needDataRequestId,
               const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &mediaSegment) = 0;
    virtual bool getPosition(int64_t &position) = 0;
    virtual bool renderFrame() = 0;
    virtual bool setVolume(double volume) = 0;
    virtual bool getVolume(double &volume) = 0;
    virtual bool setMute(bool mute) = 0;
    virtual bool getMute(bool &mute) = 0;
};
} // namespace firebolt::rialto::client
