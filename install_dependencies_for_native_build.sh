#!/bin/bash

# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2026 Sky UK
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
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
apt-get install build-essential
apt-get install cmake
apt-get install libunwind-dev libgstreamer-plugins-base1.0-dev libgstreamer-plugins-bad1.0-dev libgstreamer1.0-dev
apt-get install protobuf-compiler

# Clone the rialto repository and build the dependencies for Rialto-Gstreamer Native Build.
cd ${WORK_DIR}
git clone --branch ${BRANCH} --depth 1 https://github.com/rdkcentral/rialto.git
cd ${WORK_DIR}/rialto
NATIVE_DIR=${WORK_DIR}/native
cmake . -B build  -DCMAKE_INSTALL_PREFIX="${NATIVE_DIR}" -DNATIVE_BUILD=ON -DRIALTO_BUILD_TYPE="Debug"
make -C build install -j$(nproc)