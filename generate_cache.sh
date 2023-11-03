#!/bin/bash

mkdir build
cd build
rm CMakeCache.txt

cmakeArgs=(
  -DCMAKE_BUILD_TYPE=$1
  -DBUILD_SHARED_LIBS=OFF
  -DBUILD_CLAR=OFF
  -DBUILD_EXAMPLES=OFF
  -DUSE_BUNDLED_ZLIB=ON
  -DREGEX_BACKEND=builtin
  -DTHREADSAFE=ON
  -DUSE_SSH=OFF
  -DUSE_HTTPS=OFF
  -DUSE_THREADS=ON
  -DOPENSSL_ROOT_DIR="${OPENSSL_ROOT_DIR:-"/usr/local/ssl"}"
  -DCMAKE_C_COMPILER="${CMAKE_C_COMPILER:-"/usr/bin/gcc"}"
  -DCMAKE_CXX_COMPILER="${CMAKE_CXX_COMPILER:-"/usr/bin/g++"}"
)

# Decide if tests/ should be built
if [[ "$2" == *"t"* ]]; then
  cmakeArgs+=(
    -DBUILD_TESTS=ON
  )
else
  cmakeArgs+=(
    -DBUILD_TESTS=OFF
  )
fi

# Decide if profiling needs to be enabled
if [[ "$2" == *"p"* ]]; then
  cmakeArgs+=(
    -DMTR_ENABLED=ON
  )
else
  cmakeArgs+=(
    -DMTR_ENABLED=OFF
  )
fi

echo "Using CMake arguments: \n${cmakeArgs[@]}"
cmake .. "${cmakeArgs[@]}"
