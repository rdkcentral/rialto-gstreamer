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

#ifndef TIMER_FACTORY_MOCK_H_
#define TIMER_FACTORY_MOCK_H_

#include "ITimer.h"
#include <gmock/gmock.h>
#include <memory>

class TimerFactoryMock : public ITimerFactory
{
public:
    TimerFactoryMock() = default;
    virtual ~TimerFactoryMock() = default;
    MOCK_METHOD(std::unique_ptr<ITimer>, createTimer,
                (const std::chrono::milliseconds &timeout, const std::function<void()> &callback, TimerType timerType),
                (const, override));
};

#endif // TIMER_FACTORY_MOCK_H_
