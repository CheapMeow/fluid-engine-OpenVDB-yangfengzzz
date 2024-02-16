Fork and adapt to Windows build.

[https://github.com/yangfengzzz/fluid-engine-OpenVDB](https://github.com/yangfengzzz/fluid-engine-OpenVDB)

Specific modification

1. Use cmake menifest mode to install dependencied. 

    Because we need `task_scheduler_init.h` in old version of Tbb.

2. Replace `uint` with `unsigned int`.

    MSVC can not recongize `uint`.

3. Change `gtest`, `gmock`, `eigen` include path and CMake linking.

    For tidy include path.

4. Change library from dynamic to static.

    There is no dllexport.

5. Gitignore.

    Ignore vcpkg_installed folder.

6. FindXXX.cmake.

    Original module may no find Tbb. For convenience, all `FindXXX.cmake` files comes from openvdb and doyubkim/fluid-engine-dev.

7. Building bat.

    Vcpkg path should be adapted to personal env.

8. 

