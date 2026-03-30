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
WORK_DIR=${HOME}
if [ $# -eq 1 ]; then
    WORK_DIR=$1
fi

echo "Work dir: ${WORK_DIR}"

# Build the project.
NATIVE_DIR=$WORK_DIR/native
cd rialto-gstreamer
echo "@@@ GSTREAMER BUILD"
cmake . -B build -DCMAKE_INCLUDE_PATH="${NATIVE_DIR}/include"  -DCMAKE_LIBRARY_PATH="${NATIVE_DIR}/lib" -DNATIVE_BUILD=ON -DRIALTO_BUILD_TYPE="Debug"
make -C build -j$(nproc)