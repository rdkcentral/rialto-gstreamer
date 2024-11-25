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

#ifndef MATCHERS_H
#define MATCHERS_H

#include "IMediaPipeline.h"
#include "MediaCommon.h"
#include <gmock/gmock.h>

namespace firebolt::rialto
{
bool operator==(const VideoRequirements &lhs, const VideoRequirements &rhs);
bool operator==(const AudioConfig &lhs, const AudioConfig &rhs);
bool operator==(const WebAudioPcmConfig &lhs, const WebAudioPcmConfig &rhs);
bool matchCodecData(const std::shared_ptr<firebolt::rialto::CodecData> &lhs,
                    const std::shared_ptr<firebolt::rialto::CodecData> &rhs);
} // namespace firebolt::rialto

MATCHER_P(MediaSourceAudioMatcher, mediaSource, "")
{
    try
    {
        auto &matchedSource{dynamic_cast<firebolt::rialto::IMediaPipeline::MediaSourceAudio &>(*arg)};
        return matchedSource.getType() == mediaSource.getType() &&
               matchedSource.getMimeType() == mediaSource.getMimeType() &&
               matchedSource.getHasDrm() == mediaSource.getHasDrm() &&
               matchedSource.getAudioConfig() == mediaSource.getAudioConfig() &&
               matchedSource.getSegmentAlignment() == mediaSource.getSegmentAlignment() &&
               matchedSource.getStreamFormat() == mediaSource.getStreamFormat() &&
               firebolt::rialto::matchCodecData(matchedSource.getCodecData(), mediaSource.getCodecData()) &&
               matchedSource.getConfigType() == mediaSource.getConfigType();
    }
    catch (std::exception &)
    {
        return false;
    }
}

#endif // MATCHERS_H
