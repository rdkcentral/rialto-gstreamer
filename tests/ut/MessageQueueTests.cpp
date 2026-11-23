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
#include <atomic>
#include <condition_variable>
#include <future>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mutex>
#include <vector>

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
    rialto::MessageQueue m_sut;
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

TEST_F(MessageQueueTests, ShouldFailToPrepareCallInEventLoopWhenNotRunning)
{
    EXPECT_FALSE(m_sut.prepareCallInEventLoop([]() {}));
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

TEST_F(MessageQueueTests, ShouldPrepareAndWaitForCallInEventLoop)
{
    std::atomic_bool callExecuted{false};
    m_sut.start();
    auto call{m_sut.prepareCallInEventLoop([&]() { callExecuted = true; })};

    ASSERT_TRUE(call);
    call->wait();
    EXPECT_TRUE(callExecuted.load());
}

TEST_F(MessageQueueTests, ShouldReturnPreparedCallAfterQueueClear)
{
    std::promise<void> workerStarted;
    std::promise<void> releaseWorker;
    auto workerStartedFuture{workerStarted.get_future()};
    auto releaseWorkerFuture{releaseWorker.get_future()};
    std::atomic_bool callExecuted{false};
    m_sut.start();
    std::thread workerCall{[&]()
                           {
                               m_sut.callInEventLoop(
                                   [&]()
                                   {
                                       workerStarted.set_value();
                                       releaseWorkerFuture.wait();
                                   });
                           }};
    ASSERT_EQ(std::future_status::ready, workerStartedFuture.wait_for(std::chrono::seconds{5}));

    auto preparedCall{m_sut.prepareCallInEventLoop([&]() { callExecuted = true; })};
    ASSERT_TRUE(preparedCall);
    m_sut.clear();
    releaseWorker.set_value();
    workerCall.join();
    preparedCall->wait();

    EXPECT_FALSE(callExecuted.load());
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

TEST_F(MessageQueueTests, ShouldPrepareAndWaitForCallInEventLoopInTheSameThread)
{
    bool callWasDeferredUntilWait{false};
    bool callExecutedAfterWait{false};
    m_sut.start();
    const bool workerCallCompleted{m_sut.callInEventLoop(
        [&]()
        {
            bool callExecuted{false};
            auto call{m_sut.prepareCallInEventLoop([&]() { callExecuted = true; })};
            callWasDeferredUntilWait = call && !callExecuted;
            call->wait();
            callExecutedAfterWait = callExecuted;
        })};

    ASSERT_TRUE(workerCallCompleted);
    EXPECT_TRUE(callWasDeferredUntilWait);
    EXPECT_TRUE(callExecutedAfterWait);
}

TEST_F(MessageQueueTests, ShouldPreservePreparedCallOrderDuringWorkerReentry)
{
    std::promise<void> workerEntered;
    std::promise<void> earlierCallPrepared;
    auto workerEnteredFuture{workerEntered.get_future()};
    auto earlierCallPreparedFuture{earlierCallPrepared.get_future()};
    std::vector<int> appliedValues;
    m_sut.start();
    auto workerCall{m_sut.prepareCallInEventLoop(
        [&]()
        {
            workerEntered.set_value();
            if (earlierCallPreparedFuture.wait_for(std::chrono::seconds{5}) != std::future_status::ready)
            {
                ADD_FAILURE() << "Earlier call was not prepared";
                return;
            }
            auto laterCall{m_sut.prepareCallInEventLoop([&]() { appliedValues.push_back(2); })};
            ASSERT_TRUE(laterCall);
            laterCall->wait();
        })};
    ASSERT_TRUE(workerCall);
    const auto workerStatus{workerEnteredFuture.wait_for(std::chrono::seconds{5})};
    if (workerStatus != std::future_status::ready)
    {
        earlierCallPrepared.set_value();
        workerCall->wait();
        FAIL() << "Worker did not enter the reentry barrier";
    }

    ASSERT_TRUE(m_sut.scheduleInEventLoop([&]() { appliedValues.push_back(0); }));
    auto earlierCall{m_sut.prepareCallInEventLoop([&]() { appliedValues.push_back(1); })};
    earlierCallPrepared.set_value();
    ASSERT_TRUE(earlierCall);
    workerCall->wait();
    earlierCall->wait();

    EXPECT_EQ((std::vector<int>{0, 1, 2}), appliedValues);
}

TEST_F(MessageQueueTests, ShouldDropTaskWhenCallInEventLoopIsCalledAfterStop)
{
    std::atomic_bool t1TaskExecuted{false};
    std::atomic_bool t2TaskExecuted{false};
    std::atomic_bool t3TaskExecuted{false};

    m_sut.start();

    // First thread queues very long task
    std::thread t1{[&]()
                   {
                       EXPECT_TRUE(m_sut.callInEventLoop(
                           [&]()
                           {
                               std::this_thread::sleep_for(std::chrono::milliseconds(300));
                               t1TaskExecuted = true;
                           }));
                   }};

    // Second thread queues stop while first task is handled
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::thread t2{[&]()
                   {
                       m_sut.stop();
                       t2TaskExecuted = true;
                   }};

    // Third thread queues a task after stop. This task should be rejected.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(m_sut.callInEventLoop([&]() { t3TaskExecuted = true; }));
    t1.join();
    t2.join();

    EXPECT_TRUE(t1TaskExecuted);
    EXPECT_TRUE(t2TaskExecuted);
    EXPECT_FALSE(t3TaskExecuted);
}

TEST_F(MessageQueueTests, StopCalledInWorkerThread)
{
    m_sut.start();

    EXPECT_TRUE(m_sut.callInEventLoop([&]() { m_sut.stop(); }));
}

TEST_F(MessageQueueTests, StopCalledInWorkerThreadWhenPreviousTaskIsRunning)
{
    std::mutex mtx;
    std::condition_variable cv;
    bool t1Started{false};
    std::atomic_bool t3TaskExecuted{false};

    m_sut.start();

    // First thread queues very long task
    std::thread t1{[&]()
                   {
                       EXPECT_TRUE(m_sut.callInEventLoop(
                           [&]()
                           {
                               {
                                   std::unique_lock<std::mutex> lock{mtx};
                                   t1Started = true;
                                   cv.notify_one();
                               }
                               std::this_thread::sleep_for(std::chrono::milliseconds(300));
                           }));
                   }};

    // Second thread queues stop while first task is handled
    {
        std::unique_lock<std::mutex> lock{mtx};
        cv.wait(lock, [&]() { return t1Started; });
    }
    EXPECT_TRUE(m_sut.scheduleInEventLoop([&]() { m_sut.stop(); }));
    m_sut.callInEventLoop([&]() { t3TaskExecuted = true; });

    t1.join();
    EXPECT_FALSE(t3TaskExecuted);
}
