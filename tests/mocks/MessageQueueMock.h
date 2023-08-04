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

#include "IMessageQueue.h"
#include <gmock/gmock.h>

class MessageQueueMock : public IMessageQueue
{
public:
    MOCK_METHOD(void, start, (), (override));
    MOCK_METHOD(void, stop, (), (override));
    MOCK_METHOD(void, clear, (), (override));
    MOCK_METHOD(std::shared_ptr<Message>, waitForMessage, (), (override));
    MOCK_METHOD(bool, postMessage, (const std::shared_ptr<Message> &msg), (override));
    MOCK_METHOD(void, processMessages, (), (override));
    MOCK_METHOD(bool, callInEventLoop, (const std::function<void()> &func), (override));
};
