#include "surface_build_service.h"
#include "convex_clipper.h"
#include "../../planar_uv.h"
#include "../world/dependency_stencil.h"
#include "../../bridge_topology.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <tuple>

namespace terrain::core {

namespace {
using ClipPolygon=ConvexPolygon;
Vec3 add(Vec3 a,Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
double dot(Vec3 a,Vec3 b);
GlobalCell bridge_center_cell(GlobalCell anchor,const BridgeFrame &frame) {
    const Vec3 offset=frame.rotate({2,2,2});
    const int64_t center_y=2*anchor.y+std::llround(offset.y);
    const int64_t y=std::llround(double(center_y)/2.0);
    const int64_t parity=OctahedralLattice::parity(y);
    const int64_t center_x=4*anchor.x+2*OctahedralLattice::parity(anchor.y)+std::llround(offset.x);
    const int64_t center_z=4*anchor.z+2*OctahedralLattice::parity(anchor.y)+std::llround(offset.z);
    return {std::llround(double(center_x-2*parity)/4.0),y,
            std::llround(double(center_z-2*parity)/4.0)};
}

bool solid_cell(const WorldRepository::CellRead &read) {
    if (read.kind!=WorldRepository::ReadKind::KnownBlock) return false;
    const auto *definition=BlockRegistry::find(read.block);
    return definition && definition->solid;
}

Vec3 cell_center_relative(GlobalCell cell,ChunkKey target) {
    const int64_t parity=OctahedralLattice::parity(cell.y);
    return {double(4*cell.x+2*parity-64*target.x),
            double(2*cell.y-32*target.y),
            double(4*cell.z+2*parity-64*target.z)};
}

ClipSolid cell_solid(GlobalCell cell,ChunkKey target) {
    const Vec3 center=cell_center_relative(cell,target);
    ClipSolid solid;
    solid.id={PrimitiveKind::CellFace,cell,0};
    solid.bounds={{center.x-2,center.y-2,center.z-2},{center.x+2,center.y+2,center.z+2}};
    for(const int sign:{-1,1}) {
        const Vec3 x{double(sign),0,0},y{0,double(sign),0},z{0,0,double(sign)};
        solid.planes.push_back({x,2+dot(x,center)});
        solid.planes.push_back({y,2+dot(y,center)});
        solid.planes.push_back({z,2+dot(z,center)});
    }
    for(const int x:{-1,1}) for(const int y:{-1,1}) for(const int z:{-1,1}) {
        const Vec3 n{double(x),double(y),double(z)};
        solid.planes.push_back({n,3+dot(n,center)});
    }
    return solid;
}

ClipSolid bridge_solid(const BridgeCandidate &candidate,ChunkKey target) {
    const auto &frame=BRIDGE_FRAMES[candidate.id.element];
    const Vec3 center=cell_center_relative(candidate.id.anchor,target);
    ClipPolygon vertices;
    for(const Vec3 v:BRIDGE_VERTICES) vertices.push_back(add(center,frame.rotate(v)));
    ClipSolid solid;
    solid.id=candidate.id;
    solid.bounds=ConvexClipper::bounds(vertices);
    for(const auto &plane:BRIDGE_PLANES) {
        const Vec3 n=frame.rotate(plane.normal);
        solid.planes.push_back({n,plane.distance+dot(n,center)});
    }
    return solid;
}

Vec3 to_chunk_local(LatticePoint point,ChunkKey chunk) {
    constexpr double f=double(LatticePoint::FRACTION);
    const int64_t bx=64*chunk.x,by=32*chunk.y,bz=64*chunk.z;
    return {double(point.x-bx*LatticePoint::FRACTION)/f,
            double(point.y-by*LatticePoint::FRACTION)/f,
            double(point.z-bz*LatticePoint::FRACTION)/f};
}

LatticePoint to_global_lattice(Vec3 point,ChunkKey chunk) {
    constexpr int64_t f=LatticePoint::FRACTION;
    return {64*chunk.x*f+std::llround(point.x*f),
            32*chunk.y*f+std::llround(point.y*f),
            64*chunk.z*f+std::llround(point.z*f)};
}

using BucketKey=std::tuple<int64_t,int64_t,int64_t>;
class CellSolidIndex {
    const std::vector<ClipSolid> &solids;
    std::map<BucketKey,std::vector<size_t>> buckets;
    static constexpr double SIZE=8.0;
public:
    explicit CellSolidIndex(const std::vector<ClipSolid> &source):solids(source) {
        for(size_t i=0;i<solids.size();++i) {
            const auto &b=solids[i].bounds;
            for(int64_t x=int64_t(std::floor(b.low.x/SIZE));x<=int64_t(std::floor(b.high.x/SIZE));++x)
            for(int64_t y=int64_t(std::floor(b.low.y/SIZE));y<=int64_t(std::floor(b.high.y/SIZE));++y)
            for(int64_t z=int64_t(std::floor(b.low.z/SIZE));z<=int64_t(std::floor(b.high.z/SIZE));++z)
                buckets[{x,y,z}].push_back(i);
        }
    }
    std::vector<size_t> query(const ClipBounds &b) const {
        std::set<size_t> unique;
        for(int64_t x=int64_t(std::floor(b.low.x/SIZE));x<=int64_t(std::floor(b.high.x/SIZE));++x)
        for(int64_t y=int64_t(std::floor(b.low.y/SIZE));y<=int64_t(std::floor(b.high.y/SIZE));++y)
        for(int64_t z=int64_t(std::floor(b.low.z/SIZE));z<=int64_t(std::floor(b.high.z/SIZE));++z) {
            const auto it=buckets.find({x,y,z});
            if(it!=buckets.end()) unique.insert(it->second.begin(),it->second.end());
        }
        std::vector<size_t> result;
        for(const auto i:unique) if(ConvexClipper::overlaps(b,solids[i].bounds)) result.push_back(i);
        return result;
    }
};

std::vector<ClipPolygon> subtract_from_pieces(std::vector<ClipPolygon> pieces,Vec3 normal,
                                               const PrimitiveId &owner,const ClipSolid &solid) {
    std::vector<ClipPolygon> result;
    for(const auto &piece:pieces) {
        auto fragments=ConvexClipper::subtract(piece,normal,owner,solid);
        for(auto &fragment:fragments) result.push_back(std::move(fragment));
    }
    return result;
}

Vec3 subtract(Vec3 a,Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3 cross(Vec3 a,Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
double dot(Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
Vec3 multiply(Vec3 a,double s) { return {a.x*s,a.y*s,a.z*s}; }
Vec3 normalized(Vec3 a) { return multiply(a,1.0/std::sqrt(dot(a,a))); }

std::vector<SurfacePolygon> bridge_faces_impl(const BridgeCandidate &candidate) {
    const BridgeFrame &frame=BRIDGE_FRAMES[candidate.id.element];
    std::vector<SurfacePolygon> output;
    output.reserve(BRIDGE_PLANES.size());
    for(size_t face_index=0;face_index<BRIDGE_PLANES.size();++face_index) {
        const auto &plane=BRIDGE_PLANES[face_index];
        std::vector<Vec3> vertices;
        Vec3 centroid{};
        for(const Vec3 vertex:BRIDGE_VERTICES) {
            if(std::abs(dot(plane.normal,vertex)-plane.distance)<1e-8) {
                vertices.push_back(vertex);
                centroid.x+=vertex.x; centroid.y+=vertex.y; centroid.z+=vertex.z;
            }
        }
        if(vertices.size()<3) continue;
        centroid=multiply(centroid,1.0/double(vertices.size()));
        const Vec3 normal=normalized(plane.normal);
        const Vec3 u=normalized(subtract(vertices.front(),centroid));
        const Vec3 v=cross(normal,u);
        std::sort(vertices.begin(),vertices.end(),[&](Vec3 a,Vec3 b) {
            a=subtract(a,centroid); b=subtract(b,centroid);
            return std::atan2(dot(a,v),dot(a,u))>std::atan2(dot(b,v),dot(b,u));
        });
        SurfacePolygon polygon;
        polygon.normal=frame.rotate(normal);
        polygon.material=candidate.material;
        const uint8_t face_id=static_cast<uint8_t>(candidate.id.element*BRIDGE_PLANES.size()+face_index);
        polygon.primitive={PrimitiveKind::BridgeFace,candidate.id.anchor,face_id};
        polygon.vertices.reserve(vertices.size());
        std::vector<Vec3> transformed;
        transformed.reserve(vertices.size());
        for(const Vec3 vertex:vertices) {
            const Vec3 rotated=frame.rotate(vertex);
            transformed.push_back(rotated);
            polygon.vertices.push_back(global_lattice_point(candidate.id.anchor,rotated));
        }
        if(dot(cross(subtract(transformed[2],transformed[0]),subtract(transformed[1],transformed[0])),polygon.normal)<0)
            std::reverse(polygon.vertices.begin(),polygon.vertices.end());
        output.push_back(std::move(polygon));
    }
    return output;
}

BlockId majority_material(const std::array<GlobalCell,4> &supports,const NeighborhoodSnapshot &snapshot) {
    std::array<std::pair<BlockId,int>,4> counts{};
    size_t used=0;
    for (const auto cell:supports) {
        const auto read=snapshot.read(cell);
        bool found=false;
        for(size_t i=0;i<used;++i) if(counts[i].first==read.block) { ++counts[i].second; found=true; break; }
        if(!found) counts[used++]={read.block,1};
    }
    for(size_t i=0;i<used;++i) for(size_t j=i+1;j<used;++j) {
        const bool swap=(counts[j].second>counts[i].second) ||
            (counts[j].second==counts[i].second && static_cast<uint16_t>(counts[j].first)<static_cast<uint16_t>(counts[i].first));
        if(swap) std::swap(counts[i],counts[j]);
    }
    return counts[0].first;
}
}

SurfaceBuildResult SurfaceBuildService::build_cell_faces(const NeighborhoodSnapshot &snapshot,ChunkKey target) {
    SurfaceBuildResult result;
    result.target=target;
    const auto chunk=snapshot.find(target);
    if (!chunk) { result.status=SurfaceBuildResult::Status::TargetUnknown; return result; }

    std::set<std::tuple<int64_t,int64_t,int64_t>> missing;
    for (int y=0;y<VoxelChunk::SIZE;++y) for (int z=0;z<VoxelChunk::SIZE;++z) for (int x=0;x<VoxelChunk::SIZE;++x) {
        const Int3 local{x,y,z};
        const BlockId material=chunk->get(local);
        const auto *definition=BlockRegistry::find(material);
        if (!definition || !definition->solid) continue;
        const GlobalCell cell{target.x*VoxelChunk::SIZE+x,target.y*VoxelChunk::SIZE+y,target.z*VoxelChunk::SIZE+z};
        for (uint8_t face=0;face<14;++face) {
            const Int3 adjacent=OctahedralLattice::neighbor(cell.coordinates(),face);
            const auto read=snapshot.read({adjacent.x,adjacent.y,adjacent.z});
            if (read.kind==WorldRepository::ReadKind::Unknown) {
                const auto address=split_global_cell(adjacent);
                missing.emplace(address.chunk.x,address.chunk.y,address.chunk.z);
                continue;
            }
            if (read.kind==WorldRepository::ReadKind::OutsideOperationalRange) {
                result.status=SurfaceBuildResult::Status::OutsideOperationalRange;
                return result;
            }
            const auto *neighbor=BlockRegistry::find(read.block);
            if (read.kind==WorldRepository::ReadKind::KnownBlock && neighbor && neighbor->solid) continue;
            result.polygons.push_back(make_cell_face(cell,face,material));
        }
    }

    if (!missing.empty()) {
        result.status=SurfaceBuildResult::Status::DependenciesUnknown;
        result.polygons.clear();
        for (const auto &[x,y,z]:missing) result.missing_chunks.push_back({x,y,z});
        return result;
    }
    result.status=SurfaceBuildResult::Status::BaseCellFacesReady;
    return result;
}

BridgeCandidateResult SurfaceBuildService::find_bridge_candidates(
    const NeighborhoodSnapshot &snapshot,ChunkKey target) {
    BridgeCandidateResult result;
    result.target=target;
    if (!snapshot.find(target)) { result.status=BridgeCandidateResult::Status::TargetUnknown; return result; }
    static const std::array<std::vector<BridgeAnchorOffset>,2> offsets_by_parity={
        DependencyStencil::bridge_anchor_offsets(0),DependencyStencil::bridge_anchor_offsets(1)};
    std::set<std::tuple<int64_t,int64_t,int64_t,uint8_t>> anchors;
    for(int y=0;y<VoxelChunk::SIZE;++y) for(int z=0;z<VoxelChunk::SIZE;++z) for(int x=0;x<VoxelChunk::SIZE;++x) {
        const GlobalCell cell{target.x*VoxelChunk::SIZE+x,target.y*VoxelChunk::SIZE+y,target.z*VoxelChunk::SIZE+z};
        const auto &offsets=offsets_by_parity[static_cast<size_t>(OctahedralLattice::parity(cell.y))];
        for(const auto &offset:offsets) {
            anchors.emplace(cell.x+offset.cell_offset.x,cell.y+offset.cell_offset.y,
                            cell.z+offset.cell_offset.z,offset.direction);
        }
    }

    std::set<std::tuple<int64_t,int64_t,int64_t>> missing;
    for(const auto &[x,y,z,direction]:anchors) {
        const GlobalCell anchor{x,y,z};
        const BridgeFrame &frame=BRIDGE_FRAMES[direction];
        const std::array<GlobalCell,4> supports={anchor,
            {anchor.x+frame.du.x,anchor.y+frame.du.y,anchor.z+frame.du.z},
            {anchor.x+frame.dv.x,anchor.y+frame.dv.y,anchor.z+frame.dv.z},
            {anchor.x+frame.du.x+frame.dv.x,anchor.y+frame.du.y+frame.dv.y,anchor.z+frame.du.z+frame.dv.z}};
        bool valid=true;
        for(const auto support:supports) {
            const auto read=snapshot.read(support);
            if(read.kind==WorldRepository::ReadKind::Unknown) {
                const auto address=split_global_cell(support.coordinates());
                missing.emplace(address.chunk.x,address.chunk.y,address.chunk.z);
            }
            valid &= solid_cell(read);
            const GlobalCell exposed{support.x+frame.forward.x,support.y+frame.forward.y,support.z+frame.forward.z};
            const auto exposed_read=snapshot.read(exposed);
            if(exposed_read.kind==WorldRepository::ReadKind::Unknown) {
                const auto address=split_global_cell(exposed.coordinates());
                missing.emplace(address.chunk.x,address.chunk.y,address.chunk.z);
            }
            valid &= exposed_read.kind==WorldRepository::ReadKind::KnownAir;
        }
        const GlobalCell center=bridge_center_cell(anchor,frame);
        const auto center_read=snapshot.read(center);
        if(center_read.kind==WorldRepository::ReadKind::Unknown) {
            const auto address=split_global_cell(center.coordinates());
            missing.emplace(address.chunk.x,address.chunk.y,address.chunk.z);
        }
        valid &= center_read.kind==WorldRepository::ReadKind::KnownAir;
        if(!valid) continue;
        BridgeCandidate candidate;
        candidate.id={PrimitiveKind::BridgeFace,anchor,direction};
        candidate.supports=supports;
        candidate.central_cell=center;
        candidate.owner=primitive_owner(candidate.id,supports);
        candidate.material=majority_material(supports,snapshot);
        result.candidates.push_back(candidate);
    }

    if(!missing.empty()) {
        result.status=BridgeCandidateResult::Status::DependenciesUnknown;
        result.candidates.clear();
        for(const auto &[x,y,z]:missing) result.missing_chunks.push_back({x,y,z});
        return result;
    }
    result.status=BridgeCandidateResult::Status::Ready;
    return result;
}

std::vector<SurfacePolygon> SurfaceBuildService::build_bridge_faces(const BridgeCandidate &candidate) {
    return bridge_faces_impl(candidate);
}

FinalSurfaceResult SurfaceBuildService::build_final_surface(const NeighborhoodSnapshot &snapshot,ChunkKey target) {
    FinalSurfaceResult result;
    result.target=target;
    if(!snapshot.find(target)) { result.status=FinalSurfaceResult::Status::TargetUnknown; return result; }

    std::set<std::tuple<int64_t,int64_t,int64_t>> missing;
    constexpr int64_t min_chunk_y=WorldDescriptor::MIN_Y/VoxelChunk::SIZE;
    constexpr int64_t max_chunk_y=WorldDescriptor::MAX_Y/VoxelChunk::SIZE;
    constexpr int64_t max_chunk_xz=WorldDescriptor::HORIZONTAL_LIMIT/VoxelChunk::SIZE;
    for(const ChunkKey key:DependencyStencil::chunk_keys_for(target)) {
        if(key.y<min_chunk_y || key.y>max_chunk_y || key.x < -max_chunk_xz || key.x > max_chunk_xz ||
           key.z < -max_chunk_xz || key.z > max_chunk_xz) continue;
        if(!snapshot.find(key)) missing.emplace(key.x,key.y,key.z);
    }
    if(!missing.empty()) {
        result.status=FinalSurfaceResult::Status::DependenciesUnknown;
        for(const auto &[x,y,z]:missing) result.missing_chunks.push_back({x,y,z});
        return result;
    }

    const auto base=build_cell_faces(snapshot,target);
    if(base.status==SurfaceBuildResult::Status::OutsideOperationalRange) {
        result.status=FinalSurfaceResult::Status::OutsideOperationalRange; return result;
    }
    if(base.status!=SurfaceBuildResult::Status::BaseCellFacesReady) {
        result.status=base.status==SurfaceBuildResult::Status::TargetUnknown
            ? FinalSurfaceResult::Status::TargetUnknown : FinalSurfaceResult::Status::DependenciesUnknown;
        result.missing_chunks=base.missing_chunks; return result;
    }
    const auto bridge_result=find_bridge_candidates(snapshot,target);
    if(bridge_result.status!=BridgeCandidateResult::Status::Ready) {
        result.status=bridge_result.status==BridgeCandidateResult::Status::TargetUnknown
            ? FinalSurfaceResult::Status::TargetUnknown : FinalSurfaceResult::Status::DependenciesUnknown;
        result.missing_chunks=bridge_result.missing_chunks; return result;
    }

    std::vector<ClipSolid> cell_solids;
    for(const auto &[key,chunk]:snapshot.chunks) {
        for(int y=0;y<VoxelChunk::SIZE;++y) for(int z=0;z<VoxelChunk::SIZE;++z) for(int x=0;x<VoxelChunk::SIZE;++x) {
            const BlockId id=chunk->get({x,y,z});
            const auto *definition=BlockRegistry::find(id);
            if(!definition || !definition->solid) continue;
            const GlobalCell cell{key.x*VoxelChunk::SIZE+x,key.y*VoxelChunk::SIZE+y,key.z*VoxelChunk::SIZE+z};
            cell_solids.push_back(cell_solid(cell,target));
        }
    }
    CellSolidIndex spatial(cell_solids);
    std::vector<ClipSolid> bridge_solids;
    bridge_solids.reserve(bridge_result.candidates.size());
    for(const auto &candidate:bridge_result.candidates) bridge_solids.push_back(bridge_solid(candidate,target));

    auto publish_fragments=[&](const SurfacePolygon &source,const std::vector<ClipPolygon> &pieces) {
        for(const auto &piece:pieces) {
            if(!ConvexClipper::valid(piece)) continue;
            SurfacePolygon polygon;
            polygon.normal=source.normal;
            polygon.material=source.material;
            polygon.primitive=source.primitive;
            polygon.vertices.reserve(piece.size());
            for(const auto p:piece) polygon.vertices.push_back(to_global_lattice(p,target));
            result.polygons.push_back(std::move(polygon));
        }
    };

    for(const auto &source:base.polygons) {
        const ClipPolygon original=[&] { ClipPolygon p; for(const auto v:source.vertices) p.push_back(to_chunk_local(v,target)); return p; }();
        std::vector<ClipPolygon> pieces{original};
        for(const auto i:spatial.query(ConvexClipper::bounds(original))) {
            const auto &solid=cell_solids[i];
            if(solid.id.anchor==source.primitive.anchor) continue;
            pieces=subtract_from_pieces(std::move(pieces),source.normal,source.primitive,solid);
            if(pieces.empty()) break;
        }
        for(const auto &solid:bridge_solids) {
            if(!ConvexClipper::overlaps(ConvexClipper::bounds(original),solid.bounds)) continue;
            pieces=subtract_from_pieces(std::move(pieces),source.normal,source.primitive,solid);
            if(pieces.empty()) break;
        }
        publish_fragments(source,pieces);
    }

    for(size_t bridge_index=0;bridge_index<bridge_result.candidates.size();++bridge_index) {
        const auto &candidate=bridge_result.candidates[bridge_index];
        if(candidate.owner!=target) continue; // canonical chunk emits the bridge exactly once
        for(const auto &source:build_bridge_faces(candidate)) {
            const ClipPolygon original=[&] { ClipPolygon p; for(const auto v:source.vertices) p.push_back(to_chunk_local(v,target)); return p; }();
            std::vector<ClipPolygon> pieces{original};
            for(const auto i:spatial.query(ConvexClipper::bounds(original))) {
                pieces=subtract_from_pieces(std::move(pieces),source.normal,source.primitive,cell_solids[i]);
                if(pieces.empty()) break;
            }
            for(size_t other=0;other<bridge_solids.size() && !pieces.empty();++other) {
                if(bridge_result.candidates[other].id==candidate.id) continue;
                if(!ConvexClipper::overlaps(ConvexClipper::bounds(original),bridge_solids[other].bounds)) continue;
                pieces=subtract_from_pieces(std::move(pieces),source.normal,source.primitive,bridge_solids[other]);
            }
            publish_fragments(source,pieces);
        }
    }

    result.boundary_signature=BoundarySignature::from_polygons(result.polygons);
    result.status=FinalSurfaceResult::Status::Ready;
    return result;
}

SurfaceMeshPayload SurfaceBuildService::build_mesh_payload(const NeighborhoodSnapshot &snapshot,ChunkKey target) {
    const auto surface=build_final_surface(snapshot,target);
    SurfaceMeshPayload result;
    result.status=surface.status;
    result.target=target;
    result.missing_chunks=surface.missing_chunks;
    if(surface.status!=FinalSurfaceResult::Status::Ready) return result;

    std::map<BlockId,MeshSurfacePayload> by_material;
    for(const auto &polygon:surface.polygons) {
        if(polygon.vertices.size()<3) continue;
        auto &mesh=by_material[polygon.material];
        mesh.material=polygon.material;
        const uint32_t base=static_cast<uint32_t>(mesh.positions.size());
        const uint32_t center_index=base;
        Vec3 center{};
        std::vector<Vec3> boundary;
        boundary.reserve(polygon.vertices.size());
        for(const auto point:polygon.vertices) {
            const Vec3 p=to_chunk_local(point,target);
            boundary.push_back(multiply(p,OctahedralLattice::SCALE));
            center=add(center,boundary.back());
        }
        center=multiply(center,1.0/double(boundary.size()));
        // Project in a deterministic orthonormal plane basis. UV phase is
        // anchored to global lattice coordinates, then rebased per polygon to
        // keep coordinates precise at distant world positions.
        constexpr double period=4.0*OctahedralLattice::SCALE;
        const Vec3 world_offset=multiply({double(64*target.x),double(32*target.y),double(64*target.z)},OctahedralLattice::SCALE);
        const PlanarUVProjection projection(polygon.normal,world_offset,period);
        auto uv=[&](Vec3 local) { const auto value=projection.project(local); return Vec3{value.u,value.v,0}; };
        mesh.positions.push_back(center);
        mesh.normals.push_back(polygon.normal);
        Vec3 center_uv{};
        for(const Vec3 p:boundary) center_uv=add(center_uv,uv(p));
        mesh.uvs.push_back(multiply(center_uv,1.0/double(boundary.size())));
        for(const Vec3 p:boundary) {
            mesh.positions.push_back(p);
            mesh.normals.push_back(polygon.normal);
            mesh.uvs.push_back(uv(p));
        }
        for(uint32_t i=0;i<boundary.size();++i) {
            mesh.indices.insert(mesh.indices.end(),{center_index,base+1+i,base+1+(i+1)%uint32_t(boundary.size())});
            mesh.triangle_primitives.push_back(polygon.primitive);
        }
    }
    result.surfaces.reserve(by_material.size());
    for(auto &[material,mesh]:by_material) {
        (void)material;
        result.surfaces.push_back(std::move(mesh));
    }
    return result;
}

} // namespace terrain::core
