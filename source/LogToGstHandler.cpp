/*
 * Copyright (C) 2024 Sky UK
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

#include "IClientLogControl.h"
#include "LogToGstHandler.h"

namespace
{
GST_DEBUG_CATEGORY_STATIC(kGstRialtoCategory);
const char *kCategory = "rialto";

// Please see the comments in LogToGstHandler.h
// The following g_referenceCount variable has the following states:-
//
//  value   |   LogToGstHandler  |
//          |   is in use        |    Meaning
//  --------+--------------------+----------------------------------------
//   -2     |       NO           | logToGstPreRegister() hasn't been called yet
//   -1     |       YES          | logToGstPreRegister() has been called (and it may be called more than once)
//    1+    |       YES          | the value indicates the number of rialto sinks currently in use
//    0     |       NO           | the last sink has been finalized
int g_referenceCount{-2};

}; // namespace

using namespace firebolt::rialto::client;

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

void LogToGstHandler::logToGstPreRegister()
{
    // Please see the comment above the definition of g_referenceCount

    if (g_referenceCount == -2)
    {
        std::shared_ptr<firebolt::rialto::IClientLogHandler> logToGstHandler = std::make_shared<LogToGstHandler>();

        if (!firebolt::rialto::IClientLogControlFactory::createFactory()
                 ->createClientLogControl()
                 .registerLogHandler(logToGstHandler, true))
        {
            GST_ERROR("Unable to preRegister log handler");
        }
        else
        {
            g_referenceCount = -1;
            GST_INFO("Pre register");
        }
    }
}

void LogToGstHandler::logToGstSinkInit()
{
    // Please see the comment above the definition of g_referenceCount

    if (g_referenceCount > 0)
    {
        ++g_referenceCount;
        GST_INFO("Ref count inc %d", g_referenceCount);
    }
    else if (g_referenceCount == 0 || g_referenceCount <= -2)
    {
        if (g_referenceCount == 0)
        {
            // A sink is being used again after all sinks had previously been finalized
            // Start a new handler...
            GST_WARNING("Re-register log handler after previous cancellation");
        }
        else
        {
            // Other values of g_referenceCount should not be possible
            // -2 should not be possible because logToGstPreRegister should have been called
            // (call logToGstPreRegister during class_init)
            GST_ERROR("Call logToGstPreRegister() before logToGstSinkInit()");
            g_referenceCount = 0;
        }

        std::shared_ptr<firebolt::rialto::IClientLogHandler> logToGstHandler = std::make_shared<LogToGstHandler>();

        if (!firebolt::rialto::IClientLogControlFactory::createFactory()
                 ->createClientLogControl()
                 .registerLogHandler(logToGstHandler, true))
        {
            GST_ERROR("Unable to preRegister log handler");
        }
        else
        {
            ++g_referenceCount;
            GST_INFO("Ref count inc %d", g_referenceCount);
        }
    }
    else // if (g_referenceCount == -1)
    {
        g_referenceCount = 1;
        GST_INFO("Ref count set 1");
    }
}

void LogToGstHandler::logToGstSinkFinalize()
{
    // Please see the comment above the definition of g_referenceCount

    if (g_referenceCount > 0)
    {
        --g_referenceCount;
        GST_INFO("Ref count dec %d", g_referenceCount);

        if (g_referenceCount == 0)
        {
            GST_WARNING("Cancel log handler since sink ref count is 0");
            if (!firebolt::rialto::IClientLogControlFactory::createFactory()
                     ->createClientLogControl()
                     .registerLogHandler(nullptr, true))
            {
                GST_ERROR("Unable to cancel rialto log handler");
                g_referenceCount = -1; // This is like the pre-reg state
            }
        }
    }
    else if (g_referenceCount == 0)
    {
        GST_ERROR("logToGstSinkFinalize() called more than logToGstSinkInit()");
    }
    else // if (g_referenceCount < 0)
    {
        GST_ERROR("logToGstSinkFinalize() called before logToGstSinkInit()");
    }
}
