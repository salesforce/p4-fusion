add_executable(p4-fusion-test 
    main.cc

    ../p4-fusion/utils/std_helpers.cc
)

target_include_directories(p4-fusion-test PRIVATE
    ../p4-fusion/
    ../${HELIX_API}/include/
    ../vendor/libgit2/include/
    ../vendor/minitrace/
)
