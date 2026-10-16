#pragma once

#include "surface_contracts.h"

namespace terrain::core {

struct SurfaceBuildResult {
    enum class Status : uint8_t { BaseCellFacesReady, TargetUnknown, DependenciesUnknown, OutsideOperationalRange };
    Status status = Status::TargetUnknown;
    ChunkKey target;
    std::vector<SurfacePolygon> polygons;
    std::vector<ChunkKey> missing_chunks;
};

struct BridgeCandidate {
    PrimitiveId id;
    std::array<GlobalCell,4> supports{};
    GlobalCell central_cell;
    ChunkKey owner;
    BlockId material = BlockId::Air;
};

struct BridgeCandidateResult {
    enum class Status : uint8_t { Ready, TargetUnknown, DependenciesUnknown };
    Status status = Status::TargetUnknown;
    ChunkKey target;
    std::vector<BridgeCandidate> candidates;
    std::vector<ChunkKey> missing_chunks;
};

struct FinalSurfaceResult {
    enum class Status : uint8_t { Ready, TargetUnknown, DependenciesUnknown, OutsideOperationalRange };
    Status status = Status::TargetUnknown;
    ChunkKey target;
    std::vector<SurfacePolygon> polygons;
    BoundarySignature boundary_signature;
    std::vector<ChunkKey> missing_chunks;
};

struct MeshSurfacePayload {
    BlockId material = BlockId::Air;
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec3> uvs; // x/y are dimensionless texture coordinates
    std::vector<uint32_t> indices;
    std::vector<PrimitiveId> triangle_primitives;
};

struct SurfaceMeshPayload {
    FinalSurfaceResult::Status status = FinalSurfaceResult::Status::TargetUnknown;
    ChunkKey target;
    std::vector<MeshSurfacePayload> surfaces;
    std::vector<ChunkKey> missing_chunks;
};

// Pure C++ surface pipeline: queries real neighbor snapshots, builds the final
// cell/bridge union and serializes material-grouped mesh payloads.
class SurfaceBuildService {
public:
    static SurfaceBuildResult build_cell_faces(const NeighborhoodSnapshot &snapshot, ChunkKey target);
    static BridgeCandidateResult find_bridge_candidates(const NeighborhoodSnapshot &snapshot, ChunkKey target);
    static std::vector<SurfacePolygon> build_bridge_faces(const BridgeCandidate &candidate);
    static FinalSurfaceResult build_final_surface(const NeighborhoodSnapshot &snapshot, ChunkKey target);
    static SurfaceMeshPayload build_mesh_payload(const NeighborhoodSnapshot &snapshot, ChunkKey target);
};

} // namespace terrain::core
