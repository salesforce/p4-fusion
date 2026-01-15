find_package(OpenSSL REQUIRED)
add_executable(p4-fusion-test 
    main.cc

    tests.p4_depot_path.cc

    ../p4-fusion/utils/p4_depot_path.cc
    ../p4-fusion/utils/std_helpers.cc
    ../p4-fusion/utils/time_helpers.cc
    ../p4-fusion/git_api.cc
    ../p4-fusion/lfs/lfs_client.cc
    ../p4-fusion/log.cc
)

target_include_directories(p4-fusion-test PRIVATE
    ../p4-fusion/
    ../${HELIX_API}/include/
    ../vendor/libgit2/include/
    ../vendor/minitrace/
    ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(p4-fusion-test PRIVATE
    libgit2package
    ${OPENSSL_SSL_LIBRARIES}
    ${OPENSSL_CRYPTO_LIBRARIES}
)

if (APPLE)
    find_library(COREFOUNDATION_LIB CoreFoundation REQUIRED)
    target_link_libraries(p4-fusion-test PRIVATE ${COREFOUNDATION_LIB})
endif(APPLE)

