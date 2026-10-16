#include "core/geometry/surface_contracts.h"
#include "planar_uv.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>

using namespace terrain;
using namespace terrain::core;

int main() {
    constexpr double uv_period=4.0*OctahedralLattice::SCALE;
    for(const Vec3 normal:std::array<Vec3,3>{{{0,1,0},{1,1,1},{1,1,0}}}) {
        const Vec3 base{0.25,-0.5,0.75};
        const Vec3 delta=normal.x==0?Vec3{1,0,0}:Vec3{normal.y,-normal.x,0};
        const PlanarUVProjection projection(normal,{-13.0,7.0,-29.0},uv_period);
        const auto a=projection.project(base);
        const auto b=projection.project({base.x+delta.x,base.y+delta.y,base.z+delta.z});
        const double uv_distance=std::hypot(a.u-b.u,a.v-b.v);
        const double physical_distance=std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z);
        assert(std::abs(uv_distance-physical_distance/uv_period)<1e-9);
    }
    const Vec3 offset_a=PlanarUVProjection::multiply({-64000000.0,64000000.0,-128000000.0},OctahedralLattice::SCALE);
    const Vec3 offset_b=PlanarUVProjection::multiply({-63999936.0,64000032.0,-127999936.0},OctahedralLattice::SCALE);
    const Vec3 local_a=PlanarUVProjection::multiply({64.0,32.0,64.0},OctahedralLattice::SCALE);
    const PlanarUVProjection negative_a({1,1,0},offset_a,uv_period);
    const PlanarUVProjection negative_b({1,1,0},offset_b,uv_period);
    const auto phase_a=negative_a.project(local_a),phase_b=negative_b.project({0,0,0});
    assert(std::abs((phase_a.u-std::floor(phase_a.u))-(phase_b.u-std::floor(phase_b.u)))<1e-7);
    assert(std::abs((phase_a.v-std::floor(phase_a.v))-(phase_b.v-std::floor(phase_b.v)))<1e-7);

    const GlobalCell cell{-1,3,0};
    const auto face=make_cell_face(cell,0,BlockId::Stone);
    assert(face.vertices.size()==4 && face.material==BlockId::Stone);
    assert(face.primitive==PrimitiveId{PrimitiveKind::CellFace,cell,0});
    assert((primitive_owner(face.primitive)==ChunkKey{-1,0,0}));

    auto reversed=face;
    std::reverse(reversed.vertices.begin(),reversed.vertices.end());
    auto sig_a=BoundarySignature::from_polygons({face});
    auto sig_b=BoundarySignature::from_polygons({reversed,face});
    assert(sig_a==sig_b);
    assert(sig_a.segments.size()==4);

    const std::array<GlobalCell,4> supports={GlobalCell{0,0,0},GlobalCell{-1,1,0},
        GlobalCell{0,0,-1},GlobalCell{-1,1,-1}};
    const PrimitiveId bridge{PrimitiveKind::BridgeFace,{0,0,0},2};
    assert((primitive_owner(bridge,supports)==ChunkKey{-1,0,-1}));

    const auto point_a=global_lattice_point({-1,1,0},{2.0,1.0,0.0});
    const auto point_b=global_lattice_point({-1,1,0},{2.0+1e-10,1.0,0.0});
    assert(point_a==point_b);
    assert(make_cell_face(cell,13,BlockId::Snow).vertices.size()==6);

    std::cout << "PASS: T03 stable primitive IDs, ownership, quantized global vertices, boundary signatures\n";
}
