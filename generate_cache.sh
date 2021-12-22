mkdir build
cd build
rm CMakeCache.txt

cmakeArgs=(
    -DCMAKE_BUILD_TYPE=$1
    -DBUILD_SHARED_LIBS=OFF
    -DBUILD_CLAR=OFF
    -DTHREADSAFE=ON
    -DUSE_SSH=OFF
    -DUSE_HTTPS=OFF
    -DOPENSSL_ROOT_DIR=/usr/local/ssl
    -DCMAKE_C_COMPILER=/usr/bin/gcc
    -DCMAKE_CXX_COMPILER=/usr/bin/g++
)

if [ "$2" == "--trace" ]; then
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
