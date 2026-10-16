#include "core/world/world_repository.h"
#include "core/world/dependency_stencil.h"
#include <array>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace terrain;
using namespace terrain::core;

static std::array<BlockId,ChunkSnapshot::CELL_COUNT> empty_chunk() { return {}; }

int main() {
    WorldRepository world(2);
    const ChunkKey a{0,0,0}, b{-1,0,0}, bedrock{0,-8,0};
    auto unknown=world.read({-1,1,0});
    assert(unknown.kind==WorldRepository::ReadKind::Unknown);

    auto blocks=empty_chunk();
    blocks[static_cast<size_t>(VoxelChunk::index({15,1,0}))]=BlockId::Stone;
    assert(world.publish_generated(a,blocks)==WorldRepository::PublishResult::Published);
    auto old=world.snapshot(a);
    const auto old_handle=world.handle(a);
    assert(old && old->data_revision==1 && !old->dirty());
    assert(world.resolve(old_handle)==old);
    assert(world.read({15,1,0}).block==BlockId::Stone);
    assert(world.read({16,1,0}).kind==WorldRepository::ReadKind::Unknown);

    auto changed=world.apply_edits(a,1,{{{15,1,0},BlockId::Dirt},{{1,1,1},BlockId::Grass}});
    assert(changed.result==WorldRepository::EditResult::Applied && changed.revision==2);
    auto current=world.snapshot(a);
    assert(current!=old && current->data_revision==2 && current->dirty());
    assert(old->get({15,1,0})==BlockId::Stone); // published snapshots stay immutable
    assert(current->get({15,1,0})==BlockId::Dirt);
    assert(world.apply_edits(a,2,{{{1,1,1},BlockId::Air}}).result==WorldRepository::EditResult::Applied);
    current=world.snapshot(a);
    assert(current->get({1,1,1})==BlockId::Air);
    assert(current->data_revision==3);
    assert(world.resolve(old_handle)==current); // handle denotes slot identity, not a data revision
    assert(world.apply_edits(a,2,{{{0,0,0},BlockId::Stone}}).result==WorldRepository::EditResult::RevisionMismatch);
    assert(world.apply_edits(a,3,{{{16,0,0},BlockId::Stone}}).result==WorldRepository::EditResult::InvalidEdit);
    assert(world.apply_edits(a,3,{{{0,0,0},BlockId::Bedrock}}).result==WorldRepository::EditResult::InvalidEdit);
    assert(world.publish_generated(a,blocks)==WorldRepository::PublishResult::DirtyChunkProtected);

    auto captured=SnapshotAssembler::capture(world,{a,b,a});
    assert(captured.chunks.size()==1 && captured.find(a)==current);
    assert(captured.unknown.size()==1 && captured.unknown[0]==b);
    auto dependencies=SnapshotAssembler::capture_dependencies(world,a);
    assert(dependencies.find(a)==current && !dependencies.unknown.empty());
    for (const auto key:dependencies.unknown) assert(key.y>=-8 && key.y<=7);
    assert(dependencies.read({17,1,0}).kind==WorldRepository::ReadKind::Unknown);
    assert(dependencies.read({0,128,0}).kind==WorldRepository::ReadKind::KnownAir);
    assert(dependencies.read({0,-129,0}).block==BlockId::Bedrock);

    // Stale save ACKs do not mark a newer edit clean.
    assert(world.acknowledge_saved(a,1));
    assert(world.snapshot(a)->dirty());
    assert(world.acknowledge_saved(a,3));
    assert(!world.snapshot(a)->dirty());
    assert(!world.acknowledge_saved(a,4));

    auto bedrock_blocks=empty_chunk();
    assert(world.publish_generated(bedrock,bedrock_blocks)==WorldRepository::PublishResult::Published);
    for (int z=0;z<16;++z) for (int x=0;x<16;++x) {
        assert(world.read({x,-128,z}).kind==WorldRepository::ReadKind::KnownBlock);
        assert(world.read({x,-128,z}).block==BlockId::Bedrock);
        assert(world.read({x,-127,z}).block==BlockId::Bedrock);
    }
    assert(world.apply_edits(bedrock,1,{{{0,0,0},BlockId::Air}}).result==WorldRepository::EditResult::InvalidEdit);
    assert(world.read({0,-129,0}).kind==WorldRepository::ReadKind::KnownBlock);
    assert(world.read({0,-129,0}).block==BlockId::Bedrock);
    assert(world.read({0,128,0}).kind==WorldRepository::ReadKind::KnownAir);
    assert(world.read({(int64_t{1}<<30)+1,0,0}).kind==WorldRepository::ReadKind::OutsideOperationalRange);

    // All slots are protected while dirty; admission fails before data loss.
    auto dirty_a=world.apply_edits(a,3,{{{2,2,2},BlockId::Stone}});
    assert(dirty_a.result==WorldRepository::EditResult::Applied);
    auto dirty_edit=world.apply_edits(bedrock,1,{{{0,2,0},BlockId::Stone}});
    assert(dirty_edit.result==WorldRepository::EditResult::Applied);
    assert(world.publish_generated(b,empty_chunk())==WorldRepository::PublishResult::NoCapacity);
    assert(world.acknowledge_saved(bedrock,2));
    const ChunkHandle stale=world.handle(a);
    assert(world.publish_generated(b,empty_chunk())==WorldRepository::PublishResult::Published);
    assert(world.resolve(stale));
    assert(world.acknowledge_saved(a,4));
    const ChunkKey c{1,0,0};
    assert(world.publish_generated(c,empty_chunk())==WorldRepository::PublishResult::Published);
    assert(!world.resolve(stale));
    assert(world.snapshot(a)==nullptr);
    assert(world.resident_count()==2);

    const auto even_stencil=DependencyStencil::cell_offsets(0);
    const auto odd_stencil=DependencyStencil::cell_offsets(1);
    for (int parity=0;parity<2;++parity) {
        const Int3 anchor{0,parity,0};
        const auto &stencil=parity==0 ? even_stencil : odd_stencil;
        for (int face=0;face<14;++face) {
            const Int3 n=OctahedralLattice::neighbor(anchor,face);
            bool found=false;
            for (const auto d:stencil) found|=(d==Int3{n.x-anchor.x,n.y-anchor.y,n.z-anchor.z});
            assert(found);
        }
        assert(stencil.size()>14);
    }
    const auto chunk_stencil=DependencyStencil::chunk_keys_for({-1,0,-1});
    bool has_target=false;
    for (const auto key:chunk_stencil) {
        has_target|=(key==ChunkKey{-1,0,-1});
        assert(std::abs(key.x+1)<=1 && std::abs(key.y)<=1 && std::abs(key.z+1)<=1);
    }
    assert(has_target && chunk_stencil.size()>1);

    std::cout << "PASS: T02 copy-on-write snapshots, revisions, dirty protection, handles, reads, geometry-derived halo\n";
}
