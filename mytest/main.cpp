#include "../src.common/pch.h"

#include "../src.common/box3.h"
#include "../src.common/cylinder3.h"
#include "../src.common/flip_solver3.h"
#include "../src.common/grid_fractional_single_phase_pressure_solver3.h"
#include "../src.common/grid_point_generator3.h"
#include "../src.common/implicit_surface_set3.h"
#include "../src.common/level_set_utils.h"
#include "../src.common/particle_emitter_set3.h"
#include "../src.common/plane3.h"
#include "../src.common/rigid_body_collider3.h"
#include "../src.common/sphere3.h"
#include "../src.common/surface_to_implicit3.h"
#include "../src.common/volume_particle_emitter3.h"

#include "../src.vdb/vdb_flip_solver3.hpp"
#include "../src.vdb/vdb_fractional_single_phase_pressure_solver3.hpp"
#include "../src.vdb/vdb_point_searcher3.hpp"
#include "../src.vdb/vdb_particle_emitter_set3.hpp"
#include "../src.vdb/vdb_volume_particle_emitter3.hpp"

#include <fstream>

void CommonDamBreaking(){
    size_t resolutionX = 80;
    
    //
    // This is a replica of hybrid_liquid_sim example 3.
    //
    
    // Build solver
    vox::Size3 resolution{3 * resolutionX, 2 * resolutionX, (3 * resolutionX) / 2};
    auto solver = vox::FlipSolver3::builder()
    .withResolution(resolution)
    .withDomainSizeX(3.0)
    .makeShared();
    solver->setUseCompressedLinearSystem(true);
    
    auto grids = solver->gridSystemData();
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
    
    auto emitter = vox::VolumeParticleEmitter3::builder()
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
    for (vox::Frame frame; frame.index < 30; ++frame) {
        solver->update(frame);
        
        //saveParticleDataXy(solver->particleSystemData(), frame.index);
    }
}

void VDBDamBreaking(){
    //
    // This is a replica of hybrid_liquid_sim example 3.
    //
    // Build solver
    size_t resolutionX = 80;
    
    vox::Size3 resolution{3 * resolutionX, 2 * resolutionX, (3 * resolutionX) / 2};
    auto solver = vdb::FlipSolver3::builder()
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
    
    auto emitter = vdb::VolumeParticleEmitter3::builder()
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
    for (vox::Frame frame; frame.index < 30; ++frame) {
        solver->update(frame);
        
        //saveParticleDataXy(solver->particleSystemData(), frame.index);
        
        char filename[256];
        snprintf(
                 filename,
                 sizeof(filename),
                 "data.#point,%04d,x.vdb",
                 frame.index);
        vdb::PointVDBSearcher3Ptr
        searcher = std::dynamic_pointer_cast<vdb::PointVDBSearcher3>(solver->particleSystemData()->neighborSearcher());
        
        openvdb::io::File file(filename);
        // Add the grid pointer to a container.
        openvdb::GridPtrVec grids;
        grids.push_back(searcher->getDataGrid());
        // Write out the contents of the container.
        file.write(grids);
        file.close();
    }
}

int main(int /*argc*/, char** argv){
    openvdb::initialize();

    google::SetLogDestination(google::GLOG_INFO, "mytest_result.log");
    google::InitGoogleLogging(argv[0]);

    CommonDamBreaking();
    VDBDamBreaking();
}