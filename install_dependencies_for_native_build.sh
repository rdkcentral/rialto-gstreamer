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
# Entry script for building the dependencies for Rialto-Gstreamer Native Build.

# Read input variables.
BRANCH=master
WORK_DIR=${HOME}
if [ $# -eq 1 ]; then
    BRANCH=$1
elif [ $# -eq 2 ]; then
    BRANCH=$1
    WORK_DIR=$2
fi

echo "Branch: ${BRANCH}"
echo "Work dir: ${WORK_DIR}"

# Install the dependencies for building Rialto and Rialto-Gstreamer Native Build.
apt-get update
apt-get install -y build-essential
apt-get install -y cmake
apt-get install -y libunwind-dev libgstreamer-plugins-base1.0-dev libgstreamer-plugins-bad1.0-dev libgstreamer1.0-dev libyaml-cpp-dev
apt-get install -y protobuf-compiler

# Clone the rialto repository and build the dependencies for Rialto-Gstreamer Native Build.
cd "${WORK_DIR}"
git clone --branch "${BRANCH}" --depth 1 https://github.com/rdkcentral/rialto.git
cd "${WORK_DIR}/rialto"
NATIVE_DIR="${WORK_DIR}/native"
cmake . -B build  -DCMAKE_INSTALL_PREFIX="${NATIVE_DIR}" -DNATIVE_BUILD=ON -DRIALTO_BUILD_TYPE="Debug"
make -C build install -j$(nproc)