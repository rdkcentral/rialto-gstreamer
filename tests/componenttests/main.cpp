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

#include <google/protobuf/stubs/common.h>
#include <gst/gst.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <string>
#include <unistd.h>

class ProtobufCleaner : public ::testing::Environment
{
public:
    void TearDown() override { google::protobuf::ShutdownProtobufLibrary(); }
};

int main(int argc, char **argv) // NOLINT(build/filename_format)
{
    // Socket of the simulated RialtoServer (read by ServerStub and RialtoClient). Setting it also makes the plugin
    // register its sinks with the highest rank.
    const std::string kSocketPath{"/tmp/rialto-gst-ct-" + std::to_string(getpid())};
    setenv("RIALTO_SOCKET_PATH", kSocketPath.c_str(), 1);

    gst_init(&argc, &argv);
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new ProtobufCleaner);

    const int kResult{RUN_ALL_TESTS()};
    unlink(kSocketPath.c_str());
    return kResult;
}
