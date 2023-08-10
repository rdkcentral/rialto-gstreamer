# Copyright (C) 2023 Sky UK
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation;
# version 2.1 of the License.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

if ( COVERAGE_ENABLED )
    add_compile_options(-coverage)
endif()

add_library(
        gstRialtoTestLib

        STATIC

        ${CMAKE_SOURCE_DIR}/source/RialtoGStreamerMSEVideoSink.cpp
        ${CMAKE_SOURCE_DIR}/source/RialtoGStreamerMSEAudioSink.cpp
        ${CMAKE_SOURCE_DIR}/source/RialtoGStreamerWebAudioSink.cpp
        ${CMAKE_SOURCE_DIR}/source/GStreamerUtils.cpp
        ${CMAKE_SOURCE_DIR}/source/GStreamerEMEUtils.cpp
        ${CMAKE_SOURCE_DIR}/source/GStreamerMSEUtils.cpp
        ${CMAKE_SOURCE_DIR}/source/GStreamerMSEMediaPlayerClient.cpp
        ${CMAKE_SOURCE_DIR}/source/GStreamerWebAudioPlayerClient.cpp
        ${CMAKE_SOURCE_DIR}/source/MessageQueue.cpp
        ${CMAKE_SOURCE_DIR}/source/RialtoGSteamerPlugin.cpp
        ${CMAKE_SOURCE_DIR}/source/RialtoGStreamerMSEBaseSink.cpp
        ${CMAKE_SOURCE_DIR}/source/MediaPlayerManager.cpp
        ${CMAKE_SOURCE_DIR}/source/Timer.cpp
        ${CMAKE_SOURCE_DIR}/source/BufferParser.cpp
)

target_include_directories(
        gstRialtoTestLib

        PUBLIC
        ${GStreamerApp_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/tests/third-party/include
        ${CMAKE_SOURCE_DIR}/source
        )

target_link_libraries(
        gstRialtoTestLib

        GstRialtoMocks
        gstRialtoThirdParty
        ${GStreamerApp_LIBRARIES}
)

target_compile_options(gstRialtoTestLib
        PUBLIC
        -DPACKAGE="gstrialtosinks"
        -DVERSION="1.0"
        )

set_target_properties(gstRialtoTestLib
        PROPERTIES LINK_FLAGS "-Wl,--unresolved-symbols=report-all"
        )

if ( COVERAGE_ENABLED )
    target_link_libraries(
        gstRialtoTestLib

        gcov
        )
endif()

add_gtests (
        GstRialtoUnitTests

        # gtest code
        BufferParserTests.cpp
        ControlBackendTests.cpp
        GStreamerEmeUtilsTests.cpp
        GstreamerMseAudioSinkTests.cpp
        GstreamerMseMediaPlayerClientTests.cpp
        GStreamerUtilsTests.cpp
        GstreamerWebAudioPlayerClientTests.cpp
        Matchers.cpp
        MediaPlayerClientBackendTests.cpp
        MediaPlayerManagerTests.cpp
        MessageQueueTests.cpp
        RialtoGstTest.cpp
        TimerTests.cpp
        WebAudioClientBackendTests.cpp
        )

target_include_directories(
        GstRialtoUnitTests

        PRIVATE
        $<TARGET_PROPERTY:GstRialtoMocks,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:gstRialtoTestLib,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(
        GstRialtoUnitTests

        gstRialtoThirdParty
        gstRialtoTestLib
)

if ( COVERAGE_ENABLED )
    target_link_libraries(
        GstRialtoUnitTests

        gcov
        )
endif()
