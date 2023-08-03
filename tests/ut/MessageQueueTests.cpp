/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 Sky UK
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "MessageQueue.h"
#include <condition_variable>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mutex>

namespace
{
class TestMessage : public Message
{
public:
    TestMessage(std::mutex &mtx, std::condition_variable &cv, bool &flag) : m_mutex{mtx}, m_cv{cv}, m_callFlag{flag} {}
    ~TestMessage() override = default;
    void handle() override
    {
        std::unique_lock<std::mutex> lock{m_mutex};
        m_callFlag = true;
        m_cv.notify_one();
    }

private:
    std::mutex &m_mutex;
    std::condition_variable &m_cv;
    bool &m_callFlag;
};
} // namespace

class MessageQueueTests : public testing::Test
{
public:
    MessageQueueTests() = default;

protected:
    MessageQueue m_sut;
};

TEST_F(MessageQueueTests, ShouldStartAndStop)
{
    m_sut.start();
    m_sut.clear();
    m_sut.stop();
}

TEST_F(MessageQueueTests, ShouldSkipStartingTwice)
{
    m_sut.start();
    m_sut.start();
}

TEST_F(MessageQueueTests, ShouldFailToPostMessageWhenNotRunning)
{
    std::shared_ptr<Message> msg;
    EXPECT_FALSE(m_sut.postMessage(msg));
}

TEST_F(MessageQueueTests, ShouldPostMessage)
{
    std::mutex mtx;
    std::unique_lock<std::mutex> lock{mtx};
    std::condition_variable cv;
    bool callFlag{false};
    std::shared_ptr<Message> msg{std::make_shared<TestMessage>(mtx, cv, callFlag)};
    m_sut.start();
    EXPECT_TRUE(m_sut.postMessage(msg));
    cv.wait_for(lock, std::chrono::milliseconds(50), [&]() { return callFlag; });
    EXPECT_TRUE(callFlag);
}

TEST_F(MessageQueueTests, ShouldFailToCallInEventLoopWhenNotRunning)
{
    EXPECT_FALSE(m_sut.callInEventLoop([]() {}));
}

TEST_F(MessageQueueTests, ShouldCallInEventLoop)
{
    std::mutex mtx;
    std::unique_lock<std::mutex> lock{mtx};
    std::condition_variable cv;
    bool callFlag{false};
    m_sut.start();
    EXPECT_TRUE(m_sut.callInEventLoop(
        [&]()
        {
            std::unique_lock<std::mutex> lock;
            callFlag = true;
            cv.notify_one();
        }));
    cv.wait_for(lock, std::chrono::milliseconds(50), [&]() { return callFlag; });
    EXPECT_TRUE(callFlag);
}

TEST_F(MessageQueueTests, ShouldCallInEventLoopInTheSameThread)
{
    std::mutex mtx;
    std::unique_lock<std::mutex> lock{mtx};
    std::condition_variable cv;
    bool callFlag{false};
    auto fun = [&]()
    {
        std::unique_lock<std::mutex> lock;
        callFlag = true;
        cv.notify_one();
    };
    m_sut.start();
    EXPECT_TRUE(m_sut.callInEventLoop([&]() { m_sut.callInEventLoop(fun); }));
    cv.wait_for(lock, std::chrono::milliseconds(50), [&]() { return callFlag; });
    EXPECT_TRUE(callFlag);
}
