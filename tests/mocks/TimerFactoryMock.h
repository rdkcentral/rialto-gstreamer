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
