/*
 * Copyright (C) 2025 Sky UK
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

#include "IPlaybackDelegate.h"
#include <gmock/gmock.h>

class PlaybackDelegateMock : public IPlaybackDelegate
{
public:
    MOCK_METHOD(void, setSourceId, (int32_t sourceId), (override));
    MOCK_METHOD(void, handleEos, (), (override));
    MOCK_METHOD(void, handleFlushCompleted, (), (override));
    MOCK_METHOD(void, handleStateChanged, (firebolt::rialto::PlaybackState state), (override));
    MOCK_METHOD(void, handleError, (const char *message, gint code), (override));
    MOCK_METHOD(void, handleQos, (uint64_t processed, uint64_t dropped), (const, override));
    MOCK_METHOD(GstStateChangeReturn, changeState, (GstStateChange transition), (override));
    MOCK_METHOD(void, postAsyncStart, (), (override));
    MOCK_METHOD(void, setProperty, (const Property &type, const GValue *value), (override));
    MOCK_METHOD(void, getProperty, (const Property &type, GValue *value), (override));
    MOCK_METHOD(std::optional<gboolean>, handleQuery, (GstQuery * query), (const, override));
    MOCK_METHOD(gboolean, handleSendEvent, (GstEvent * event), (override));
    MOCK_METHOD(gboolean, handleEvent, (GstPad * pad, GstObject *parent, GstEvent *event), (override));
    MOCK_METHOD(GstFlowReturn, handleBuffer, (GstBuffer * buffer), (override));
    MOCK_METHOD(GstRefSample, getFrontSample, (), (const, override));
    MOCK_METHOD(void, popSample, (), (override));
    MOCK_METHOD(bool, isEos, (), (const, override));
    MOCK_METHOD(void, lostState, (), (override));
    MOCK_METHOD(bool, attachToMediaClientAndSetStreamsNumber,
                (const uint32_t maxVideoWidth, const uint32_t maxVideoHeight), (override));
};
