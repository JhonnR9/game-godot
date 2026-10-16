#include "core/geometry/surface_build_service.h"
#include "core/world/dependency_stencil.h"
#include <array>
#include <cassert>
#include <iostream>
#include <map>
#include <tuple>

using namespace terrain;
using namespace terrain::core;
using KeyTuple=std::tuple<int64_t,int64_t,int64_t>;

int main() {
    const GlobalCell left{15,1,0}, right{16,1,0};
    const ChunkKey left_key{0,0,0}, right_key{1,0,0};
    std::map<KeyTuple,std::array<BlockId,ChunkSnapshot::CELL_COUNT>> data;
    data[{left_key.x,left_key.y,left_key.z}]={};
    data[{right_key.x,right_key.y,right_key.z}]={};
    data[{left_key.x,left_key.y,left_key.z}][static_cast<size_t>(VoxelChunk::index({15,1,0}))]=BlockId::Stone;
    data[{right_key.x,right_key.y,right_key.z}][static_cast<size_t>(VoxelChunk::index({0,1,0}))]=BlockId::Dirt;
    for (const GlobalCell cell:{left,right}) for (int face=0;face<14;++face) {
        const Int3 adjacent=OctahedralLattice::neighbor(cell.coordinates(),face);
        const auto split=split_global_cell(adjacent);
        data.try_emplace({split.chunk.x,split.chunk.y,split.chunk.z});
    }
    for(const ChunkKey key:DependencyStencil::chunk_keys_for(left_key)) data.try_emplace({key.x,key.y,key.z});
    for(const ChunkKey key:DependencyStencil::chunk_keys_for(right_key)) data.try_emplace({key.x,key.y,key.z});

    WorldRepository repository(64);
    std::vector<ChunkKey> required;
    for (const auto &[key,blocks]:data) {
        const auto [x,y,z]=key;
        const ChunkKey chunk{x,y,z};
        required.push_back(chunk);
        assert(repository.publish_generated(chunk,blocks)==WorldRepository::PublishResult::Published);
    }

    const auto partial=SnapshotAssembler::capture(repository,{left_key});
    const auto pending=SurfaceBuildService::build_cell_faces(partial,left_key);
    assert(pending.status==SurfaceBuildResult::Status::DependenciesUnknown);
    assert(pending.polygons.empty() && !pending.missing_chunks.empty());

    const auto neighborhood=SnapshotAssembler::capture(repository,required);
    const auto built_left=SurfaceBuildService::build_cell_faces(neighborhood,left_key);
    const auto built_right=SurfaceBuildService::build_cell_faces(neighborhood,right_key);
    assert(built_left.status==SurfaceBuildResult::Status::BaseCellFacesReady);
    assert(built_right.status==SurfaceBuildResult::Status::BaseCellFacesReady);
    assert(built_left.polygons.size()==13 && built_right.polygons.size()==13);
    for (const auto &polygon:built_left.polygons)
        assert(polygon.primitive.kind==PrimitiveKind::CellFace && polygon.primitive.anchor==left);
    for (const auto &polygon:built_right.polygons)
        assert(polygon.primitive.kind==PrimitiveKind::CellFace && polygon.primitive.anchor==right);
    assert(BoundarySignature::from_polygons(built_left.polygons).segments.size()>0);
    auto full_left=SurfaceBuildService::build_final_surface(neighborhood,left_key);
    auto full_right=SurfaceBuildService::build_final_surface(neighborhood,right_key);
    assert(full_left.status==FinalSurfaceResult::Status::Ready);
    assert(full_right.status==FinalSurfaceResult::Status::Ready);
    assert(full_left.polygons.size()+full_right.polygons.size()==26);
    std::cout << "PASS: T03 base surfaces read real neighboring chunks and keep unknown dependencies closed\n";
}
