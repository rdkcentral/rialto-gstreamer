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

#include <gst/gst.h>

#include "FaultInjection.h"
#include "FrameCounters.h"

#include <charconv>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace
{
constexpr std::uint64_t DEFAULT_TIMEOUT_SECONDS{30};
constexpr int EXIT_SUCCESS_CODE{0};
constexpr int EXIT_PLAYBACK_FAILURE{1};
constexpr int EXIT_ARGUMENT_ERROR{2};
constexpr int EXIT_GSTREAMER_INIT_FAILURE{3};

struct Options
{
    std::string uri;
    std::uint64_t timeoutSeconds{DEFAULT_TIMEOUT_SECONDS};
    const metricscli::FaultScenarioInfo *faultScenario{nullptr};
    metricscli::FaultParameters faultParameters;
};

void printUsage(const char *program)
{
    std::cout << "Usage: " << program
              << " --uri URI [--timeout SECONDS] [--fault-scenario NAME [fault options]]\n"
                 "Options:\n"
                 "  --uri URI                 Media URI to test (required)\n"
                 "  --timeout SECONDS         Test timeout in seconds (default: 30; must be positive)\n"
                 "  --fault-scenario NAME     Optional fault scenario: "
              << metricscli::faultScenarioNames()
              << "\n"
                 "Fault options (require --fault-scenario; positive integers):\n"
                 "  --fault-buffers N         Buffers affected by frame-drop and decode-error (default: 3)\n"
                 "  --fault-delay-frames N    Frame durations a buffer is held by frame-repeat (default: 3)\n"
                 "  --fault-stall-ms N        Milliseconds video is blocked by buffer-underflow (default: 2000)\n"
                 "  --fault-window-seconds N  Maximum time the fault probe stays installed (default: 10)\n"
                 "  --help                    Show this help and exit\n";
}

bool parseTimeout(std::string_view value, std::uint64_t &timeoutSeconds)
{
    if (value.empty())
    {
        return false;
    }

    std::uint64_t parsedValue{0};
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsedValue);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || parsedValue == 0 ||
        parsedValue > std::numeric_limits<GstClockTime>::max() / GST_SECOND)
    {
        return false;
    }

    timeoutSeconds = parsedValue;
    return true;
}

bool parsePositiveUint32(std::string_view value, std::uint32_t &result)
{
    std::uint32_t parsedValue{0};
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), parsedValue);
    if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || parsedValue == 0)
    {
        return false;
    }
    result = parsedValue;
    return true;
}

std::uint32_t *faultParameterField(std::string_view argument, metricscli::FaultParameters &parameters)
{
    if (argument == "--fault-buffers")
    {
        return &parameters.bufferCount;
    }
    if (argument == "--fault-delay-frames")
    {
        return &parameters.delayFrames;
    }
    if (argument == "--fault-stall-ms")
    {
        return &parameters.stallMs;
    }
    if (argument == "--fault-window-seconds")
    {
        return &parameters.windowSeconds;
    }
    return nullptr;
}

bool parseOptions(int argc, char **argv, Options &options)
{
    bool hasUri{false};
    bool hasTimeout{false};
    bool hasFaultScenario{false};
    std::set<std::string_view> faultParameterArguments;

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view argument{argv[i]};
        if (argument == "--help")
        {
            if (argc != 2)
            {
                std::cerr << "--help cannot be combined with other options\n";
                return false;
            }
            printUsage(argv[0]);
            return true;
        }

        if (argument != "--uri" && argument != "--timeout" && argument != "--fault-scenario" &&
            !faultParameterField(argument, options.faultParameters))
        {
            std::cerr << "Unknown option: " << argument << '\n';
            return false;
        }
        const std::string_view valueCandidate{i + 1 < argc ? argv[i + 1] : ""};
        if (i + 1 >= argc || valueCandidate.compare(0, 2, "--") == 0)
        {
            std::cerr << "Missing value for " << argument << '\n';
            return false;
        }

        const std::string_view value{argv[++i]};
        if (argument == "--uri")
        {
            if (hasUri)
            {
                std::cerr << "--uri may only be specified once\n";
                return false;
            }
            options.uri = value;
            hasUri = true;
        }
        else if (argument == "--timeout")
        {
            if (hasTimeout)
            {
                std::cerr << "--timeout may only be specified once\n";
                return false;
            }
            if (!parseTimeout(value, options.timeoutSeconds))
            {
                std::cerr << "Invalid --timeout value: " << value << " (expected positive seconds)\n";
                return false;
            }
            hasTimeout = true;
        }
        else if (argument == "--fault-scenario")
        {
            if (hasFaultScenario)
            {
                std::cerr << "--fault-scenario may only be specified once\n";
                return false;
            }
            options.faultScenario = metricscli::findFaultScenario(value);
            if (!options.faultScenario)
            {
                std::cerr << "Unknown fault scenario: " << value
                          << " (expected one of: " << metricscli::faultScenarioNames() << ")\n";
                return false;
            }
            hasFaultScenario = true;
        }
        else
        {
            if (!faultParameterArguments.insert(argument).second)
            {
                std::cerr << argument << " may only be specified once\n";
                return false;
            }
            if (!parsePositiveUint32(value, *faultParameterField(argument, options.faultParameters)))
            {
                std::cerr << "Invalid " << argument << " value: " << value << " (expected a positive integer)\n";
                return false;
            }
        }
    }

    if (!hasUri)
    {
        std::cerr << "Missing required --uri option\n";
        return false;
    }
    if (!faultParameterArguments.empty() && !hasFaultScenario)
    {
        std::cerr << "Fault options require --fault-scenario\n";
        return false;
    }
    return true;
}

void reportPipelineError(GstMessage *message)
{
    GError *error{nullptr};
    gchar *debug{nullptr};
    gst_message_parse_error(message, &error, &debug);

    std::cerr << "Pipeline error";
    if (error)
    {
        std::cerr << ": " << error->message;
        g_error_free(error);
    }
    if (debug)
    {
        std::cerr << " (" << debug << ')';
        g_free(debug);
    }
    std::cerr << '\n';
}

bool validateFrameCounters(GstElement *videoSink)
{
    GstStructure *structure{nullptr};
    g_object_get(videoSink, "frame-counters", &structure, nullptr);
    metricscli::FrameCounters counters;
    std::string error;
    const bool valid{metricscli::parseFrameCounters(structure, counters, error)};
    if (valid)
    {
        std::cout << "Frame counters: rendered=" << counters.rendered << ", dropped=" << counters.dropped
                  << ", corrupted=" << counters.corrupted << ", timestamp=" << counters.timestamp << '\n';
    }
    else
    {
        std::cerr << error << '\n';
    }
    if (structure)
    {
        gst_structure_free(structure);
    }
    return valid;
}

int runPlayback(const Options &options)
{
    if (options.faultScenario && !options.faultScenario->handler)
    {
        std::cerr << "Fault scenario '" << options.faultScenario->name
                  << "' is not implemented; refusing to run without applying it.\n";
        return EXIT_PLAYBACK_FAILURE;
    }

    GstElement *pipeline{gst_element_factory_make("playbin", "metrics-test-playbin")};
    if (!pipeline)
    {
        std::cerr << "Failed to create playbin; the required GStreamer plugin may be missing.\n";
        return EXIT_PLAYBACK_FAILURE;
    }

    int exitCode{EXIT_PLAYBACK_FAILURE};
    GstBus *bus{nullptr};
    std::optional<metricscli::FaultInjector> faultInjector;
    GstElement *videoSink{gst_element_factory_make("rialtomsevideosink", "metrics-test-video-sink")};
    if (!videoSink)
    {
        std::cerr << "Failed to create rialtomsevideosink; the Rialto GStreamer plugin is unavailable.\n";
    }
    else
    {
        g_object_set(pipeline, "uri", options.uri.c_str(), "video-sink", videoSink, nullptr);

        bus = gst_element_get_bus(pipeline);
        if (!bus)
        {
            std::cerr << "Failed to retrieve the playbin message bus.\n";
        }
        else
        {
            const GstClockTime timeout = static_cast<GstClockTime>(options.timeoutSeconds) * GST_SECOND;
            const GstClockTime startTime = gst_util_get_timestamp();
            const auto remainingTimeout = [timeout, startTime]() {
                const GstClockTime elapsed = gst_util_get_timestamp() - startTime;
                return elapsed >= timeout ? GstClockTime{0} : timeout - elapsed;
            };
            const GstStateChangeReturn stateResult = gst_element_set_state(pipeline, GST_STATE_PLAYING);
            if (stateResult == GST_STATE_CHANGE_FAILURE)
            {
                GstMessage *message = gst_bus_pop_filtered(bus, GST_MESSAGE_ERROR);
                if (message)
                {
                    reportPipelineError(message);
                    gst_message_unref(message);
                }
                else
                {
                    std::cerr << "Failed to start playbin.\n";
                }
            }
            else
            {
                std::cout << "Playing " << options.uri << " through rialtomsevideosink.\n";
                GstMessage *message{nullptr};
                bool reachedEos{false};
                bool readyForCounters{stateResult != GST_STATE_CHANGE_ASYNC};
                if (!readyForCounters)
                {
                    message = gst_bus_timed_pop_filtered(
                        bus, remainingTimeout(),
                        static_cast<GstMessageType>(GST_MESSAGE_ASYNC_DONE | GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
                    if (!message)
                    {
                        std::cerr << "Playback timed out after " << options.timeoutSeconds
                                  << " seconds while waiting for the video source.\n";
                    }
                    else if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR)
                    {
                        reportPipelineError(message);
                    }
                    else
                    {
                        reachedEos = GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS;
                        readyForCounters = true;
                    }
                    if (message)
                    {
                        gst_message_unref(message);
                    }
                }

                bool faultReady{true};
                if (options.faultScenario && readyForCounters)
                {
                    std::string faultError;
                    if (reachedEos)
                    {
                        std::cerr << "Playback reached EOS before fault scenario '" << options.faultScenario->name
                                  << "' could be applied.\n";
                        faultReady = false;
                    }
                    else
                    {
                        faultInjector.emplace(*options.faultScenario, options.faultParameters, pipeline);
                        faultReady = faultInjector->start(videoSink, faultError);
                        if (faultReady)
                        {
                            std::cout << "Fault injection started: " << options.faultScenario->name << '\n';
                        }
                        else
                        {
                            std::cerr << faultError << '\n';
                        }
                    }
                }

                if (readyForCounters && faultReady && validateFrameCounters(videoSink))
                {
                    if (reachedEos)
                    {
                        std::cout << "Playback reached EOS.\n";
                        exitCode = EXIT_SUCCESS_CODE;
                    }
                    else
                    {
                        message = gst_bus_timed_pop_filtered(
                            bus, remainingTimeout(), static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
                        if (!message)
                        {
                            std::cerr << "Playback timed out after " << options.timeoutSeconds << " seconds.\n";
                        }
                        else
                        {
                            if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS)
                            {
                                std::cout << "Playback reached EOS.\n";
                                exitCode = EXIT_SUCCESS_CODE;
                            }
                            else
                            {
                                reportPipelineError(message);
                            }
                            gst_message_unref(message);
                        }
                    }
                }
            }
        }
    }

    if (bus)
    {
        gst_object_unref(bus);
    }
    if (faultInjector)
    {
        faultInjector->stop();
        std::cout << "Fault injected: " << faultInjector->summary() << '\n';
        if (faultInjector->context().failed())
        {
            std::cerr << faultInjector->context().failure() << '\n';
            exitCode = EXIT_PLAYBACK_FAILURE;
        }
        else if (exitCode == EXIT_SUCCESS_CODE && faultInjector->buffersAffected() == 0)
        {
            std::cerr << "Fault scenario '" << faultInjector->info().name << "' did not affect any buffers.\n";
            exitCode = EXIT_PLAYBACK_FAILURE;
        }
    }
    faultInjector.reset();
    if (gst_element_set_state(pipeline, GST_STATE_NULL) == GST_STATE_CHANGE_FAILURE)
    {
        std::cerr << "Failed to stop the playbin pipeline during cleanup.\n";
        exitCode = EXIT_PLAYBACK_FAILURE;
    }
    if (videoSink)
    {
        gst_object_unref(videoSink);
    }
    gst_object_unref(pipeline);
    return exitCode;
}
} // namespace

int main(int argc, char **argv)
{
    Options options;
    if (!parseOptions(argc, argv, options))
    {
        printUsage(argv[0]);
        return EXIT_ARGUMENT_ERROR;
    }
    if (argc == 2 && std::string_view{argv[1]} == "--help")
    {
        return EXIT_SUCCESS_CODE;
    }

    GError *error{nullptr};
    if (!gst_init_check(nullptr, nullptr, &error))
    {
        std::cerr << "Failed to initialize GStreamer";
        if (error)
        {
            std::cerr << ": " << error->message;
            g_error_free(error);
        }
        std::cerr << '\n';
        return EXIT_GSTREAMER_INIT_FAILURE;
    }

    if (!gst_uri_is_valid(options.uri.c_str()))
    {
        std::cerr << "Invalid URI: " << options.uri << '\n';
        return EXIT_ARGUMENT_ERROR;
    }

    return runPlayback(options);
}
