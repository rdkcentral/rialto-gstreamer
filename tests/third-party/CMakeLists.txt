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

# If branch is not specified, cmake will check, if branch with the same name exists in rialto repository
# If not, 'master' branch will be used.
if(BUILD_BRANCH)
    message("======= Branch specified, using '${BUILD_BRANCH}' branch =======")
else()
    # Get current branch name
    execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE RIALTO_OCDM_BRANCH_NAME
        OUTPUT_STRIP_TRAILING_WHITESPACE
	)
    # Check if branch with the same name exists in rialto repository
    execute_process(
        COMMAND git ls-remote --exit-code --heads git@github.com:rdkcentral/rialto.git refs/heads/${RIALTO_OCDM_BRANCH_NAME}
        RESULT_VARIABLE LS_REMOTE_RESULT
    )

    if("0" STREQUAL ${LS_REMOTE_RESULT})
        message("======= No branch specified, using: '${RIALTO_OCDM_BRANCH_NAME}' branch as default =======")
        set(BUILD_BRANCH ${RIALTO_OCDM_BRANCH_NAME})
    else()
        message("======= No branch specified, using 'master' branch as default =======")
        set(BUILD_BRANCH "master")
    endif()
endif()

function(copyLocalFallback FILE)
    set(CANDIDATE_DIRS
        # Check sibling directory first (for local builds)
        "${CMAKE_SOURCE_DIR}/../rialto/common/public/include"
        "${CMAKE_SOURCE_DIR}/../rialto/media/public/include"
        "${CMAKE_SOURCE_DIR}/../rialto/media/public/interface"
        "${CMAKE_SOURCE_DIR}/../rialto/media/client/main/include"
        "${CMAKE_SOURCE_DIR}/../rialto/media/client/ipc/include"
        "${CMAKE_SOURCE_DIR}/../rialto/serverManager/public/include"
        # Check subdirectory (for GitHub Actions CI)
        "${CMAKE_SOURCE_DIR}/rialto/common/public/include"
        "${CMAKE_SOURCE_DIR}/rialto/media/public/include"
        "${CMAKE_SOURCE_DIR}/rialto/media/public/interface"
        "${CMAKE_SOURCE_DIR}/rialto/media/client/main/include"
        "${CMAKE_SOURCE_DIR}/rialto/media/client/ipc/include"
        "${CMAKE_SOURCE_DIR}/rialto/serverManager/public/include")

    message(STATUS "Looking for ${FILE} in fallback locations...")
    foreach(candidate IN LISTS CANDIDATE_DIRS)
        message(STATUS "  Checking: ${candidate}/${FILE}")
        if(EXISTS "${candidate}/${FILE}")
            message(STATUS "  ✓ Found at: ${candidate}/${FILE}")
            file(READ "${candidate}/${FILE}" fallbackContents)
            file(WRITE "${CMAKE_SOURCE_DIR}/tests/third-party/include/${FILE}" "${fallbackContents}")
            return()
        endif()
    endforeach()

    message(FATAL_ERROR "No local fallback found for ${FILE}")
endfunction()

function(downloadAndCheck URL FILE)
    if(FILE MATCHES "RialtoGStreamerEMEProtectionMetadata.cpp")
        set(FILE_PATH ${CMAKE_SOURCE_DIR}/tests/third-party/source/${FILE})
    else()
        set(FILE_PATH ${CMAKE_SOURCE_DIR}/tests/third-party/include/${FILE})
    endif()

    if(FILE STREQUAL "MediaCommon.h" OR FILE STREQUAL "AudioDecoderCapabilities.h" OR FILE STREQUAL "VideoDecoderCapabilities.h" OR FILE STREQUAL "IMediaPipelineCapabilities.h" OR FILE STREQUAL "IMediaCapabilities.h")
        message(WARNING "Using local Rialto header for ${FILE} to match the current API")
        copyLocalFallback(${FILE})
        return()
    endif()

    if(NOT EXISTS ${FILE_PATH})
        file(DOWNLOAD ${URL} ${FILE_PATH} STATUS downloadStatus LOG downloadLog)
        list(GET downloadStatus 0 statusCode)
        if(NOT statusCode EQUAL 0 OR NOT EXISTS ${FILE_PATH})
            message(WARNING "Download of ${FILE} failed (${URL}); using local Rialto fallback")
            copyLocalFallback(${FILE})
        else()
            message("Downloaded ${FILE}")
        endif()
    endif()

    if(EXISTS ${FILE_PATH})
        file(READ ${FILE_PATH} fileContents)
        if("${fileContents}" STREQUAL "")
            message(WARNING "The downloaded ${FILE} file is empty; using local Rialto fallback")
            copyLocalFallback(${FILE})
        endif()
    else()
        message(FATAL_ERROR "The file ${FILE} is missing after download and fallback attempts.")
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
    IMediaCapabilities.h
    ControlCommon.h
    IControlClient.h
    IControl.h
    IWebAudioPlayer.h
    IWebAudioPlayerClient.h
    IClientLogHandler.h
    IClientLogControl.h
    AudioDecoderCapabilities.h
    VideoDecoderCapabilities.h
)

# Loop through the list and call the function
foreach(FILE_NAME ${FILE_LIST})
    if(FILE_NAME MATCHES "RialtoGStreamerEMEProtectionMetadata.h")
        set(BASE_URL https://raw.githubusercontent.com/rdkcentral/rialto-ocdm/master/library/include/)
    elseif(FILE_NAME MATCHES "RialtoGStreamerEMEProtectionMetadata.cpp")
        set(BASE_URL https://raw.githubusercontent.com/rdkcentral/rialto-ocdm/master/library/source/)
    elseif(FILE_NAME STREQUAL "AudioDecoderCapabilities.h" OR FILE_NAME STREQUAL "VideoDecoderCapabilities.h")
        set(BASE_URL https://raw.githubusercontent.com/rdkcentral/rialto/${BUILD_BRANCH}/common/public/include/)
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
    source/MediaCapabilitiesFactory.cpp
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
