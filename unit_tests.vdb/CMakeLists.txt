CMAKE_MINIMUM_REQUIRED(VERSION 3.14)

project(unit_tests_vdb)
aux_source_directory(./ DIR_SRCS)

# list(APPEND CMAKE_MODULE_PATH "/Users/yangfeng/Desktop/DigitalFlex2/cmake")
find_package(OpenVDB REQUIRED)
find_package(TBB)
find_package(glog)
find_package(GTest)

link_directories("../build/src.common")
link_directories("../build/src.vdb")

set(CMAKE_CXX_FLAGS "-std=c++14 -O3 -fPIC -g ${CMAKE_CXX_FLAGS}")
set(CMAKE_LD_FLAGS "${CMAKE_LD_FLAGS}")

add_executable(unit_tests_vdb_app ${DIR_SRCS})
target_link_libraries(unit_tests_vdb_app GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main glog::glog TBB::tbb OpenVDB::openvdb Flex_Common Flex_VDB)

set_target_properties(unit_tests_vdb_app PROPERTIES VS_GLOBAL_VcpkgEnabled true)