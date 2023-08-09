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

#include "ControlBackend.h"
#include "ControlMock.h"
#include <gtest/gtest.h>
#include <memory>

using firebolt::rialto::ApplicationState;
using firebolt::rialto::ControlFactoryMock;
using firebolt::rialto::ControlMock;
using firebolt::rialto::IControlClient;
using firebolt::rialto::IControlFactory;
using firebolt::rialto::client::ControlBackend;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;
using testing::SetArgReferee;
using testing::StrictMock;

class ControlBackendTests : public testing::Test
{
public:
    std::shared_ptr<StrictMock<ControlFactoryMock>> m_controlFactoryMock{
        std::dynamic_pointer_cast<StrictMock<ControlFactoryMock>>(IControlFactory::createFactory())};
    std::shared_ptr<StrictMock<ControlMock>> m_controlMock{std::make_shared<StrictMock<ControlMock>>()};
    std::unique_ptr<ControlBackend> m_sut{nullptr};
};

TEST_F(ControlBackendTests, ShouldFailToStartWhenControlIsNull)
{
    EXPECT_CALL(*m_controlFactoryMock, createControl()).WillOnce(Return(nullptr));
    m_sut = std::make_unique<ControlBackend>();
}

TEST_F(ControlBackendTests, ShouldFailToStartWhenRegisterClientFails)
{
    EXPECT_CALL(*m_controlFactoryMock, createControl()).WillOnce(Return(m_controlMock));
    EXPECT_CALL(*m_controlMock, registerClient(_, _)).WillOnce(Return(false));
    m_sut = std::make_unique<ControlBackend>();
}

TEST_F(ControlBackendTests, ShouldSkipWaitingForRunningWhenRunningStateWasSetDuringInitialisation)
{
    EXPECT_CALL(*m_controlFactoryMock, createControl()).WillOnce(Return(m_controlMock));
    EXPECT_CALL(*m_controlMock, registerClient(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(ApplicationState::RUNNING), Return(true)));
    m_sut = std::make_unique<ControlBackend>();
    EXPECT_TRUE(m_sut->waitForRunning());
}

TEST_F(ControlBackendTests, ShouldSkipWaitingForRunningWhenRunningStateWasSetEarlier)
{
    std::weak_ptr<IControlClient> weakClient;
    EXPECT_CALL(*m_controlFactoryMock, createControl()).WillOnce(Return(m_controlMock));
    EXPECT_CALL(*m_controlMock, registerClient(_, _))
        .WillOnce(DoAll(SaveArg<0>(&weakClient), SetArgReferee<1>(ApplicationState::INACTIVE), Return(true)));
    m_sut = std::make_unique<ControlBackend>();
    auto client = weakClient.lock();
    ASSERT_TRUE(client);
    client->notifyApplicationState(ApplicationState::RUNNING);
    EXPECT_TRUE(m_sut->waitForRunning());
}

TEST_F(ControlBackendTests, ShouldFailToWaitForRunning)
{
    EXPECT_CALL(*m_controlFactoryMock, createControl()).WillOnce(Return(m_controlMock));
    EXPECT_CALL(*m_controlMock, registerClient(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(ApplicationState::INACTIVE), Return(true)));
    m_sut = std::make_unique<ControlBackend>();
    EXPECT_FALSE(m_sut->waitForRunning());
}

TEST_F(ControlBackendTests, ShouldRemoveControlBackend)
{
    EXPECT_CALL(*m_controlFactoryMock, createControl()).WillOnce(Return(m_controlMock));
    EXPECT_CALL(*m_controlMock, registerClient(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(ApplicationState::RUNNING), Return(true)));
    m_sut = std::make_unique<ControlBackend>();
    m_sut->removeControlBackend();
}
