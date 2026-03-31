#!/bin/bash
#
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
#
# Entry script for building the Rialto-Gstreamer Native Build.

# Read input variables.
WORK_DIR=${HOME}
if [ $# -eq 1 ]; then
    WORK_DIR=$1
fi

echo "Work dir: ${WORK_DIR}"

# Build the project.
NATIVE_DIR="${WORK_DIR}/native"
echo "Native dir: ${NATIVE_DIR}"
echo "@@@ GSTREAMER BUILD"
cd "${WORK_DIR}/rialto-gstreamer"
cmake . -B build -DCMAKE_INCLUDE_PATH="${NATIVE_DIR}/include"  -DCMAKE_LIBRARY_PATH="${NATIVE_DIR}/lib" -DNATIVE_BUILD=ON -DRIALTO_BUILD_TYPE="Debug"
make -C build -j$(nproc)