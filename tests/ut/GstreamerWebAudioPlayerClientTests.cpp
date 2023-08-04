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
#include "RialtoGstTest.h"
#include "TimerFactoryMock.h"
#include "TimerMock.h"
#include "WebAudioClientBackendMock.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>

using firebolt::rialto::client::WebAudioClientBackendMock;
using testing::_;
using testing::ByMove;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::SetArgReferee;
using testing::StrictMock;

namespace
{
class CallbackMock
{
public:
    static CallbackMock &instance()
    {
        static CallbackMock callbackMock;
        return callbackMock;
    }
    MOCK_METHOD(void, errorCallback, (const char *message), (const));
    MOCK_METHOD(void, eosCallback, (), (const));
    MOCK_METHOD(void, stateChangedCallback, (firebolt::rialto::WebAudioPlayerState), (const));
};
void errorCallback(const char *message)
{
    CallbackMock::instance().errorCallback(message);
}
void eosCallback()
{
    CallbackMock::instance().eosCallback();
}
void stateChangedCallback(firebolt::rialto::WebAudioPlayerState state)
{
    CallbackMock::instance().stateChangedCallback(state);
}
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
const std::vector<uint8_t> kBytes{1, 2, 3, 4, 5, 6, 7, 8};
constexpr std::chrono::milliseconds kTimeout{100};
constexpr auto kTimerType{TimerType::ONE_SHOT};
MATCHER_P(WebAudioConfigMatcher, config, "")
{
    return arg && arg->pcm.rate == config.rate && arg->pcm.channels == config.channels &&
           arg->pcm.sampleSize == config.sampleSize && arg->pcm.isBigEndian == config.isBigEndian &&
           arg->pcm.isSigned == config.isSigned && arg->pcm.isFloat == config.isFloat;
}
} // namespace

class GstreamerWebAudioPlayerClientTests : public RialtoGstTest
{
public:
    GstreamerWebAudioPlayerClientTests()
    {
        EXPECT_CALL(m_messageQueueMock, start());
        EXPECT_CALL(m_messageQueueMock, stop());
        WebAudioSinkCallbacks callbacks{errorCallback, eosCallback, stateChangedCallback};
        m_sut = std::make_shared<GStreamerWebAudioPlayerClient>(std::move(m_webAudioClientBackend),
                                                                std::move(m_messageQueue), callbacks, m_timerFactoryMock);
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

    void open()
    {
        expectCallInEventLoop();
        EXPECT_CALL(m_webAudioClientBackendMock,
                    createWebAudioBackend(_, kMimeType, kPriority, WebAudioConfigMatcher(kSignedFormatConfig)))
            .WillOnce(Return(true));
        EXPECT_CALL(m_webAudioClientBackendMock, getDeviceInfo(_, _, _)).WillOnce(Return(true));
        GstCaps *caps = gst_caps_new_simple(kMimeType.c_str(), "rate", G_TYPE_INT, kRate, "channels", G_TYPE_INT,
                                            kChannels, "format", G_TYPE_STRING, kSignedFormat.c_str(), nullptr);
        EXPECT_TRUE(m_sut->open(caps));
        gst_caps_unref(caps);
    }

protected:
    std::unique_ptr<StrictMock<WebAudioClientBackendMock>> m_webAudioClientBackend{
        std::make_unique<StrictMock<WebAudioClientBackendMock>>()};
    StrictMock<WebAudioClientBackendMock> &m_webAudioClientBackendMock{*m_webAudioClientBackend};
    std::unique_ptr<StrictMock<MessageQueueMock>> m_messageQueue{std::make_unique<StrictMock<MessageQueueMock>>()};
    StrictMock<MessageQueueMock> &m_messageQueueMock{*m_messageQueue};
    std::shared_ptr<StrictMock<TimerFactoryMock>> m_timerFactoryMock{std::make_shared<StrictMock<TimerFactoryMock>>()};
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
    open();
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
    open();
    GstCaps *caps = gst_caps_new_simple(kMimeType.c_str(), "rate", G_TYPE_INT, kRate, "channels", G_TYPE_INT, kChannels,
                                        "format", G_TYPE_STRING, kSignedFormat.c_str(), nullptr);
    EXPECT_FALSE(m_sut->open(caps));
    gst_caps_unref(caps);
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldOpenTheSameConfigTwiceWhenMimeTypeChanged)
{
    open();

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
    open();

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

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldOpenAgainAfterClose)
{
    open();
    EXPECT_CALL(m_webAudioClientBackendMock, destroyWebAudioBackend());
    EXPECT_TRUE(m_sut->close());
    open();
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldFailToPlayWhenNotOpened)
{
    expectCallInEventLoop();
    EXPECT_FALSE(m_sut->play());
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldFailToPlayWhenOperationFails)
{
    open();
    EXPECT_CALL(m_webAudioClientBackendMock, play()).WillOnce(Return(false));
    EXPECT_FALSE(m_sut->play());
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldPlay)
{
    open();
    EXPECT_CALL(m_webAudioClientBackendMock, play()).WillOnce(Return(true));
    EXPECT_TRUE(m_sut->play());
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldFailToPauseWhenNotOpened)
{
    expectCallInEventLoop();
    EXPECT_FALSE(m_sut->pause());
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldFailToPauseWhenOperationFails)
{
    open();
    EXPECT_CALL(m_webAudioClientBackendMock, pause()).WillOnce(Return(false));
    EXPECT_FALSE(m_sut->pause());
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldPause)
{
    open();
    EXPECT_CALL(m_webAudioClientBackendMock, pause()).WillOnce(Return(true));
    EXPECT_TRUE(m_sut->pause());
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldFailToSetEosWhenNotOpened)
{
    expectCallInEventLoop();
    EXPECT_FALSE(m_sut->setEos());
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldFailToSetEosWhenOperationFails)
{
    open();
    EXPECT_CALL(m_webAudioClientBackendMock, setEos()).WillOnce(Return(false));
    EXPECT_FALSE(m_sut->setEos());
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldSetEos)
{
    open();
    EXPECT_CALL(m_webAudioClientBackendMock, setEos()).WillOnce(Return(true));
    EXPECT_TRUE(m_sut->setEos());
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldSetEosAndTryPushBuffer)
{
    GstBuffer *buffer = gst_buffer_new_allocate(nullptr, kBytes.size(), nullptr);
    gst_buffer_fill(buffer, 0, kBytes.data(), kBytes.size());

    open();
    EXPECT_CALL(m_webAudioClientBackendMock, getBufferAvailable(_)).WillOnce(Return(true));
    std::unique_ptr<StrictMock<TimerMock>> timer{std::make_unique<StrictMock<TimerMock>>()};
    EXPECT_CALL(*m_timerFactoryMock, createTimer(kTimeout, _, kTimerType)).WillOnce(Return(ByMove(std::move(timer))));
    m_sut->notifyNewSample(buffer);

    EXPECT_CALL(m_webAudioClientBackendMock, getBufferAvailable(_)).WillOnce(Return(false));
    EXPECT_CALL(m_webAudioClientBackendMock, setEos()).WillRepeatedly(Return(true));
    EXPECT_TRUE(m_sut->setEos());

    gst_buffer_unref(buffer);
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldNotSetEosTwice)
{
    open();
    EXPECT_CALL(m_webAudioClientBackendMock, setEos()).WillOnce(Return(true));
    EXPECT_TRUE(m_sut->setEos());
    EXPECT_FALSE(m_sut->setEos());
}

// TODO dodac z push samples tutaj

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldNotBeOpened)
{
    expectCallInEventLoop();
    EXPECT_FALSE(m_sut->isOpen());
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldBeOpened)
{
    open();
    EXPECT_TRUE(m_sut->isOpen());
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldNotPushSamplesWhenNotOpened)
{
    expectCallInEventLoop();
    m_sut->notifyPushSamplesTimerExpired();
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldNotPushSamplesWhenGetAvailableBuffersFail)
{
    GstBuffer *buffer = gst_buffer_new_allocate(nullptr, kBytes.size(), nullptr);
    gst_buffer_fill(buffer, 0, kBytes.data(), kBytes.size());

    open();
    EXPECT_CALL(m_webAudioClientBackendMock, getBufferAvailable(_)).WillOnce(Return(false));
    m_sut->notifyNewSample(buffer);

    gst_buffer_unref(buffer);
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldNotPushSamplesWhenThereIsNoBufferAvailable)
{
    GstBuffer *buffer = gst_buffer_new_allocate(nullptr, kBytes.size(), nullptr);
    gst_buffer_fill(buffer, 0, kBytes.data(), kBytes.size());

    open();
    EXPECT_CALL(m_webAudioClientBackendMock, getBufferAvailable(_)).WillOnce(Return(true));
    std::unique_ptr<StrictMock<TimerMock>> timer{std::make_unique<StrictMock<TimerMock>>()};
    EXPECT_CALL(*m_timerFactoryMock, createTimer(kTimeout, _, kTimerType)).WillOnce(Return(ByMove(std::move(timer))));
    m_sut->notifyNewSample(buffer);

    gst_buffer_unref(buffer);
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldTryPushBufferTwiceWhenTimerExpires)
{
    GstBuffer *buffer = gst_buffer_new_allocate(nullptr, kBytes.size(), nullptr);
    gst_buffer_fill(buffer, 0, kBytes.data(), kBytes.size());

    open();
    EXPECT_CALL(m_webAudioClientBackendMock, getBufferAvailable(_)).WillOnce(Return(true));
    std::function<void()> timerCallback;
    std::unique_ptr<StrictMock<TimerMock>> timer{std::make_unique<StrictMock<TimerMock>>()};
    EXPECT_CALL(*m_timerFactoryMock, createTimer(kTimeout, _, kTimerType))
        .WillOnce(Invoke(
            [&](const auto &, const auto &cb, auto)
            {
                timerCallback = cb;
                return std::move(timer);
            }));
    m_sut->notifyNewSample(buffer);

    EXPECT_CALL(m_webAudioClientBackendMock, getBufferAvailable(_)).WillOnce(Return(false));

    ASSERT_TRUE(timerCallback);
    timerCallback();

    gst_buffer_unref(buffer);
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldFailToPushBuffer)
{
    GstBuffer *buffer = gst_buffer_new_allocate(nullptr, kBytes.size(), nullptr);
    gst_buffer_fill(buffer, 0, kBytes.data(), kBytes.size());

    open();
    EXPECT_CALL(m_webAudioClientBackendMock, getBufferAvailable(_))
        .WillOnce(DoAll(SetArgReferee<0>(kBytes.size()), Return(true)));
    EXPECT_CALL(m_webAudioClientBackendMock, writeBuffer(2, _)).WillOnce(Return(false));
    m_sut->notifyNewSample(buffer);
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldPushBuffer)
{
    GstBuffer *buffer = gst_buffer_new_allocate(nullptr, kBytes.size(), nullptr);
    gst_buffer_fill(buffer, 0, kBytes.data(), kBytes.size());

    open();
    EXPECT_CALL(m_webAudioClientBackendMock, getBufferAvailable(_))
        .WillOnce(DoAll(SetArgReferee<0>(kBytes.size()), Return(true)))
        .WillOnce(Return(false));
    EXPECT_CALL(m_webAudioClientBackendMock, writeBuffer(2, _)).WillOnce(Return(true));
    m_sut->notifyNewSample(buffer);

    gst_buffer_unref(buffer);
}

TEST_F(GstreamerWebAudioPlayerClientTests, ShouldAppendBuffer)
{
    GstBuffer *buffer = gst_buffer_new_allocate(nullptr, kBytes.size(), nullptr);
    gst_buffer_fill(buffer, 0, kBytes.data(), kBytes.size());
    GstBuffer *secondBuffer = gst_buffer_new_allocate(nullptr, kBytes.size(), nullptr);
    gst_buffer_fill(secondBuffer, 0, kBytes.data(), kBytes.size());

    open();
    EXPECT_CALL(m_webAudioClientBackendMock, getBufferAvailable(_))
        .WillOnce(DoAll(SetArgReferee<0>(1), Return(true)))
        .WillOnce(DoAll(SetArgReferee<0>(0), Return(true)));
    EXPECT_CALL(m_webAudioClientBackendMock, writeBuffer(1, _)).WillOnce(Return(true));
    std::unique_ptr<StrictMock<TimerMock>> timer{std::make_unique<StrictMock<TimerMock>>()};
    EXPECT_CALL(*timer, cancel());
    EXPECT_CALL(*m_timerFactoryMock, createTimer(kTimeout, _, kTimerType)).WillOnce(Return(ByMove(std::move(timer))));
    m_sut->notifyNewSample(buffer);
    EXPECT_CALL(m_webAudioClientBackendMock, getBufferAvailable(_))
        .WillOnce(DoAll(SetArgReferee<0>(1), Return(true)))
        .WillOnce(DoAll(SetArgReferee<0>(0), Return(true)));
    EXPECT_CALL(m_webAudioClientBackendMock, writeBuffer(1, _)).WillOnce(Return(true));
    std::unique_ptr<StrictMock<TimerMock>> secondTimer{std::make_unique<StrictMock<TimerMock>>()};
    EXPECT_CALL(*m_timerFactoryMock, createTimer(kTimeout, _, kTimerType)).WillOnce(Return(ByMove(std::move(secondTimer))));
    m_sut->notifyNewSample(secondBuffer);
}

TEST_F(GstreamerWebAudioPlayerClientTests, shouldNotifyEos)
{
    EXPECT_CALL(CallbackMock::instance(), eosCallback());
    m_sut->notifyState(firebolt::rialto::WebAudioPlayerState::END_OF_STREAM);
}

TEST_F(GstreamerWebAudioPlayerClientTests, shouldNotifyFailure)
{
    EXPECT_CALL(CallbackMock::instance(), errorCallback(_));
    m_sut->notifyState(firebolt::rialto::WebAudioPlayerState::FAILURE);
}

TEST_F(GstreamerWebAudioPlayerClientTests, shouldNotifyStateChange)
{
    EXPECT_CALL(CallbackMock::instance(), stateChangedCallback(firebolt::rialto::WebAudioPlayerState::IDLE));
    m_sut->notifyState(firebolt::rialto::WebAudioPlayerState::IDLE);
    EXPECT_CALL(CallbackMock::instance(), stateChangedCallback(firebolt::rialto::WebAudioPlayerState::PLAYING));
    m_sut->notifyState(firebolt::rialto::WebAudioPlayerState::PLAYING);
    EXPECT_CALL(CallbackMock::instance(), stateChangedCallback(firebolt::rialto::WebAudioPlayerState::PAUSED));
    m_sut->notifyState(firebolt::rialto::WebAudioPlayerState::PAUSED);
}

TEST_F(GstreamerWebAudioPlayerClientTests, shouldNotCallAnyCallbackWhenUnknownStateIsNotified)
{
    m_sut->notifyState(firebolt::rialto::WebAudioPlayerState::UNKNOWN);
}
