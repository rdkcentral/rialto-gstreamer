/*
 * Copyright (C) 2024 Sky UK
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

#ifndef FIREBOLT_RIALTO_CLIENT_CONTROL_LOG_MOCK_H_
#define FIREBOLT_RIALTO_CLIENT_CONTROL_LOG_MOCK_H_

#include "IClientLogControl.h"
#include <gmock/gmock.h>
#include <memory>

namespace firebolt::rialto
{
class ClientLogControlFactoryMock : public IClientLogControlFactory
{
public:
    MOCK_METHOD(IClientLogControl &, createClientLogControl, (), (override));
};

class ClientLogControlMock : public IClientLogControl
{
public:
    MOCK_METHOD(bool, registerLogHandler, (std::shared_ptr<IClientLogHandler> & handler, bool ignoreLogLevels),
                (override));
};
} // namespace firebolt::rialto

#endif // FIREBOLT_RIALTO_CLIENT_CONTROL_LOG_MOCK_H_
