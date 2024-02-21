CMAKE_MINIMUM_REQUIRED(VERSION 3.14)

project(MyTest)

file(GLOB_RECURSE MyTest_HEADER_FILES CONFIGURE_DEPENDS ./*.h ./*.hpp)
file(GLOB_RECURSE MyTest_SOURCE_FILES CONFIGURE_DEPENDS ./*.cpp)

find_package(OpenVDB REQUIRED)
find_package(TBB)
find_package(glog)

link_directories("../build/src.common")
link_directories("../build/src.vdb")

set(CMAKE_CXX_FLAGS "-std=c++14 -O3 -fPIC -g ${CMAKE_CXX_FLAGS}")
set(CMAKE_LD_FLAGS "${CMAKE_LD_FLAGS}")

add_executable(mytest_app ${MyTest_HEADER_FILES} ${MyTest_SOURCE_FILES})
target_link_libraries(mytest_app glog::glog TBB::tbb OpenVDB::openvdb Flex_Common Flex_VDB)

set_target_properties(mytest_app PROPERTIES VS_GLOBAL_VcpkgEnabled true)
