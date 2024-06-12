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

#ifndef FIREBOLT_RIALTO_CLIENT_LOG_HANDLER_H_
#define FIREBOLT_RIALTO_CLIENT_LOG_HANDLER_H_

#include "IClientLogHandler.h"

namespace firebolt::rialto::client
{
class LogToGstHandler : public IClientLogHandler
{
public:
    LogToGstHandler();
    ~LogToGstHandler();

    void log(Level level, const std::string &file, int line, const std::string &function, const std::string &message);

    // When the last sink is finalised the log handler is un-registered via these callbacks.
    // This is done because, for example, ClientController::~ClientController() runs AFTER main() finishes
    // and this method attempts to use the log (if configured to do so). However, gstreamer logging has potentially
    // been freed at this point if the client has called gst_deinit. And this would cause read and write to freed
    // memory. Therefore, to prevent this, the logToGst handler is disabled when the last sink is finalised...
    static void logToGstPreRegister(); // This registers the log handler before the first sink uses it
    static void logToGstSinkInit();
    static void logToGstSinkFinalize();
};
} // namespace firebolt::rialto::client
#endif // FIREBOLT_RIALTO_RIALTO_CLIENT_LOG_HANDLER_H_
