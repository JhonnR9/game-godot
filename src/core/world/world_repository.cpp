#include "world_repository.h"
#include "dependency_stencil.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace terrain::core {

size_t ChunkKeyHash::operator()(const ChunkKey &key) const noexcept {
    auto mix=[](uint64_t x) {
        x ^= x >> 30; x *= UINT64_C(0xbf58476d1ce4e5b9);
        x ^= x >> 27; x *= UINT64_C(0x94d049bb133111eb);
        return x ^ (x >> 31);
    };
    const uint64_t a=mix(static_cast<uint64_t>(key.x));
    const uint64_t b=mix(static_cast<uint64_t>(key.y)+UINT64_C(0x9e3779b97f4a7c15));
    const uint64_t c=mix(static_cast<uint64_t>(key.z)+UINT64_C(0x3c6ef372fe94f82a));
    return static_cast<size_t>(a ^ (b<<1) ^ (c>>1));
}

BlockId ChunkSnapshot::get(Int3 local) const {
    if (!VoxelChunk::contains(local)) return BlockId::Air;
    return blocks[static_cast<size_t>(VoxelChunk::index(local))];
}

WorldRepository::WorldRepository(size_t capacity, WorldDescriptor descriptor)
    : slots(capacity), world(std::move(descriptor)) {
    if (capacity == 0) throw std::invalid_argument("WorldRepository capacity must be positive");
}

bool WorldRepository::valid_key(ChunkKey key) const {
    constexpr int64_t min_chunk=WorldDescriptor::MIN_Y/VoxelChunk::SIZE;
    constexpr int64_t max_chunk=WorldDescriptor::MAX_Y/VoxelChunk::SIZE;
    if (key.y<min_chunk || key.y>max_chunk) return false;
    const int64_t max_chunk_x=WorldDescriptor::HORIZONTAL_LIMIT/VoxelChunk::SIZE;
    return key.x>=-max_chunk_x && key.x<=max_chunk_x && key.z>=-max_chunk_x && key.z<=max_chunk_x;
}

void WorldRepository::enforce_bedrock(ChunkSnapshot &snapshot) const {
    for (int y=0;y<VoxelChunk::SIZE;++y) {
        const int64_t global_y=snapshot.key.y*VoxelChunk::SIZE+y;
        if (global_y<WorldDescriptor::MIN_Y || global_y>WorldDescriptor::MAX_BEDROCK_Y) continue;
        for (int z=0;z<VoxelChunk::SIZE;++z) for (int x=0;x<VoxelChunk::SIZE;++x)
            snapshot.blocks[static_cast<size_t>(VoxelChunk::index({x,y,z}))]=BlockId::Bedrock;
    }
}

size_t WorldRepository::choose_slot() {
    for (size_t offset=0;offset<slots.size();++offset) {
        const size_t candidate=(next_victim+offset)%slots.size();
        if (!slots[candidate].snapshot) {
            next_victim=(candidate+1)%slots.size();
            return candidate;
        }
    }
    for (size_t offset=0;offset<slots.size();++offset) {
        const size_t candidate=(next_victim+offset)%slots.size();
        if (!slots[candidate].snapshot->dirty()) {
            const ChunkKey old_key=slots[candidate].snapshot->key;
            index.erase(old_key);
            slots[candidate].snapshot.reset();
            if (resident>0) --resident;
            auto &generation=slots[candidate].generation;
            generation = generation==std::numeric_limits<uint64_t>::max() ? 1 : generation+1;
            next_victim=(candidate+1)%slots.size();
            return candidate;
        }
    }
    return slots.size();
}

WorldRepository::PublishResult WorldRepository::publish_generated(
    ChunkKey key, const std::array<BlockId,ChunkSnapshot::CELL_COUNT> &blocks) {
    if (!valid_key(key)) return PublishResult::InvalidChunk;
    for (size_t i=0;i<blocks.size();++i) {
        const BlockId id=blocks[i];
        if (!BlockRegistry::find(id)) return PublishResult::InvalidBlockData;
        if (id==BlockId::Bedrock) {
            const int local_y=static_cast<int>(i/(VoxelChunk::SIZE*VoxelChunk::SIZE));
            const int64_t global_y=key.y*VoxelChunk::SIZE+local_y;
            if (global_y>WorldDescriptor::MAX_BEDROCK_Y) return PublishResult::InvalidBlockData;
        }
    }
    auto found=index.find(key);
    size_t slot_index;
    uint64_t revision=1;
    uint64_t persisted=1;
    if (found!=index.end()) {
        return slots[found->second].snapshot->dirty()
            ? PublishResult::DirtyChunkProtected : PublishResult::AlreadyResident;
    } else {
        slot_index=choose_slot();
        if (slot_index==slots.size()) return PublishResult::NoCapacity;
        ++resident;
    }
    auto next=std::make_shared<ChunkSnapshot>();
    next->key=key;
    next->data_revision=revision;
    next->persisted_revision=persisted;
    next->blocks=blocks;
    enforce_bedrock(*next);
    slots[slot_index].snapshot=std::move(next);
    index[key]=slot_index;
    return PublishResult::Published;
}

ChunkHandle WorldRepository::handle(ChunkKey key) const {
    auto found=index.find(key);
    if (found==index.end()) return {slots.size(),0,key};
    const size_t slot_index=found->second;
    return {slot_index,slots[slot_index].generation,key};
}

std::shared_ptr<const ChunkSnapshot> WorldRepository::resolve(ChunkHandle handle) const {
    if (handle.slot>=slots.size()) return {};
    const auto &slot=slots[handle.slot];
    if (slot.generation!=handle.slot_generation || !slot.snapshot || slot.snapshot->key!=handle.key) return {};
    return slot.snapshot;
}

std::shared_ptr<const ChunkSnapshot> WorldRepository::snapshot(ChunkKey key) const {
    auto found=index.find(key);
    return found==index.end() ? std::shared_ptr<const ChunkSnapshot>{} : slots[found->second].snapshot;
}

WorldRepository::EditOutcome WorldRepository::apply_edits(
    ChunkKey key, uint64_t expected_revision, const std::vector<LocalEdit> &edits) {
    auto found=index.find(key);
    if (found==index.end()) return {EditResult::UnknownChunk,0};
    const size_t slot_index=found->second;
    const auto old=slots[slot_index].snapshot;
    if (old->data_revision!=expected_revision) return {EditResult::RevisionMismatch,old->data_revision};
    if (old->data_revision==std::numeric_limits<uint64_t>::max()) return {EditResult::RevisionExhausted,old->data_revision};
    auto next=std::make_shared<ChunkSnapshot>(*old);
    bool changed=false;
    for (const auto &edit:edits) {
        if (!VoxelChunk::contains(edit.local)) return {EditResult::InvalidEdit,old->data_revision};
        const auto *definition=BlockRegistry::find(edit.value);
        if (!definition || (edit.value!=BlockId::Air && !definition->placeable))
            return {EditResult::InvalidEdit,old->data_revision};
        const int64_t global_y=key.y*VoxelChunk::SIZE+edit.local.y;
        if (global_y<WorldDescriptor::MIN_Y || global_y>WorldDescriptor::MAX_Y ||
            global_y<=WorldDescriptor::MAX_BEDROCK_Y) return {EditResult::InvalidEdit,old->data_revision};
        const size_t cell=static_cast<size_t>(VoxelChunk::index(edit.local));
        if (next->blocks[cell]!=edit.value) { next->blocks[cell]=edit.value; changed=true; }
    }
    if (!changed) return {EditResult::NoChange,old->data_revision};
    next->data_revision=old->data_revision+1;
    // Keep persisted_revision at the old value: the new revision is dirty.
    slots[slot_index].snapshot=std::move(next);
    return {EditResult::Applied,old->data_revision+1};
}

bool WorldRepository::acknowledge_saved(ChunkKey key, uint64_t revision) {
    auto found=index.find(key);
    if (found==index.end()) return false;
    auto &current=slots[found->second].snapshot;
    if (revision>current->data_revision) return false;
    if (revision<=current->persisted_revision) return true;
    auto updated=std::make_shared<ChunkSnapshot>(*current);
    updated->persisted_revision=revision;
    current=std::move(updated);
    return true;
}

WorldRepository::CellRead WorldRepository::read(GlobalCell cell) const {
    const Int3 global=cell.coordinates();
    if (!WorldDescriptor::within_operational_xz(global)) return {ReadKind::OutsideOperationalRange,BlockId::Air};
    const auto boundary=WorldDescriptor::classify_boundary(global);
    if (boundary.kind==WorldDescriptor::BoundaryKind::VirtualBedrock)
        return {ReadKind::KnownBlock,BlockId::Bedrock};
    if (boundary.kind==WorldDescriptor::BoundaryKind::AboveWorld)
        return {ReadKind::KnownAir,BlockId::Air};
    if (global.y<=WorldDescriptor::MAX_BEDROCK_Y)
        return {ReadKind::KnownBlock,BlockId::Bedrock};
    const auto address=split_global_cell(global);
    auto data=snapshot({address.chunk.x,address.chunk.y,address.chunk.z});
    if (!data) return {ReadKind::Unknown,BlockId::Air};
    const BlockId value=data->get(address.local);
    return value==BlockId::Air ? CellRead{ReadKind::KnownAir,value} : CellRead{ReadKind::KnownBlock,value};
}

std::shared_ptr<const ChunkSnapshot> NeighborhoodSnapshot::find(ChunkKey key) const {
    const auto found=chunks.find(key);
    return found==chunks.end() ? std::shared_ptr<const ChunkSnapshot>{} : found->second;
}

WorldRepository::CellRead NeighborhoodSnapshot::read(GlobalCell cell) const {
    const Int3 global=cell.coordinates();
    if (!WorldDescriptor::within_operational_xz(global))
        return {WorldRepository::ReadKind::OutsideOperationalRange,BlockId::Air};
    const auto boundary=WorldDescriptor::classify_boundary(global);
    if (boundary.kind==WorldDescriptor::BoundaryKind::VirtualBedrock)
        return {WorldRepository::ReadKind::KnownBlock,BlockId::Bedrock};
    if (boundary.kind==WorldDescriptor::BoundaryKind::AboveWorld)
        return {WorldRepository::ReadKind::KnownAir,BlockId::Air};
    if (global.y<=WorldDescriptor::MAX_BEDROCK_Y)
        return {WorldRepository::ReadKind::KnownBlock,BlockId::Bedrock};
    const auto address=split_global_cell(global);
    const auto data=find({address.chunk.x,address.chunk.y,address.chunk.z});
    if (!data) return {WorldRepository::ReadKind::Unknown,BlockId::Air};
    const BlockId value=data->get(address.local);
    return value==BlockId::Air ? WorldRepository::CellRead{WorldRepository::ReadKind::KnownAir,value}
                               : WorldRepository::CellRead{WorldRepository::ReadKind::KnownBlock,value};
}

NeighborhoodSnapshot SnapshotAssembler::capture(const WorldRepository &repository,std::vector<ChunkKey> required) {
    std::sort(required.begin(),required.end(),[](const ChunkKey &a,const ChunkKey &b) {
        if (a.x!=b.x) return a.x<b.x;
        if (a.y!=b.y) return a.y<b.y;
        return a.z<b.z;
    });
    required.erase(std::unique(required.begin(),required.end()),required.end());
    NeighborhoodSnapshot output;
    output.descriptor=repository.world_descriptor();
    constexpr int64_t min_chunk_y=WorldDescriptor::MIN_Y/VoxelChunk::SIZE;
    constexpr int64_t max_chunk_y=WorldDescriptor::MAX_Y/VoxelChunk::SIZE;
    constexpr int64_t max_chunk_xz=WorldDescriptor::HORIZONTAL_LIMIT/VoxelChunk::SIZE;
    for (const ChunkKey key:required) {
        // World bounds are known; they never become missing-data dependencies.
        if (key.y<min_chunk_y || key.y>max_chunk_y || key.x < -max_chunk_xz || key.x > max_chunk_xz ||
            key.z < -max_chunk_xz || key.z > max_chunk_xz) continue;
        const auto data=repository.snapshot(key);
        if (data) output.chunks.emplace(key,data);
        else output.unknown.push_back(key);
    }
    return output;
}

NeighborhoodSnapshot SnapshotAssembler::capture_dependencies(const WorldRepository &repository,ChunkKey target) {
    return capture(repository,DependencyStencil::chunk_keys_for(target));
}

} // namespace terrain::core
