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

#ifndef FIREBOLT_RIALTO_MEDIA_PIPELINE_CAPABILITIES_MOCK_H_
#define FIREBOLT_RIALTO_MEDIA_PIPELINE_CAPABILITIES_MOCK_H_

#include "IMediaPipelineCapabilities.h"
#include <gmock/gmock.h>

namespace firebolt::rialto
{
class MediaPipelineCapabilitiesFactoryMock : public IMediaPipelineCapabilitiesFactory
{
public:
    MOCK_METHOD(std::unique_ptr<IMediaPipelineCapabilities>, createMediaPipelineCapabilities, (), (const, override));
};

class MediaPipelineCapabilitiesMock : public IMediaPipelineCapabilities
{
public:
    MOCK_METHOD(std::vector<std::string>, getSupportedMimeTypes, (MediaSourceType sourceType), (override));
    MOCK_METHOD(bool, isMimeTypeSupported, (const std::string &mimeType), (override));
    MOCK_METHOD(std::vector<std::string>, getSupportedProperties,
                (MediaSourceType mediaType, const std::vector<std::string> &propertyNames), (override));
};
} // namespace firebolt::rialto

#endif // FIREBOLT_RIALTO_MEDIA_PIPELINE_CAPABILITIES_MOCK_H_
