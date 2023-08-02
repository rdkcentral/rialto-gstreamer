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

#include "GStreamerWebAudioPlayerClient.h"
#include "MessageQueueMock.h"
#include "WebAudioClientBackendMock.h"
#include <gtest/gtest.h>

using firebolt::rialto::client::WebAudioClientBackendMock;
using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

namespace
{
void errorCallback(const char *message) {}
void eosCallback(void) {}
void stateChangedCallback(firebolt::rialto::WebAudioPlayerState) {}
constexpr int kRate{12};
constexpr int kChannels{2};
const std::string kMimeType{"audio/x-raw"};
const std::string kMp4MimeType{"audio/mp4"};
constexpr uint32_t kPriority{1};
const std::string kSignedFormat{"S12BE"};
constexpr firebolt::rialto::WebAudioPcmConfig kSignedFormatConfig{kRate, kChannels, 12, true, true, false};
const std::string kUnsignedFormat{"U12BE"};
constexpr firebolt::rialto::WebAudioPcmConfig kUnsignedFormatConfig{kRate, kChannels, 12, true, false, false};
const std::string kFloatFormat{"F12BE"};
constexpr firebolt::rialto::WebAudioPcmConfig kFloatFormatConfig{kRate, kChannels, 12, true, false, true};
const std::string kLittleEndian{"U12LE"};
constexpr firebolt::rialto::WebAudioPcmConfig kLittleEndianFormatConfig{kRate, kChannels, 12, false, false, false};
MATCHER_P(WebAudioConfigMatcher, config, "")
{
    return arg && arg->pcm.rate == config.rate && arg->pcm.channels == config.channels &&
           arg->pcm.sampleSize == config.sampleSize && arg->pcm.isBigEndian == config.isBigEndian &&
           arg->pcm.isSigned == config.isSigned && arg->pcm.isFloat == config.isFloat;
}
} // namespace

class GstreamerWebAudioPlayerClientTests : public testing::Test
{
public:
    GstreamerWebAudioPlayerClientTests()
    {
        gst_init(nullptr, nullptr);
        EXPECT_CALL(m_messageQueueMock, start());
        EXPECT_CALL(m_messageQueueMock, stop());
        WebAudioSinkCallbacks callbacks{errorCallback, eosCallback, stateChangedCallback};
        m_sut = std::make_shared<GStreamerWebAudioPlayerClient>(std::move(m_webAudioClientBackend),
                                                                std::move(m_messageQueue), callbacks);
    }

    void expectCallInEventLoop()
    {
        EXPECT_CALL(m_messageQueueMock, callInEventLoop(_))
            .WillRepeatedly(Invoke(
                [](const auto &f)
                {
                    f();
                    return true;
                }));
    }

protected:
    std::unique_ptr<StrictMock<WebAudioClientBackendMock>> m_webAudioClientBackend{
        std::make_unique<StrictMock<WebAudioClientBackendMock>>()};
    StrictMock<WebAudioClientBackendMock> &m_webAudioClientBackendMock{*m_webAudioClientBackend};
    std::unique_ptr<StrictMock<MessageQueueMock>> m_messageQueue{std::make_unique<StrictMock<MessageQueueMock>>()};
    StrictMock<MessageQueueMock> &m_messageQueueMock{*m_messageQueue};
    std::shared_ptr<GStreamerWebAudioPlayerClient> m_sut;
};

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldNotOpenWhenFormatIsNotPresentInCaps)
{
    GstCaps *caps =
        gst_caps_new_simple(kMimeType.c_str(), "rate", G_TYPE_INT, kRate, "channels", G_TYPE_INT, kChannels, nullptr);
    EXPECT_FALSE(m_sut->open(caps));
    gst_caps_unref(caps);
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldNotOpenWhenFormatIsEmpty)
{
    GstCaps *caps = gst_caps_new_simple(kMimeType.c_str(), "rate", G_TYPE_INT, kRate, "channels", G_TYPE_INT, kChannels,
                                        "format", G_TYPE_STRING, "", nullptr);
    EXPECT_FALSE(m_sut->open(caps));
    gst_caps_unref(caps);
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldNotOpenWhenRateIsNotPresent)
{
    GstCaps *caps = gst_caps_new_simple(kMimeType.c_str(), "channels", G_TYPE_INT, kChannels, "format", G_TYPE_STRING,
                                        kSignedFormat.c_str(), nullptr);
    EXPECT_FALSE(m_sut->open(caps));
    gst_caps_unref(caps);
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldNotOpenWhenChannelsAreNotPresent)
{
    GstCaps *caps = gst_caps_new_simple(kMimeType.c_str(), "rate", G_TYPE_INT, kRate, "format", G_TYPE_STRING,
                                        kSignedFormat.c_str(), nullptr);
    EXPECT_FALSE(m_sut->open(caps));
    gst_caps_unref(caps);
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldNotOpenWhenFormatHasWrongSize)
{
    GstCaps *caps = gst_caps_new_simple(kMimeType.c_str(), "rate", G_TYPE_INT, kRate, "channels", G_TYPE_INT, kChannels,
                                        "format", G_TYPE_STRING, "toolongformat", nullptr);
    EXPECT_FALSE(m_sut->open(caps));
    gst_caps_unref(caps);
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldNotOpenWhenFormatHasInvalidType)
{
    GstCaps *caps = gst_caps_new_simple(kMimeType.c_str(), "rate", G_TYPE_INT, kRate, "channels", G_TYPE_INT, kChannels,
                                        "format", G_TYPE_STRING, "I12BE", nullptr);
    EXPECT_FALSE(m_sut->open(caps));
    gst_caps_unref(caps);
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldNotOpenWhenCreateBackendFails)
{
    expectCallInEventLoop();
    EXPECT_CALL(m_webAudioClientBackendMock,
                createWebAudioBackend(_, kMimeType, kPriority, WebAudioConfigMatcher(kSignedFormatConfig)))
        .WillOnce(Return(false));
    GstCaps *caps = gst_caps_new_simple(kMimeType.c_str(), "rate", G_TYPE_INT, kRate, "channels", G_TYPE_INT, kChannels,
                                        "format", G_TYPE_STRING, kSignedFormat.c_str(), nullptr);
    EXPECT_FALSE(m_sut->open(caps));
    gst_caps_unref(caps);
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldOpenWithFailedGetDeviceInfo)
{
    expectCallInEventLoop();
    EXPECT_CALL(m_webAudioClientBackendMock,
                createWebAudioBackend(_, kMimeType, kPriority, WebAudioConfigMatcher(kSignedFormatConfig)))
        .WillOnce(Return(true));
    EXPECT_CALL(m_webAudioClientBackendMock, getDeviceInfo(_, _, _)).WillOnce(Return(false));
    GstCaps *caps = gst_caps_new_simple(kMimeType.c_str(), "rate", G_TYPE_INT, kRate, "channels", G_TYPE_INT, kChannels,
                                        "format", G_TYPE_STRING, kSignedFormat.c_str(), nullptr);
    EXPECT_TRUE(m_sut->open(caps));
    gst_caps_unref(caps);
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldOpenWithSignedFormat)
{
    expectCallInEventLoop();
    EXPECT_CALL(m_webAudioClientBackendMock,
                createWebAudioBackend(_, kMimeType, kPriority, WebAudioConfigMatcher(kSignedFormatConfig)))
        .WillOnce(Return(true));
    EXPECT_CALL(m_webAudioClientBackendMock, getDeviceInfo(_, _, _)).WillOnce(Return(true));
    GstCaps *caps = gst_caps_new_simple(kMimeType.c_str(), "rate", G_TYPE_INT, kRate, "channels", G_TYPE_INT, kChannels,
                                        "format", G_TYPE_STRING, kSignedFormat.c_str(), nullptr);
    EXPECT_TRUE(m_sut->open(caps));
    gst_caps_unref(caps);
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldOpenWithUnsignedFormat)
{
    expectCallInEventLoop();
    EXPECT_CALL(m_webAudioClientBackendMock,
                createWebAudioBackend(_, kMimeType, kPriority, WebAudioConfigMatcher(kUnsignedFormatConfig)))
        .WillOnce(Return(true));
    EXPECT_CALL(m_webAudioClientBackendMock, getDeviceInfo(_, _, _)).WillOnce(Return(true));
    GstCaps *caps = gst_caps_new_simple(kMimeType.c_str(), "rate", G_TYPE_INT, kRate, "channels", G_TYPE_INT, kChannels,
                                        "format", G_TYPE_STRING, kUnsignedFormat.c_str(), nullptr);
    EXPECT_TRUE(m_sut->open(caps));
    gst_caps_unref(caps);
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldOpenWithFloatFormat)
{
    expectCallInEventLoop();
    EXPECT_CALL(m_webAudioClientBackendMock,
                createWebAudioBackend(_, kMimeType, kPriority, WebAudioConfigMatcher(kFloatFormatConfig)))
        .WillOnce(Return(true));
    EXPECT_CALL(m_webAudioClientBackendMock, getDeviceInfo(_, _, _)).WillOnce(Return(true));
    GstCaps *caps = gst_caps_new_simple(kMimeType.c_str(), "rate", G_TYPE_INT, kRate, "channels", G_TYPE_INT, kChannels,
                                        "format", G_TYPE_STRING, kFloatFormat.c_str(), nullptr);
    EXPECT_TRUE(m_sut->open(caps));
    gst_caps_unref(caps);
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldOpenWithLittleEndianFormat)
{
    expectCallInEventLoop();
    EXPECT_CALL(m_webAudioClientBackendMock,
                createWebAudioBackend(_, kMimeType, kPriority, WebAudioConfigMatcher(kLittleEndianFormatConfig)))
        .WillOnce(Return(true));
    EXPECT_CALL(m_webAudioClientBackendMock, getDeviceInfo(_, _, _)).WillOnce(Return(true));
    GstCaps *caps = gst_caps_new_simple(kMimeType.c_str(), "rate", G_TYPE_INT, kRate, "channels", G_TYPE_INT, kChannels,
                                        "format", G_TYPE_STRING, kLittleEndian.c_str(), nullptr);
    EXPECT_TRUE(m_sut->open(caps));
    gst_caps_unref(caps);
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldFailToOpenTheSameConfigTwice)
{
    expectCallInEventLoop();
    EXPECT_CALL(m_webAudioClientBackendMock,
                createWebAudioBackend(_, kMimeType, kPriority, WebAudioConfigMatcher(kSignedFormatConfig)))
        .WillOnce(Return(true));
    EXPECT_CALL(m_webAudioClientBackendMock, getDeviceInfo(_, _, _)).WillOnce(Return(true));
    GstCaps *caps = gst_caps_new_simple(kMimeType.c_str(), "rate", G_TYPE_INT, kRate, "channels", G_TYPE_INT, kChannels,
                                        "format", G_TYPE_STRING, kSignedFormat.c_str(), nullptr);
    EXPECT_TRUE(m_sut->open(caps));
    EXPECT_FALSE(m_sut->open(caps));
    gst_caps_unref(caps);
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldOpenTheSameConfigTwiceWhenMimeTypeChanged)
{
    expectCallInEventLoop();
    EXPECT_CALL(m_webAudioClientBackendMock,
                createWebAudioBackend(_, kMimeType, kPriority, WebAudioConfigMatcher(kSignedFormatConfig)))
        .WillOnce(Return(true));
    EXPECT_CALL(m_webAudioClientBackendMock, getDeviceInfo(_, _, _)).WillOnce(Return(true));
    GstCaps *caps = gst_caps_new_simple(kMimeType.c_str(), "rate", G_TYPE_INT, kRate, "channels", G_TYPE_INT, kChannels,
                                        "format", G_TYPE_STRING, kSignedFormat.c_str(), nullptr);
    EXPECT_TRUE(m_sut->open(caps));
    gst_caps_unref(caps);

    GstCaps *newCaps = gst_caps_new_simple(kMp4MimeType.c_str(), "rate", G_TYPE_INT, kRate, "channels", G_TYPE_INT,
                                           kChannels, "format", G_TYPE_STRING, kSignedFormat.c_str(), nullptr);
    EXPECT_CALL(m_webAudioClientBackendMock, destroyWebAudioBackend());
    EXPECT_CALL(m_webAudioClientBackendMock,
                createWebAudioBackend(_, kMp4MimeType, kPriority, WebAudioConfigMatcher(kSignedFormatConfig)))
        .WillOnce(Return(true));
    EXPECT_CALL(m_webAudioClientBackendMock, getDeviceInfo(_, _, _)).WillOnce(Return(true));
    EXPECT_TRUE(m_sut->open(newCaps));
    gst_caps_unref(newCaps);
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldOpenTheSameConfigTwiceWhenMimeTypeIsNotRaw)
{
    expectCallInEventLoop();
    EXPECT_CALL(m_webAudioClientBackendMock,
                createWebAudioBackend(_, kMp4MimeType, kPriority, WebAudioConfigMatcher(kSignedFormatConfig)))
        .WillOnce(Return(true));
    EXPECT_CALL(m_webAudioClientBackendMock, getDeviceInfo(_, _, _)).WillOnce(Return(true));
    GstCaps *caps = gst_caps_new_simple(kMp4MimeType.c_str(), "rate", G_TYPE_INT, kRate, "channels", G_TYPE_INT,
                                        kChannels, "format", G_TYPE_STRING, kSignedFormat.c_str(), nullptr);
    EXPECT_TRUE(m_sut->open(caps));

    EXPECT_CALL(m_webAudioClientBackendMock, destroyWebAudioBackend());
    EXPECT_CALL(m_webAudioClientBackendMock,
                createWebAudioBackend(_, kMp4MimeType, kPriority, WebAudioConfigMatcher(kSignedFormatConfig)))
        .WillOnce(Return(true));
    EXPECT_CALL(m_webAudioClientBackendMock, getDeviceInfo(_, _, _)).WillOnce(Return(true));
    EXPECT_TRUE(m_sut->open(caps));

    gst_caps_unref(caps);
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldOpenTheSameConfigTwiceWhenPcmIsChanged)
{
    expectCallInEventLoop();
    EXPECT_CALL(m_webAudioClientBackendMock,
                createWebAudioBackend(_, kMimeType, kPriority, WebAudioConfigMatcher(kSignedFormatConfig)))
        .WillOnce(Return(true));
    EXPECT_CALL(m_webAudioClientBackendMock, getDeviceInfo(_, _, _)).WillOnce(Return(true));
    GstCaps *caps = gst_caps_new_simple(kMimeType.c_str(), "rate", G_TYPE_INT, kRate, "channels", G_TYPE_INT, kChannels,
                                        "format", G_TYPE_STRING, kSignedFormat.c_str(), nullptr);
    EXPECT_TRUE(m_sut->open(caps));
    gst_caps_unref(caps);

    GstCaps *newCaps = gst_caps_new_simple(kMimeType.c_str(), "rate", G_TYPE_INT, kRate, "channels", G_TYPE_INT,
                                           kChannels, "format", G_TYPE_STRING, kUnsignedFormat.c_str(), nullptr);
    EXPECT_CALL(m_webAudioClientBackendMock, destroyWebAudioBackend());
    EXPECT_CALL(m_webAudioClientBackendMock,
                createWebAudioBackend(_, kMimeType, kPriority, WebAudioConfigMatcher(kUnsignedFormatConfig)))
        .WillOnce(Return(true));
    EXPECT_CALL(m_webAudioClientBackendMock, getDeviceInfo(_, _, _)).WillOnce(Return(true));
    EXPECT_TRUE(m_sut->open(newCaps));
    gst_caps_unref(newCaps);
}
