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

#include "ITimer.h"
#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>

TEST(TimerTests, ShouldTimeoutOneShotTimer)
{
    std::mutex mtx;
    std::condition_variable cv;
    std::unique_lock<std::mutex> lock{mtx};
    bool callFlag{false};
    std::unique_ptr<ITimer> timer{ITimerFactory::getFactory()->createTimer(std::chrono::milliseconds{100},
                                                                           [&]()
                                                                           {
                                                                               std::unique_lock<std::mutex> lock{mtx};
                                                                               callFlag = true;
                                                                               cv.notify_one();
                                                                           })};
    EXPECT_TRUE(timer->isActive());
    cv.wait_for(lock, std::chrono::milliseconds{110}, [&]() { return callFlag; });
    EXPECT_TRUE(callFlag);
}

TEST(TimerTests, ShouldCancelTimer)
{
    std::atomic_bool callFlag{false};
    std::unique_ptr<ITimer> timer{
        ITimerFactory::getFactory()->createTimer(std::chrono::milliseconds{100}, [&]() { callFlag = true; })};
    EXPECT_TRUE(timer->isActive());
    timer->cancel();
    EXPECT_FALSE(timer->isActive());
    EXPECT_FALSE(callFlag);
}

TEST(TimerTests, ShouldTimeoutPeriodicTimer)
{
    std::mutex mtx;
    std::condition_variable cv;
    std::unique_lock<std::mutex> lock{mtx};
    unsigned callCounter{0};
    std::unique_ptr<ITimer> timer{ITimerFactory::getFactory()->createTimer(
        std::chrono::milliseconds{30},
        [&]()
        {
            std::unique_lock<std::mutex> lock{mtx};
            ++callCounter;
            cv.notify_one();
        },
        TimerType::PERIODIC)};
    EXPECT_TRUE(timer->isActive());
    cv.wait_for(lock, std::chrono::milliseconds{110}, [&]() { return callCounter >= 3; });
    EXPECT_TRUE(callCounter >= 3);
}
