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

#ifndef FIREBOLT_RIALTO_MEDIA_PIPELINE_MOCK_H_
#define FIREBOLT_RIALTO_MEDIA_PIPELINE_MOCK_H_

#include "IMediaPipeline.h"
#include <gmock/gmock.h>

namespace firebolt::rialto
{
class MediaPipelineFactoryMock : public IMediaPipelineFactory
{
public:
    MOCK_METHOD(std::unique_ptr<IMediaPipeline>, createMediaPipeline,
                (std::weak_ptr<IMediaPipelineClient> client, const VideoRequirements &videoRequirements),
                (const, override));
};

class MediaPipelineMock : public IMediaPipeline
{
public:
    MOCK_METHOD(std::weak_ptr<IMediaPipelineClient>, getClient, (), (override));
    MOCK_METHOD(bool, load, (MediaType type, const std::string &mimeType, const std::string &url, bool isLive), (override));
    MOCK_METHOD(bool, attachSource, (const std::unique_ptr<MediaSource> &source), (override));
    MOCK_METHOD(bool, removeSource, (int32_t id), (override));
    MOCK_METHOD(bool, allSourcesAttached, (), (override));
    MOCK_METHOD(bool, play, (bool &async), (override));
    MOCK_METHOD(bool, pause, (), (override));
    MOCK_METHOD(bool, stop, (), (override));
    MOCK_METHOD(bool, setPlaybackRate, (double rate), (override));
    MOCK_METHOD(bool, setPosition, (int64_t position), (override));
    MOCK_METHOD(bool, getPosition, (int64_t & position), (override));
    MOCK_METHOD(bool, setReportDecodeErrors, (int32_t sourceId, bool reportDecodeErrors), (override));
    MOCK_METHOD(bool, setImmediateOutput, (int32_t sourceId, bool immediateOutput), (override));
    MOCK_METHOD(bool, getImmediateOutput, (int32_t sourceId, bool &immediateOutput), (override));
    MOCK_METHOD(bool, setReportDecodeErrors, (int32_t sourceId, bool reportDecodeErrors), (override));
};
} // namespace firebolt::rialto

#endif // FIREBOLT_RIALTO_MEDIA_PIPELINE_MOCK_H_
