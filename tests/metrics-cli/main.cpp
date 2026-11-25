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
#include "MetricMessage.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <iomanip>
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

bool g_logBus{false};

// Progress line prefixed with the seconds elapsed since the first log line.
void logStep(const std::string &text)
{
    static const GstClockTime start{gst_util_get_timestamp()};
    const double seconds{static_cast<double>(gst_util_get_timestamp() - start) / GST_SECOND};
    std::cout << "[metrics-test +" << std::fixed << std::setprecision(1) << seconds << "s] " << text << std::endl;
    std::cout.unsetf(std::ios::floatfield);
}

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
                 "  --fault-buffers N         Buffers affected by frame-drop and decode-error (default: 10)\n"
                 "  --fault-drop-shift-ms N   Milliseconds frame-drop moves buffer timestamps back, so each\n"
                 "                            affected frame is late by about that much (default: 500)\n"
                 "  --fault-delay-frames N    Frame durations the renderer misses new frames for with frame-repeat\n"
                 "                            (default: 10; the queued lead is added to the hold)\n"
                 "  --fault-stall-ms N        Milliseconds the renderer is starved by buffer-underflow\n"
                 "                            (default: 5000; the queued lead is added to the hold)\n"
                 "  --fault-window-seconds N  Time the fault probe stays installed once a fault is first applied\n"
                 "                            (default: 10)\n"
                 "  --decode-error-mode MODE  How decode-error damages a slice: tail (default; most of the slice\n"
                 "                            data, start of the slice header kept), payload (whole slice data)\n"
                 "                            or header (slice header only). Harsher modes may stall the decoder.\n"
                 "  --log-bus                 Print every playbin bus message (type, source, structure)\n"
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
    if (argument == "--fault-drop-shift-ms")
    {
        return &parameters.dropShiftMs;
    }
    return nullptr;
}

bool parseOptions(int argc, char **argv, Options &options)
{
    bool hasUri{false};
    bool hasTimeout{false};
    bool hasFaultScenario{false};
    bool hasDecodeErrorMode{false};
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

        if (argument == "--log-bus")
        {
            g_logBus = true;
            continue;
        }

        if (argument != "--uri" && argument != "--timeout" && argument != "--fault-scenario" &&
            argument != "--decode-error-mode" && !faultParameterField(argument, options.faultParameters))
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
        else if (argument == "--decode-error-mode")
        {
            if (hasDecodeErrorMode)
            {
                std::cerr << "--decode-error-mode may only be specified once\n";
                return false;
            }
            const std::optional<metricscli::DecodeErrorMode> mode{metricscli::findDecodeErrorMode(value)};
            if (!mode)
            {
                std::cerr << "Unknown decode-error mode: " << value << " (expected one of: tail, payload, header)\n";
                return false;
            }
            options.faultParameters.decodeErrorMode = *mode;
            hasDecodeErrorMode = true;
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
    if (hasDecodeErrorMode && (!hasFaultScenario || options.faultScenario->scenario != metricscli::FaultScenario::DecodeError))
    {
        std::cerr << "--decode-error-mode requires --fault-scenario decode-error\n";
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

constexpr GstClockTime kFrameCountersInterval{15 * GST_SECOND};

bool validateFrameCounters(GstElement *videoSink)
{
    logStep("Querying frame-counters from rialtomsevideosink");
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

// Set by --log-bus: every message popped from the bus is printed, and the bus is read unfiltered so
// that messages the CLI would otherwise leave queued are seen as well.
void logBusMessage(GstMessage *message)
{
    if (!g_logBus)
    {
        return;
    }
    const GstStructure *structure{gst_message_get_structure(message)};
    gchar *text{structure ? gst_structure_to_string(structure) : nullptr};
    std::cout << "Bus message: type=" << gst_message_type_get_name(GST_MESSAGE_TYPE(message))
              << " src=" << (GST_MESSAGE_SRC(message) ? GST_OBJECT_NAME(GST_MESSAGE_SRC(message)) : "(none)");
    if (text)
    {
        std::cout << " structure=" << text;
        g_free(text);
    }
    std::cout << '\n';
}

// Waits for one of the requested message types. GST_MESSAGE_APPLICATION messages seen while waiting
// are handed to the observer and do not end the wait, so metric messages are monitored for the
// whole run.
GstMessage *popPlaybackMessage(GstBus *bus, GstClockTime timeout, GstMessageType types,
                               metricscli::MetricObserver &observer)
{
    const GstClockTime start{gst_util_get_timestamp()};
    const GstMessageType mask{g_logBus ? GST_MESSAGE_ANY : static_cast<GstMessageType>(types | GST_MESSAGE_APPLICATION)};
    while (true)
    {
        const GstClockTime elapsed{gst_util_get_timestamp() - start};
        const GstClockTime remaining{elapsed >= timeout ? GstClockTime{0} : timeout - elapsed};
        GstMessage *message{gst_bus_timed_pop_filtered(bus, remaining, mask)};
        if (!message)
        {
            return nullptr;
        }
        logBusMessage(message);
        if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_APPLICATION)
        {
            observer.handle(message);
        }
        else if (GST_MESSAGE_TYPE(message) & types)
        {
            return message;
        }
        gst_message_unref(message);
    }
}

void drainMetricMessages(GstBus *bus, metricscli::MetricObserver &observer)
{
    const GstMessageType mask{g_logBus ? GST_MESSAGE_ANY : GST_MESSAGE_APPLICATION};
    while (GstMessage *message{gst_bus_pop_filtered(bus, mask)})
    {
        logBusMessage(message);
        if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_APPLICATION)
        {
            observer.handle(message);
        }
        gst_message_unref(message);
    }
}

// Waits until a valid metric message has been observed or the timeout expires. Returns false if the
// pipeline reported an error while waiting.
bool waitForValidMetric(GstBus *bus, GstClockTime timeout, metricscli::MetricObserver &observer)
{
    const GstClockTime start{gst_util_get_timestamp()};
    const GstMessageType mask{g_logBus ? GST_MESSAGE_ANY
                                       : static_cast<GstMessageType>(GST_MESSAGE_APPLICATION | GST_MESSAGE_ERROR)};
    while (observer.validCount() == 0)
    {
        const GstClockTime elapsed{gst_util_get_timestamp() - start};
        const GstClockTime remaining{elapsed >= timeout ? GstClockTime{0} : timeout - elapsed};
        GstMessage *message{gst_bus_timed_pop_filtered(bus, remaining, mask)};
        if (!message)
        {
            return true;
        }
        logBusMessage(message);
        const bool isError{GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR};
        if (isError)
        {
            reportPipelineError(message);
        }
        else if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_APPLICATION)
        {
            observer.handle(message);
        }
        gst_message_unref(message);
        if (isError)
        {
            return false;
        }
    }
    return true;
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
    logStep("Created playbin: " + std::string{pipeline ? "ok" : "FAILED"});
    if (!pipeline)
    {
        std::cerr << "Failed to create playbin; the required GStreamer plugin may be missing.\n";
        return EXIT_PLAYBACK_FAILURE;
    }

    int exitCode{EXIT_PLAYBACK_FAILURE};
    GstBus *bus{nullptr};
    metricscli::MetricObserver metricObserver;
    std::optional<metricscli::FaultInjector> faultInjector;
    GstElement *videoSink{gst_element_factory_make("rialtomsevideosink", "metrics-test-video-sink")};
    logStep("Created rialtomsevideosink: " + std::string{videoSink ? "ok" : "FAILED"});
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
            logStep("Starting playback (setting playbin to PLAYING), timeout=" + std::to_string(options.timeoutSeconds) +
                    "s");
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
                    logStep("Waiting for the pipeline to preroll (ASYNC_DONE)");
                    message = popPlaybackMessage(
                        bus, remainingTimeout(),
                        static_cast<GstMessageType>(GST_MESSAGE_ASYNC_DONE | GST_MESSAGE_ERROR | GST_MESSAGE_EOS),
                        metricObserver);
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
                        logStep(reachedEos ? "Reached EOS while prerolling" : "Pipeline prerolled, playback running");
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
                        logStep(std::string{"Starting fault injection: "} + options.faultScenario->name);
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
                        // Wait in slices so the frame counters are reported periodically until playback ends.
                        logStep("Waiting for EOS or error; frame counters are re-read every 15s");
                        while (true)
                        {
                            const GstClockTime remaining{remainingTimeout()};
                            if (remaining == 0)
                            {
                                break;
                            }
                            message = popPlaybackMessage(bus, std::min(remaining, kFrameCountersInterval),
                                                         static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS),
                                                         metricObserver);
                            if (message)
                            {
                                break;
                            }
                            if (remainingTimeout() > 0)
                            {
                                validateFrameCounters(videoSink);
                            }
                        }
                        if (!message)
                        {
                            std::cerr << "Playback timed out after " << options.timeoutSeconds << " seconds.\n";
                        }
                        else
                        {
                            if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS)
                            {
                                std::cout << "Playback reached EOS.\n";
                                std::cout << "Final ";
                                validateFrameCounters(videoSink);
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

                // Playback can end before the metric crosses IPC, so a fault run keeps waiting until the timeout.
                if (faultInjector && exitCode == EXIT_SUCCESS_CODE && metricObserver.validCount() == 0)
                {
                    std::cout << "Waiting for a valid " << metricscli::kMetricMessageName << " message.\n";
                    if (!waitForValidMetric(bus, remainingTimeout(), metricObserver))
                    {
                        exitCode = EXIT_PLAYBACK_FAILURE;
                    }
                }
            }
        }
    }

    if (bus)
    {
        logStep("Draining remaining bus messages");
        drainMetricMessages(bus, metricObserver);
        std::cout << "Metric messages observed: valid=" << metricObserver.validCount()
                  << ", invalid=" << metricObserver.invalidCount() << '\n';
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
        else if (metricObserver.validCount() == 0 && faultInjector->buffersAffected() > 0)
        {
            std::cerr << "Fault scenario '" << faultInjector->info().name << "' was applied but no valid "
                      << metricscli::kMetricMessageName << " message arrived within " << options.timeoutSeconds
                      << " seconds";
            if (metricObserver.invalidCount() > 0)
            {
                std::cerr << " (" << metricObserver.invalidCount() << " invalid message(s) were observed)";
            }
            std::cerr << ".\n";
            exitCode = EXIT_PLAYBACK_FAILURE;
        }
    }
    faultInjector.reset();
    logStep("Stopping playback (setting playbin to NULL)");
    if (gst_element_set_state(pipeline, GST_STATE_NULL) == GST_STATE_CHANGE_FAILURE)
    {
        std::cerr << "Failed to stop the playbin pipeline during cleanup.\n";
        exitCode = EXIT_PLAYBACK_FAILURE;
    }
    gst_object_unref(pipeline); // Also releases the video sink, which playbin owns after g_object_set
    logStep("Finished, exit code " + std::to_string(exitCode));
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
