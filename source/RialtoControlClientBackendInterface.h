/*
 * Copyright (C) 2022 Sky UK
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

#pragma once

#include <IRialtoControl.h>

#include <string>

namespace firebolt::rialto::client
{
class RialtoControlClientBackendInterface
{
public:
    virtual ~RialtoControlClientBackendInterface() = default;
    virtual void getRialtoControlBackend() = 0;
    virtual void removeRialtoControlBackend() = 0;
    virtual bool isRialtoControlBackendCreated() const = 0;
    virtual bool setApplicationState(ApplicationState state) = 0;
};
} // namespace firebolt::rialto::client
