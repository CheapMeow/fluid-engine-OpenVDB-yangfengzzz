@echo off

cls

REM Configure a debug build
cmake -S . -B build/ -D CMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=E:/vcpkg/scripts/buildsystems/vcpkg.cmake

REM Actually build the binaries
cmake --build build/ --config Debug --parallel 4

pause