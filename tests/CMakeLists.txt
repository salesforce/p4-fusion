add_executable(p4-fusion-test 
    main.cc

    ../p4-fusion/utils/std_helpers.cc
    ../p4-fusion/utils/time_helpers.cc
    ../p4-fusion/git_api.cc
    ../p4-fusion/p4_api.cc
)

target_include_directories(p4-fusion-test PRIVATE
    ../p4-fusion/
    ../${HELIX_API}/include/
    ../vendor/libgit2/include/
    ../vendor/minitrace/
)

target_link_libraries(p4-fusion-test PRIVATE
    git2
)
