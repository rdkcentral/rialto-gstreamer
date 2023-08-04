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

#ifndef FIREBOLT_RIALTO_MEDIA_PIPELINE_CLIENT_MOCK_H_
#define FIREBOLT_RIALTO_MEDIA_PIPELINE_CLIENT_MOCK_H_

#include "IMediaPipelineClient.h"
#include <gmock/gmock.h>

namespace firebolt::rialto
{
class MediaPipelineClientMock : public IMediaPipelineClient
{
public:
    MOCK_METHOD(void, notifyDuration, (int64_t duration), (override));
    MOCK_METHOD(void, notifyPosition, (int64_t position), (override));
    MOCK_METHOD(void, notifyNativeSize, (uint32_t width, uint32_t height, double aspect), (override));
    MOCK_METHOD(void, notifyNetworkState, (NetworkState state), (override));
    MOCK_METHOD(void, notifyPlaybackState, (PlaybackState state), (override));
    MOCK_METHOD(void, notifyVideoData, (bool hasData), (override));
    MOCK_METHOD(void, notifyAudioData, (bool hasData), (override));
    MOCK_METHOD(void, notifyNeedMediaData,
                (int32_t sourceId, size_t frameCount, uint32_t needDataRequestId,
                 const std::shared_ptr<MediaPlayerShmInfo> &shmInfo),
                (override));
    MOCK_METHOD(void, notifyCancelNeedMediaData, (int32_t sourceId), (override));
    MOCK_METHOD(void, notifyQos, (int32_t sourceId, const QosInfo &qosInfo), (override));
    MOCK_METHOD(void, notifyBufferUnderflow, (int32_t sourceId), (override));
};
} // namespace firebolt::rialto

#endif // FIREBOLT_RIALTO_MEDIA_PIPELINE_CLIENT_MOCK_H_
