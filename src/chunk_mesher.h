#pragma once
#include "voxel_chunk.h"
#include "octahedron_geometry.h"

namespace terrain {
// Consumes already generated data. No noise evaluation or engine dependencies.
class ChunkMesher {
public:
    MeshBuffers build(const VoxelChunk &chunk, bool connect_aligned_surfaces = true) const;
};
} // namespace terrain
