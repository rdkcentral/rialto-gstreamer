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

#ifndef FIREBOLT_RIALTO_CLIENT_MEDIA_PLAYER_CLIENT_BACKEND_MOCK_H_
#define FIREBOLT_RIALTO_CLIENT_MEDIA_PLAYER_CLIENT_BACKEND_MOCK_H_

#include "MediaPlayerClientBackendInterface.h"
#include <gmock/gmock.h>

namespace firebolt::rialto::client
{
class MediaPlayerClientBackendMock : public MediaPlayerClientBackendInterface
{
public:
    MOCK_METHOD(void, createMediaPlayerBackend,
                (std::weak_ptr<IMediaPipelineClient> client, uint32_t maxWidth, uint32_t maxHeight), (override));
    MOCK_METHOD(bool, isMediaPlayerBackendCreated, (), (const, override));
    MOCK_METHOD(bool, attachSource, (std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> & source),
                (override));
    MOCK_METHOD(bool, removeSource, (int32_t id), (override));
    MOCK_METHOD(bool, allSourcesAttached, (), (override));
    MOCK_METHOD(bool, load, (firebolt::rialto::MediaType type, const std::string &mimeType, const std::string &url),
                (override));
    MOCK_METHOD(bool, play, (), (override));
    MOCK_METHOD(bool, pause, (), (override));
    MOCK_METHOD(bool, stop, (), (override));
    MOCK_METHOD(bool, haveData, (firebolt::rialto::MediaSourceStatus status, unsigned int needDataRequestId), (override));
    MOCK_METHOD(bool, seek, (int64_t seekPosition), (override));
    MOCK_METHOD(bool, setPlaybackRate, (double rate), (override));
    MOCK_METHOD(bool, setVideoWindow, (unsigned int x, unsigned int y, unsigned int width, unsigned int height),
                (override));
    MOCK_METHOD(firebolt::rialto::AddSegmentStatus, addSegment,
                (unsigned int needDataRequestId,
                 const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &mediaSegment),
                (override));
    MOCK_METHOD(bool, getPosition, (int64_t & position), (override));
    MOCK_METHOD(bool, renderFrame, (), (override));
    MOCK_METHOD(bool, setVolume, (double volume), (override));
    MOCK_METHOD(bool, getVolume, (double &volume), (override));
    MOCK_METHOD(bool, setMute, (bool mute), (override));
    MOCK_METHOD(bool, getMute, (bool &mute), (override));
};
} // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_MEDIA_PLAYER_CLIENT_BACKEND_MOCK_H_
