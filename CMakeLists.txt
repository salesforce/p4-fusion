cmake_minimum_required(VERSION 3.16)
variable_watch(LIBCXX_INSTALL_DIR)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif ()

# include alternative libc installation iff LIBCXX_INSTALL_DIR is non empty
if (DEFINED ENV{LIBCXX_INSTALL_DIR})
    set(LIBCXX_INSTALL_DIR $ENV{LIBCXX_INSTALL_DIR})
else ()
    set(LIBCXX_INSTALL_DIR "")
endif ()

if (EXISTS ${LIBCXX_INSTALL_DIR})
    message(STATUS, "Using libcxx from ${LIBCXX_INSTALL_DIR}")
    include_directories(SYSTEM "${LIBCXX_INSTALL_DIR}/include/c++/v1")
    link_directories("${LIBCXX_INSTALL_DIR}/lib")
endif ()

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(BUILD_CLAR "Build Clar" OFF)
option(BUILD_EXAMPLES "Build examples" OFF)
option(USE_BUNDLED_ZLIB "Use bundled zlib" ON)
option(THREADSAFE "Threadsafe" ON)
option(USE_SSH "Use SSH" OFF)
option(USE_HTTPS "Use HTTPS" OFF)
option(USE_THREADS "Use threads" ON)
option(PRINT_TEST_OUTPUT "Print additional lines used for the validate-migration script" OFF)

if (NOT DEFINED OPENSSL_ROOT_DIR)
    if (APPLE)
        set(OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl@1.1/")
    elseif (UNIX)
        set(OPENSSL_ROOT_DIR "/usr/local/ssl/")
    endif ()
endif ()

option(MTR_ENABLED "Enable minitrace profiling" OFF)
option(BUILD_TESTS "Build tests" OFF)

set(REGEX_BACKEND "Regex backend" "builtin")
set(CXX_STANDARD_REQUIRED true)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_C_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

set(CMAKE_CXX_FLAGS "$ENV{CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "$ENV{CMAKE_EXE_LINKER_FLAGS} ${CMAKE_EXE_LINKER_FLAGS}")

set(PRINT_TEST_OUTPUT $ENV{PRINT_TEST_OUTPUT})

if (PRINT_TEST_OUTPUT)
    add_definitions(-DPRINT_TEST_OUTPUT)
endif ()

project(
        p4-fusion
        LANGUAGES CXX
        VERSION 1.0
        DESCRIPTION "Custom tool to convert P4 depots to Git repositories using the Helix Core C++ API"
)

set(BUILD_CLAR OFF)
set(BUILD_SHARED_LIBS OFF)

if (APPLE)
    set(HELIX_API vendor/helix-core-api/mac)
elseif (UNIX)
    set(HELIX_API vendor/helix-core-api/linux)
endif ()

add_subdirectory(vendor)
add_subdirectory(p4-fusion)

if (BUILD_TESTS)
    message(STATUS "Building tests")
    add_subdirectory(tests)
endif ()
