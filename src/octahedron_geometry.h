#pragma once
#include "voxel_chunk.h"
#include <vector>
#include <cstdint>

namespace terrain {
struct Color { float r=1, g=1, b=1, a=1; };
struct MeshBuffers {
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Color> colors;
    std::vector<Voxel> materials;
    std::vector<uint32_t> indices;
    int face_count = 0;
};
// Geometry of one cell: creates clockwise polygon fans with flat normals.
// Plain arrays only; does not know about Godot, noise or voxel occupancy.
class OctahedronGeometry {
public:
    static constexpr double EDGE_LENGTH = 0.5;
    static void append_face(int face, Int3 lattice_center, Color color, Voxel material, MeshBuffers &output);
};
} // namespace terrain
