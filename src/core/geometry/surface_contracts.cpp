#include "surface_contracts.h"
#include "../../truncated_octahedron_geometry.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace terrain::core {

ChunkKey primitive_owner(const PrimitiveId &primitive,const std::array<GlobalCell,4> &bridge_supports) {
    GlobalCell owner=primitive.anchor;
    if (primitive.kind==PrimitiveKind::BridgeFace) {
        owner=bridge_supports[0];
        for (size_t i=1;i<bridge_supports.size();++i) {
            const auto &candidate=bridge_supports[i];
            if (candidate.x<owner.x || (candidate.x==owner.x && candidate.y<owner.y) ||
                (candidate.x==owner.x && candidate.y==owner.y && candidate.z<owner.z)) owner=candidate;
        }
    }
    const auto split=split_global_cell(owner.coordinates());
    return {split.chunk.x,split.chunk.y,split.chunk.z};
}

LatticePoint global_lattice_point(GlobalCell cell,Vec3 local_vertex) {
    const int64_t parity=OctahedralLattice::parity(cell.y);
    const int64_t base_x=4*cell.x+2*parity;
    const int64_t base_y=2*cell.y;
    const int64_t base_z=4*cell.z+2*parity;
    const auto quantize=[](double value) -> int64_t { return std::llround(value*LatticePoint::FRACTION); };
    return {(base_x*LatticePoint::FRACTION)+quantize(local_vertex.x),
            (base_y*LatticePoint::FRACTION)+quantize(local_vertex.y),
            (base_z*LatticePoint::FRACTION)+quantize(local_vertex.z)};
}

bool BoundarySegment::operator<(const BoundarySegment &b) const {
    if (first<b.first) return true;
    if (b.first<first) return false;
    if (second<b.second) return true;
    if (b.second<second) return false;
    return material<b.material;
}

BoundarySignature BoundarySignature::from_polygons(const std::vector<SurfacePolygon> &polygons) {
    BoundarySignature result;
    for (const auto &polygon:polygons) {
        if (polygon.vertices.size()<2) continue;
        for (size_t i=0;i<polygon.vertices.size();++i) {
            auto a=polygon.vertices[i];
            auto b=polygon.vertices[(i+1)%polygon.vertices.size()];
            if (b<a) std::swap(a,b);
            result.segments.push_back({a,b,polygon.material});
        }
    }
    std::sort(result.segments.begin(),result.segments.end());
    result.segments.erase(std::unique(result.segments.begin(),result.segments.end()),result.segments.end());
    return result;
}

SurfacePolygon make_cell_face(GlobalCell cell,uint8_t face,BlockId material) {
    if (face>=truncated_octahedron_geometry::faces.size()) throw std::out_of_range("face14 index");
    const auto &shape_face=truncated_octahedron_geometry::faces[face];
    const auto &vertices=truncated_octahedron_geometry::vertices;
    const auto &a=vertices[shape_face.indices[0]];
    const auto &b=vertices[shape_face.indices[1]];
    const auto &c=vertices[shape_face.indices[2]];
    const Vec3 u{c.x-a.x,c.y-a.y,c.z-a.z}, v{b.x-a.x,b.y-a.y,b.z-a.z};
    Vec3 normal{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
    const double length=std::sqrt(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z);
    normal={normal.x/length,normal.y/length,normal.z/length};

    SurfacePolygon polygon;
    polygon.normal=normal;
    polygon.material=material;
    polygon.primitive={PrimitiveKind::CellFace,cell,face};
    polygon.vertices.reserve(static_cast<size_t>(shape_face.count));
    for (int i=0;i<shape_face.count;++i) {
        const auto &vtx=vertices[shape_face.indices[i]];
        polygon.vertices.push_back(global_lattice_point(cell,{vtx.x,vtx.y,vtx.z}));
    }
    return polygon;
}

} // namespace terrain::core
