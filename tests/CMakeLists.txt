include_directories(../include)

add_executable(asyncjsonrpc_tests_exe
    test_general.cpp
    test_threadpool.cpp
    ${GTEST_PATH}/src/gtest_main.cc
    )

target_include_directories(asyncjsonrpc_tests_exe PRIVATE ${GTEST_PATH}/include)

target_link_libraries(asyncjsonrpc_tests_exe
    gtest
    async_json_rpc_lib
    -ljsoncpp
    ${CONAN_LIBS}
    )

add_test(
    NAME etcd-beast-tests
    COMMAND etcd-beast-tests
    --datadir ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
