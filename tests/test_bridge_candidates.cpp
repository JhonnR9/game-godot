#include "core/geometry/surface_build_service.h"
#include "core/world/dependency_stencil.h"
#include "core/geometry/picking_bvh.h"
#include "bridge_topology.h"
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <tuple>

using namespace terrain;
using namespace terrain::core;
using KeyTuple=std::tuple<int64_t,int64_t,int64_t>;

static Vec3 physical(LatticePoint p) {
    const double scale=OctahedralLattice::SCALE/double(LatticePoint::FRACTION);
    return {p.x*scale,p.y*scale,p.z*scale};
}
static Vec3 cross3(Vec3 a,Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
static double dot3(Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
static void verify_closed_volume(const std::vector<SurfacePolygon> &polygons,double expected) {
    std::map<std::pair<LatticePoint,LatticePoint>,int> edges;
    double volume=0;
    for(const auto &polygon:polygons) {
        assert(polygon.vertices.size()>=3);
        for(size_t i=0;i<polygon.vertices.size();++i) {
            auto a=polygon.vertices[i],b=polygon.vertices[(i+1)%polygon.vertices.size()];
            if(b<a) std::swap(a,b);
            ++edges[{a,b}];
        }
        const Vec3 p0=physical(polygon.vertices[0]);
        for(size_t i=1;i+1<polygon.vertices.size();++i)
            volume-=dot3(p0,cross3(physical(polygon.vertices[i]),physical(polygon.vertices[i+1])))/6.0;
    }
    for(const auto &[edge,count]:edges) { (void)edge; assert(count==2); }
    assert(std::abs(volume-expected)<1e-5);
}
static double signed_volume(const std::vector<SurfacePolygon> &polygons) {
    double volume=0;
    for(const auto &polygon:polygons) {
        const Vec3 p0=physical(polygon.vertices[0]);
        for(size_t i=1;i+1<polygon.vertices.size();++i)
            volume-=dot3(p0,cross3(physical(polygon.vertices[i]),physical(polygon.vertices[i+1])))/6.0;
    }
    return volume;
}

static void verify_multichunk_frame(uint8_t direction,GlobalCell anchor) {
    const auto &frame=BRIDGE_FRAMES[direction];
    const std::array<GlobalCell,4> supports={anchor,
        GlobalCell{anchor.x+frame.du.x,anchor.y+frame.du.y,anchor.z+frame.du.z},
        GlobalCell{anchor.x+frame.dv.x,anchor.y+frame.dv.y,anchor.z+frame.dv.z},
        GlobalCell{anchor.x+frame.du.x+frame.dv.x,anchor.y+frame.du.y+frame.dv.y,anchor.z+frame.du.z+frame.dv.z}};
    std::set<KeyTuple> target_chunks;
    std::map<KeyTuple,std::array<BlockId,ChunkSnapshot::CELL_COUNT>> data;
    for(const auto cell:supports) {
        const auto split=split_global_cell(cell.coordinates());
        const KeyTuple key{split.chunk.x,split.chunk.y,split.chunk.z};
        target_chunks.insert(key);
        data.try_emplace(key);
        data[key][static_cast<size_t>(VoxelChunk::index(split.local))]=BlockId::Stone;
    }
    std::set<KeyTuple> required_set;
    for(const auto &[x,y,z]:target_chunks)
        for(const auto key:DependencyStencil::chunk_keys_for({x,y,z})) required_set.emplace(key.x,key.y,key.z);
    for(const auto key:required_set) data.try_emplace(key);

    WorldRepository repository(64);
    std::vector<ChunkKey> required;
    for(const auto &[key,blocks]:data) {
        const auto [x,y,z]=key;
        assert(repository.publish_generated({x,y,z},blocks)==WorldRepository::PublishResult::Published);
    }
    for(const auto &[x,y,z]:required_set) required.push_back({x,y,z});
    const auto neighborhood=SnapshotAssembler::capture(repository,std::move(required));
    std::vector<SurfacePolygon> union_surface;
    for(const auto &[x,y,z]:target_chunks) {
        const auto built=SurfaceBuildService::build_final_surface(neighborhood,{x,y,z});
        assert(built.status==FinalSurfaceResult::Status::Ready);
        union_surface.insert(union_surface.end(),built.polygons.begin(),built.polygons.end());
    }
    verify_closed_volume(union_surface,6.65858885617);
}

int main() {
    const ChunkKey left{0,0,0}, right{1,0,0};
    const std::array<GlobalCell,4> supports={GlobalCell{15,5,5},GlobalCell{16,5,5},
        GlobalCell{15,5,6},GlobalCell{16,5,6}};
    std::map<KeyTuple,std::array<BlockId,ChunkSnapshot::CELL_COUNT>> data;
    for(const auto key:DependencyStencil::chunk_keys_for(left)) data[{key.x,key.y,key.z}]={};
    for(const auto key:DependencyStencil::chunk_keys_for(right)) data[{key.x,key.y,key.z}]={};
    for(size_t i=0;i<supports.size();++i) {
        const auto address=split_global_cell(supports[i].coordinates());
        auto &chunk=data[{address.chunk.x,address.chunk.y,address.chunk.z}];
        chunk[static_cast<size_t>(VoxelChunk::index(address.local))]=i<2?BlockId::Stone:BlockId::Dirt;
    }

    WorldRepository repository(64);
    for(const auto &[key,blocks]:data) {
        const auto [x,y,z]=key;
        assert(repository.publish_generated({x,y,z},blocks)==WorldRepository::PublishResult::Published);
    }
    auto required=DependencyStencil::chunk_keys_for(left);
    const auto right_dependencies=DependencyStencil::chunk_keys_for(right);
    required.insert(required.end(),right_dependencies.begin(),right_dependencies.end());
    const auto neighborhood=SnapshotAssembler::capture(repository,std::move(required));
    assert(neighborhood.unknown.empty());
    const auto built_left=SurfaceBuildService::find_bridge_candidates(neighborhood,left);
    assert(built_left.status==BridgeCandidateResult::Status::Ready);
    const BridgeCandidate *found=nullptr;
    for(const auto &candidate:built_left.candidates)
        if(candidate.id.anchor==supports[0] && candidate.id.element==0) found=&candidate;
    assert(found);
    assert((found->owner==left));
    assert((found->central_cell==GlobalCell{16,6,6}));
    assert(found->material==BlockId::Dirt); // 2:2 tie resolves by persistent BlockId
    for(size_t i=0;i<supports.size();++i) assert(found->supports[i]==supports[i]);
    const auto bridge_faces=SurfaceBuildService::build_bridge_faces(*found);
    assert(bridge_faces.size()==10);
    for(size_t i=0;i<bridge_faces.size();++i) {
        assert(bridge_faces[i].vertices.size()>=3);
        assert(bridge_faces[i].primitive.kind==PrimitiveKind::BridgeFace);
        assert(bridge_faces[i].primitive.anchor==supports[0]);
        assert(bridge_faces[i].primitive.element==i);
        const double normal_length=std::sqrt(bridge_faces[i].normal.x*bridge_faces[i].normal.x+
            bridge_faces[i].normal.y*bridge_faces[i].normal.y+bridge_faces[i].normal.z*bridge_faces[i].normal.z);
        assert(std::abs(normal_length-1.0)<1e-12);
    }
    assert(!BoundarySignature::from_polygons(bridge_faces).segments.empty());
    constexpr double single_bridge_volume=(34.0/3.0)*std::pow(OctahedralLattice::SCALE,3);
    for(uint8_t direction=0;direction<6;++direction) {
        BridgeCandidate oriented;
        oriented.id={PrimitiveKind::BridgeFace,{-2,1,3},direction};
        oriented.material=BlockId::Stone;
        const auto faces=SurfaceBuildService::build_bridge_faces(oriented);
        assert(faces.size()==10);
        assert(std::abs(signed_volume(faces)-single_bridge_volume)<1e-5);
    }
    // Three bridge frames cross support chunks on X/Z, Y/Z, and X/Y;
    // together they exercise face, edge, and corner halo ownership.
    verify_multichunk_frame(0,{15,5,15});
    verify_multichunk_frame(2,{15,14,15});
    verify_multichunk_frame(4,{15,14,15});

    const auto built_right=SurfaceBuildService::find_bridge_candidates(neighborhood,right);
    assert(built_right.status==BridgeCandidateResult::Status::Ready);
    size_t matching=0;
    for(const auto &candidate:built_right.candidates)
        if(candidate.id==found->id) { ++matching; assert(candidate.owner==left); }
    assert(matching==1); // one stable global ID; only the canonical owner emits it

    const auto final_left=SurfaceBuildService::build_final_surface(neighborhood,left);
    const auto final_right=SurfaceBuildService::build_final_surface(neighborhood,right);
    assert(final_left.status==FinalSurfaceResult::Status::Ready);
    assert(final_right.status==FinalSurfaceResult::Status::Ready);
    size_t left_bridge_faces=0,right_bridge_faces=0;
    for(const auto &polygon:final_left.polygons)
        if(polygon.primitive.kind==PrimitiveKind::BridgeFace && polygon.primitive.anchor==supports[0]) ++left_bridge_faces;
    for(const auto &polygon:final_right.polygons)
        if(polygon.primitive.kind==PrimitiveKind::BridgeFace && polygon.primitive.anchor==supports[0]) ++right_bridge_faces;
    assert(left_bridge_faces>0 && right_bridge_faces==0);
    assert(!final_left.boundary_signature.segments.empty() && !final_right.boundary_signature.segments.empty());
    const auto payload=SurfaceBuildService::build_mesh_payload(neighborhood,left);
    assert(payload.status==FinalSurfaceResult::Status::Ready && !payload.surfaces.empty());
    for(const auto &surface:payload.surfaces) {
        assert(surface.indices.size()%3==0);
        assert(surface.triangle_primitives.size()==surface.indices.size()/3);
        assert(surface.uvs.size()==surface.positions.size());
        for(const auto uv:surface.uvs) assert(std::isfinite(uv.x) && std::isfinite(uv.y));
        assert(surface.positions.size()==surface.normals.size());
        for(const uint32_t index:surface.indices) assert(index<surface.positions.size());
    }
    const auto bvh=PickingBvh::build(payload);
    assert(!bvh.nodes.empty() && !bvh.triangles.empty());
    const double s=OctahedralLattice::SCALE;
    const auto bridge_hit=raycast(payload,bvh,{64*s,6.0,24*s},{0,-1,0},5.0);
    assert(bridge_hit.hit && bridge_hit.primitive.kind==PrimitiveKind::BridgeFace);
    assert(bridge_hit.primitive.anchor==supports[0] && bridge_hit.primitive.element==0);
    const auto miss=raycast(payload,bvh,{40,6,40},{0,-1,0},5.0);
    assert(!miss.hit);
    auto combined_surface=final_left.polygons;
    combined_surface.insert(combined_surface.end(),final_right.polygons.begin(),final_right.polygons.end());
    verify_closed_volume(combined_surface,6.65859);

    const auto incomplete=SnapshotAssembler::capture(repository,{left});
    const auto pending=SurfaceBuildService::find_bridge_candidates(incomplete,left);
    assert(pending.status==BridgeCandidateResult::Status::DependenciesUnknown);
    assert(pending.candidates.empty() && !pending.missing_chunks.empty());
    std::cout << "PASS: bridge candidates span chunks with stable ID/owner, center, material, and Unknown handling\n";
}
