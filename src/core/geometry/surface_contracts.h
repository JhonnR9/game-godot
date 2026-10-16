#pragma once

#include "../world/world_repository.h"
#include <array>
#include <cstdint>
#include <vector>

namespace terrain::core {

enum class PrimitiveKind : uint8_t { CellFace = 0, BridgeFace = 1 };

struct PrimitiveId {
    PrimitiveKind kind = PrimitiveKind::CellFace;
    GlobalCell anchor;
    uint8_t element = 0;

    bool operator==(const PrimitiveId &b) const {
        return kind==b.kind && anchor==b.anchor && element==b.element;
    }
    bool operator<(const PrimitiveId &b) const {
        if (kind!=b.kind) return kind<b.kind;
        if (anchor.x!=b.anchor.x) return anchor.x<b.anchor.x;
        if (anchor.y!=b.anchor.y) return anchor.y<b.anchor.y;
        if (anchor.z!=b.anchor.z) return anchor.z<b.anchor.z;
        return element<b.element;
    }
};

ChunkKey primitive_owner(const PrimitiveId &primitive, const std::array<GlobalCell,4> &bridge_supports = {});

// Exact global lattice point in 1e-8 units of the canonical geometry lattice.
// Integer cell centers are combined before any floating point world transform.
struct LatticePoint {
    static constexpr int64_t FRACTION = 100000000;
    int64_t x = 0, y = 0, z = 0;
    bool operator==(const LatticePoint &b) const { return x==b.x && y==b.y && z==b.z; }
    bool operator<(const LatticePoint &b) const {
        if (x!=b.x) return x<b.x;
        if (y!=b.y) return y<b.y;
        return z<b.z;
    }
};

LatticePoint global_lattice_point(GlobalCell cell, Vec3 local_vertex);

struct SurfacePolygon {
    std::vector<LatticePoint> vertices;
    Vec3 normal;
    BlockId material = BlockId::Air;
    PrimitiveId primitive;
};

struct BoundarySegment {
    LatticePoint first, second;
    BlockId material = BlockId::Air;
    bool operator<(const BoundarySegment &b) const;
    bool operator==(const BoundarySegment &b) const {
        return first==b.first && second==b.second && material==b.material;
    }
};

struct BoundarySignature {
    std::vector<BoundarySegment> segments;
    static BoundarySignature from_polygons(const std::vector<SurfacePolygon> &polygons);
    bool operator==(const BoundarySignature &b) const { return segments==b.segments; }
};

SurfacePolygon make_cell_face(GlobalCell cell, uint8_t face, BlockId material);

} // namespace terrain::core
