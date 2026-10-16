#include "core/runtime/surface_job_queue.h"
#include "core/world/dependency_stencil.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <iostream>

using namespace terrain::core;
using namespace terrain;

static bool same(const SurfaceMeshPayload &a,const SurfaceMeshPayload &b) {
    if(a.status!=b.status || a.target!=b.target || a.surfaces.size()!=b.surfaces.size()) return false;
    for(size_t i=0;i<a.surfaces.size();++i) {
        const auto &x=a.surfaces[i],&y=b.surfaces[i];
        if(x.material!=y.material || x.indices!=y.indices || x.triangle_primitives.size()!=y.triangle_primitives.size() ||
           x.positions.size()!=y.positions.size() || x.normals.size()!=y.normals.size() || x.uvs.size()!=y.uvs.size()) return false;
        for(size_t j=0;j<x.positions.size();++j)
            if(x.positions[j].x!=y.positions[j].x || x.positions[j].y!=y.positions[j].y || x.positions[j].z!=y.positions[j].z ||
               x.normals[j].x!=y.normals[j].x || x.normals[j].y!=y.normals[j].y || x.normals[j].z!=y.normals[j].z ||
               x.uvs[j].x!=y.uvs[j].x || x.uvs[j].y!=y.uvs[j].y) return false;
        for(size_t j=0;j<x.triangle_primitives.size();++j)
            if(!(x.triangle_primitives[j]==y.triangle_primitives[j])) return false;
    }
    return true;
}

int main() {
    SurfaceJobQueue queue(2,2,1024*1024,{2,2,2});
    NeighborhoodSnapshot empty;
    assert(queue.submit({0,0,0},1,JobClass::Visible,empty));
    assert(queue.submit({1,0,0},1,JobClass::Background,empty));
    assert(!queue.submit({2,0,0},1,JobClass::Background,empty)); // no output credit
    auto metrics=queue.metrics();
    assert(metrics.reserved_outputs==2 && metrics.reserved_bytes==2*1024*1024);
    queue.cancel({1,0,0});
    auto result=queue.wait_take({0,0,0},std::chrono::seconds(2));
    assert(result && result->revision==1 && result->payload.status==FinalSurfaceResult::Status::TargetUnknown);
    metrics=queue.metrics();
    assert(metrics.reserved_outputs==0 && metrics.ready_outputs==0 && metrics.reserved_bytes==0);
    queue.shutdown();
    metrics=queue.metrics();
    assert(metrics.jobs.running==0 && metrics.jobs.completed<=2);
    assert(!queue.submit({3,0,0},1,JobClass::Visible,empty));

    WorldRepository repository(128);
    std::vector<ChunkKey> required=DependencyStencil::chunk_keys_for({0,0,0});
    required.push_back({0,0,0});
    std::sort(required.begin(),required.end(),[](ChunkKey a,ChunkKey b){
        if(a.x!=b.x) return a.x<b.x;
        if(a.y!=b.y) return a.y<b.y;
        return a.z<b.z;
    });
    required.erase(std::unique(required.begin(),required.end()),required.end());
    for(const auto key:required) {
        std::array<BlockId,ChunkSnapshot::CELL_COUNT> blocks{};
        if(key==ChunkKey{0,0,0}) blocks[size_t(VoxelChunk::index({8,8,8}))]=BlockId::Stone;
        assert(repository.publish_generated(key,blocks)==WorldRepository::PublishResult::Published);
    }
    auto neighborhood=SnapshotAssembler::capture(repository,required);
    SurfaceJobQueue one_worker(1,1,1024*1024,{2,2,2});
    SurfaceJobQueue four_workers(4,1,1024*1024,{2,2,2});
    assert(one_worker.submit({0,0,0},1,JobClass::Visible,neighborhood));
    assert(four_workers.submit({0,0,0},1,JobClass::Visible,neighborhood));
    const auto serial=one_worker.wait_take({0,0,0},std::chrono::seconds(10));
    const auto parallel=four_workers.wait_take({0,0,0},std::chrono::seconds(10));
    assert(serial && parallel && serial->payload.status==FinalSurfaceResult::Status::Ready);
    assert(same(serial->payload,parallel->payload));
    one_worker.shutdown(); four_workers.shutdown();
    std::cout << "PASS: surface jobs consume immutable snapshots and publish through bounded output credits\n";
}
