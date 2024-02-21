# Modification

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

# Test Result

In my testing project `MyTest`, I choose FlipSolver3 to test efficiency of OpenVDB, but the result shows that method with OpenVDB is much slower. How does it happen?

Build type: Release
CPU: AMD Ryzen 7 4800H with Radeon Graphics 2.90 GHz
Particles number: 875619

|Steps|Original Method|VDB Method|
|:-:|:-:|:-:|
|transferFromParticlesToGrids|0.12778|1.09503|
|buildSignedDistanceField|0.138452|1.26094|
|Computing pressure|0.465408|0.764281|
|moveParticles|0.265097|0.456756|
|Computing advection|0.424833|0.812436|
|Other|…|…|
|Total|1.5621|5.39702|
