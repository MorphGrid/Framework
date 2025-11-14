#!/usr/bin/env bash
set -euo pipefail

BUILD_TYPE=${1:-debug}
LINK_TYPE=${2:-static}

mkdir -p build
cd build

if [[ "$LINK_TYPE" == "static" ]]; then
ENABLE_STATIC="ON"
else
ENABLE_STATIC="OFF"
fi
cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DENABLE_TESTS=ON  -DENABLE_CI=ON -DENABLE_STATIC_LINKING=$ENABLE_STATIC

make -j4
ctest --output-on-failure --verbose

cd ..
gcovr --filter='src/' --print-summary --sort-percentage --gcov-ignore-parse-errors=negative_hits.warn
gcovr --filter='src/' --sonarqube > coverage.xml