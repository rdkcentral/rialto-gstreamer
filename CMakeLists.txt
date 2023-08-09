# Copyright (C) 2022 Sky UK
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

cmake_minimum_required( VERSION 3.10 )
project( gstrialtosinks LANGUAGES C CXX VERSION 1.0 )

set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_STANDARD 17 )

# Add our local cmake directory to search for components
set( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake )
add_compile_options(-Wall -Werror)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER_APP REQUIRED IMPORTED_TARGET gstreamer-app-1.0 gstreamer-pbutils-1.0 gstreamer-audio-1.0)

# Retrieve the commit ID
execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} 
    RESULT_VARIABLE RESULT 
    OUTPUT_VARIABLE COMMIT_ID 
    OUTPUT_STRIP_TRAILING_WHITESPACE 
)

if(RESULT)
    message("Failed to get git commit ID: ${RESULT}")
endif()

# Preprocesser Variable
add_compile_definitions(COMMIT_ID="${COMMIT_ID}")

# Config and target for building the unit tests
if( NOT CMAKE_BUILD_FLAG STREQUAL "UnitTests" )
    add_subdirectory(source)
else() # UnitTests
    include( cmake/googletest.cmake )

    add_subdirectory( tests/third-party EXCLUDE_FROM_ALL )
    add_subdirectory( tests/mocks EXCLUDE_FROM_ALL )
    add_subdirectory( tests/ut EXCLUDE_FROM_ALL )
endif()
