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

#include "Matchers.h"

namespace firebolt::rialto
{
bool operator==(const VideoRequirements &lhs, const VideoRequirements &rhs)
{
    return lhs.maxWidth == rhs.maxWidth && lhs.maxHeight == rhs.maxHeight;
}

bool operator==(const AudioConfig &lhs, const AudioConfig &rhs)
{
    // Skip checking codecSpecificConfig, as it is returned by gstreamer function
    return lhs.numberOfChannels == rhs.numberOfChannels && lhs.sampleRate == rhs.sampleRate &&
           lhs.format == rhs.format && lhs.layout == rhs.layout && lhs.channelMask == rhs.channelMask &&
           lhs.streamHeader == rhs.streamHeader && lhs.framed == rhs.framed;
}

bool operator==(const WebAudioPcmConfig &lhs, const WebAudioPcmConfig &rhs)
{
    return lhs.rate == rhs.rate && lhs.channels == rhs.channels && lhs.sampleSize == rhs.sampleSize &&
           lhs.isBigEndian == rhs.isBigEndian && lhs.isSigned == rhs.isSigned && lhs.isFloat == rhs.isFloat;
}

bool matchCodecData(const std::shared_ptr<firebolt::rialto::CodecData> &lhs,
                    const std::shared_ptr<firebolt::rialto::CodecData> &rhs)
{
    if (lhs == rhs) // If ptrs are both null or point to the same objects
    {
        return true;
    }
    if (lhs && rhs)
    {
        return lhs->data == rhs->data && lhs->type == rhs->type;
    }
    return false;
}
} // namespace firebolt::rialto
