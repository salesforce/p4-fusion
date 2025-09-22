file(GLOB_RECURSE Headers **.h)
file(GLOB_RECURSE Sources **.cc)

if (NOT OPENSSL_ROOT_DIR)
    set(OPENSSL_ROOT_DIR /usr/local/ssl)
endif()

set(OPENSSL_USE_STATIC_LIBS true)
find_package(OpenSSL)
find_package(CURL REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(p4-fusion ${Headers} ${Sources})

target_include_directories(p4-fusion PUBLIC
    ../${HELIX_API}/include/
    ../vendor/libgit2/include/
    ../vendor/minitrace/
    ../vendor/rapidjson/include/
    ${OPENSSL_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_directories(p4-fusion PUBLIC
    ../${HELIX_API}/lib/
)

set(Frameworks "")

if (APPLE)
    find_library(COREFOUNDATION_LIB CoreFoundation REQUIRED)
    find_library(CFNETWORK_LIB CFNetwork REQUIRED)
    find_library(COCOA_LIB Cocoa REQUIRED)
    find_library(SECURITY_LIB Security REQUIRED)
    find_library(NETWORK_LIB Network REQUIRED)
    set(Frameworks
        ${Frameworks}
        ${CFNETWORK_LIB}
        ${COREFOUNDATION_LIB}
        ${COCOA_LIB}
        ${SECURITY_LIB}
        ${NETWORK_LIB}
    )
endif(APPLE)

message(${OPENSSL_SSL_LIBRARIES})
message(${OPENSSL_CRYPTO_LIBRARIES})

include(ExternalProject)

ExternalProject_Add(aws-sdk
    GIT_REPOSITORY    https://github.com/aws/aws-sdk-cpp.git
    GIT_TAG           1.11.640
    CMAKE_ARGS        -DBUILD_SHARED_LIBS=OFF
                      -DAUTORUN_UNIT_TESTS=OFF
                      -DBUILD_ONLY=s3
                      -DCMAKE_INSTALL_PREFIX=${CMAKE_SOURCE_DIR}/vendor/aws-sdk
    PREFIX            ${CMAKE_CURRENT_BINARY_DIR}/aws-sdk
    BUILD_ALWAYS      TRUE
    TEST_COMMAND      ""
)

add_dependencies(p4-fusion aws-sdk)

target_include_directories(p4-fusion PUBLIC
    ../vendor/aws-sdk/include
)

target_link_directories(p4-fusion PUBLIC
    ../vendor/aws-sdk/lib
)

set(AWSSDK_LINK_LIBRARIES
    libaws-c-auth.a
    libaws-c-cal.a
    libaws-c-common.a
    libaws-c-compression.a
    libaws-c-event-stream.a
    libaws-c-http.a
    libaws-c-io.a
    libaws-c-mqtt.a
    libaws-c-s3.a
    libaws-c-sdkutils.a
    libaws-checksums.a
    libaws-cpp-sdk-core.a
    libaws-cpp-sdk-s3.a
    libaws-crt-cpp.a
)

target_link_libraries(p4-fusion PUBLIC
    ${Frameworks}
    client
    rpc
    supp
    p4script_cstub
    ${OPENSSL_SSL_LIBRARIES}
    ${OPENSSL_CRYPTO_LIBRARIES}
    ${CURL_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${AWSSDK_LINK_LIBRARIES}
    git2
    minitrace
)

add_custom_target(deepclean
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_SOURCE_DIR}/build
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_SOURCE_DIR}/vendor/aws-sdk
    COMMENT "Removing AWS SDK on clean"
)
