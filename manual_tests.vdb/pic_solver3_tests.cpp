// Copyright (c) 2018 Doyub Kim
//
// I am making my contributions/submissions to this project solely in my
// personal capacity and am not conveying any rights to any intellectual
// property of any third parties.

#include "manual_tests.h"

#include "../src.common/box3.h"
#include "../src.common/cylinder3.h"
#include "../src.vdb/vdb_fractional_single_phase_pressure_solver3.hpp"
#include "../src.common/grid_point_generator3.h"
#include "../src.common/implicit_surface_set3.h"
#include "../src.common/level_set_utils.h"
#include "../src.vdb/vdb_particle_emitter_set3.hpp"
#include "../src.vdb/vdb_pic_solver3.hpp"
#include "../src.common/plane3.h"
#include "../src.common/rigid_body_collider3.h"
#include "../src.common/sphere3.h"
#include "../src.common/surface_to_implicit3.h"
#include "../src.vdb/vdb_volume_particle_emitter3.hpp"
#include "../src.common/triangle_mesh3.h"

using namespace vdb;

JET_TESTS(PicSolver3);

JET_BEGIN_TEST_F(PicSolver3, WaterDrop) {
    //
    // This is a replica of hybrid_liquid_sim example 2.
    //
    unsigned int resolutionX = 32;
    
    // Build solver
    auto solver =
    PicSolver3::builder()
    .withResolution({resolutionX, 2 * resolutionX, resolutionX})
    .withDomainSizeX(1.0)
    .makeShared();
    
    auto grids = solver->vdbSystemData();
    auto particles = solver->particleSystemData();
    
    vox::Vector3D gridSpacing = grids->gridSpacing();
    double dx = gridSpacing.x;
    vox::BoundingBox3D domain = grids->boundingBox();
    
    // Build emitter
    auto plane = vox::Plane3::builder()
    .withNormal({0, 1, 0})
    .withPoint({0, 0.25 * domain.height(), 0})
    .makeShared();
    
    auto sphere = vox::Sphere3::builder()
    .withCenter(domain.midPoint())
    .withRadius(0.15 * domain.width())
    .makeShared();
    
    auto emitter1 = VolumeParticleEmitter3::builder()
    .withSurface(plane)
    .withSpacing(0.5 * dx)
    .withMaxRegion(domain)
    .withIsOneShot(true)
    .makeShared();
    emitter1->setPointGenerator(std::make_shared<vox::GridPointGenerator3>());
    
    auto emitter2 = VolumeParticleEmitter3::builder()
    .withSurface(sphere)
    .withSpacing(0.5 * dx)
    .withMaxRegion(domain)
    .withIsOneShot(true)
    .makeShared();
    emitter2->setPointGenerator(std::make_shared<vox::GridPointGenerator3>());
    
    auto emitterSet = ParticleEmitterSet3::builder()
    .withEmitters({emitter1, emitter2})
    .makeShared();

    solver->setParticleEmitter(emitterSet);

    for (vox::Frame frame; frame.index < 120; ++frame) {
        solver->update(frame);

        saveParticleDataXy(solver->particleSystemData(), frame.index);
    }
}
JET_END_TEST_F

JET_BEGIN_TEST_F(PicSolver3, DamBreakingWithCollider) {
    //
    // This is a replica of hybrid_liquid_sim example 4.
    //
    size_t resolutionX = 50;
    
    // Build solver
    vox::Size3 resolution{3 * resolutionX, 2 * resolutionX, (3 * resolutionX) / 2};
    auto solver = PicSolver3::builder()
    .withResolution(resolution)
    .withDomainSizeX(3.0)
    .makeShared();
    solver->setUseCompressedLinearSystem(true);
    
    auto grids = solver->vdbSystemData();
    double dx = grids->gridSpacing().x;
    vox::BoundingBox3D domain = grids->boundingBox();
    double lz = domain.depth();
    
    // Build emitter
     auto box1 =
     vox::Box3::builder()
     .withLowerCorner({0, 0, 0})
     .withUpperCorner({0.5 + 0.001, 0.75 + 0.001, 0.75 * lz + 0.001})
     .makeShared();
     
     auto box2 =
     vox::Box3::builder()
     .withLowerCorner({2.5 - 0.001, 0, 0.25 * lz - 0.001})
     .withUpperCorner({3.5 + 0.001, 0.75 + 0.001, 1.5 * lz + 0.001})
     .makeShared();
     
     auto boxSet = vox::ImplicitSurfaceSet3::builder()
     .withExplicitSurfaces({box1, box2})
     .makeShared();
     
     auto emitter = VolumeParticleEmitter3::builder()
     .withSurface(boxSet)
     .withMaxRegion(domain)
     .withSpacing(0.5 * dx)
     .makeShared();
     
     emitter->setPointGenerator(std::make_shared<vox::GridPointGenerator3>());
     solver->setParticleEmitter(emitter);
     
     // Build collider
     auto cyl1 = vox::Cylinder3::builder()
     .withCenter({1, 0.375, 0.375})
     .withRadius(0.1)
     .withHeight(0.75)
     .makeShared();
     
     auto cyl2 = vox::Cylinder3::builder()
     .withCenter({1.5, 0.375, 0.75})
     .withRadius(0.1)
     .withHeight(0.75)
     .makeShared();
     
     auto cyl3 = vox::Cylinder3::builder()
     .withCenter({2, 0.375, 1.125})
     .withRadius(0.1)
     .withHeight(0.75)
     .makeShared();
     
     auto cylSet = vox::ImplicitSurfaceSet3::builder()
     .withExplicitSurfaces({cyl1, cyl2, cyl3})
     .makeShared();
     
     auto collider =
     vox::RigidBodyCollider3::builder().withSurface(cylSet).makeShared();
     
     solver->setCollider(collider);
    
    // Run simulation
    for (vox::Frame frame; frame.index < 200; ++frame) {
        solver->update(frame);
        
        saveParticleDataXy(solver->particleSystemData(), frame.index);
    }
}
JET_END_TEST_F
