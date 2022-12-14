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

# Add our local cmake directory to search for components
set( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake )
add_compile_options(-Wall -Werror)

find_package(Rialto 1.0 REQUIRED)
find_package(ocdmRialto 1.0 REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER_APP REQUIRED IMPORTED_TARGET gstreamer-app-1.0 gstreamer-pbutils-1.0)

add_library(gstrialtosinks

        SHARED
        source/RialtoGStreamerMSEVideoSink.cpp
        source/RialtoGStreamerMSEAudioSink.cpp
        source/GStreamerUtils.cpp
        source/GStreamerEMEUtils.cpp
        source/GStreamerMSEUtils.cpp
        source/GStreamerMSEMediaPlayerClient.cpp
        source/RialtoGSteamerPlugin.cpp
        source/RialtoGStreamerMSEBaseSink.cpp
        source/MediaPlayerManager.cpp
        source/Timer.cpp
        source/BufferParser.cpp
        )

target_include_directories(gstrialtosinks
        PUBLIC
        $<INSTALL_INTERFACE:include/rialto>

        PRIVATE
        ${RIALTO_INCLUDE_DIR}
        ${RIALTOOCDM_INCLUDE_DIR}
        ${GSTREAMER_APP_INCLUDE_DIRS}
        )

target_link_libraries(gstrialtosinks
        PUBLIC
        ${GSTREAMER_APP_LIBRARIES}

        PRIVATE
	Rialto::ocdmRialto
        Rialto::RialtoClient
        Threads::Threads
        )

#needed by GST_PLUGIN_DEFINE
target_compile_options(gstrialtosinks
        PRIVATE
        -DPACKAGE="gstrialtosinks"
        -DVERSION="1.0"
        )

set_target_properties(gstrialtosinks
        PROPERTIES LINK_FLAGS "-Wl,--unresolved-symbols=report-all"
        )

install(
       TARGETS gstrialtosinks
       DESTINATION lib/gstreamer-1.0
       )
