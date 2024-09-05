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
#include <iostream>
using namespace std;

#include "ClientLogControlMock.h"
#include "LogToGstHandler.h"

using testing::_;
using testing::IsNull;
using testing::NotNull;
using testing::Return;
using testing::StrictMock;

using namespace firebolt::rialto;

class LogToGstHandlerTest : public testing::Test
{
public:
    void logTest(client::LogToGstHandler &logHandler, IClientLogHandler::Level level)
    {
        logHandler.log(level, "testFile", 1, "testFunction", "testMessage");
    }

    ~LogToGstHandlerTest() override {}

    std::shared_ptr<StrictMock<firebolt::rialto::ClientLogControlFactoryMock>> m_clientLogControlFactoryMock{
        std::dynamic_pointer_cast<StrictMock<firebolt::rialto::ClientLogControlFactoryMock>>(
            firebolt::rialto::IClientLogControlFactory::createFactory())};
    ::StrictMock<firebolt::rialto::ClientLogControlMock> m_clientLogControlMock;
};

TEST_F(LogToGstHandlerTest, callingLogHandlerAtAllLevelsShouldSucceed)
{
    client::LogToGstHandler logToGstHandler;
    logTest(logToGstHandler, IClientLogHandler::Level::Fatal);
    logTest(logToGstHandler, IClientLogHandler::Level::Error);
    logTest(logToGstHandler, IClientLogHandler::Level::Warning);
    logTest(logToGstHandler, IClientLogHandler::Level::Milestone);
    logTest(logToGstHandler, IClientLogHandler::Level::Info);
    logTest(logToGstHandler, IClientLogHandler::Level::Debug);
    logTest(logToGstHandler, IClientLogHandler::Level::External);
}

TEST_F(LogToGstHandlerTest, callingLogToGstSinkInitShouldWork)
{
    EXPECT_CALL(*m_clientLogControlFactoryMock, createClientLogControl()).WillRepeatedly(ReturnRef(m_clientLogControlMock));
    EXPECT_CALL(m_clientLogControlMock, registerLogHandler(NotNull(), _)).WillOnce(Return(true));
    EXPECT_CALL(m_clientLogControlMock, registerLogHandler(IsNull(), _)).WillOnce(Return(true));

    client::LogToGstHandler::logToGstSinkInit();
    client::LogToGstHandler::logToGstSinkFinalize();
}

TEST_F(LogToGstHandlerTest, ifRegisterLogHandlerFailsThenItShouldRetry)
{
    EXPECT_CALL(*m_clientLogControlFactoryMock, createClientLogControl()).WillRepeatedly(ReturnRef(m_clientLogControlMock));

    EXPECT_CALL(m_clientLogControlMock, registerLogHandler(NotNull(), _)).WillOnce(Return(false));
    client::LogToGstHandler::logToGstSinkInit(); // This should call registerLogHandler()

    EXPECT_CALL(m_clientLogControlMock, registerLogHandler(NotNull(), _)).WillOnce(Return(true));
    client::LogToGstHandler::logToGstSinkInit(); // This should retry calling registerLogHandler()

    client::LogToGstHandler::logToGstSinkInit(); // This should NOT call registerLogHandler() again

    client::LogToGstHandler::logToGstSinkFinalize(); // This should NOT call registerLogHandler(), refcount = 2

    EXPECT_CALL(m_clientLogControlMock, registerLogHandler(IsNull(), _)).WillOnce(Return(true));
    client::LogToGstHandler::logToGstSinkFinalize(); // This should call registerLogHandler() to de-register
}

TEST_F(LogToGstHandlerTest, ifRegisterLogHandlerFailsToCancelThenItsLikePreregistration)
{
    EXPECT_CALL(*m_clientLogControlFactoryMock, createClientLogControl()).WillRepeatedly(ReturnRef(m_clientLogControlMock));

    EXPECT_CALL(m_clientLogControlMock, registerLogHandler(NotNull(), _)).WillOnce(Return(true));
    client::LogToGstHandler::logToGstSinkInit(); // This should retry calling registerLogHandler()

    EXPECT_CALL(m_clientLogControlMock, registerLogHandler(IsNull(), _)).WillOnce(Return(false));
    client::LogToGstHandler::logToGstSinkFinalize(); // This should call registerLogHandler() to de-register

    client::LogToGstHandler::logToGstSinkInit(); // This should not call registerLogHandler()

    EXPECT_CALL(m_clientLogControlMock, registerLogHandler(IsNull(), _)).WillOnce(Return(true));
    client::LogToGstHandler::logToGstSinkFinalize(); // This should call registerLogHandler() to de-register
}

TEST_F(LogToGstHandlerTest, logToGstSinkFinalizeCalledTooMuchShouldDoNothing)
{
    // Calling finalise before init should do nothing
    client::LogToGstHandler::logToGstSinkFinalize();

    // Functionality should still be normal...
    EXPECT_CALL(*m_clientLogControlFactoryMock, createClientLogControl()).WillRepeatedly(ReturnRef(m_clientLogControlMock));
    EXPECT_CALL(m_clientLogControlMock, registerLogHandler(NotNull(), _)).WillOnce(Return(true));
    EXPECT_CALL(m_clientLogControlMock, registerLogHandler(IsNull(), _)).WillOnce(Return(true));

    client::LogToGstHandler::logToGstSinkInit();
    client::LogToGstHandler::logToGstSinkFinalize();
}
