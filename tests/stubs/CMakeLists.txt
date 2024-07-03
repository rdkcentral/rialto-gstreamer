# Copyright (C) 2024 Sky UK
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

add_library(
    gstRialtoStubs

    STATIC

    source/PlaybinStub.cpp
)

target_compile_options(gstRialtoStubs
        PUBLIC
        -DPACKAGE="gstrialtosinks"
        -DVERSION="1.0"
        )

target_include_directories(
    gstRialtoStubs

    PUBLIC
    include

    PRIVATE
    ${GSTREAMER_APP_INCLUDE_DIRS}
)

target_link_libraries(
    gstRialtoStubs

    ${GSTREAMER_APP_LIBRARIES}
)
