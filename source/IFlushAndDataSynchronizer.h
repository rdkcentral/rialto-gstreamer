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

#ifndef IFLUSH_AND_DATA_SYNCHRONIZER_H_
#define IFLUSH_AND_DATA_SYNCHRONIZER_H_

#include <cstdint>

class IFlushAndDataSynchronizer
{
public:
    virtual ~IFlushAndDataSynchronizer() = default;

    virtual void addSource(int32_t sourceId) = 0;
    virtual void removeSource(int32_t sourceId) = 0;
    virtual void notifyFlushStarted(int32_t sourceId) = 0;
    virtual void notifyFlushCompleted(int32_t sourceId) = 0;
    virtual void notifyDataReceived(int32_t sourceId) = 0;
    virtual void notifyDataPushed(int32_t sourceId) = 0;
    virtual void waitIfRequired(int32_t sourceId) = 0;
    virtual bool isAnySourceFlushing() const = 0;
};

#endif // IFLUSH_AND_DATA_SYNCHRONIZER_H_