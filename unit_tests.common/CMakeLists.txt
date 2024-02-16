CMAKE_MINIMUM_REQUIRED(VERSION 3.14)

project(unit_tests_common)
aux_source_directory(./ DIR_SRCS)

find_package(TBB)
find_package(glog)
find_package(GTest)

link_directories("../build/src.common")

set(CMAKE_CXX_FLAGS "-std=c++14 -O3 -fPIC -g ${CMAKE_CXX_FLAGS}")
set(CMAKE_LD_FLAGS "${CMAKE_LD_FLAGS}")

add_executable(unit_tests_common_app ${DIR_SRCS})
target_link_libraries(unit_tests_common_app GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main glog::glog TBB::tbb Flex_Common)

# 设置默认打开VCPKG
set_target_properties(unit_tests_common_app PROPERTIES VS_GLOBAL_VcpkgEnabled true)
