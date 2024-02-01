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

function(downloadAndCheck URL FILE)
    if(FILE MATCHES "RialtoGStreamerEMEProtectionMetadata.cpp") # Special case for the source file
        set(FILE_PATH ${CMAKE_SOURCE_DIR}/tests/third-party/source/${FILE})
    else()
        set(FILE_PATH ${CMAKE_SOURCE_DIR}/tests/third-party/include/${FILE})
    endif()

    if(NOT EXISTS ${FILE_PATH})
        file(DOWNLOAD ${URL} ${FILE_PATH})
        message("Downloaded ${FILE}")
        
        # If the file is empty
        file(READ ${FILE_PATH} fileContents)
        if("${fileContents}" STREQUAL "")
            message(FATAL_ERROR "The downloaded ${FILE} file is empty!")
        endif()
    endif()
endfunction()


# List of files to download
set(FILE_LIST
    RialtoGStreamerEMEProtectionMetadata.h
    RialtoGStreamerEMEProtectionMetadata.cpp
    IMediaPipeline.h
    IMediaPipelineClient.h
    MediaCommon.h
    IMediaPipelineCapabilities.h
    ControlCommon.h
    IControlClient.h
    IControl.h
    IWebAudioPlayer.h
    IWebAudioPlayerClient.h
    IClientLogHandler.h
    IClientLogControl.h
)

# Loop through the list and call the function
foreach(FILE_NAME ${FILE_LIST})
    if(FILE_NAME MATCHES "RialtoGStreamerEMEProtectionMetadata.h") # Special case for header file as the path is different
        set(BASE_URL https://raw.githubusercontent.com/rdkcentral/rialto-ocdm/master/library/include/)
    elseif(FILE_NAME MATCHES "RialtoGStreamerEMEProtectionMetadata.cpp") # Special case for source file as the path is different
        set(BASE_URL https://raw.githubusercontent.com/rdkcentral/rialto-ocdm/master/library/source/)
    else()
        set(BASE_URL https://raw.githubusercontent.com/rdkcentral/rialto/${BUILD_BRANCH}/media/public/include/)
    endif()

    downloadAndCheck(${BASE_URL}${FILE_NAME} ${FILE_NAME})
endforeach()

add_library(
    gstRialtoThirdParty

    STATIC

    source/ControlFactory.cpp
    source/ClientLogControlFactory.cpp
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
