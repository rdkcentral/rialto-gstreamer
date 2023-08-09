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

#include "RialtoGstTest.h"

class GstreamerMseAudioSinkTests : public RialtoGstTest
{
public:
    GstreamerMseAudioSinkTests() = default;
    ~GstreamerMseAudioSinkTests() override = default;
};

TEST_F(GstreamerMseAudioSinkTests, ShouldReachPausedState)
{
    RialtoMSEBaseSink *audioSink = createAudioSink();
    GstElement *pipeline = createPipelineWithSink(audioSink);

    setPausedState(pipeline, audioSink);

    GstCaps *caps{gst_caps_new_simple("audio/mpeg", "mpegversion", G_TYPE_INT, 4, nullptr)};
    setCaps(audioSink, caps);

    sendPlaybackStateNotification(audioSink, firebolt::rialto::PlaybackState::PAUSED);

    const auto kReceivedMessages{getMessages(pipeline)};
    EXPECT_TRUE(kReceivedMessages.contains(GST_MESSAGE_STATE_CHANGED));
    EXPECT_TRUE(kReceivedMessages.contains(GST_MESSAGE_ASYNC_DONE));

    setNullState(pipeline);

    gst_caps_unref(caps);
    gst_object_unref(pipeline);
}
