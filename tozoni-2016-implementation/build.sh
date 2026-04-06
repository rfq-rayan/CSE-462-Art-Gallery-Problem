#!/bin/sh
# Configure and build (Release) with optional CMake args, e.g.:
#   ./build.sh -DGLPK_ROOT=/usr/local
set -e
cmake -S "$(dirname "$0")" -B build -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build build -j"$(nproc)"
