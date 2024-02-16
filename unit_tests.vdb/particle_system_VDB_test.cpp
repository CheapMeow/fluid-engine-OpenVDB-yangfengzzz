////
////  particle_system_VDB_test.cpp
////  unittest
////
////  Created by Feng Yang on 2020/1/5.
////  Copyright © 2020 Feng Yang. All rights reserved.
////
//
//#include "../src.vdb/point_VDB_searcher3.hpp"
//#include "gtest/gtest.h"
//
//using namespace vox;
//
//TEST(ParticleSystemVDBSolver3, createVDBGrid) {
//    openvdb::initialize();
//
//    std::vector<openvdb::Vec3R> positions;
//    positions.push_back(openvdb::Vec3R(0, 1, 0));
//    positions.push_back(openvdb::Vec3R(1.5, 3.5, 1));
//    positions.push_back(openvdb::Vec3R(-1, 6, -2));
//    positions.push_back(openvdb::Vec3R(1.1, 1.25, 0.06));
//
//    PointVDBSearcher3 searcher;
//    searcher.build(positions);
//
//    openvdb::points::PointAttributeVector<openvdb::Vec3R> positionsWrapper(positions);
//    openvdb::points::PointDataGrid::Ptr grid =
//    openvdb::points::createPointDataGrid<openvdb::points::NullCodec,
//        openvdb::points::PointDataGrid>(*searcher.getGrid(), positionsWrapper, *searcher.getTransform());
//
//    // Set the name of the grid
//    grid->setName("Points");
//    // Create a VDB file object and write out the grid.
//    openvdb::io::File("mypoints.vdb").write({grid});
//    // Create a new VDB file object for reading.
//    openvdb::io::File newFile("mypoints.vdb");
//
//    // Open the file. This reads the file header, but not any grids.
//    newFile.open();
//    // Read the grid by name.
//    openvdb::GridBase::Ptr baseGrid = newFile.readGrid("Points");
//    newFile.close();
//
//    // From the example above, "Points" is known to be a PointDataGrid,
//    // so cast the generic grid pointer to a PointDataGrid pointer.
//    grid = openvdb::gridPtrCast<openvdb::points::PointDataGrid>(baseGrid);
//    openvdb::Index64 count = openvdb::points::pointCount(grid->tree());
//    std::cout << "PointCount=" << count << std::endl;
//    // Iterate over all the leaf nodes in the grid.
//    for (auto leafIter = grid->tree().cbeginLeaf(); leafIter; ++leafIter) {
//        // Verify the leaf origin.
//        std::cout << "Leaf" << leafIter->origin() << std::endl;
//        // Extract the position attribute from the leaf by name (P is position).
//        const openvdb::points::AttributeArray& array = leafIter->constAttributeArray("P");
//        // Create a read-only AttributeHandle. Position always uses Vec3f.
//        openvdb::points::AttributeHandle<openvdb::Vec3f> positionHandle(array);
//        // Iterate over the point indices in the leaf.
//        for (auto indexIter = leafIter->beginIndexOn(); indexIter; ++indexIter) {
//            // Extract the voxel-space position of the point.
//            openvdb::Vec3f voxelPosition = positionHandle.get(*indexIter);
//            // Extract the world-space position of the voxel.
//            const openvdb::Vec3d xyz = indexIter.getCoord().asVec3d();
//            // Compute the world-space position of the point.
//            openvdb::Vec3f worldPosition = grid->transform().indexToWorld(voxelPosition + xyz);
//            // Verify the index and world-space position of the point
//            std::cout << "* PointIndex=[" << *indexIter << "] ";
//            std::cout << "WorldPosition=" << worldPosition << std::endl;
//        }
//    }
//}
