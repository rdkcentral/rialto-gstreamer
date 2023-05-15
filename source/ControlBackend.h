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

#include "ControlBackendInterface.h"

#include <IControl.h>

#include <condition_variable>
#include <gst/gst.h>
#include <mutex>

#include <memory>

namespace firebolt::rialto::client
{
class ControlBackend final : public ControlBackendInterface
{
    class ControlClient : public IControlClient
    {
    public:
        ControlClient(ControlBackend &backend) : mBackend{backend} {}
        ~ControlClient() override = default;
        void notifyApplicationState(ApplicationState state) override
        {
            GST_INFO("ApplicationStateChanged received by rialto sink");
            mBackend.onApplicationStateChanged(state);
        }

    private:
        ControlBackend &mBackend;
    };

public:
    ControlBackend() : mRialtoClientState{ApplicationState::UNKNOWN}
    {
        mControlClient = std::make_shared<ControlClient>(*this);
        if (!mControlClient)
        {
            GST_ERROR("Unable to create control client");
            return;
        }
        mControl = IControlFactory::createFactory()->createControl();
        if (!mControl)
        {
            GST_ERROR("Unable to create control");
            return;
        }
        if (!mControl->registerClient(mControlClient, mRialtoClientState))
        {
            GST_ERROR("Unable to register client");
            return;
        }
    }

    ~ControlBackend() final { removeControlBackend(); }

    void removeControlBackend() override { mControl.reset(); }

    bool waitForRunning() override
    {
        std::unique_lock<std::mutex> lock{mMutex};
        if (ApplicationState::RUNNING == mRialtoClientState)
        {
            return true;
        }
        mStateCv.wait_for(lock, std::chrono::seconds{1},
                          [&]() { return mRialtoClientState == ApplicationState::RUNNING; });
        return ApplicationState::RUNNING == mRialtoClientState;
    }

private:
    void onApplicationStateChanged(ApplicationState state)
    {
        GST_INFO("Rialto Client application state changed to: %s",
                 state == ApplicationState::RUNNING ? "Active" : "Inactive/Unknown");
        std::unique_lock<std::mutex> lock{mMutex};
        mRialtoClientState = state;
        mStateCv.notify_one();
    }

private:
    ApplicationState mRialtoClientState;
    std::shared_ptr<ControlClient> mControlClient;
    std::shared_ptr<IControl> mControl;
    std::mutex mMutex;
    std::condition_variable mStateCv;
};
} // namespace firebolt::rialto::client
