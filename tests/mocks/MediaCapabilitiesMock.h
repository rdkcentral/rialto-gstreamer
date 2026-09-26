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

#ifndef FIREBOLT_RIALTO_MEDIA_CAPABILITIES_MOCK_H_
#define FIREBOLT_RIALTO_MEDIA_CAPABILITIES_MOCK_H_

#include "IMediaCapabilities.h"
#include <gmock/gmock.h>

namespace firebolt::rialto
{
class MediaCapabilitiesFactoryMock : public IMediaCapabilitiesFactory
{
public:
    MOCK_METHOD(std::unique_ptr<IMediaCapabilities>, createMediaCapabilities, (), (const, override));
};

class MediaCapabilitiesMock : public IMediaCapabilities
{
public:
    MOCK_METHOD(firebolt::rialto::common::AudioDecoderCapabilities, getSupportedAudioCapabilities, (), (override));
    MOCK_METHOD(firebolt::rialto::common::VideoDecoderCapabilities, getSupportedVideoCapabilities, (), (override));
};
} // namespace firebolt::rialto

#endif // FIREBOLT_RIALTO_MEDIA_CAPABILITIES_MOCK_H_
