/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 Sky UK
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//
// This allows, for example, passing the following environment variable
// to the client app which will enable rialto logging via gstreamer...
//   GST_DEBUG=6
//

#include <string>
#include <gst/gst.h>

#include "LogToGstHandler.h"

namespace {
GST_DEBUG_CATEGORY_STATIC(kGstRialtoCategory);
const char* kCategory = "rialto";
};

using namespace firebolt::rialto;

LogToGstHandler::LogToGstHandler()
{
    GST_DEBUG_CATEGORY_INIT (kGstRialtoCategory, kCategory, 0, "Messages from rialto client library");
}

LogToGstHandler::~LogToGstHandler()
{
}

void LogToGstHandler::log(Level level, const std::string &file, int line, const std::string &function,
                     const std::string &message)
{
    std::string toReport;
    toReport += "M:" + file;
    toReport += " F:" + function;
    toReport += " L:" + std::to_string(line);
    toReport += " > " + message;

    switch (level) {
    case Level::Fatal:
    case Level::Error:
        GST_CAT_ERROR(kGstRialtoCategory, "%s", toReport.c_str());
        break;

    case Level::Warning:
        GST_CAT_WARNING(kGstRialtoCategory, "%s", toReport.c_str());
        break;

    case Level::Milestone:
    case Level::Info:
        GST_CAT_INFO(kGstRialtoCategory, "%s", toReport.c_str());
        break;

    case Level::Debug:
        GST_CAT_DEBUG(kGstRialtoCategory, "%s", toReport.c_str());
        break;

    case Level::External:
        GST_CAT_LOG(kGstRialtoCategory, "%s", toReport.c_str());
        break;
    }
}
