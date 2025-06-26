cmake_minimum_required(VERSION 3.16)

option(MTR_ENABLED "Enable minitrace profiling" OFF)
option(BUILD_TESTS "Build tests" OFF)

set(CXX_STANDARD_REQUIRED true)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
endif()
if (APPLE)
    set(HELIX_API vendor/helix-core-api/mac)
endif()

add_subdirectory(vendor)
add_subdirectory(p4-fusion)

if (BUILD_TESTS)
    message(STATUS "Building tests")
    add_subdirectory(tests)
endif()
