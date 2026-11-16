# Copyright (C) 2026 Sky UK
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

# Component tests: the plugin code and the real RialtoClient library talk over the real Rialto IPC to the
# ServerStub of the Rialto client component tests (tests/componenttests/client in the Rialto repository),
# whose server modules are GoogleMock objects controlled by the tests.

set( RIALTO_SOURCE_DIR "" CACHE PATH "Path to the Rialto source tree (https://github.com/rdkcentral/rialto)" )
get_filename_component( RIALTO_SOURCE_DIR "${RIALTO_SOURCE_DIR}" ABSOLUTE BASE_DIR "${PROJECT_SOURCE_DIR}" )
if( NOT EXISTS "${RIALTO_SOURCE_DIR}/CMakeLists.txt" )
    message( FATAL_ERROR "Rialto source not found. Configure with -DRIALTO_SOURCE_DIR=<path to the rialto repository>" )
endif()
set( RIALTO_CT_CLIENT_DIR "${RIALTO_SOURCE_DIR}/tests/componenttests/client" )

set( RIALTO_OCDM_BRANCH "master" CACHE STRING "rialto-ocdm branch used for the protection metadata sources" )

# Rialto defines its own SRCREV/TAGS; inheriting ours would redefine them and fail with -Werror
set_property( DIRECTORY PROPERTY COMPILE_DEFINITIONS "" )

# Rialto is configured with the same CMAKE_BUILD_FLAG (ComponentTests): this builds RialtoClient, the IPC and the
# protobuf libraries without the platform dependencies (ocdm, WPEFramework) that a CI machine does not have.
# Only the targets linked below are built.
add_subdirectory( ${RIALTO_SOURCE_DIR} rialto EXCLUDE_FROM_ALL )

find_package( Threads REQUIRED )
find_package( Protobuf REQUIRED ) # protobuf::libprotobuf is part of the link interface of the Rialto IPC libraries
find_package( GTest REQUIRED )

if( CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 13.0 )
    set( CT_EXTRA_WARNING_OPTIONS -Wno-error=overloaded-virtual ) # same as Rialto, needed by the module mocks
endif()

# ------------------------------------------------------------------------------------------------------------------
# rialto-ocdm protection metadata (used by the plugin for encrypted buffers), same files as the unit tests use
set( OCDM_DIR ${CMAKE_CURRENT_BINARY_DIR}/rialto-ocdm )
set( OCDM_URL https://raw.githubusercontent.com/rdkcentral/rialto-ocdm/${RIALTO_OCDM_BRANCH}/library )
foreach( OCDM_FILE include/RialtoGStreamerEMEProtectionMetadata.h source/RialtoGStreamerEMEProtectionMetadata.cpp )
    get_filename_component( OCDM_FILE_NAME ${OCDM_FILE} NAME )
    if( NOT EXISTS ${OCDM_DIR}/${OCDM_FILE_NAME} )
        file( DOWNLOAD ${OCDM_URL}/${OCDM_FILE} ${OCDM_DIR}/${OCDM_FILE_NAME} STATUS OCDM_DOWNLOAD_STATUS )
        list( GET OCDM_DOWNLOAD_STATUS 0 OCDM_DOWNLOAD_RESULT )
        if( NOT OCDM_DOWNLOAD_RESULT EQUAL 0 )
            file( REMOVE ${OCDM_DIR}/${OCDM_FILE_NAME} )
            message( FATAL_ERROR "Could not download ${OCDM_URL}/${OCDM_FILE}: ${OCDM_DOWNLOAD_STATUS}" )
        endif()
    endif()
endforeach()

# ------------------------------------------------------------------------------------------------------------------
# Code under test: the plugin sources (same list as source/CMakeLists.txt), built as a static GStreamer plugin.
# GST_PLUGIN_BUILD_STATIC makes GST_PLUGIN_DEFINE generate gst_plugin_rialtosinks_register().
set( PLUGIN_DIR ${PROJECT_SOURCE_DIR}/source )
add_library(
        gstRialtoCtPlugin
        STATIC

        ${PLUGIN_DIR}/GstreamerCatLog.cpp
        ${PLUGIN_DIR}/RialtoGStreamerMSEVideoSink.cpp
        ${PLUGIN_DIR}/RialtoGStreamerMSEAudioSink.cpp
        ${PLUGIN_DIR}/RialtoGStreamerWebAudioSink.cpp
        ${PLUGIN_DIR}/RialtoGStreamerMSESubtitleSink.cpp
        ${PLUGIN_DIR}/GStreamerUtils.cpp
        ${PLUGIN_DIR}/GStreamerEMEUtils.cpp
        ${PLUGIN_DIR}/GStreamerMSEUtils.cpp
        ${PLUGIN_DIR}/GStreamerMSEMediaPlayerClient.cpp
        ${PLUGIN_DIR}/GStreamerWebAudioPlayerClient.cpp
        ${PLUGIN_DIR}/MessageQueue.cpp
        ${PLUGIN_DIR}/PullModeAudioPlaybackDelegate.cpp
        ${PLUGIN_DIR}/PullModePlaybackDelegate.cpp
        ${PLUGIN_DIR}/PullModeSubtitlePlaybackDelegate.cpp
        ${PLUGIN_DIR}/PullModeVideoPlaybackDelegate.cpp
        ${PLUGIN_DIR}/PushModeAudioPlaybackDelegate.cpp
        ${PLUGIN_DIR}/RialtoGSteamerPlugin.cpp
        ${PLUGIN_DIR}/RialtoGStreamerMSEBaseSink.cpp
        ${PLUGIN_DIR}/MediaPlayerManager.cpp
        ${PLUGIN_DIR}/Timer.cpp
        ${PLUGIN_DIR}/BufferParser.cpp
        ${PLUGIN_DIR}/LogToGstHandler.cpp
        ${PLUGIN_DIR}/FlushAndDataSynchronizer.cpp
        ${OCDM_DIR}/RialtoGStreamerEMEProtectionMetadata.cpp
        )

target_include_directories(
        gstRialtoCtPlugin

        PUBLIC
        ${PLUGIN_DIR}
        ${OCDM_DIR}
        )

target_compile_definitions(
        gstRialtoCtPlugin

        PUBLIC
        GST_PLUGIN_BUILD_STATIC

        PRIVATE
        PACKAGE="gstrialtosinks"
        VERSION="1.0"
        SRCREV="${SRCREV}"
        TAGS="${TAGS}"
        COMMIT_ID="${COMMIT_ID}"
        )

target_compile_options( gstRialtoCtPlugin PRIVATE -Wno-deprecated-declarations )

target_link_libraries(
        gstRialtoCtPlugin

        PUBLIC
        RialtoClient
        RialtoPlayerPublic
        RialtoCommonPublic
        PkgConfig::GSTREAMER_APP
        Threads::Threads
        )

# ------------------------------------------------------------------------------------------------------------------
# Simulated RialtoServer: ServerStub and module stubs of the Rialto client component tests (compiled as they are)
add_library(
        gstRialtoCtServerStub
        STATIC

        ${RIALTO_CT_CLIENT_DIR}/stubs/ServerStub.cpp
        ${RIALTO_CT_CLIENT_DIR}/stubs/ControlModuleStub.cpp
        ${RIALTO_CT_CLIENT_DIR}/stubs/MediaPipelineModuleStub.cpp
        ${RIALTO_CT_CLIENT_DIR}/stubs/MediaCapabilitiesModuleStub.cpp
        ${RIALTO_CT_CLIENT_DIR}/stubs/MediaKeysModuleStub.cpp
        ${RIALTO_CT_CLIENT_DIR}/stubs/MediaKeysCapabilitiesModuleStub.cpp
        ${RIALTO_CT_CLIENT_DIR}/stubs/MediaPipelineCapabilitiesModuleStub.cpp
        ${RIALTO_CT_CLIENT_DIR}/stubs/WebAudioPlayerModuleStub.cpp
        )

target_include_directories(
        gstRialtoCtServerStub

        PUBLIC
        ${RIALTO_CT_CLIENT_DIR}/stubs
        ${RIALTO_CT_CLIENT_DIR}/mocks
        ${RIALTO_SOURCE_DIR}/tests/common/protoUtils
        $<TARGET_PROPERTY:RialtoPlayerPublic,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoPlayerCommon,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoCommonPublic,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoIpcServer,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoProtobuf,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:RialtoLogging,INTERFACE_INCLUDE_DIRECTORIES>
        )

target_compile_definitions(
        gstRialtoCtServerStub

        PRIVATE
        RIALTO_LOG_FATAL_ENABLED
        RIALTO_LOG_ERROR_ENABLED
        RIALTO_LOG_WARN_ENABLED
        RIALTO_LOG_MIL_ENABLED
        RIALTO_LOG_INFO_ENABLED
        RIALTO_LOG_DEBUG_ENABLED
        )

# Rialto test code: do not fail the rialto-gstreamer build on warnings in it
target_compile_options( gstRialtoCtServerStub PRIVATE -Wno-error PUBLIC ${CT_EXTRA_WARNING_OPTIONS} )

target_link_libraries(
        gstRialtoCtServerStub

        PUBLIC
        RialtoIpcServer
        RialtoIpcCommon
        RialtoPlayerCommon
        RialtoProtobuf
        RialtoLogging
        protobuf::libprotobuf
        GTest::gmock
        GTest::gtest
        Threads::Threads
        )

# ------------------------------------------------------------------------------------------------------------------
add_executable(
        GstRialtoComponentTests

        main.cpp
        base/GstRialtoComponentTest.cpp
        tests/AudioSinkPlaybackTest.cpp
        )

target_include_directories( GstRialtoComponentTests PRIVATE base )

target_link_libraries(
        GstRialtoComponentTests

        PRIVATE
        gstRialtoCtPlugin
        gstRialtoCtServerStub
        GTest::gmock
        GTest::gtest
        )
