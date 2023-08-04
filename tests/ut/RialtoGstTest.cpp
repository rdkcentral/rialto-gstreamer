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

#include "RialtoGstTest.h"
#include "MediaPipelineCapabilitiesMock.h"
#include "RialtoGSteamerPlugin.cpp" // urgh... disgusting!
#include <gst/gst.h>
#include <string>
#include <vector>

using firebolt::rialto::IMediaPipelineCapabilitiesFactory;
using firebolt::rialto::MediaPipelineCapabilitiesFactoryMock;
using firebolt::rialto::MediaPipelineCapabilitiesMock;
using testing::ByMove;
using testing::Return;
using testing::StrictMock;

namespace
{
const std::vector<std::string> kSupportedAudioMimeTypes{"audio/mp4", "audio/aac", "audio/x-eac3", "audio/x-opus"};
const std::vector<std::string> kSupportedVideoMimeTypes{"video/h264", "video/h265", "video/x-av1", "video/x-vp9",
                                                        "video/unsupported"};
} // namespace

RialtoGstTest::RialtoGstTest()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag,
                   [this]()
                   {
                       expectSinksInitialisation();
                       gst_init(nullptr, nullptr);
                       const auto registerResult =
                           gst_plugin_register_static(GST_VERSION_MAJOR, GST_VERSION_MINOR, "rialtosinks",
                                                      "Sinks which communicate with RialtoServer", rialto_mse_sinks_init,
                                                      "1.0", "LGPL", PACKAGE, PACKAGE, "http://gstreamer.net/");
                       EXPECT_TRUE(registerResult);
                   });
}

void RialtoGstTest::expectSinksInitialisation()
{
    // Media Pipeline Capabilities will be created two times during class_init of audio and video sink
    std::unique_ptr<StrictMock<MediaPipelineCapabilitiesMock>> capabilitiesMockAudio{
        std::make_unique<StrictMock<MediaPipelineCapabilitiesMock>>()};
    std::unique_ptr<StrictMock<MediaPipelineCapabilitiesMock>> capabilitiesMockVideo{
        std::make_unique<StrictMock<MediaPipelineCapabilitiesMock>>()};
    EXPECT_CALL(*capabilitiesMockAudio, getSupportedMimeTypes(firebolt::rialto::MediaSourceType::AUDIO))
        .WillOnce(Return(kSupportedAudioMimeTypes));
    EXPECT_CALL(*capabilitiesMockVideo, getSupportedMimeTypes(firebolt::rialto::MediaSourceType::VIDEO))
        .WillOnce(Return(kSupportedVideoMimeTypes));
    std::shared_ptr<StrictMock<MediaPipelineCapabilitiesFactoryMock>> capabilitiesFactoryMock{
        std::dynamic_pointer_cast<StrictMock<MediaPipelineCapabilitiesFactoryMock>>(
            IMediaPipelineCapabilitiesFactory::createFactory())};
    ASSERT_TRUE(capabilitiesFactoryMock);
    // Video sink is registered first
    EXPECT_CALL(*capabilitiesFactoryMock, createMediaPipelineCapabilities())
        .WillOnce(Return(ByMove(std::move(capabilitiesMockVideo))))
        .WillOnce(Return(ByMove(std::move(capabilitiesMockAudio))));
}
