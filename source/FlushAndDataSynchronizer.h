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

#ifndef FLUSH_AND_DATA_SYNCHRONIZER_H_
#define FLUSH_AND_DATA_SYNCHRONIZER_H_

#include "IFlushAndDataSynchronizer.h"
#include <condition_variable>
#include <map>
#include <mutex>
#include <vector>

class FlushAndDataSynchronizer : public IFlushAndDataSynchronizer
{
    enum class FlushState
    {
        IDLE,
        FLUSHING,
        FLUSHED
    };

    enum class DataState
    {
        NO_DATA,
        DATA_RECEIVED,
        DATA_PUSHED
    };

    struct SourceState
    {
        FlushState flushState;
        DataState dataState;
    };

    struct FlushData
    {
        int32_t sourceId;
        bool resetTime;
    };

public:
    FlushAndDataSynchronizer() = default;
    ~FlushAndDataSynchronizer() override = default;

    void addSource(int32_t sourceId) override;
    void removeSource(int32_t sourceId) override;
    void notifyFlushStarted(int32_t sourceId) override;
    void notifyFlushCompleted(int32_t sourceId) override;
    void notifyDataReceived(int32_t sourceId) override;
    void notifyDataPushed(int32_t sourceId) override;
    void waitIfRequired(int32_t sourceId) override;

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::map<int32_t, SourceState> m_sourceStates;
};

#endif // FLUSH_AND_DATA_SYNCHRONIZER_H_