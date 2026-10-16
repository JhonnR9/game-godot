#include "dependency_stencil.h"
#include "../../bridge_topology.h"
#include "../../octahedron_geometry.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <tuple>

namespace terrain::core {
namespace {
struct Bounds { Vec3 low, high; };
struct BridgeBounds { Bounds bounds; };
struct CandidateBridge { size_t frame_index; Int3 anchor; Vec3 center; Bounds bounds; };

constexpr double EPS=1e-9;

Bounds cell_bounds(Vec3 center) {
    const double extent=2.0*OctahedralLattice::SCALE; // max absolute coordinate of the approved cell vertices
    return {{center.x-extent,center.y-extent,center.z-extent},
            {center.x+extent,center.y+extent,center.z+extent}};
}

bool intersects(const Bounds &a,const Bounds &b) {
    return a.low.x<=b.high.x+EPS && a.high.x+EPS>=b.low.x &&
           a.low.y<=b.high.y+EPS && a.high.y+EPS>=b.low.y &&
           a.low.z<=b.high.z+EPS && a.high.z+EPS>=b.low.z;
}

Vec3 add(Vec3 a,Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vec3 scale(Vec3 a,double s) { return {a.x*s,a.y*s,a.z*s}; }
Vec3 center_relative(Int3 offset,Int3 reference) {
    const Vec3 a=OctahedralLattice::center(reference+offset);
    const Vec3 b=OctahedralLattice::center(reference);
    return {a.x-b.x,a.y-b.y,a.z-b.z};
}

Bounds bridge_bounds(const BridgeFrame &frame,Vec3 anchor_center) {
    const double s=OctahedralLattice::SCALE;
    Vec3 first=add(anchor_center,scale(frame.rotate(BRIDGE_VERTICES[0]),s));
    Bounds result{first,first};
    for(size_t i=1;i<BRIDGE_VERTICES.size();++i) {
        const Vec3 p=add(anchor_center,scale(frame.rotate(BRIDGE_VERTICES[i]),s));
        result.low.x=std::min(result.low.x,p.x); result.high.x=std::max(result.high.x,p.x);
        result.low.y=std::min(result.low.y,p.y); result.high.y=std::max(result.high.y,p.y);
        result.low.z=std::min(result.low.z,p.z); result.high.z=std::max(result.high.z,p.z);
    }
    return result;
}

Int3 central_cell(Vec3 physical,Int3 reference) {
    // Invert the documented BCC center map, after returning to global coords.
    const Vec3 ref=OctahedralLattice::center(reference);
    const Vec3 absolute=add(ref,physical);
    const int64_t y=std::llround(absolute.y/(2.0*OctahedralLattice::SCALE));
    const int p=OctahedralLattice::parity(y);
    const int64_t x=std::llround((absolute.x/OctahedralLattice::SCALE-2.0*p)/4.0);
    const int64_t z=std::llround((absolute.z/OctahedralLattice::SCALE-2.0*p)/4.0);
    return {x,y,z};
}

void insert_relative(std::set<std::tuple<int64_t,int64_t,int64_t>> &out,Int3 cell,Int3 target) {
    out.emplace(cell.x-target.x,cell.y-target.y,cell.z-target.z);
}
}

std::vector<Int3> DependencyStencil::cell_offsets(int y_parity) {
    const Int3 target{0,int64_t(y_parity&1),0};
    const Vec3 target_center=OctahedralLattice::center(target);
    const Bounds target_bounds=cell_bounds(target_center);
    std::set<std::tuple<int64_t,int64_t,int64_t>> offsets;
    offsets.emplace(0,0,0);

    // Original exposed faces depend on all 14 lattice neighbors.
    for(int face=0;face<14;++face) insert_relative(offsets,OctahedralLattice::neighbor(target,face),target);

    // Candidate bridges are selected by physical AABB overlap, expanded by the
    // actual truncated-octahedron bound. The search range exceeds the maximum
    // bridge span (4 geometry units) plus one cell radius on either side.
    std::vector<CandidateBridge> candidates;
    for(size_t fi=0;fi<BRIDGE_FRAMES.size();++fi) {
        const BridgeFrame &frame=BRIDGE_FRAMES[fi];
        for(int64_t dy=-4;dy<=4;++dy) for(int64_t dz=-4;dz<=4;++dz) for(int64_t dx=-4;dx<=4;++dx) {
            const Int3 anchor=target+Int3{dx,dy,dz};
            const Vec3 anchor_center=center_relative({dx,dy,dz},target);
            const Bounds bridge=bridge_bounds(frame,anchor_center);
            if(!intersects(bridge,target_bounds)) continue;
            candidates.push_back({fi,anchor,anchor_center,bridge});
        }
    }

    // A bridge face can itself be cut by another bridge. Add all bridge solids
    // whose bounds intersect the selected bridge bounds before collecting data.
    const std::vector<CandidateBridge> primary=candidates;
    for(size_t fi=0;fi<BRIDGE_FRAMES.size();++fi) {
        const BridgeFrame &frame=BRIDGE_FRAMES[fi];
        for(int64_t dy=-5;dy<=5;++dy) for(int64_t dz=-5;dz<=5;++dz) for(int64_t dx=-5;dx<=5;++dx) {
            const Int3 offset{dx,dy,dz};
            const Int3 anchor=target+offset;
            bool already=false;
            for(const auto &entry:candidates)
                if(entry.frame_index==fi && entry.anchor==anchor) { already=true; break; }
            if(already) continue;
            const Vec3 anchor_center=center_relative(offset,target);
            const Bounds bridge=bridge_bounds(frame,anchor_center);
            bool overlaps=false;
            for(const auto &entry:primary) overlaps|=intersects(bridge,entry.bounds);
            if(overlaps) candidates.push_back({fi,anchor,anchor_center,bridge});
        }
    }

    for(const auto &candidate_bridge:candidates) {
            const BridgeFrame &frame=BRIDGE_FRAMES[candidate_bridge.frame_index];
            const Int3 anchor=candidate_bridge.anchor;
            const Vec3 anchor_center=candidate_bridge.center;
            const Bounds &bridge=candidate_bridge.bounds;

            // Four supports, their exposed-side cells, and the central opening.
            const std::array<Int3,4> supports={anchor,anchor+frame.du,anchor+frame.dv,anchor+frame.du+frame.dv};
            for(const Int3 support:supports) {
                insert_relative(offsets,support,target);
                insert_relative(offsets,support+frame.forward,target);
            }
            const Vec3 local_center=add(anchor_center,scale(frame.rotate({2,2,2}),OctahedralLattice::SCALE));
            insert_relative(offsets,central_cell(local_center,target),target);

            // Every cell whose solid can overlap the bridge may clip a bridge
            // face. This is a conservative AABB broad phase, not a fixed halo.
            for(int64_t cy=-5;cy<=5;++cy) for(int64_t cz=-5;cz<=5;++cz) for(int64_t cx=-5;cx<=5;++cx) {
                const Int3 candidate=anchor+Int3{cx,cy,cz};
                if(intersects(cell_bounds(center_relative({candidate.x,candidate.y,candidate.z},target)),bridge))
                    insert_relative(offsets,candidate,target);
            }
    }

    std::vector<Int3> result;
    result.reserve(offsets.size());
    for(const auto &[x,y,z]:offsets) result.push_back({x,y,z});
    return result;
}

std::vector<BridgeAnchorOffset> DependencyStencil::bridge_anchor_offsets(int y_parity) {
    const Int3 target{0,int64_t(y_parity&1),0};
    const Bounds target_bounds=cell_bounds(OctahedralLattice::center(target));
    std::vector<CandidateBridge> primary;
    for(size_t fi=0;fi<BRIDGE_FRAMES.size();++fi) {
        const auto &frame=BRIDGE_FRAMES[fi];
        for(int64_t dy=-4;dy<=4;++dy) for(int64_t dz=-4;dz<=4;++dz) for(int64_t dx=-4;dx<=4;++dx) {
            const Int3 offset{dx,dy,dz};
            const Vec3 center=center_relative(offset,target);
            const Bounds bounds=bridge_bounds(frame,center);
            if(intersects(bounds,target_bounds)) primary.push_back({fi,target+offset,center,bounds});
        }
    }
    std::vector<CandidateBridge> candidates=primary;
    for(size_t fi=0;fi<BRIDGE_FRAMES.size();++fi) {
        const auto &frame=BRIDGE_FRAMES[fi];
        for(int64_t dy=-5;dy<=5;++dy) for(int64_t dz=-5;dz<=5;++dz) for(int64_t dx=-5;dx<=5;++dx) {
            const Int3 offset{dx,dy,dz}, anchor=target+offset;
            bool already=false;
            for(const auto &entry:candidates)
                if(entry.frame_index==fi && entry.anchor==anchor) { already=true; break; }
            if(already) continue;
            const Vec3 center=center_relative(offset,target);
            const Bounds bounds=bridge_bounds(frame,center);
            bool overlaps=false;
            for(const auto &entry:primary) overlaps|=intersects(bounds,entry.bounds);
            if(overlaps) candidates.push_back({fi,anchor,center,bounds});
        }
    }
    std::vector<BridgeAnchorOffset> result;
    result.reserve(candidates.size());
    for(const auto &candidate:candidates)
        result.push_back({{candidate.anchor.x-target.x,candidate.anchor.y-target.y,candidate.anchor.z-target.z},
                          static_cast<uint8_t>(candidate.frame_index)});
    return result;
}

std::vector<ChunkKey> DependencyStencil::chunk_keys_for(ChunkKey target) {
    std::set<std::tuple<int64_t,int64_t,int64_t>> keys;
    static const std::array<std::vector<Int3>,2> offsets_by_parity={cell_offsets(0),cell_offsets(1)};
    for(int y=0;y<VoxelChunk::SIZE;++y) for(int z=0;z<VoxelChunk::SIZE;++z) for(int x=0;x<VoxelChunk::SIZE;++x) {
        const Int3 global{target.x*VoxelChunk::SIZE+x,target.y*VoxelChunk::SIZE+y,target.z*VoxelChunk::SIZE+z};
        const auto &offsets=offsets_by_parity[static_cast<size_t>(OctahedralLattice::parity(global.y))];
        for(const Int3 offset:offsets) {
            const Int3 dependency=global+offset;
            const auto split=split_global_cell(dependency);
            keys.emplace(split.chunk.x,split.chunk.y,split.chunk.z);
        }
    }
    std::vector<ChunkKey> result;
    result.reserve(keys.size());
    for(const auto &[x,y,z]:keys) result.push_back({x,y,z});
    return result;
}

} // namespace terrain::core
