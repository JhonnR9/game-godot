#pragma once

#include "voxel_chunk.h"
#include <array>
#include <cstdint>

namespace terrain {

// Stable on-disk identifiers. Never persist enum ordinals from Voxel.
enum class BlockId : uint16_t {
    Air = 0,
    Grass = 1,
    Dirt = 2,
    Stone = 3,
    Sand = 4,
    Snow = 5,
    Wood = 6,
    Leaves = 7,
    Bedrock = 8,
};

struct BlockDefinition {
    BlockId id;
    const char *name;
    bool solid;
    bool removable;
    bool placeable;
    BlockId drop;
    const char *texture;
};

class BlockRegistry {
public:
    static constexpr std::array<BlockDefinition, 9> DEFINITIONS{{
        {BlockId::Air, "air", false, false, false, BlockId::Air, ""},
        {BlockId::Grass, "grass", true, true, true, BlockId::Grass, "grass"},
        {BlockId::Dirt, "dirt", true, true, true, BlockId::Dirt, "dirt"},
        {BlockId::Stone, "stone", true, true, true, BlockId::Stone, "stone"},
        {BlockId::Sand, "sand", true, true, true, BlockId::Sand, "sand"},
        {BlockId::Snow, "snow", true, true, true, BlockId::Snow, "snow"},
        {BlockId::Wood, "wood", true, true, true, BlockId::Wood, "wood"},
        {BlockId::Leaves, "leaves", true, true, true, BlockId::Leaves, "leaves"},
        {BlockId::Bedrock, "bedrock", true, false, false, BlockId::Bedrock, "bedrock"},
    }};

    static const BlockDefinition *find(BlockId id) {
        const auto raw = static_cast<uint16_t>(id);
        return raw < DEFINITIONS.size() ? &DEFINITIONS[raw] : nullptr;
    }
};

struct WorldDescriptor {
    static constexpr uint32_t FORMAT_VERSION = 1;
    static constexpr uint32_t GENERATOR_VERSION = 1;
    static constexpr int64_t MIN_Y = -128;
    static constexpr int64_t MAX_Y = 127;
    static constexpr int64_t MIN_BEDROCK_Y = -128;
    static constexpr int64_t MAX_BEDROCK_Y = -127;
    static constexpr int64_t HORIZONTAL_LIMIT = int64_t{1} << 30;

    std::array<uint8_t, 16> uuid{};
    int64_t seed = 1337;
    uint32_t format_version = FORMAT_VERSION;
    uint32_t generator_version = GENERATOR_VERSION;

    enum class VerticalDomain : uint8_t { InRange, VirtualBedrock, AboveCeiling };
    enum class BoundaryKind : uint8_t { Stored, VirtualBedrock, AboveWorld, OutsideOperationalRange };

    struct BoundaryCell {
        BoundaryKind kind;
        BlockId block;
        bool known;
        bool editable;
    };

    static VerticalDomain classify_y(int64_t y) {
        if (y < MIN_Y) return VerticalDomain::VirtualBedrock;
        if (y > MAX_Y) return VerticalDomain::AboveCeiling;
        return VerticalDomain::InRange;
    }

    static bool within_operational_xz(Int3 cell) {
        return cell.x >= -HORIZONTAL_LIMIT && cell.x <= HORIZONTAL_LIMIT &&
               cell.z >= -HORIZONTAL_LIMIT && cell.z <= HORIZONTAL_LIMIT;
    }

    // Classify cells that do not require chunk storage. The bottom two layers
    // are real bedrock; cells below them are a virtual solid safety boundary.
    static BoundaryCell classify_boundary(Int3 cell) {
        if (!within_operational_xz(cell)) {
            return {BoundaryKind::OutsideOperationalRange, BlockId::Air, true, false};
        }
        if (cell.y < MIN_Y) {
            return {BoundaryKind::VirtualBedrock, BlockId::Bedrock, true, false};
        }
        if (cell.y > MAX_Y) {
            return {BoundaryKind::AboveWorld, BlockId::Air, true, false};
        }
        if (cell.y <= MAX_BEDROCK_Y) {
            return {BoundaryKind::Stored, BlockId::Bedrock, true, false};
        }
        return {BoundaryKind::Stored, BlockId::Air, false, true};
    }
};

} // namespace terrain
