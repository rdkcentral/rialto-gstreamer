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

# RialtoGStreamerEMEProtectionMetadata.h

set( CMAKE_CXX_STANDARD 17 )

set( CMAKE_CXX_STANDARD_REQUIRED ON )
include( CheckCXXCompilerFlag )

# Default to 'master' if branch is not specified
if(BUILD_BRANCH)
    message("======= Branch specified, using '${BUILD_BRANCH}' branch =======")
else()
    set(BUILD_BRANCH "master")
    message("======= No branch specified, using 'master' branch as default =======")
endif()


if (NOT EXISTS ${CMAKE_SOURCE_DIR}/tests/third-party/include/IMediaPipeline.h)
    file(DOWNLOAD
        https://raw.githubusercontent.com/rdkcentral/rialto/${BUILD_BRANCH}/media/public/include/IMediaPipeline.h
        ${CMAKE_SOURCE_DIR}/tests/third-party/include/IMediaPipeline.h)
    message("Downloaded IMediaPipeline.h")

    # Check if file is empty or not
    file(READ 
        ${CMAKE_SOURCE_DIR}/tests/third-party/include/IMediaPipeline.h
        fileContents)
    if("${fileContents}" STREQUAL "")
        message(FATAL_ERROR "The downloaded IMediaPipeline.h file is empty!")
    endif()
endif()

if (NOT EXISTS ${CMAKE_SOURCE_DIR}/tests/third-party/include/IMediaPipelineClient.h)
    file(DOWNLOAD
        https://raw.githubusercontent.com/rdkcentral/rialto/${BUILD_BRANCH}/media/public/include/IMediaPipelineClient.h
        ${CMAKE_SOURCE_DIR}/tests/third-party/include/IMediaPipelineClient.h)
    message("Downloaded IMediaPipelineClient.h")

    # Check if file is empty or not
    file(READ 
        ${CMAKE_SOURCE_DIR}/tests/third-party/include/IMediaPipelineClient.h
        fileContents)
    if("${fileContents}" STREQUAL "")
        message(FATAL_ERROR "The downloaded IMediaPipelineClient.h file is empty!")
    endif()
endif()

if (NOT EXISTS ${CMAKE_SOURCE_DIR}/tests/third-party/include/MediaCommon.h)
    file(DOWNLOAD
        https://raw.githubusercontent.com/rdkcentral/rialto/${BUILD_BRANCH}/media/public/include/MediaCommon.h
        ${CMAKE_SOURCE_DIR}/tests/third-party/include/MediaCommon.h)
    message("Downloaded MediaCommon.h")

    # Check if file is empty or not
    file(READ 
        ${CMAKE_SOURCE_DIR}/tests/third-party/include/MediaCommon.h
        fileContents)
    if("${fileContents}" STREQUAL "")
        message(FATAL_ERROR "The downloaded MediaCommon.h file is empty!")
    endif()
endif()

if (NOT EXISTS ${CMAKE_SOURCE_DIR}/tests/third-party/include/IMediaPipelineCapabilities.h)
    file(DOWNLOAD
        https://raw.githubusercontent.com/rdkcentral/rialto/${BUILD_BRANCH}/media/public/include/IMediaPipelineCapabilities.h
        ${CMAKE_SOURCE_DIR}/tests/third-party/include/IMediaPipelineCapabilities.h)
    message("Downloaded IMediaPipelineCapabilities.h")

    # Check if file is empty or not
    file(READ 
        ${CMAKE_SOURCE_DIR}/tests/third-party/include/IMediaPipelineCapabilities.h
        fileContents)
    if("${fileContents}" STREQUAL "")
        message(FATAL_ERROR "The downloaded IMediaPipelineCapabilities.h file is empty!")
    endif()
endif()

if (NOT EXISTS ${CMAKE_SOURCE_DIR}/tests/third-party/include/ControlCommon.h)
    file(DOWNLOAD
        https://raw.githubusercontent.com/rdkcentral/rialto/${BUILD_BRANCH}/media/public/include/ControlCommon.h
        ${CMAKE_SOURCE_DIR}/tests/third-party/include/ControlCommon.h)
    message("Downloaded ControlCommon.h")

    # Check if file is empty or not
    file(READ 
        ${CMAKE_SOURCE_DIR}/tests/third-party/include/ControlCommon.h
        fileContents)
    if("${fileContents}" STREQUAL "")
        message(FATAL_ERROR "The downloaded ControlCommon.h file is empty!")
    endif()
endif()

if (NOT EXISTS ${CMAKE_SOURCE_DIR}/tests/third-party/include/IControlClient.h)
    file(DOWNLOAD
        https://raw.githubusercontent.com/rdkcentral/rialto/${BUILD_BRANCH}/media/public/include/IControlClient.h
        ${CMAKE_SOURCE_DIR}/tests/third-party/include/IControlClient.h)
    message("Downloaded IControlClient.h")

    # Check if file is empty or not
    file(READ 
        ${CMAKE_SOURCE_DIR}/tests/third-party/include/IControlClient.h
        fileContents)
    if("${fileContents}" STREQUAL "")
        message(FATAL_ERROR "The downloaded IControlClient.h file is empty!")
    endif()
endif()

if (NOT EXISTS ${CMAKE_SOURCE_DIR}/tests/third-party/include/IControl.h)
    file(DOWNLOAD
        https://raw.githubusercontent.com/rdkcentral/rialto/${BUILD_BRANCH}/media/public/include/IControl.h
        ${CMAKE_SOURCE_DIR}/tests/third-party/include/IControl.h)
    message("Downloaded IControl.h")

    # Check if file is empty or not
    file(READ 
        ${CMAKE_SOURCE_DIR}/tests/third-party/include/IControl.h
        fileContents)
    if("${fileContents}" STREQUAL "")
        message(FATAL_ERROR "The downloaded IControl.h file is empty!")
    endif()
endif()

if (NOT EXISTS ${CMAKE_SOURCE_DIR}/tests/third-party/include/IWebAudioPlayer.h)
    file(DOWNLOAD
        https://raw.githubusercontent.com/rdkcentral/rialto/${BUILD_BRANCH}/media/public/include/IWebAudioPlayer.h
        ${CMAKE_SOURCE_DIR}/tests/third-party/include/IWebAudioPlayer.h)
    message("Downloaded IWebAudioPlayer.h")

    # Check if file is empty or not
    file(READ 
    ${CMAKE_SOURCE_DIR}/tests/third-party/include/IWebAudioPlayer.h
        fileContents)
    if("${fileContents}" STREQUAL "")
        message(FATAL_ERROR "The downloaded IwebAudioPlayer.h file is empty!")
    endif()
endif()

if (NOT EXISTS ${CMAKE_SOURCE_DIR}/tests/third-party/include/IWebAudioPlayerClient.h)
    file(DOWNLOAD
        https://raw.githubusercontent.com/rdkcentral/rialto/${BUILD_BRANCH}/media/public/include/IWebAudioPlayerClient.h
        ${CMAKE_SOURCE_DIR}/tests/third-party/include/IWebAudioPlayerClient.h)
    message("Downloaded IWebAudioPlayerClient.h")

    # Check if file is empty or not
    file(READ 
    ${CMAKE_SOURCE_DIR}/tests/third-party/include/IWebAudioPlayerClient.h
        fileContents)
    if("${fileContents}" STREQUAL "")
        message(FATAL_ERROR "The downloaded IWebAudioPlayerClient.h file is empty!")
    endif()
endif()

if (NOT EXISTS ${CMAKE_SOURCE_DIR}/tests/third-party/include/RialtoGStreamerEMEProtectionMetadata.h)
    file(DOWNLOAD
        https://raw.githubusercontent.com/rdkcentral/rialto-ocdm/master/library/include/RialtoGStreamerEMEProtectionMetadata.h
        ${CMAKE_SOURCE_DIR}/tests/third-party/include/RialtoGStreamerEMEProtectionMetadata.h)
    message("Downloaded RialtoGStreamerEMEProtectionMetadata.h")        

    # Check if file is empty or not
    file(READ 
        ${CMAKE_SOURCE_DIR}/tests/third-party/include/RialtoGStreamerEMEProtectionMetadata.h
        fileContents)
    if("${fileContents}" STREQUAL "")
        message(FATAL_ERROR "The downloaded RialtoGStreamerEMEProtectionMetadata.h file is empty!")
    endif()
endif()

if (NOT EXISTS ${CMAKE_SOURCE_DIR}/tests/third-party/source/RialtoGStreamerEMEProtectionMetadata.cpp)
    file(DOWNLOAD
        https://raw.githubusercontent.com/rdkcentral/rialto-ocdm/master/library/source/RialtoGStreamerEMEProtectionMetadata.cpp
        ${CMAKE_SOURCE_DIR}/tests/third-party/source/RialtoGStreamerEMEProtectionMetadata.cpp)
    message("Downloaded RialtoGStreamerEMEProtectionMetadata.cpp")

    # Check if file is empty or not
    file(READ 
        ${CMAKE_SOURCE_DIR}/tests/third-party/source/RialtoGStreamerEMEProtectionMetadata.cpp
        fileContents)
    if("${fileContents}" STREQUAL "")
        message(FATAL_ERROR "The downloaded RialtoGStreamerEMEProtectionMetadata.cpp file is empty!")
    endif()
endif()

add_library(
    gstRialtoThirdParty

    STATIC

    source/ControlFactory.cpp
    source/MediaPipelineCapabilitiesFactory.cpp
    source/MediaPipelineFactory.cpp
    source/RialtoGStreamerEMEProtectionMetadata.cpp
    source/WebAudioPlayerFactory.cpp
)

target_include_directories(
    gstRialtoThirdParty

    PUBLIC
    include

    ${CMAKE_SOURCE_DIR}/source
    ${GSTREAMER_APP_INCLUDE_DIRS}
    $<TARGET_PROPERTY:GstRialtoMocks,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(
    gstRialtoThirdParty

    GstRialtoMocks
    ${GSTREAMER_APP_LIBRARIES}
    GoogleTest::gmock
)
