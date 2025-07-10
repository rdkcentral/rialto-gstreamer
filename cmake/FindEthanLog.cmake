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

find_path(EthanLog_INCLUDE_DIR
  NAMES ethanlog.h
  )

find_library(EthanLog_LIBRARY
  NAMES ethanlog
  )

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(EthanLog DEFAULT_MSG EthanLog_INCLUDE_DIR EthanLog_LIBRARY)

mark_as_advanced(EthanLog_INCLUDE_DIR EthanLog_LIBRARY)

if (ETHANLOG_FOUND OR EthanLog_FOUND)
  set(EthanLog_INCLUDE_DIRS "${EthanLog_INCLUDE_DIR}")
  set(EthanLog_LIBRARIES "${EthanLog_LIBRARY}")
  set(EthanLog_FOUND TRUE)
endif()

if (EthanLog_FOUND AND NOT TARGET EthanLog::EthanLog)
  add_library(EthanLog::EthanLog INTERFACE IMPORTED)
  set_property(TARGET EthanLog::EthanLog PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${EthanLog_INCLUDE_DIR}")
  set_property(TARGET EthanLog::EthanLog PROPERTY INTERFACE_LINK_LIBRARIES "${EthanLog_LIBRARY}")
endif()
