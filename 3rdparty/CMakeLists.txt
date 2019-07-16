# GTEST ##############
# Important for 3rd party apps (JWT)
set(GTEST_PATH ${CMAKE_SOURCE_DIR}/3rdparty/googletest/googletest)
set(GTEST_PATH ${GTEST_PATH} PARENT_SCOPE)
find_package(Threads REQUIRED)
include_directories(${GTEST_PATH}/include)
include_directories(${GTEST_PATH}/)
add_library(gtest ${GTEST_PATH}/src/gtest-all.cc)
target_link_libraries(gtest Threads::Threads)
######################
