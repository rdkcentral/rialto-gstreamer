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
