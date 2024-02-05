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

#include <gtest/gtest.h>

#include "LogToGstHandler.h"

using namespace firebolt::rialto;

class LogToGstHandlerTest : public testing::Test
{
public:
    void logTest(LogToGstHandler &logHandler, IClientLogHandler::Level level)
    {
        logHandler.log(level, "testFile", 1, "testFunction", "testMessage");
    }

    ~LogToGstHandlerTest() override {}
};

TEST_F(LogToGstHandlerTest, callingLogHandlerAtAllLevelsShouldSucceed)
{
    LogToGstHandler logToGstHandler;
    logTest(logToGstHandler, IClientLogHandler::Level::Fatal);
    logTest(logToGstHandler, IClientLogHandler::Level::Error);
    logTest(logToGstHandler, IClientLogHandler::Level::Warning);
    logTest(logToGstHandler, IClientLogHandler::Level::Milestone);
    logTest(logToGstHandler, IClientLogHandler::Level::Info);
    logTest(logToGstHandler, IClientLogHandler::Level::Debug);
    logTest(logToGstHandler, IClientLogHandler::Level::External);
}
