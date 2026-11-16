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

#include "FaultInjection.h"
#include "RialtoGstTest.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <vector>

using metricscli::FaultContext;
using metricscli::FaultParameters;
using metricscli::FaultScenario;
using metricscli::FaultScenarioInfo;

namespace
{
constexpr gint64 kSettledPosition{10 * GST_SECOND};
constexpr gint64 kUnsettledPosition{1 * GST_SECOND};

struct PadDeleter
{
    void operator()(GstPad *pad) const { gst_object_unref(pad); }
};
using PadPtr = std::unique_ptr<GstPad, PadDeleter>;

struct BufferDeleter
{
    void operator()(GstBuffer *buffer) const { gst_buffer_unref(buffer); }
};
using BufferPtr = std::unique_ptr<GstBuffer, BufferDeleter>;

// A source pad that holds the sticky events (stream-start, caps, segment) a real video sink pad would.
PadPtr createVideoPad(const char *capsString)
{
    PadPtr pad{gst_pad_new("src", GST_PAD_SRC)};
    gst_pad_set_active(pad.get(), TRUE);
    gst_pad_push_event(pad.get(), gst_event_new_stream_start("fault-injection-test"));
    if (capsString)
    {
        GstCaps *caps{gst_caps_from_string(capsString)};
        gst_pad_push_event(pad.get(), gst_event_new_caps(caps));
        gst_caps_unref(caps);
    }
    GstSegment segment;
    gst_segment_init(&segment, GST_FORMAT_TIME);
    gst_pad_push_event(pad.get(), gst_event_new_segment(&segment));
    return pad;
}

GstBuffer *createBuffer(const std::vector<guint8> &data, GstClockTime pts = GST_CLOCK_TIME_NONE,
                        GstClockTime dts = GST_CLOCK_TIME_NONE, GstClockTime duration = GST_CLOCK_TIME_NONE)
{
    GstBuffer *buffer{gst_buffer_new_allocate(nullptr, data.size(), nullptr)};
    gst_buffer_fill(buffer, 0, data.data(), data.size());
    GST_BUFFER_PTS(buffer) = pts;
    GST_BUFFER_DTS(buffer) = dts;
    GST_BUFFER_DURATION(buffer) = duration;
    return buffer;
}

std::vector<guint8> readBytes(GstBuffer *buffer)
{
    GstMapInfo map;
    EXPECT_TRUE(gst_buffer_map(buffer, &map, GST_MAP_READ));
    std::vector<guint8> bytes(map.data, map.data + map.size);
    gst_buffer_unmap(buffer, &map);
    return bytes;
}

struct HandlerResult
{
    GstPadProbeReturn result;
    bool applied;
    BufferPtr buffer; // The buffer the handler left in the probe info (possibly a writable copy)
};

HandlerResult runHandler(const char *scenario, GstPad *pad, GstBuffer *buffer, FaultContext &context)
{
    const FaultScenarioInfo *info{metricscli::findFaultScenario(scenario)};
    EXPECT_NE(info, nullptr);
    GstPadProbeInfo probeInfo{};
    probeInfo.type = GST_PAD_PROBE_TYPE_BUFFER;
    probeInfo.data = buffer;
    bool applied{false};
    const GstPadProbeReturn result{info->handler(pad, &probeInfo, context, 0, applied)};
    return {result, applied, BufferPtr{GST_BUFFER_CAST(probeInfo.data)}};
}

std::optional<gint64> positionAt(gint64 position)
{
    return position;
}

FaultContext::PositionQuery settledPosition()
{
    return []() { return positionAt(kSettledPosition); };
}

FaultContext::PositionQuery unsettledPosition()
{
    return []() { return positionAt(kUnsettledPosition); };
}

FaultContext::PositionQuery missingPosition()
{
    return []() -> std::optional<gint64> { return std::nullopt; };
}

constexpr guint8 kFill{0xAA}; // Never forms a 00 00 01 start code, and is changed by inversion

// 00 00 00 01 | SPS (6 bytes) | 00 00 00 01 | slice (header byte + 40 bytes)
std::vector<guint8> createH264ByteStream(guint8 sliceHeader)
{
    std::vector<guint8> data{0, 0, 0, 1, 0x67, kFill, kFill, kFill, kFill, kFill, 0, 0, 0, 1, sliceHeader};
    data.insert(data.end(), 40, kFill);
    return data;
}

constexpr std::size_t kH264SpsOffset{4};
constexpr std::size_t kH264SpsSize{6};
constexpr std::size_t kH264SliceOffset{14};
constexpr std::size_t kH264SliceSize{41};
// The second half of the slice payload is inverted; the header byte and first half stay intact.
constexpr std::size_t kH264CorruptFrom{kH264SliceOffset + 1 + (kH264SliceSize - 1) / 2};

std::atomic<int> g_handlerCalls{0};

GstPadProbeReturn countingHandler(GstPad *, GstPadProbeInfo *, FaultContext &, std::uint32_t, bool &applied)
{
    ++g_handlerCalls;
    applied = true;
    return GST_PAD_PROBE_OK;
}

GstPadProbeReturn failingHandler(GstPad *, GstPadProbeInfo *, FaultContext &context, std::uint32_t, bool &)
{
    ++g_handlerCalls;
    context.fail("handler failed");
    return GST_PAD_PROBE_PASS;
}

// A bare element exposing the "sink" pad FaultInjector attaches to.
class TestSinkElement
{
public:
    explicit TestSinkElement(bool withSinkPad = true) : m_element{gst_bin_new("fault-test-sink")}
    {
        if (withSinkPad)
        {
            GstPad *pad{gst_pad_new("sink", GST_PAD_SINK)};
            gst_pad_set_chain_function(pad,
                                       [](GstPad *, GstObject *, GstBuffer *buffer)
                                       {
                                           gst_buffer_unref(buffer);
                                           return GST_FLOW_OK;
                                       });
            gst_pad_set_active(pad, TRUE);
            gst_element_add_pad(m_element, pad);
        }
    }
    ~TestSinkElement() { gst_object_unref(m_element); }
    TestSinkElement(const TestSinkElement &) = delete;
    TestSinkElement &operator=(const TestSinkElement &) = delete;

    GstElement *element() const { return m_element; }

    void push(const std::vector<guint8> &data = {0})
    {
        GstPad *pad{gst_element_get_static_pad(m_element, "sink")};
        ASSERT_NE(pad, nullptr);
        EXPECT_EQ(gst_pad_chain(pad, createBuffer(data)), GST_FLOW_OK);
        gst_object_unref(pad);
    }

private:
    GstElement *m_element;
};

FaultScenarioInfo testScenario(metricscli::FaultHandler handler)
{
    return {FaultScenario::FrameDrop, "test-scenario", handler, metricscli::detail::bufferCountLimit};
}
} // namespace

class FaultInjectionTests : public RialtoGstTest
{
protected:
    FaultParameters m_parameters;
};

TEST_F(FaultInjectionTests, ShouldRegisterAllScenarios)
{
    for (const char *name : {"frame-drop", "frame-repeat", "buffer-underflow", "decode-error"})
    {
        const FaultScenarioInfo *info{metricscli::findFaultScenario(name)};
        ASSERT_NE(info, nullptr) << name;
        EXPECT_NE(info->handler, nullptr) << name;
        EXPECT_NE(metricscli::faultScenarioNames().find(name), std::string::npos) << name;
    }
    EXPECT_EQ(metricscli::findFaultScenario("decoder-error"), nullptr);
    EXPECT_EQ(metricscli::findFaultScenario("unknown"), nullptr);
}

TEST_F(FaultInjectionTests, ShouldMoveFrameDropBuffersBehindThePlaybackPosition)
{
    PadPtr pad{createVideoPad("video/x-h264, stream-format=byte-stream")};
    FaultContext context{m_parameters, nullptr, settledPosition()};

    // Position is 10 s and the margin is 1 s, so the first buffer (10 s) moves back by 1 s.
    auto first{runHandler("frame-drop", pad.get(), createBuffer({0}, 10 * GST_SECOND, 9900 * GST_MSECOND), context)};
    EXPECT_TRUE(first.applied);
    EXPECT_EQ(GST_BUFFER_PTS(first.buffer.get()), 9 * GST_SECOND);
    EXPECT_EQ(GST_BUFFER_DTS(first.buffer.get()), 8900 * GST_MSECOND);

    // The same shift is applied to following buffers so their spacing is preserved.
    auto second{runHandler("frame-drop", pad.get(), createBuffer({0}, 10040 * GST_MSECOND), context)};
    EXPECT_TRUE(second.applied);
    EXPECT_EQ(GST_BUFFER_PTS(second.buffer.get()), 9040 * GST_MSECOND);
    EXPECT_FALSE(context.failed());
}

TEST_F(FaultInjectionTests, ShouldNotShiftFrameDropBuffersThatAreAlreadyLate)
{
    PadPtr pad{createVideoPad("video/x-h264, stream-format=byte-stream")};
    FaultContext context{m_parameters, nullptr, settledPosition()};

    auto result{runHandler("frame-drop", pad.get(), createBuffer({0}, 5 * GST_SECOND), context)};
    EXPECT_TRUE(result.applied);
    EXPECT_EQ(GST_BUFFER_PTS(result.buffer.get()), 5 * GST_SECOND);
}

TEST_F(FaultInjectionTests, ShouldPassFrameDropBuffersThroughUntilPlaybackSettles)
{
    PadPtr pad{createVideoPad("video/x-h264, stream-format=byte-stream")};
    FaultContext context{m_parameters, nullptr, unsettledPosition()};

    auto result{runHandler("frame-drop", pad.get(), createBuffer({0}, 10 * GST_SECOND), context)};
    EXPECT_FALSE(result.applied);
    EXPECT_FALSE(context.failed());
    EXPECT_EQ(GST_BUFFER_PTS(result.buffer.get()), 10 * GST_SECOND);
}

TEST_F(FaultInjectionTests, ShouldPassFrameDropBuffersWithoutTimestampsThrough)
{
    PadPtr pad{createVideoPad("video/x-h264, stream-format=byte-stream")};
    FaultContext context{m_parameters, nullptr, settledPosition()};

    auto result{runHandler("frame-drop", pad.get(), createBuffer({0}), context)};
    EXPECT_FALSE(result.applied);
    EXPECT_FALSE(context.failed());
}

TEST_F(FaultInjectionTests, ShouldFailFrameDropWhenPositionIsUnavailable)
{
    PadPtr pad{createVideoPad("video/x-h264, stream-format=byte-stream")};
    FaultContext context{m_parameters, nullptr, missingPosition()};

    auto result{runHandler("frame-drop", pad.get(), createBuffer({0}, 10 * GST_SECOND), context)};
    EXPECT_FALSE(result.applied);
    ASSERT_TRUE(context.failed());
    EXPECT_NE(context.failure().find("playback position"), std::string::npos);
}

TEST_F(FaultInjectionTests, ShouldFailFrameDropWhenNoSegmentWasReceived)
{
    PadPtr pad{gst_pad_new("src", GST_PAD_SRC)};
    gst_pad_set_active(pad.get(), TRUE);
    FaultContext context{m_parameters, nullptr, settledPosition()};

    auto result{runHandler("frame-drop", pad.get(), createBuffer({0}, 10 * GST_SECOND), context)};
    EXPECT_FALSE(result.applied);
    ASSERT_TRUE(context.failed());
    EXPECT_NE(context.failure().find("segment"), std::string::npos);
}

TEST_F(FaultInjectionTests, ShouldHoldFrameRepeatBufferForConfiguredFrameDurations)
{
    // 1000 fps gives a 1 ms frame duration, so a 3 frame delay is 3 ms.
    PadPtr pad{createVideoPad("video/x-h264, framerate=1000/1")};
    m_parameters.delayFrames = 3;
    FaultContext context{m_parameters, nullptr, settledPosition()};

    const auto start{std::chrono::steady_clock::now()};
    auto result{runHandler("frame-repeat", pad.get(), createBuffer({0}, 10 * GST_SECOND), context)};
    const auto elapsed{std::chrono::steady_clock::now() - start};

    EXPECT_TRUE(result.applied);
    ASSERT_TRUE(context.appliedDelay.has_value());
    EXPECT_EQ(*context.appliedDelay, 3 * GST_MSECOND);
    EXPECT_GE(elapsed, std::chrono::milliseconds(3));
    EXPECT_EQ(GST_BUFFER_PTS(result.buffer.get()), 10 * GST_SECOND); // The buffer itself is unchanged
}

TEST_F(FaultInjectionTests, ShouldUseBufferDurationWhenFramerateIsMissing)
{
    PadPtr pad{createVideoPad("video/x-h264")};
    m_parameters.delayFrames = 2;
    FaultContext context{m_parameters, nullptr, settledPosition()};

    auto result{runHandler("frame-repeat", pad.get(), createBuffer({0}, 10 * GST_SECOND, GST_CLOCK_TIME_NONE, GST_MSECOND),
                           context)};
    EXPECT_TRUE(result.applied);
    ASSERT_TRUE(context.appliedDelay.has_value());
    EXPECT_EQ(*context.appliedDelay, 2 * GST_MSECOND);
}

TEST_F(FaultInjectionTests, ShouldFailFrameRepeatWhenFrameDurationIsUnknown)
{
    PadPtr pad{createVideoPad("video/x-h264")};
    FaultContext context{m_parameters, nullptr, settledPosition()};

    auto result{runHandler("frame-repeat", pad.get(), createBuffer({0}, 10 * GST_SECOND), context)};
    EXPECT_FALSE(result.applied);
    ASSERT_TRUE(context.failed());
    EXPECT_NE(context.failure().find("frame duration"), std::string::npos);
}

TEST_F(FaultInjectionTests, ShouldFailFrameRepeatWhenDelayDoesNotFitInTheWindow)
{
    PadPtr pad{createVideoPad("video/x-h264, framerate=1/10")}; // 10 s frames, 3 frames = 30 s
    m_parameters.windowSeconds = 10;
    FaultContext context{m_parameters, nullptr, settledPosition()};

    auto result{runHandler("frame-repeat", pad.get(), createBuffer({0}, 10 * GST_SECOND), context)};
    EXPECT_FALSE(result.applied);
    ASSERT_TRUE(context.failed());
    EXPECT_NE(context.failure().find("window"), std::string::npos);
}

TEST_F(FaultInjectionTests, ShouldPassFrameRepeatBufferThroughUntilPlaybackSettles)
{
    PadPtr pad{createVideoPad("video/x-h264, framerate=1000/1")};
    FaultContext context{m_parameters, nullptr, unsettledPosition()};

    auto result{runHandler("frame-repeat", pad.get(), createBuffer({0}, 10 * GST_SECOND), context)};
    EXPECT_FALSE(result.applied);
    EXPECT_FALSE(context.failed());
    EXPECT_FALSE(context.appliedDelay.has_value());
}

TEST_F(FaultInjectionTests, ShouldFailFrameRepeatWhenPositionIsUnavailable)
{
    PadPtr pad{createVideoPad("video/x-h264, framerate=1000/1")};
    FaultContext context{m_parameters, nullptr, missingPosition()};

    auto result{runHandler("frame-repeat", pad.get(), createBuffer({0}, 10 * GST_SECOND), context)};
    EXPECT_FALSE(result.applied);
    ASSERT_TRUE(context.failed());
    EXPECT_NE(context.failure().find("playback position"), std::string::npos);
}

TEST_F(FaultInjectionTests, ShouldBlockBufferUnderflowForConfiguredStall)
{
    PadPtr pad{createVideoPad("video/x-h264")};
    m_parameters.stallMs = 30;
    FaultContext context{m_parameters, nullptr, settledPosition()};

    const auto start{std::chrono::steady_clock::now()};
    auto result{runHandler("buffer-underflow", pad.get(), createBuffer({0}, 10 * GST_SECOND), context)};
    const auto elapsed{std::chrono::steady_clock::now() - start};

    EXPECT_TRUE(result.applied);
    EXPECT_GE(elapsed, std::chrono::milliseconds(30));
    ASSERT_TRUE(context.appliedStallMs.has_value());
    EXPECT_EQ(*context.appliedStallMs, 30u);
}

TEST_F(FaultInjectionTests, ShouldAbandonBufferUnderflowStallWhenCancelled)
{
    PadPtr pad{createVideoPad("video/x-h264")};
    m_parameters.stallMs = 5000;
    m_parameters.windowSeconds = 10;
    FaultContext context{m_parameters, nullptr, settledPosition()};
    context.cancel();

    const auto start{std::chrono::steady_clock::now()};
    auto result{runHandler("buffer-underflow", pad.get(), createBuffer({0}, 10 * GST_SECOND), context)};
    EXPECT_TRUE(result.applied);
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(1));
}

TEST_F(FaultInjectionTests, ShouldFailBufferUnderflowWhenStallDoesNotFitInTheWindow)
{
    PadPtr pad{createVideoPad("video/x-h264")};
    m_parameters.stallMs = 20000;
    m_parameters.windowSeconds = 10;
    FaultContext context{m_parameters, nullptr, settledPosition()};

    auto result{runHandler("buffer-underflow", pad.get(), createBuffer({0}, 10 * GST_SECOND), context)};
    EXPECT_FALSE(result.applied);
    ASSERT_TRUE(context.failed());
    EXPECT_NE(context.failure().find("window"), std::string::npos);
}

TEST_F(FaultInjectionTests, ShouldPassBufferUnderflowBufferThroughUntilPlaybackSettles)
{
    PadPtr pad{createVideoPad("video/x-h264")};
    FaultContext context{m_parameters, nullptr, unsettledPosition()};

    auto result{runHandler("buffer-underflow", pad.get(), createBuffer({0}, 10 * GST_SECOND), context)};
    EXPECT_FALSE(result.applied);
    EXPECT_FALSE(context.failed());
}

TEST_F(FaultInjectionTests, ShouldFailBufferUnderflowWhenPositionIsUnavailable)
{
    PadPtr pad{createVideoPad("video/x-h264")};
    FaultContext context{m_parameters, nullptr, missingPosition()};

    auto result{runHandler("buffer-underflow", pad.get(), createBuffer({0}, 10 * GST_SECOND), context)};
    EXPECT_FALSE(result.applied);
    ASSERT_TRUE(context.failed());
    EXPECT_NE(context.failure().find("playback position"), std::string::npos);
}

TEST_F(FaultInjectionTests, ShouldCorruptOnlySliceDataOfH264ByteStream)
{
    PadPtr pad{createVideoPad("video/x-h264, stream-format=byte-stream")};
    FaultContext context{m_parameters, nullptr, settledPosition()};
    const std::vector<guint8> original{createH264ByteStream(0x41)}; // non-IDR slice

    auto result{runHandler("decode-error", pad.get(), createBuffer(original), context)};
    EXPECT_TRUE(result.applied);
    EXPECT_FALSE(context.failed());
    EXPECT_EQ(context.corruptedSlices.load(), 1u);

    const std::vector<guint8> corrupted{readBytes(result.buffer.get())};
    ASSERT_EQ(corrupted.size(), original.size());
    // Start codes, the SPS, the slice NAL header and the first half of the slice are untouched.
    for (std::size_t i = 0; i < kH264CorruptFrom; ++i)
    {
        EXPECT_EQ(corrupted[i], original[i]) << "byte " << i;
    }
    for (std::size_t i = kH264CorruptFrom; i < original.size(); ++i)
    {
        EXPECT_EQ(corrupted[i], static_cast<guint8>(original[i] ^ 0xFF)) << "byte " << i;
    }
    EXPECT_EQ(corrupted[kH264SpsOffset], 0x67);
    EXPECT_EQ(kH264SpsOffset + kH264SpsSize + 4, kH264SliceOffset);
}

TEST_F(FaultInjectionTests, ShouldNotCorruptBuffersContainingAKeyFrame)
{
    PadPtr pad{createVideoPad("video/x-h264, stream-format=byte-stream")};
    FaultContext context{m_parameters, nullptr, settledPosition()};
    const std::vector<guint8> original{createH264ByteStream(0x65)}; // IDR slice

    auto result{runHandler("decode-error", pad.get(), createBuffer(original), context)};
    EXPECT_FALSE(result.applied);
    EXPECT_FALSE(context.failed());
    EXPECT_EQ(readBytes(result.buffer.get()), original);
}

TEST_F(FaultInjectionTests, ShouldCorruptLengthPrefixedH264Slices)
{
    PadPtr pad{createVideoPad("video/x-h264, stream-format=avc")}; // No codec_data: 4 byte lengths
    FaultContext context{m_parameters, nullptr, settledPosition()};

    std::vector<guint8> original{0, 0, 0, 41, 0x41};
    original.insert(original.end(), 40, kFill);

    auto result{runHandler("decode-error", pad.get(), createBuffer(original), context)};
    EXPECT_TRUE(result.applied);
    EXPECT_EQ(context.corruptedSlices.load(), 1u);

    const std::vector<guint8> corrupted{readBytes(result.buffer.get())};
    constexpr std::size_t kSliceOffset{4};
    constexpr std::size_t kCorruptFrom{kSliceOffset + 1 + (41 - 1) / 2};
    for (std::size_t i = 0; i < kCorruptFrom; ++i)
    {
        EXPECT_EQ(corrupted[i], original[i]) << "byte " << i;
    }
    for (std::size_t i = kCorruptFrom; i < original.size(); ++i)
    {
        EXPECT_EQ(corrupted[i], static_cast<guint8>(original[i] ^ 0xFF)) << "byte " << i;
    }
}

TEST_F(FaultInjectionTests, ShouldCorruptH265Slices)
{
    PadPtr pad{createVideoPad("video/x-h265, stream-format=byte-stream")};
    FaultContext context{m_parameters, nullptr, settledPosition()};

    // 00 00 01 | TRAIL_R slice (2 byte header + 40 bytes)
    std::vector<guint8> original{0, 0, 1, 0x02, 0x01};
    original.insert(original.end(), 40, kFill);

    auto result{runHandler("decode-error", pad.get(), createBuffer(original), context)};
    EXPECT_TRUE(result.applied);
    EXPECT_EQ(context.corruptedSlices.load(), 1u);

    const std::vector<guint8> corrupted{readBytes(result.buffer.get())};
    constexpr std::size_t kSliceOffset{3};
    constexpr std::size_t kSliceSize{42};
    constexpr std::size_t kCorruptFrom{kSliceOffset + 2 + (kSliceSize - 2) / 2};
    EXPECT_EQ(corrupted[3], 0x02); // NAL header intact
    EXPECT_EQ(corrupted[4], 0x01);
    for (std::size_t i = kSliceOffset + 2; i < kCorruptFrom; ++i)
    {
        EXPECT_EQ(corrupted[i], original[i]) << "byte " << i;
    }
    for (std::size_t i = kCorruptFrom; i < original.size(); ++i)
    {
        EXPECT_EQ(corrupted[i], static_cast<guint8>(original[i] ^ 0xFF)) << "byte " << i;
    }
}

TEST_F(FaultInjectionTests, ShouldNotCorruptH265KeyFrames)
{
    PadPtr pad{createVideoPad("video/x-h265, stream-format=byte-stream")};
    FaultContext context{m_parameters, nullptr, settledPosition()};

    std::vector<guint8> original{0, 0, 1, 0x26, 0x01}; // IDR_W_RADL
    original.insert(original.end(), 40, kFill);

    auto result{runHandler("decode-error", pad.get(), createBuffer(original), context)};
    EXPECT_FALSE(result.applied);
    EXPECT_EQ(readBytes(result.buffer.get()), original);
}

TEST_F(FaultInjectionTests, ShouldFailDecodeErrorForEncryptedMedia)
{
    PadPtr pad{createVideoPad("application/x-cenc, original-media-type=video/x-h264")};
    FaultContext context{m_parameters, nullptr, settledPosition()};
    const std::vector<guint8> original{createH264ByteStream(0x41)};

    auto result{runHandler("decode-error", pad.get(), createBuffer(original), context)};
    EXPECT_FALSE(result.applied);
    ASSERT_TRUE(context.failed());
    EXPECT_NE(context.failure().find("encrypted"), std::string::npos);
    EXPECT_EQ(readBytes(result.buffer.get()), original);
}

TEST_F(FaultInjectionTests, ShouldFailDecodeErrorForBuffersWithProtectionMetadata)
{
    PadPtr pad{createVideoPad("video/x-h264, stream-format=byte-stream")};
    FaultContext context{m_parameters, nullptr, settledPosition()};
    const std::vector<guint8> original{createH264ByteStream(0x41)};

    GstBuffer *buffer{createBuffer(original)};
    gst_buffer_add_protection_meta(buffer, gst_structure_new_empty("application/x-cenc"));

    auto result{runHandler("decode-error", pad.get(), buffer, context)};
    EXPECT_FALSE(result.applied);
    ASSERT_TRUE(context.failed());
    EXPECT_NE(context.failure().find("encrypted"), std::string::npos);
    EXPECT_EQ(readBytes(result.buffer.get()), original);
}

TEST_F(FaultInjectionTests, ShouldFailDecodeErrorForUnsupportedCodecs)
{
    PadPtr pad{createVideoPad("video/x-vp9")};
    FaultContext context{m_parameters, nullptr, settledPosition()};

    auto result{runHandler("decode-error", pad.get(), createBuffer(createH264ByteStream(0x41)), context)};
    EXPECT_FALSE(result.applied);
    ASSERT_TRUE(context.failed());
    EXPECT_NE(context.failure().find("unsupported"), std::string::npos);
}

TEST_F(FaultInjectionTests, ShouldFailDecodeErrorWhenNoCapsWereNegotiated)
{
    PadPtr pad{createVideoPad(nullptr)};
    FaultContext context{m_parameters, nullptr, settledPosition()};

    auto result{runHandler("decode-error", pad.get(), createBuffer(createH264ByteStream(0x41)), context)};
    EXPECT_FALSE(result.applied);
    ASSERT_TRUE(context.failed());
    EXPECT_NE(context.failure().find("caps"), std::string::npos);
}

TEST_F(FaultInjectionTests, ShouldNotCountDecodeErrorBuffersWithoutEligibleSlices)
{
    PadPtr pad{createVideoPad("video/x-h264, stream-format=byte-stream")};
    FaultContext context{m_parameters, nullptr, settledPosition()};

    // Only a parameter set: nothing to corrupt, and it must stay untouched.
    const std::vector<guint8> original{0, 0, 0, 1, 0x67, kFill, kFill, kFill, kFill, kFill};
    auto result{runHandler("decode-error", pad.get(), createBuffer(original), context)};
    EXPECT_FALSE(result.applied);
    EXPECT_FALSE(context.failed());
    EXPECT_EQ(readBytes(result.buffer.get()), original);
}

TEST_F(FaultInjectionTests, ShouldPassDecodeErrorBuffersThroughUntilPlaybackSettles)
{
    PadPtr pad{createVideoPad("video/x-h264, stream-format=byte-stream")};
    FaultContext context{m_parameters, nullptr, unsettledPosition()};
    const std::vector<guint8> original{createH264ByteStream(0x41)};

    auto result{runHandler("decode-error", pad.get(), createBuffer(original), context)};
    EXPECT_FALSE(result.applied);
    EXPECT_FALSE(context.failed());
    EXPECT_EQ(readBytes(result.buffer.get()), original);
}

TEST_F(FaultInjectionTests, ShouldFailDecodeErrorWhenPositionIsUnavailable)
{
    PadPtr pad{createVideoPad("video/x-h264, stream-format=byte-stream")};
    FaultContext context{m_parameters, nullptr, missingPosition()};

    auto result{runHandler("decode-error", pad.get(), createBuffer(createH264ByteStream(0x41)), context)};
    EXPECT_FALSE(result.applied);
    ASSERT_TRUE(context.failed());
    EXPECT_NE(context.failure().find("playback position"), std::string::npos);
}

TEST_F(FaultInjectionTests, ShouldOnlyAffectTheConfiguredNumberOfBuffers)
{
    g_handlerCalls = 0;
    TestSinkElement sink;
    m_parameters.bufferCount = 2;
    const FaultScenarioInfo info{testScenario(countingHandler)};
    metricscli::FaultInjector injector{info, m_parameters, nullptr};

    std::string error;
    ASSERT_TRUE(injector.start(sink.element(), error)) << error;
    for (int i = 0; i < 5; ++i)
    {
        sink.push();
    }
    injector.stop();

    EXPECT_EQ(g_handlerCalls.load(), 2);
    EXPECT_EQ(injector.buffersAffected(), 2u);
    EXPECT_NE(injector.summary().find("buffers-affected=2"), std::string::npos);
}

TEST_F(FaultInjectionTests, ShouldNotAffectBuffersAfterTheProbeIsStopped)
{
    g_handlerCalls = 0;
    TestSinkElement sink;
    const FaultScenarioInfo info{testScenario(countingHandler)};
    metricscli::FaultInjector injector{info, m_parameters, nullptr};

    std::string error;
    ASSERT_TRUE(injector.start(sink.element(), error)) << error;
    sink.push();
    injector.stop();
    sink.push();
    sink.push();

    EXPECT_EQ(g_handlerCalls.load(), 1);
    injector.stop(); // Stopping twice is harmless
}

TEST_F(FaultInjectionTests, ShouldRemoveTheProbeAfterTheFaultWindowExpires)
{
    g_handlerCalls = 0;
    TestSinkElement sink;
    m_parameters.windowSeconds = 1;
    const FaultScenarioInfo info{testScenario(countingHandler)};
    metricscli::FaultInjector injector{info, m_parameters, nullptr};

    std::string error;
    ASSERT_TRUE(injector.start(sink.element(), error)) << error;
    g_usleep(1200 * 1000);
    sink.push();
    sink.push();
    injector.stop();

    EXPECT_EQ(g_handlerCalls.load(), 0);
    EXPECT_EQ(injector.buffersAffected(), 0u);
}

TEST_F(FaultInjectionTests, ShouldStopInjectingAfterAHandlerReportsAFailure)
{
    g_handlerCalls = 0;
    TestSinkElement sink;
    const FaultScenarioInfo info{testScenario(failingHandler)};
    metricscli::FaultInjector injector{info, m_parameters, nullptr};

    std::string error;
    ASSERT_TRUE(injector.start(sink.element(), error)) << error;
    sink.push();
    sink.push();
    sink.push();
    injector.stop();

    EXPECT_EQ(g_handlerCalls.load(), 1);
    EXPECT_TRUE(injector.context().failed());
    EXPECT_EQ(injector.context().failure(), "handler failed");
    EXPECT_EQ(injector.buffersAffected(), 0u);
}

TEST_F(FaultInjectionTests, ShouldRefuseToStartAnUnimplementedScenario)
{
    TestSinkElement sink;
    const FaultScenarioInfo info{testScenario(nullptr)};
    metricscli::FaultInjector injector{info, m_parameters, nullptr};

    std::string error;
    EXPECT_FALSE(injector.start(sink.element(), error));
    EXPECT_NE(error.find("not implemented"), std::string::npos);
}

TEST_F(FaultInjectionTests, ShouldFailToStartWithoutASinkPad)
{
    TestSinkElement sink{false};
    const FaultScenarioInfo info{testScenario(countingHandler)};
    metricscli::FaultInjector injector{info, m_parameters, nullptr};

    std::string error;
    EXPECT_FALSE(injector.start(sink.element(), error));
    EXPECT_NE(error.find("sink pad"), std::string::npos);
}

TEST_F(FaultInjectionTests, ShouldNotStartTwice)
{
    TestSinkElement sink;
    const FaultScenarioInfo info{testScenario(countingHandler)};
    metricscli::FaultInjector injector{info, m_parameters, nullptr};

    std::string error;
    ASSERT_TRUE(injector.start(sink.element(), error)) << error;
    EXPECT_FALSE(injector.start(sink.element(), error));
    EXPECT_NE(error.find("already running"), std::string::npos);
}
