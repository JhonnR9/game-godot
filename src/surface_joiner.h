#pragma once
#include "octahedron_geometry.h"
#include "voxel_chunk.h"

namespace terrain {
// Adds planar eight-vertex bridges to exposed aligned 2x2 arrangements in all six axis directions.
// Convex clipping removes buried faces; all surviving facets stay flat.
class SurfaceJoiner {
public:
    static MeshBuffers join(const VoxelChunk &chunk, const MeshBuffers &input);
};
}
