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

#include "MetricMessage.h"
#include "RialtoGstTest.h"

#include <memory>

using metricscli::MetricObservation;

namespace
{
GstStructure *metric(guint observationKind, const char *extraField = nullptr)
{
    GstStructure *structure{gst_structure_new("media-pipeline-metric", "metric", G_TYPE_UINT, guint{5},
                                              "observation-kind", G_TYPE_UINT, observationKind, "source_id",
                                              G_TYPE_UINT, guint{7}, nullptr)};
    if (extraField)
    {
        // Fields are added by name so each test controls exactly which optional field is present.
        if (g_strcmp0(extraField, "pts-ns") == 0)
        {
            gst_structure_set(structure, "pts-ns", G_TYPE_INT64, gint64{1000}, nullptr);
        }
        else if (g_strcmp0(extraField, "duration-ns") == 0)
        {
            gst_structure_set(structure, "duration-ns", G_TYPE_UINT64, guint64{2000}, nullptr);
        }
        else if (g_strcmp0(extraField, "count") == 0)
        {
            gst_structure_set(structure, "count", G_TYPE_UINT64, guint64{3}, nullptr);
        }
        else if (g_strcmp0(extraField, "session_id") == 0)
        {
            gst_structure_set(structure, "session_id", G_TYPE_UINT, guint{9}, nullptr);
        }
    }
    return structure;
}

struct StructureDeleter
{
    void operator()(GstStructure *structure) const { gst_structure_free(structure); }
};
using StructurePtr = std::unique_ptr<GstStructure, StructureDeleter>;

bool parse(const StructurePtr &structure, MetricObservation &observation, std::string &error)
{
    return metricscli::parseMetricMessage(structure.get(), observation, error);
}
} // namespace

TEST_F(RialtoGstTest, ShouldParseOccurrenceMetricMessage)
{
    StructurePtr structure{metric(metricscli::kObservationOccurrence, "pts-ns")};
    MetricObservation observation;
    std::string error;
    EXPECT_TRUE(parse(structure, observation, error)) << error;
    EXPECT_EQ(observation.metric, 5u);
    EXPECT_EQ(observation.observationKind, metricscli::kObservationOccurrence);
    EXPECT_EQ(observation.sourceId, 7u);
    EXPECT_EQ(observation.ptsNs, std::optional<gint64>{1000});
    EXPECT_FALSE(observation.durationNs.has_value());
    EXPECT_FALSE(observation.count.has_value());
    EXPECT_FALSE(observation.sessionId.has_value());
}

TEST_F(RialtoGstTest, ShouldParseEpisodeMetricMessageWithOptionalFields)
{
    StructurePtr structure{metric(metricscli::kObservationEpisodeResolved)};
    gst_structure_set(structure.get(), "pts-ns", G_TYPE_INT64, gint64{1}, "duration-ns", G_TYPE_UINT64, guint64{2},
                      "count", G_TYPE_UINT64, guint64{3}, "session_id", G_TYPE_UINT, guint{4}, nullptr);
    MetricObservation observation;
    std::string error;
    EXPECT_TRUE(parse(structure, observation, error)) << error;
    EXPECT_EQ(observation.durationNs, std::optional<guint64>{2});
    EXPECT_EQ(observation.count, std::optional<guint64>{3});
    EXPECT_EQ(observation.sessionId, std::optional<guint>{4});
    EXPECT_NE(metricscli::describeMetricObservation(observation).find("episode-resolved"), std::string::npos);
}

TEST_F(RialtoGstTest, ShouldNotValidateTheMetricCategory)
{
    StructurePtr structure{metric(metricscli::kObservationEpisodeStarted)};
    gst_structure_set(structure.get(), "metric", G_TYPE_UINT, guint{12345}, nullptr);
    MetricObservation observation;
    std::string error;
    EXPECT_TRUE(parse(structure, observation, error)) << error;
    EXPECT_EQ(observation.metric, 12345u);
}

TEST_F(RialtoGstTest, ShouldRejectOccurrenceWithDurationOrCount)
{
    for (const char *field : {"duration-ns", "count"})
    {
        StructurePtr structure{metric(metricscli::kObservationOccurrence, field)};
        MetricObservation observation;
        std::string error;
        EXPECT_FALSE(parse(structure, observation, error)) << field;
        EXPECT_NE(error.find("Occurrence"), std::string::npos) << field;
    }
}

TEST_F(RialtoGstTest, ShouldRejectInvalidObservationKind)
{
    for (guint kind : {guint{0}, guint{4}})
    {
        StructurePtr structure{metric(kind)};
        MetricObservation observation;
        std::string error;
        EXPECT_FALSE(parse(structure, observation, error)) << kind;
        EXPECT_NE(error.find("observation-kind"), std::string::npos) << kind;
    }
}

TEST_F(RialtoGstTest, ShouldRejectMissingRequiredFields)
{
    for (const char *field : {"metric", "observation-kind", "source_id"})
    {
        StructurePtr structure{metric(metricscli::kObservationOccurrence)};
        gst_structure_remove_field(structure.get(), field);
        MetricObservation observation;
        std::string error;
        EXPECT_FALSE(parse(structure, observation, error)) << field;
        EXPECT_NE(error.find(field), std::string::npos) << field;
    }
}

TEST_F(RialtoGstTest, ShouldRejectRequiredFieldsWithWrongType)
{
    for (const char *field : {"metric", "observation-kind", "source_id"})
    {
        StructurePtr structure{metric(metricscli::kObservationOccurrence)};
        gst_structure_set(structure.get(), field, G_TYPE_INT, 1, nullptr);
        MetricObservation observation;
        std::string error;
        EXPECT_FALSE(parse(structure, observation, error)) << field;
        EXPECT_NE(error.find(field), std::string::npos) << field;
    }
}

TEST_F(RialtoGstTest, ShouldRejectOptionalFieldsWithWrongType)
{
    for (const char *field : {"session_id", "pts-ns", "duration-ns", "count"})
    {
        StructurePtr structure{metric(metricscli::kObservationEpisodeStarted)};
        gst_structure_set(structure.get(), field, G_TYPE_STRING, "wrong", nullptr);
        MetricObservation observation;
        std::string error;
        EXPECT_FALSE(parse(structure, observation, error)) << field;
        EXPECT_NE(error.find(field), std::string::npos) << field;
    }
}

TEST_F(RialtoGstTest, ShouldRejectStructuresWithOtherNames)
{
    StructurePtr structure{gst_structure_new_empty("something-else")};
    MetricObservation observation;
    std::string error;
    EXPECT_FALSE(metricscli::isMetricMessage(structure.get()));
    EXPECT_FALSE(parse(structure, observation, error));
    EXPECT_FALSE(metricscli::isMetricMessage(nullptr));
}

TEST_F(RialtoGstTest, ShouldCountOnlyValidMetricApplicationMessages)
{
    metricscli::MetricObserver observer;

    auto post = [&observer](GstStructure *structure)
    {
        GstMessage *message{gst_message_new_application(nullptr, structure)};
        observer.handle(message);
        gst_message_unref(message);
    };

    post(metric(metricscli::kObservationOccurrence));                         // valid
    post(metric(metricscli::kObservationEpisodeStarted, "duration-ns"));      // valid
    post(metric(metricscli::kObservationOccurrence, "count"));                // invalid
    post(metric(7));                                                          // invalid kind
    post(gst_structure_new_empty("unrelated-application-message"));           // ignored

    GstMessage *notApplication{gst_message_new_eos(nullptr)};
    observer.handle(notApplication); // ignored
    gst_message_unref(notApplication);

    EXPECT_EQ(observer.validCount(), 2u);
    EXPECT_EQ(observer.invalidCount(), 2u);
}
