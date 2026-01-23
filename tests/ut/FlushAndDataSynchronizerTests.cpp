/*
 * Copyright (C) 2026 Sky UK
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

#include "FlushAndDataSynchronizer.h"
#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>

namespace
{
constexpr int32_t kAudioSourceId{1};
constexpr int32_t kVideoSourceId{2};
} // namespace

class FlushAndDataSynchronizerTests : public testing::Test
{
public:
    FlushAndDataSynchronizerTests()
    {
        m_sut.addSource(kAudioSourceId);
        m_sut.addSource(kVideoSourceId);
    }
    ~FlushAndDataSynchronizerTests() override
    {
        m_sut.removeSource(kAudioSourceId);
        m_sut.removeSource(kVideoSourceId);
    }

protected:
    FlushAndDataSynchronizer m_sut;
};

TEST_F(FlushAndDataSynchronizerTests, ShouldAllowToFlushBothSources)
{
    m_sut.waitIfRequired(kAudioSourceId);
    m_sut.notifyFlushStarted(kAudioSourceId);
    m_sut.waitIfRequired(kVideoSourceId);
    m_sut.notifyFlushStarted(kVideoSourceId);
}

TEST_F(FlushAndDataSynchronizerTests, ShouldWaitForPreviousFlushCompletion)
{
    std::mutex mutex;
    std::condition_variable cv;
    bool waiting{false};
    bool waitFinished{false};

    m_sut.waitIfRequired(kAudioSourceId);
    m_sut.notifyFlushStarted(kAudioSourceId);

    std::thread waitingThread(
        [&]()
        {
            std::unique_lock lock(mutex);
            waiting = true;
            cv.notify_one();
            lock.unlock();
            m_sut.waitIfRequired(kAudioSourceId);
            waitFinished = true;
        });

    std::unique_lock lock(mutex);
    cv.wait(lock, [&waiting]() { return waiting; });
    EXPECT_FALSE(waitFinished);
    m_sut.notifyFlushCompleted(kAudioSourceId);
    waitingThread.join();
}

TEST_F(FlushAndDataSynchronizerTests, SecondReceivedDataShouldNotMessUpTheStateIfPushedEarlier)
{
    m_sut.waitIfRequired(kAudioSourceId);
    m_sut.notifyFlushStarted(kAudioSourceId);
    m_sut.notifyFlushCompleted(kAudioSourceId);
    m_sut.notifyDataReceived(kAudioSourceId);
    m_sut.notifyDataPushed(kAudioSourceId);
    m_sut.notifyDataReceived(kAudioSourceId); // second data received should be ignored
    m_sut.waitIfRequired(kAudioSourceId);     // should not block
}

TEST_F(FlushAndDataSynchronizerTests, ShouldWaitForDataPush)
{
    std::mutex mutex;
    std::condition_variable cv;
    bool waiting{false};
    bool waitFinished{false};

    m_sut.waitIfRequired(kAudioSourceId);
    m_sut.notifyFlushStarted(kAudioSourceId);
    m_sut.notifyFlushCompleted(kAudioSourceId);
    m_sut.notifyDataReceived(kAudioSourceId);

    std::thread waitingThread(
        [&]()
        {
            std::unique_lock lock(mutex);
            waiting = true;
            cv.notify_one();
            lock.unlock();
            m_sut.waitIfRequired(kAudioSourceId);
            waitFinished = true;
        });

    std::unique_lock lock(mutex);
    cv.wait(lock, [&waiting]() { return waiting; });
    EXPECT_FALSE(waitFinished);
    m_sut.notifyDataPushed(kAudioSourceId);
    waitingThread.join();
}

TEST_F(FlushAndDataSynchronizerTests, ShouldWaitForDataPushOfSecondSource)
{
    std::mutex mutex;
    std::condition_variable cv;
    bool waiting{false};
    bool waitFinished{false};

    m_sut.waitIfRequired(kAudioSourceId);
    m_sut.notifyFlushStarted(kAudioSourceId);

    m_sut.waitIfRequired(kVideoSourceId);
    m_sut.notifyFlushStarted(kVideoSourceId);

    m_sut.notifyFlushCompleted(kAudioSourceId);
    m_sut.notifyDataReceived(kAudioSourceId);

    m_sut.notifyFlushCompleted(kVideoSourceId);
    m_sut.notifyDataReceived(kVideoSourceId);

    m_sut.notifyDataPushed(kAudioSourceId);

    std::thread waitingThread(
        [&]()
        {
            std::unique_lock lock(mutex);
            waiting = true;
            cv.notify_one();
            lock.unlock();
            m_sut.waitIfRequired(kAudioSourceId);
            waitFinished = true;
        });

    std::unique_lock lock(mutex);
    cv.wait(lock, [&waiting]() { return waiting; });
    EXPECT_FALSE(waitFinished);
    m_sut.notifyDataPushed(kVideoSourceId);
    waitingThread.join();
}