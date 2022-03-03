file(GLOB_RECURSE Headers **.h)
file(GLOB_RECURSE Sources **.cc)

if (NOT OPENSSL_ROOT_DIR)
    set(OPENSSL_ROOT_DIR /usr/local/ssl)
endif()

set(OPENSSL_USE_STATIC_LIBS true)
find_package(OpenSSL)

add_executable(p4-fusion ${Headers} ${Sources})

target_include_directories(p4-fusion PUBLIC
    ../${HELIX_API}/include/
    ../vendor/libgit2/include/
    ../vendor/minitrace/
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_directories(p4-fusion PUBLIC
    ../${HELIX_API}/lib/
)

set(Frameworks "")

if (APPLE)
    find_library(corefoundation_lib CoreFoundation REQUIRED)
    find_library(cfnetwork_lib CFNetwork REQUIRED)
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    set(Frameworks
        ${Frameworks}
        ${cfnetwork_lib}
        ${corefoundation_lib}
        ${COCOA_LIBRARY}
    )
endif(APPLE)

message(${OPENSSL_SSL_LIBRARIES})
message(${OPENSSL_CRYPTO_LIBRARIES})

target_link_libraries(p4-fusion PUBLIC
    ${Frameworks}
    client
    rpc
    supp
    ${OPENSSL_SSL_LIBRARIES}
    ${OPENSSL_CRYPTO_LIBRARIES}
    p4script
    p4script_c
    git2
    minitrace
)
