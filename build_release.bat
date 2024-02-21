@echo off

cls

REM Configure a release build
cmake -S . -B build/ -D CMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=E:/vcpkg/scripts/buildsystems/vcpkg.cmake

REM Actually build the binaries
cmake --build build/ --config Release --parallel 4

pause