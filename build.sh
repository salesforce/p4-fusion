#!/bin/bash

cd build/
CORES=$(grep -c \^processor /proc/cpuinfo 2>/dev/null || sysctl -n hw.ncpu)
cmake --build . -j$CORES --target all
