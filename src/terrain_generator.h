#pragma once
#include "voxel_chunk.h"
#include "gradient_noise.h"

namespace terrain {
class TerrainGenerator {
    int64_t seed;
    GradientNoise surface_noise;
    GradientNoise cave_noise;
public:
    explicit TerrainGenerator(int64_t p_seed);
    double surface_height(double world_x, double world_z) const;
    bool is_cave(Vec3 world_position, double surface) const;
    Voxel sample(Int3 global_cell) const;
    VoxelChunk generate(Int3 chunk_coordinates) const;
};
} // namespace terrain
