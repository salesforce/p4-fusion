#!/bin/bash

cd "$(dirname "${BASH_SOURCE[0]}")"
set -euxo pipefail

CORES=$(grep -c \^processor /proc/cpuinfo 2>/dev/null || sysctl -n hw.ncpu)
cmake --build build/ -j$CORES --target all
