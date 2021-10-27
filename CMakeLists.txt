cmake_minimum_required(VERSION 3.16)

set(CXX_STANDARD_REQUIRED true)
set(CMAKE_CXX_STANDARD 11)

project(
    p4-fusion
    LANGUAGES CXX
    VERSION 1.0
    DESCRIPTION "Custom tool to convert P4 depots to Git repositories using the Helix Core C++ API"
)

set(BUILD_CLAR OFF)
set(BUILD_SHARED_LIBS OFF)

add_subdirectory(libgit2)

add_subdirectory(p4-fusion)
