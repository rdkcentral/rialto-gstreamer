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

//
// This allows, for example, passing the following environment variable
// to the client app which will enable rialto logging via gstreamer...
//   GST_DEBUG=6
//

#include <string>

#include <gst/gst.h>

#include "LogToGstHandler.h"

namespace
{
GST_DEBUG_CATEGORY_STATIC(kGstRialtoCategory);
const char *kCategory = "rialto";
}; // namespace

using namespace firebolt::rialto;

LogToGstHandler::LogToGstHandler()
{
    GST_DEBUG_CATEGORY_INIT(kGstRialtoCategory, kCategory, 0, "Messages from rialto client library");
}

LogToGstHandler::~LogToGstHandler() {}

void LogToGstHandler::log(Level level, const std::string &file, int line, const std::string &function,
                          const std::string &message)
{
    std::string toReport;
    toReport += "M:" + file;
    toReport += " F:" + function;
    toReport += " L:" + std::to_string(line);
    toReport += " > " + message;

    switch (level)
    {
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
    default:
        GST_CAT_LOG(kGstRialtoCategory, "%s", toReport.c_str());
        break;
    }
}
