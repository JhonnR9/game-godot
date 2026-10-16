#include "terrain_generator.h"

namespace terrain {
TerrainGenerator::TerrainGenerator(int64_t p_seed) : seed(p_seed),
    surface_noise(uint64_t(p_seed)), cave_noise(uint64_t(p_seed) ^ UINT64_C(0x63d83595a76b4f21)) {}

double TerrainGenerator::surface_height(double x, double z) const {
    // World-space 2D fBm height field; four octaves, independent of chunk origin.
    return 10.0 + 9.0 * surface_noise.fbm2(x * 0.055 + 0.37, z * 0.055 + 0.71);
}
bool TerrainGenerator::is_cave(Vec3 p, double surface) const {
    // Keep a solid roof; caves only carve voxels at least 3 layers below surface.
    if (surface - p.y < 3.0) { return false; }
    return cave_noise.fbm3(p.x * 0.13 + 11.17, p.y * 0.13 + 7.31, p.z * 0.13 - 5.43) > 0.10;
}
Voxel TerrainGenerator::sample(Int3 global) const {
    const Vec3 p = OctahedralLattice::noise_position(global);
    const double height = surface_height(p.x, p.z);
    const double depth = height - p.y;
    if (depth < 0 || is_cave(p, height)) { return Voxel::Empty; }
    if (depth < 1.5) { return Voxel::Grass; }
    if (depth < 3.5) { return Voxel::Dirt; }
    return Voxel::Stone;
}
VoxelChunk TerrainGenerator::generate(Int3 coordinates) const {
    VoxelChunk chunk;
    chunk.coordinates = coordinates;
    chunk.seed = seed;
    for (int y=0;y<VoxelChunk::SIZE;++y) {
        for (int z=0;z<VoxelChunk::SIZE;++z) {
            for (int x=0;x<VoxelChunk::SIZE;++x) {
                const Int3 local{x,y,z};
                chunk.set(local, sample(chunk.world_cell(local)));
            }
        }
    }
    return chunk;
}
} // namespace terrain
