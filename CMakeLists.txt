cmake_minimum_required(VERSION 3.14)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(fluid-engine-OpenVDB)

add_subdirectory(src.common)
add_subdirectory(src.vdb)
add_subdirectory(unit_tests.common)
add_subdirectory(unit_tests.vdb)
add_subdirectory(manual_tests.common)
add_subdirectory(manual_tests.vdb)