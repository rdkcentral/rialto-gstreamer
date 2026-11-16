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

#include "FrameCounters.h"
#include "RialtoGstTest.h"

TEST_F(RialtoGstTest, ShouldParseFrameCounters)
{
    GstStructure *structure{gst_structure_new("frame-counters", "rendered", G_TYPE_UINT64, guint64{0}, "dropped",
                                              G_TYPE_UINT64, guint64{7}, "corrupted", G_TYPE_UINT64, guint64{3},
                                              "timestamp", G_TYPE_UINT64, guint64{12345}, nullptr)};

    metricscli::FrameCounters counters;
    std::string error;
    EXPECT_TRUE(metricscli::parseFrameCounters(structure, counters, error)) << error;
    EXPECT_EQ(counters.rendered, 0u);
    EXPECT_EQ(counters.dropped, 7u);
    EXPECT_EQ(counters.corrupted, 3u);
    EXPECT_EQ(counters.timestamp, 12345u);
    EXPECT_TRUE(error.empty());

    gst_structure_free(structure);
}

TEST_F(RialtoGstTest, ShouldRejectMissingFrameCountersStructure)
{
    metricscli::FrameCounters counters;
    std::string error;
    EXPECT_FALSE(metricscli::parseFrameCounters(nullptr, counters, error));
    EXPECT_FALSE(error.empty());
}

TEST_F(RialtoGstTest, ShouldRejectFrameCountersWithWrongName)
{
    GstStructure *structure{gst_structure_new_empty("unexpected-name")};

    metricscli::FrameCounters counters;
    std::string error;
    EXPECT_FALSE(metricscli::parseFrameCounters(structure, counters, error));
    EXPECT_NE(error.find("structure name"), std::string::npos);

    gst_structure_free(structure);
}

TEST_F(RialtoGstTest, ShouldRejectFrameCountersWithMissingField)
{
    GstStructure *structure{gst_structure_new("frame-counters", "rendered", G_TYPE_UINT64, guint64{0}, "dropped",
                                              G_TYPE_UINT64, guint64{7}, "timestamp", G_TYPE_UINT64, guint64{12345},
                                              nullptr)};

    metricscli::FrameCounters counters;
    std::string error;
    EXPECT_FALSE(metricscli::parseFrameCounters(structure, counters, error));
    EXPECT_NE(error.find("corrupted"), std::string::npos);

    gst_structure_free(structure);
}

TEST_F(RialtoGstTest, ShouldRejectFrameCountersWithWrongFieldType)
{
    GstStructure *structure{gst_structure_new("frame-counters", "rendered", G_TYPE_UINT64, guint64{0}, "dropped",
                                              G_TYPE_UINT64, guint64{7}, "corrupted", G_TYPE_UINT, guint{3}, "timestamp",
                                              G_TYPE_UINT64, guint64{12345}, nullptr)};

    metricscli::FrameCounters counters;
    std::string error;
    EXPECT_FALSE(metricscli::parseFrameCounters(structure, counters, error));
    EXPECT_NE(error.find("corrupted"), std::string::npos);

    gst_structure_free(structure);
}
