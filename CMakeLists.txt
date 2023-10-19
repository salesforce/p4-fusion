cmake_minimum_required(VERSION 3.16)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif ()

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(BUILD_CLAR "Build Clar" OFF)
option(BUILD_EXAMPLES "Build examples" OFF)
option(USE_BUNDLED_ZLIB "Use bundled zlib" ON)
option(THREADSAFE "Threadsafe" ON)
option(USE_SSH "Use SSH" OFF)
option(USE_HTTPS "Use HTTPS" OFF)
option(USE_THREADS "Use threads" ON)
if (APPLE)
    set(OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl@1.1/")
endif()
if (UNIX)
    set(OPENSSL_ROOT_DIR "/usr/local/ssl/")
endif()
set(CMAKE_C_COMPILER "/usr/bin/gcc")
set(CMAKE_CXX_COMPILER "/usr/bin/g++")

option(MTR_ENABLED "Enable minitrace profiling" OFF)
option(BUILD_TESTS "Build tests" OFF)

set(REGEX_BACKEND "Regex backend" "builtin")
set(CXX_STANDARD_REQUIRED true)
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_C_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

project(
        p4-fusion
        LANGUAGES CXX
        VERSION 1.0
        DESCRIPTION "Custom tool to convert P4 depots to Git repositories using the Helix Core C++ API"
)

set(BUILD_CLAR OFF)
set(BUILD_SHARED_LIBS OFF)

if (UNIX)
    set(HELIX_API vendor/helix-core-api/linux)
endif ()
if (APPLE)
    set(HELIX_API vendor/helix-core-api/mac)
endif ()

add_subdirectory(vendor)
add_subdirectory(p4-fusion)

if (BUILD_TESTS)
    message(STATUS "Building tests")
    add_subdirectory(tests)
endif ()
