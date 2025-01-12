//
//  vdb_fractional_single_phase_pressure_solver3.cpp
//  Solvers
//
//  Created by Feng Yang on 2020/2/7.
//  Copyright © 2020 Feng Yang. All rights reserved.
//

#include "../src.common/pch.h"
#include "vdb_helper.h"
#include "../src.common/constants.h"
#include "../src.common/fdm_iccg_solver3.h"
#include "vdb_fractional_boundary_condition_solver3.hpp"
#include "vdb_fractional_single_phase_pressure_solver3.hpp"
#include "../src.common/level_set_utils.h"

using namespace vdb;

const double kDefaultTolerance = 1e-6;
const double kMinWeight = 0.01;

namespace {

void restricted(const vox::Array3<float>& finer, vox::Array3<float>* coarser) {
    // --*--|--*--|--*--|--*--
    //  1/8   3/8   3/8   1/8
    //           to
    // -----|-----*-----|-----
    static const std::array<float, 4> centeredKernel = {
        {0.125f, 0.375f, 0.375f, 0.125f}};
    
    // -|----|----|----|----|-
    //      1/4  1/2  1/4
    //           to
    // -|---------|---------|-
    static const std::array<float, 4> staggeredKernel = {{0.f, 1.f, 0.f, 0.f}};
    
    std::array<int, 3> kernelSize;
    kernelSize[0] = finer.size().x != 2 * coarser->size().x ? 3 : 4;
    kernelSize[1] = finer.size().y != 2 * coarser->size().y ? 3 : 4;
    kernelSize[2] = finer.size().z != 2 * coarser->size().z ? 3 : 4;
    
    std::array<std::array<float, 4>, 3> kernels;
    kernels[0] = (kernelSize[0] == 3) ? staggeredKernel : centeredKernel;
    kernels[1] = (kernelSize[1] == 3) ? staggeredKernel : centeredKernel;
    kernels[2] = (kernelSize[2] == 3) ? staggeredKernel : centeredKernel;
    
    const vox::Size3 n = coarser->size();
    vox::parallelRangeFor(
                          vox::kZeroSize, n.x,
                          vox::kZeroSize, n.y,
                          vox::kZeroSize, n.z,
                          [&](size_t iBegin, size_t iEnd,
                              size_t jBegin, size_t jEnd,
                              size_t kBegin, size_t kEnd) {
        std::array<size_t, 4> kIndices;
        
        for (size_t k = kBegin; k < kEnd; ++k) {
            if (kernelSize[2] == 3) {
                kIndices[0] = (k > 0) ? 2 * k - 1 : 2 * k;
                kIndices[1] = 2 * k;
                kIndices[2] = (k + 1 < n.z) ? 2 * k + 1 : 2 * k;
            } else {
                kIndices[0] = (k > 0) ? 2 * k - 1 : 2 * k;
                kIndices[1] = 2 * k;
                kIndices[2] = 2 * k + 1;
                kIndices[3] = (k + 1 < n.z) ? 2 * k + 2 : 2 * k + 1;
            }
            
            std::array<size_t, 4> jIndices;
            
            for (size_t j = jBegin; j < jEnd; ++j) {
                if (kernelSize[1] == 3) {
                    jIndices[0] = (j > 0) ? 2 * j - 1 : 2 * j;
                    jIndices[1] = 2 * j;
                    jIndices[2] = (j + 1 < n.y) ? 2 * j + 1 : 2 * j;
                } else {
                    jIndices[0] = (j > 0) ? 2 * j - 1 : 2 * j;
                    jIndices[1] = 2 * j;
                    jIndices[2] = 2 * j + 1;
                    jIndices[3] = (j + 1 < n.y) ? 2 * j + 2 : 2 * j + 1;
                }
                
                std::array<size_t, 4> iIndices;
                for (size_t i = iBegin; i < iEnd; ++i) {
                    if (kernelSize[0] == 3) {
                        iIndices[0] = (i > 0) ? 2 * i - 1 : 2 * i;
                        iIndices[1] = 2 * i;
                        iIndices[2] = (i + 1 < n.x) ? 2 * i + 1 : 2 * i;
                    } else {
                        iIndices[0] = (i > 0) ? 2 * i - 1 : 2 * i;
                        iIndices[1] = 2 * i;
                        iIndices[2] = 2 * i + 1;
                        iIndices[3] = (i + 1 < n.x) ? 2 * i + 2 : 2 * i + 1;
                    }
                    
                    float sum = 0.0f;
                    for (int z = 0; z < kernelSize[2]; ++z) {
                        for (int y = 0; y < kernelSize[1]; ++y) {
                            for (int x = 0; x < kernelSize[0]; ++x) {
                                float w = kernels[0][x] * kernels[1][y] *
                                kernels[2][z];
                                sum += w * finer(iIndices[x], jIndices[y],
                                                 kIndices[z]);
                            }
                        }
                    }
                    (*coarser)(i, j, k) = sum;
                }
            }
        }
    });
}

void buildSingleSystem(vox::FdmMatrix3* A, vox::FdmVector3* b,
                       const vox::Array3<float>& fluidSdf,
                       const vox::Array3<float>& uWeights,
                       const vox::Array3<float>& vWeights,
                       const vox::Array3<float>& wWeights,
                       std::function<vox::Vector3D(const vox::Vector3D&)> boundaryVel,
                       const FaceCenteredGrid3& input) {
    const vox::Size3 size = input.resolution();
    const auto uPos = input.uPosition();
    const auto vPos = input.vPosition();
    const auto wPos = input.wPosition();
    
    const vox::Vector3D invH = 1.0 / input.gridSpacing();
    const vox::Vector3D invHSqr = invH * invH;
    
    // Build linear system
    A->parallelForEachIndex([&](unsigned int i, unsigned int j, unsigned int k) {
        auto& row = (*A)(i, j, k);
        
        // initialize
        row.center = row.right = row.up = row.front = 0.0;
        (*b)(i, j, k) = 0.0;
        
        double centerPhi = fluidSdf(i, j, k);
        
        if (vox::isInsideSdf(centerPhi)) {
            double term;
            
            if (i + 1 < size.x) {
                term = uWeights(i + 1, j, k) * invHSqr.x;
                double rightPhi = fluidSdf(i + 1, j, k);
                if (vox::isInsideSdf(rightPhi)) {
                    row.center += term;
                    row.right -= term;
                } else {
                    double theta = vox::fractionInsideSdf(centerPhi, rightPhi);
                    theta = std::max(theta, 0.01);
                    row.center += term / theta;
                }
                (*b)(i, j, k) +=
                uWeights(i + 1, j, k) * input.u(openvdb::Coord(i + 1, j, k)) * invH.x;
            } else {
                (*b)(i, j, k) += input.u(openvdb::Coord(i + 1, j, k)) * invH.x;
            }
            
            if (i > 0) {
                term = uWeights(i, j, k) * invHSqr.x;
                double leftPhi = fluidSdf(i - 1, j, k);
                if (vox::isInsideSdf(leftPhi)) {
                    row.center += term;
                } else {
                    double theta = vox::fractionInsideSdf(centerPhi, leftPhi);
                    theta = std::max(theta, 0.01);
                    row.center += term / theta;
                }
                (*b)(i, j, k) -= uWeights(i, j, k) * input.u(openvdb::Coord(i, j, k)) * invH.x;
            } else {
                (*b)(i, j, k) -= input.u(openvdb::Coord(i, j, k)) * invH.x;
            }
            
            if (j + 1 < size.y) {
                term = vWeights(i, j + 1, k) * invHSqr.y;
                double upPhi = fluidSdf(i, j + 1, k);
                if (vox::isInsideSdf(upPhi)) {
                    row.center += term;
                    row.up -= term;
                } else {
                    double theta = vox::fractionInsideSdf(centerPhi, upPhi);
                    theta = std::max(theta, 0.01);
                    row.center += term / theta;
                }
                (*b)(i, j, k) +=
                vWeights(i, j + 1, k) * input.v(openvdb::Coord(i, j + 1, k)) * invH.y;
            } else {
                (*b)(i, j, k) += input.v(openvdb::Coord(i, j + 1, k)) * invH.y;
            }
            
            if (j > 0) {
                term = vWeights(i, j, k) * invHSqr.y;
                double downPhi = fluidSdf(i, j - 1, k);
                if (vox::isInsideSdf(downPhi)) {
                    row.center += term;
                } else {
                    double theta = vox::fractionInsideSdf(centerPhi, downPhi);
                    theta = std::max(theta, 0.01);
                    row.center += term / theta;
                }
                (*b)(i, j, k) -= vWeights(i, j, k) * input.v(openvdb::Coord(i, j, k)) * invH.y;
            } else {
                (*b)(i, j, k) -= input.v(openvdb::Coord(i, j, k)) * invH.y;
            }
            
            if (k + 1 < size.z) {
                term = wWeights(i, j, k + 1) * invHSqr.z;
                double frontPhi = fluidSdf(i, j, k + 1);
                if (vox::isInsideSdf(frontPhi)) {
                    row.center += term;
                    row.front -= term;
                } else {
                    double theta = vox::fractionInsideSdf(centerPhi, frontPhi);
                    theta = std::max(theta, 0.01);
                    row.center += term / theta;
                }
                (*b)(i, j, k) +=
                wWeights(i, j, k + 1) * input.w(openvdb::Coord(i, j, k + 1)) * invH.z;
            } else {
                (*b)(i, j, k) += input.w(openvdb::Coord(i, j, k + 1)) * invH.z;
            }
            
            if (k > 0) {
                term = wWeights(i, j, k) * invHSqr.z;
                double backPhi = fluidSdf(i, j, k - 1);
                if (vox::isInsideSdf(backPhi)) {
                    row.center += term;
                } else {
                    double theta = vox::fractionInsideSdf(centerPhi, backPhi);
                    theta = std::max(theta, 0.01);
                    row.center += term / theta;
                }
                (*b)(i, j, k) -= wWeights(i, j, k) * input.w(openvdb::Coord(i, j, k)) * invH.z;
            } else {
                (*b)(i, j, k) -= input.w(openvdb::Coord(i, j, k)) * invH.z;
            }
            
            // Accumulate contributions from the moving boundary
            double boundaryContribution =
            (1.0 - uWeights(i + 1, j, k)) *
            boundaryVel(uPos(openvdb::Coord(i + 1, j, k))).x * invH.x -
            (1.0 - uWeights(i, j, k)) * boundaryVel(uPos(openvdb::Coord(i, j, k))).x *
            invH.x +
            (1.0 - vWeights(i, j + 1, k)) *
            boundaryVel(vPos(openvdb::Coord(i, j + 1, k))).y * invH.y -
            (1.0 - vWeights(i, j, k)) * boundaryVel(vPos(openvdb::Coord(i, j, k))).y *
            invH.y +
            (1.0 - wWeights(i, j, k + 1)) *
            boundaryVel(wPos(openvdb::Coord(i, j, k + 1))).z * invH.z -
            (1.0 - wWeights(i, j, k)) * boundaryVel(wPos(openvdb::Coord(i, j, k))).z *
            invH.z;
            (*b)(i, j, k) += boundaryContribution;
            
            // If row.center is near-zero, the cell is likely inside a solid
            // boundary.
            if (row.center < vox::kEpsilonD) {
                row.center = 1.0;
                (*b)(i, j, k) = 0.0;
            }
        } else {
            row.center = 1.0;
        }
    });
}

void buildSingleSystem(vox::MatrixCsrD* A, vox::VectorND* x, vox::VectorND* b,
                       const vox::Array3<float>& fluidSdf,
                       const vox::Array3<float>& uWeights,
                       const vox::Array3<float>& vWeights,
                       const vox::Array3<float>& wWeights,
                       std::function<vox::Vector3D(const vox::Vector3D&)> boundaryVel,
                       const FaceCenteredGrid3& input) {
    const vox::Size3 size = input.resolution();
    const auto uPos = input.uPosition();
    const auto vPos = input.vPosition();
    const auto wPos = input.wPosition();
    
    const vox::Vector3D invH = 1.0 / input.gridSpacing();
    const vox::Vector3D invHSqr = invH * invH;
    
    const auto fluidSdfAcc = fluidSdf.constAccessor();
    
    A->clear();
    b->clear();
    
    size_t numRows = 0;
    vox::Array3<size_t> coordToIndex(size);
    fluidSdf.forEachIndex([&](size_t i, size_t j, size_t k) {
        const size_t cIdx = fluidSdfAcc.index(i, j, k);
        const double centerPhi = fluidSdf[cIdx];
        
        if (vox::isInsideSdf(centerPhi)) {
            coordToIndex[cIdx] = numRows++;
        }
    });
    
    fluidSdf.forEachIndex([&](unsigned int i, unsigned int j, unsigned int k) {
        const size_t cIdx = fluidSdfAcc.index(i, j, k);
        
        const double centerPhi = fluidSdf[cIdx];
        
        if (vox::isInsideSdf(centerPhi)) {
            double bijk = 0.0;
            
            std::vector<double> row(1, 0.0);
            std::vector<size_t> colIdx(1, coordToIndex[cIdx]);
            
            double term;
            
            if (i + 1 < size.x) {
                term = uWeights(i + 1, j, k) * invHSqr.x;
                const double rightPhi = fluidSdf(i + 1, j, k);
                if (vox::isInsideSdf(rightPhi)) {
                    row[0] += term;
                    row.push_back(-term);
                    colIdx.push_back(coordToIndex(i + 1, j, k));
                } else {
                    double theta = vox::fractionInsideSdf(centerPhi, rightPhi);
                    theta = std::max(theta, 0.01);
                    row[0] += term / theta;
                }
                bijk += uWeights(i + 1, j, k) * input.u(openvdb::Coord(i + 1, j, k)) * invH.x;
            } else {
                bijk += input.u(openvdb::Coord(i + 1, j, k)) * invH.x;
            }
            
            if (i > 0) {
                term = uWeights(i, j, k) * invHSqr.x;
                const double leftPhi = fluidSdf(i - 1, j, k);
                if (vox::isInsideSdf(leftPhi)) {
                    row[0] += term;
                    row.push_back(-term);
                    colIdx.push_back(coordToIndex(i - 1, j, k));
                } else {
                    double theta = vox::fractionInsideSdf(centerPhi, leftPhi);
                    theta = std::max(theta, 0.01);
                    row[0] += term / theta;
                }
                bijk -= uWeights(i, j, k) * input.u(openvdb::Coord(i, j, k)) * invH.x;
            } else {
                bijk -= input.u(openvdb::Coord(i, j, k)) * invH.x;
            }
            
            if (j + 1 < size.y) {
                term = vWeights(i, j + 1, k) * invHSqr.y;
                const double upPhi = fluidSdf(i, j + 1, k);
                if (vox::isInsideSdf(upPhi)) {
                    row[0] += term;
                    row.push_back(-term);
                    colIdx.push_back(coordToIndex(i, j + 1, k));
                } else {
                    double theta = vox::fractionInsideSdf(centerPhi, upPhi);
                    theta = std::max(theta, 0.01);
                    row[0] += term / theta;
                }
                bijk += vWeights(i, j + 1, k) * input.v(openvdb::Coord(i, j + 1, k)) * invH.y;
            } else {
                bijk += input.v(openvdb::Coord(i, j + 1, k)) * invH.y;
            }
            
            if (j > 0) {
                term = vWeights(i, j, k) * invHSqr.y;
                const double downPhi = fluidSdf(i, j - 1, k);
                if (vox::isInsideSdf(downPhi)) {
                    row[0] += term;
                    row.push_back(-term);
                    colIdx.push_back(coordToIndex(i, j - 1, k));
                } else {
                    double theta = vox::fractionInsideSdf(centerPhi, downPhi);
                    theta = std::max(theta, 0.01);
                    row[0] += term / theta;
                }
                bijk -= vWeights(i, j, k) * input.v(openvdb::Coord(i, j, k)) * invH.y;
            } else {
                bijk -= input.v(openvdb::Coord(i, j, k)) * invH.y;
            }
            
            if (k + 1 < size.z) {
                term = wWeights(i, j, k + 1) * invHSqr.z;
                const double frontPhi = fluidSdf(i, j, k + 1);
                if (vox::isInsideSdf(frontPhi)) {
                    row[0] += term;
                    row.push_back(-term);
                    colIdx.push_back(coordToIndex(i, j, k + 1));
                } else {
                    double theta = vox::fractionInsideSdf(centerPhi, frontPhi);
                    theta = std::max(theta, 0.01);
                    row[0] += term / theta;
                }
                bijk += wWeights(i, j, k + 1) * input.w(openvdb::Coord(i, j, k + 1)) * invH.z;
            } else {
                bijk += input.w(openvdb::Coord(i, j, k + 1)) * invH.z;
            }
            
            if (k > 0) {
                term = wWeights(i, j, k) * invHSqr.z;
                const double backPhi = fluidSdf(i, j, k - 1);
                if (vox::isInsideSdf(backPhi)) {
                    row[0] += term;
                    row.push_back(-term);
                    colIdx.push_back(coordToIndex(i, j, k - 1));
                } else {
                    double theta = vox::fractionInsideSdf(centerPhi, backPhi);
                    theta = std::max(theta, 0.01);
                    row[0] += term / theta;
                }
                bijk -= wWeights(i, j, k) * input.w(openvdb::Coord(i, j, k)) * invH.z;
            } else {
                bijk -= input.w(openvdb::Coord(i, j, k)) * invH.z;
            }
            
            // Accumulate contributions from the moving boundary
            double boundaryContribution =
            (1.0 - uWeights(i + 1, j, k)) *
            boundaryVel(uPos(openvdb::Coord(i + 1, j, k))).x * invH.x -
            (1.0 - uWeights(i, j, k)) * boundaryVel(uPos(openvdb::Coord(i, j, k))).x *
            invH.x +
            (1.0 - vWeights(i, j + 1, k)) *
            boundaryVel(vPos(openvdb::Coord(i, j + 1, k))).y * invH.y -
            (1.0 - vWeights(i, j, k)) * boundaryVel(vPos(openvdb::Coord(i, j, k))).y *
            invH.y +
            (1.0 - wWeights(i, j, k + 1)) *
            boundaryVel(wPos(openvdb::Coord(i, j, k + 1))).z * invH.z -
            (1.0 - wWeights(i, j, k)) * boundaryVel(wPos(openvdb::Coord(i, j, k))).z *
            invH.z;
            bijk += boundaryContribution;
            
            // If row.center is near-zero, the cell is likely inside a solid
            // boundary.
            if (row[0] < vox::kEpsilonD) {
                row[0] = 1.0;
                bijk = 0.0;
            }
            
            A->addRow(row, colIdx);
            b->append(bijk);
        }
    });
    
    x->resize(b->size(), 0.0);
}

} // namespace

FractionalSinglePhasePressureSolver3::
FractionalSinglePhasePressureSolver3() {
    _systemSolver = std::make_shared<vox::FdmIccgSolver3>(100,
                                                          kDefaultTolerance);
}

FractionalSinglePhasePressureSolver3::
~FractionalSinglePhasePressureSolver3() {}

BoundaryConditionSolver3Ptr
FractionalSinglePhasePressureSolver3::suggestedBoundaryConditionSolver()
const {
    return std::make_shared<FractionalBoundaryConditionSolver3>();
}


const vox::FdmLinearSystemSolver3Ptr&
FractionalSinglePhasePressureSolver3::linearSystemSolver() const {
    return _systemSolver;
}

void FractionalSinglePhasePressureSolver3::setLinearSystemSolver(
                                                                 const vox::FdmLinearSystemSolver3Ptr& solver) {
    _systemSolver = solver;
    _mgSystemSolver = std::dynamic_pointer_cast<vox::FdmMgSolver3>(_systemSolver);
    
    if (_mgSystemSolver == nullptr) {
        // In case of non-mg system, use flat structure.
        _mgSystem.clear();
    } else {
        // In case of mg system, use multi-level structure.
        _system.clear();
        _compSystem.clear();
    }
}

const vox::FdmVector3& FractionalSinglePhasePressureSolver3::pressure() const {
    if (_mgSystemSolver == nullptr) {
        return _system.x;
    } else {
        return _mgSystem.x.levels.front();
    }
}

void FractionalSinglePhasePressureSolver3::buildWeights(
                                                        const FaceCenteredGrid3& input,
                                                        const vox::ScalarField3& boundarySdf,
                                                        const vox::VectorField3& boundaryVelocity,
                                                        const vox::ScalarField3& fluidSdf) {
    auto size = input.resolution();
    
    // Build levels
    size_t maxLevels = 1;
    if (_mgSystemSolver != nullptr) {
        maxLevels = _mgSystemSolver->params().maxNumberOfLevels;
    }
    vox::FdmMgUtils3::resizeArrayWithFinest(size, maxLevels, &_fluidSdf);
    _uWeights.resize(_fluidSdf.size());
    _vWeights.resize(_fluidSdf.size());
    _wWeights.resize(_fluidSdf.size());
    for (size_t l = 0; l < _fluidSdf.size(); ++l) {
        _uWeights[l].resize(_fluidSdf[l].size() + vox::Size3(1, 0, 0));
        _vWeights[l].resize(_fluidSdf[l].size() + vox::Size3(0, 1, 0));
        _wWeights[l].resize(_fluidSdf[l].size() + vox::Size3(0, 0, 1));
    }
    
    // Build top-level grids
    auto cellPos = input.cellCenterPosition();
    auto uPos = input.uPosition();
    auto vPos = input.vPosition();
    auto wPos = input.wPosition();
    _boundaryVel = boundaryVelocity.sampler();
    vox::Vector3D h = input.gridSpacing();
    
    _fluidSdf[0].parallelForEachIndex([&](unsigned int i, unsigned int j, unsigned int k) {
        _fluidSdf[0](i, j, k) =
        static_cast<float>(fluidSdf.sample(cellPos(openvdb::Coord(i, j, k))));
    });
    
    _uWeights[0].parallelForEachIndex([&](unsigned int i, unsigned int j, unsigned int k) {
        vox::Vector3D pt = uPos(openvdb::Coord(i, j, k));
        double phi0 =
        boundarySdf.sample(pt + vox::Vector3D(0.0, -0.5 * h.y, -0.5 * h.z));
        double phi1 =
        boundarySdf.sample(pt + vox::Vector3D(0.0, 0.5 * h.y, -0.5 * h.z));
        double phi2 =
        boundarySdf.sample(pt + vox::Vector3D(0.0, -0.5 * h.y, 0.5 * h.z));
        double phi3 =
        boundarySdf.sample(pt + vox::Vector3D(0.0, 0.5 * h.y, 0.5 * h.z));
        double frac = vox::fractionInside(phi0, phi1, phi2, phi3);
        double weight = vox::clamp(1.0 - frac, 0.0, 1.0);
        
        // Clamp non-zero weight to kMinWeight. Having nearly-zero element
        // in the matrix can be an issue.
        if (weight < kMinWeight && weight > 0.0) {
            weight = kMinWeight;
        }
        
        _uWeights[0](i, j, k) = static_cast<float>(weight);
    });
    
    _vWeights[0].parallelForEachIndex([&](unsigned int i, unsigned int j, unsigned int k) {
        vox::Vector3D pt = vPos(openvdb::Coord(i, j, k));
        double phi0 =
        boundarySdf.sample(pt + vox::Vector3D(-0.5 * h.x, 0.0, -0.5 * h.z));
        double phi1 =
        boundarySdf.sample(pt + vox::Vector3D(-0.5 * h.x, 0.0, 0.5 * h.z));
        double phi2 =
        boundarySdf.sample(pt + vox::Vector3D(0.5 * h.x, 0.0, -0.5 * h.z));
        double phi3 =
        boundarySdf.sample(pt + vox::Vector3D(0.5 * h.x, 0.0, 0.5 * h.z));
        double frac = vox::fractionInside(phi0, phi1, phi2, phi3);
        double weight = vox::clamp(1.0 - frac, 0.0, 1.0);
        
        // Clamp non-zero weight to kMinWeight. Having nearly-zero element
        // in the matrix can be an issue.
        if (weight < kMinWeight && weight > 0.0) {
            weight = kMinWeight;
        }
        
        _vWeights[0](i, j, k) = static_cast<float>(weight);
    });
    
    _wWeights[0].parallelForEachIndex([&](unsigned int i, unsigned int j, unsigned int k) {
        vox::Vector3D pt = wPos(openvdb::Coord(i, j, k));
        double phi0 =
        boundarySdf.sample(pt + vox::Vector3D(-0.5 * h.x, -0.5 * h.y, 0.0));
        double phi1 =
        boundarySdf.sample(pt + vox::Vector3D(-0.5 * h.x, 0.5 * h.y, 0.0));
        double phi2 =
        boundarySdf.sample(pt + vox::Vector3D(0.5 * h.x, -0.5 * h.y, 0.0));
        double phi3 =
        boundarySdf.sample(pt + vox::Vector3D(0.5 * h.x, 0.5 * h.y, 0.0));
        double frac = vox::fractionInside(phi0, phi1, phi2, phi3);
        double weight = vox::clamp(1.0 - frac, 0.0, 1.0);
        
        // Clamp non-zero weight to kMinWeight. Having nearly-zero element
        // in the matrix can be an issue.
        if (weight < kMinWeight && weight > 0.0) {
            weight = kMinWeight;
        }
        
        _wWeights[0](i, j, k) = static_cast<float>(weight);
    });
    
    // Build sub-levels
    for (size_t l = 1; l < _fluidSdf.size(); ++l) {
        const auto& finerFluidSdf = _fluidSdf[l - 1];
        auto& coarserFluidSdf = _fluidSdf[l];
        const auto& finerUWeight = _uWeights[l - 1];
        auto& coarserUWeight = _uWeights[l];
        const auto& finerVWeight = _vWeights[l - 1];
        auto& coarserVWeight = _vWeights[l];
        const auto& finerWWeight = _wWeights[l - 1];
        auto& coarserWWeight = _wWeights[l];
        
        // Fluid SDF
        restricted(finerFluidSdf, &coarserFluidSdf);
        restricted(finerUWeight, &coarserUWeight);
        restricted(finerVWeight, &coarserVWeight);
        restricted(finerWWeight, &coarserWWeight);
    }
}

void FractionalSinglePhasePressureSolver3::decompressSolution() {
    const auto acc = _fluidSdf[0].constAccessor();
    _system.x.resize(acc.size());
    
    size_t row = 0;
    _fluidSdf[0].forEachIndex([&](size_t i, size_t j, size_t k) {
        if (vox::isInsideSdf(acc(i, j, k))) {
            _system.x(i, j, k) = _compSystem.x[row];
            ++row;
        }
    });
}

void FractionalSinglePhasePressureSolver3::buildSystem(
                                                       const FaceCenteredGrid3& input,
                                                       bool useCompressed) {
    vox::Size3 size = input.resolution();
    size_t numLevels = 1;
    
    if (_mgSystemSolver == nullptr) {
        if (!useCompressed) {
            _system.resize(size);
        }
    } else {
        // Build levels
        size_t maxLevels = _mgSystemSolver->params().maxNumberOfLevels;
        vox::FdmMgUtils3::resizeArrayWithFinest(size, maxLevels,
                                                &_mgSystem.A.levels);
        vox::FdmMgUtils3::resizeArrayWithFinest(size, maxLevels,
                                                &_mgSystem.x.levels);
        vox::FdmMgUtils3::resizeArrayWithFinest(size, maxLevels,
                                                &_mgSystem.b.levels);
        
        numLevels = _mgSystem.A.levels.size();
    }
    
    // Build top level
    const FaceCenteredGrid3* finer = &input;
    if (_mgSystemSolver == nullptr) {
        if (useCompressed) {
            buildSingleSystem(&_compSystem.A, &_compSystem.x, &_compSystem.b,
                              _fluidSdf[0], _uWeights[0], _vWeights[0],
                              _wWeights[0], _boundaryVel, *finer);
        } else {
            buildSingleSystem(&_system.A, &_system.b, _fluidSdf[0],
                              _uWeights[0], _vWeights[0], _wWeights[0],
                              _boundaryVel, *finer);
        }
    } else {
        buildSingleSystem(&_mgSystem.A.levels.front(),
                          &_mgSystem.b.levels.front(), _fluidSdf[0],
                          _uWeights[0], _vWeights[0], _wWeights[0],
                          _boundaryVel, *finer);
    }
    
    // Build sub-levels
    FaceCenteredGrid3 coarser;
    for (size_t l = 1; l < numLevels; ++l) {
        auto res = finer->resolution();
        auto h = finer->gridSpacing();
        auto o = finer->origin();
        res.x = res.x >> 1;
        res.y = res.y >> 1;
        res.z = res.z >> 1;
        h *= 2.0;
        
        // Down sample
        coarser.resize(res, h, o);
        coarser.fill(finer->sampler());
        
        buildSingleSystem(&_mgSystem.A.levels[l], &_mgSystem.b.levels[l],
                          _fluidSdf[l], _uWeights[l], _vWeights[l],
                          _wWeights[l], _boundaryVel, coarser);
        
        finer = &coarser;
    }
}

void FractionalSinglePhasePressureSolver3::applyPressureGradient(
                                                                 const FaceCenteredGrid3& input,
                                                                 FaceCenteredGrid3* output) {
    vox::Size3 size = input.resolution();
    const auto uPos = input.uPosition();
    const auto vPos = input.vPosition();
    const auto wPos = input.wPosition();
    auto u = input.getUGrid()->tree();
    auto v = input.getVGrid()->tree();
    auto w = input.getWGrid()->tree();
    
    const auto& x = pressure();
    
    vox::Vector3D invH = 1.0 / input.gridSpacing();
    
    x.forEachIndex([&](unsigned int i, unsigned int j, unsigned int k) {
        double centerPhi = _fluidSdf[0](i, j, k);
        
        if (i + 1 < size.x && _uWeights[0](i + 1, j, k) > 0.0 &&
            (vox::isInsideSdf(centerPhi) ||
             vox::isInsideSdf(_fluidSdf[0](i + 1, j, k)))) {
            double rightPhi = _fluidSdf[0](i + 1, j, k);
            double theta = vox::fractionInsideSdf(centerPhi, rightPhi);
            theta = std::max(theta, 0.01);
            
            output->getUGrid()->tree().setValue(openvdb::Coord(i + 1, j, k),
                                                input.u(openvdb::Coord(i + 1, j, k))
                                                + invH.x / theta * (x(i + 1, j, k) - x(i, j, k)));
        }
        
        if (j + 1 < size.y && _vWeights[0](i, j + 1, k) > 0.0 &&
            (vox::isInsideSdf(centerPhi) ||
             vox::isInsideSdf(_fluidSdf[0](i, j + 1, k)))) {
            double upPhi = _fluidSdf[0](i, j + 1, k);
            double theta = vox::fractionInsideSdf(centerPhi, upPhi);
            theta = std::max(theta, 0.01);
            
            output->getVGrid()->tree().setValue(openvdb::Coord(i, j + 1, k),
                                                input.v(openvdb::Coord(i, j + 1, k))
                                                + invH.y / theta * (x(i, j + 1, k) - x(i, j, k)));
        }
        
        if (k + 1 < size.z && _wWeights[0](i, j, k + 1) > 0.0 &&
            (vox::isInsideSdf(centerPhi) ||
             vox::isInsideSdf(_fluidSdf[0](i, j, k + 1)))) {
            double frontPhi = _fluidSdf[0](i, j, k + 1);
            double theta = vox::fractionInsideSdf(centerPhi, frontPhi);
            theta = std::max(theta, 0.01);
            
            output->getWGrid()->tree().setValue(openvdb::Coord(i, j, k + 1),
                                                input.w(openvdb::Coord(i, j, k + 1))
                                                + invH.z / theta * (x(i, j, k + 1) - x(i, j, k)));
        }
    });
}

void FractionalSinglePhasePressureSolver3::solve(
                                                 const FaceCenteredGrid3& input,
                                                 double timeIntervalInSeconds,
                                                 FaceCenteredGrid3* output,
                                                 const vox::ScalarField3& boundarySdf,
                                                 const vox::VectorField3& boundaryVelocity,
                                                 const vox::ScalarField3& fluidSdf,
                                                 bool useCompressed) {
    UNUSED_VARIABLE(timeIntervalInSeconds);
    
    buildWeights(input, boundarySdf, boundaryVelocity, fluidSdf);
    buildSystem(input, useCompressed);
    
    if (_systemSolver != nullptr) {
        // Solve the system
        if (_mgSystemSolver == nullptr) {
            if (useCompressed) {
                _system.clear();
                _systemSolver->solveCompressed(&_compSystem);
                decompressSolution();
            } else {
                _compSystem.clear();
                _systemSolver->solve(&_system);
            }
        } else {
            _mgSystemSolver->solve(&_mgSystem);
        }
        
        // Apply pressure gradient
        applyPressureGradient(input, output);
    }
}
