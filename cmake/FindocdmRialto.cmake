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

# - Try to find the Rialto library.
#
# The following are set after configuration is done:
#  RIALTOOCDM_FOUND
#  RIALTOOCDM_INCLUDE_DIRS
#  RIALTOOCDM_LIBRARY_DIRS
#  RIALTOOCDM_LIBRARIES
find_path( RIALTOOCDM_INCLUDE_DIR NAMES RialtoGStreamerEMEProtectionMetadata.h PATH_SUFFIXES rialto)
find_library( RIALTOOCDM_LIBRARY NAMES libocdmRialto.so ocdmRialto )

#message( "RIALTOOCDM_INCLUDE_DIR include dir = ${RIALTOOCDM_INCLUDE_DIR}" )
#message( "RIALTOOCDM_LIBRARY lib = ${RIALTOOCDM_LIBRARY}" )

include( FindPackageHandleStandardArgs )

# Handle the QUIETLY and REQUIRED arguments and set the RIALTOOCDM_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args( ocdmRialto DEFAULT_MSG
        RIALTOOCDM_LIBRARY RIALTOOCDM_INCLUDE_DIR )

mark_as_advanced( RIALTOOCDM_INCLUDE_DIR RIALTOOCDM_LIBRARY )

if( ocdmRialto_FOUND )
    set( RIALTOOCDM_LIBRARIES ${RIALTOOCDM_LIBRARY} )
    set( RIALTOOCDM_INCLUDE_DIRS ${RIALTOOCDM_INCLUDE_DIR} )
endif()

if( ocdmRialto_FOUND AND NOT TARGET Rialto::ocdmRialto )
    add_library( Rialto::ocdmRialto SHARED IMPORTED )
    set_target_properties( Rialto::ocdmRialto PROPERTIES
            IMPORTED_LOCATION "${RIALTOOCDM_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${RIALTOOCDM_INCLUDE_DIR}" )
endif()
