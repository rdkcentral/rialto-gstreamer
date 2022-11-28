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

#include "RialtoControlClientBackendInterface.h"
#include <IRialtoControl.h>
#include <gst/gst.h>

#include <memory>

namespace firebolt::rialto::client
{
class RialtoControlClientBackend final : public RialtoControlClientBackendInterface
{
public:
    RialtoControlClientBackend() {}
    ~RialtoControlClientBackend() final { removeRialtoControlBackend(); }

    void getRialtoControlBackend() override
    {
        mRialtoControl = firebolt::rialto::IRialtoControlFactory::createFactory()->getRialtoControl();
        if (!mRialtoControl)
        {
            GST_ERROR("Could not create rialto control");
            return;
        }
    }

    void removeRialtoControlBackend() override { mRialtoControl.reset(); }

    bool isRialtoControlBackendCreated() const override { return static_cast<bool>(mRialtoControl); }

    bool setApplicationState(ApplicationState state) override { return mRialtoControl->setApplicationState(state); }

private:
    std::shared_ptr<IRialtoControl> mRialtoControl;
};
} // namespace firebolt::rialto::client
