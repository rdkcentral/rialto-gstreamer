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
