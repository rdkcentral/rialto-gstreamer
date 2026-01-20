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

#include "FlushAndDataSynchronizer.h"
#include "GStreamerMSEMediaPlayerClient.h"
#include "GstreamerCatLog.h"
#include <algorithm>
#include <gst/gst.h>

#define GST_CAT_DEFAULT rialtoGStreamerCat

void FlushAndDataSynchronizer::addSource(int32_t sourceId)
{
    std::unique_lock lock(m_mutex);
    m_sourceStates[sourceId] = {FlushState::IDLE, DataState::NO_DATA};
    GST_INFO("Added source %d to FlushAndDataSynchronizer", sourceId);
}

void FlushAndDataSynchronizer::removeSource(int32_t sourceId)
{
    std::unique_lock lock(m_mutex);
    m_sourceStates.erase(sourceId);
    m_cv.notify_all();
    GST_INFO("Removed source %d from FlushAndDataSynchronizer", sourceId);
}

void FlushAndDataSynchronizer::notifyFlushStarted(int32_t sourceId)
{
    std::unique_lock lock(m_mutex);
    m_sourceStates[sourceId].flushState = FlushState::FLUSHING;
    m_sourceStates[sourceId].dataState = DataState::NO_DATA;
    GST_INFO("FlushAndDataSynchronizer: Flush started for source %d", sourceId);
}

void FlushAndDataSynchronizer::notifyFlushCompleted(int32_t sourceId)
{
    std::unique_lock lock(m_mutex);
    m_sourceStates[sourceId].flushState = FlushState::FLUSHED;
    m_cv.notify_all();
    GST_INFO("FlushAndDataSynchronizer: Flush completed for source %d", sourceId);
}

void FlushAndDataSynchronizer::notifyDataReceived(int32_t sourceId)
{
    std::unique_lock lock(m_mutex);
    if (m_sourceStates[sourceId].dataState == DataState::NO_DATA)
    {
        m_sourceStates[sourceId].dataState = DataState::DATA_RECEIVED;
        GST_INFO("FlushAndDataSynchronizer: Data received for source %d", sourceId);
    }
}

void FlushAndDataSynchronizer::notifyDataPushed(int32_t sourceId)
{
    std::unique_lock lock(m_mutex);
    m_sourceStates[sourceId].dataState = DataState::DATA_PUSHED;
    m_sourceStates[sourceId].flushState = FlushState::IDLE;
    m_cv.notify_all();
    GST_INFO("FlushAndDataSynchronizer: Data pushed for source %d", sourceId);
}

void FlushAndDataSynchronizer::waitIfRequired(int32_t sourceId)
{
    std::unique_lock lock(m_mutex);
    GST_INFO("FlushAndDataSynchronizer: waitIfRequired enter for source %d", sourceId);
    m_cv.wait(lock,
              [this]()
              {
                  return std::none_of(m_sourceStates.begin(), m_sourceStates.end(),
                                      [](const auto &state)
                                      {
                                          return state.second.flushState == FlushState::FLUSHING ||
                                                 (state.second.flushState == FlushState::FLUSHED &&
                                                  state.second.dataState == DataState::DATA_RECEIVED);
                                      });
              });
    GST_INFO("FlushAndDataSynchronizer: waitIfRequired exit for source %d", sourceId);
}
