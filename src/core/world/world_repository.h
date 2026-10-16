#pragma once

#include "../../world_types.h"
#include <array>
#include <cstddef>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace terrain::core {

struct GlobalCell {
    int64_t x = 0, y = 0, z = 0;
    Int3 coordinates() const { return {x,y,z}; }
    bool operator==(const GlobalCell &b) const { return x==b.x && y==b.y && z==b.z; }
};

struct ChunkKey {
    int64_t x = 0, y = 0, z = 0;
    Int3 coordinates() const { return {x,y,z}; }
    bool operator==(const ChunkKey &b) const { return x==b.x && y==b.y && z==b.z; }
    bool operator!=(const ChunkKey &b) const { return !(*this==b); }
};

struct ChunkKeyHash {
    size_t operator()(const ChunkKey &key) const noexcept;
};

struct ChunkSnapshot {
    static constexpr size_t CELL_COUNT = size_t(VoxelChunk::CAPACITY);
    ChunkKey key;
    uint64_t data_revision = 0;
    uint64_t persisted_revision = 0;
    std::array<BlockId, CELL_COUNT> blocks{};

    BlockId get(Int3 local) const;
    bool dirty() const { return data_revision > persisted_revision; }
};

struct ChunkHandle {
    size_t slot = 0;
    uint64_t slot_generation = 0;
    ChunkKey key{};
};

struct LocalEdit {
    Int3 local;
    BlockId value = BlockId::Air;
};

class WorldRepository {
public:
    enum class PublishResult : uint8_t { Published, AlreadyResident, NoCapacity, DirtyChunkProtected, InvalidChunk, InvalidBlockData };
    enum class EditResult : uint8_t { Applied, NoChange, UnknownChunk, RevisionMismatch, InvalidEdit, RevisionExhausted };
    enum class ReadKind : uint8_t { KnownBlock, KnownAir, Unknown, OutsideOperationalRange };
    struct CellRead {
        ReadKind kind = ReadKind::Unknown;
        BlockId block = BlockId::Air;
    };
    struct EditOutcome {
        EditResult result;
        uint64_t revision;
    };

    explicit WorldRepository(size_t capacity, WorldDescriptor descriptor = {});

    PublishResult publish_generated(ChunkKey key, const std::array<BlockId, ChunkSnapshot::CELL_COUNT> &blocks);
    ChunkHandle handle(ChunkKey key) const;
    std::shared_ptr<const ChunkSnapshot> resolve(ChunkHandle handle) const;
    std::shared_ptr<const ChunkSnapshot> snapshot(ChunkKey key) const;
    EditOutcome apply_edits(ChunkKey key, uint64_t expected_revision, const std::vector<LocalEdit> &edits);
    bool acknowledge_saved(ChunkKey key, uint64_t revision);
    CellRead read(GlobalCell cell) const;
    size_t capacity() const { return slots.size(); }
    size_t resident_count() const { return resident; }
    const WorldDescriptor &world_descriptor() const { return world; }

private:
    struct Slot {
        uint64_t generation = 1;
        std::shared_ptr<const ChunkSnapshot> snapshot;
    };
    std::vector<Slot> slots;
    std::unordered_map<ChunkKey,size_t,ChunkKeyHash> index;
    WorldDescriptor world;
    size_t resident = 0;
    size_t next_victim = 0;

    bool valid_key(ChunkKey key) const;
    size_t choose_slot();
    void enforce_bedrock(ChunkSnapshot &snapshot) const;
};

struct NeighborhoodSnapshot {
    std::unordered_map<ChunkKey,std::shared_ptr<const ChunkSnapshot>,ChunkKeyHash> chunks;
    std::vector<ChunkKey> unknown;
    WorldDescriptor descriptor;
    std::shared_ptr<const ChunkSnapshot> find(ChunkKey key) const;
    WorldRepository::CellRead read(GlobalCell cell) const;
};

class SnapshotAssembler {
public:
    static NeighborhoodSnapshot capture(const WorldRepository &repository, std::vector<ChunkKey> required);
    static NeighborhoodSnapshot capture_dependencies(const WorldRepository &repository, ChunkKey target);
};

} // namespace terrain::core
