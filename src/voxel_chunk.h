#pragma once

#include <array>
#include <cstdint>
#include <algorithm>

namespace terrain {
struct Int3 {
    int64_t x = 0, y = 0, z = 0;
    Int3 operator+(const Int3 &b) const { return {x + b.x, y + b.y, z + b.z}; }
    bool operator==(const Int3 &b) const { return x == b.x && y == b.y && z == b.z; }
};
struct Vec3 { double x = 0, y = 0, z = 0; };
enum class Voxel : uint8_t { Empty, Grass, Dirt, Stone };

// Integer lattice coordinates are the persistent world identity. Keep all
// chunk conversion here so negative coordinates use mathematical floor.
struct ChunkAddress {
    Int3 chunk;
    Int3 local;
};

inline int64_t floor_div(int64_t value, int64_t divisor) {
    const int64_t quotient = value / divisor;
    const int64_t remainder = value % divisor;
    return quotient - ((remainder != 0 && remainder < 0) ? 1 : 0);
}

inline int64_t floor_mod(int64_t value, int64_t divisor) {
    const int64_t remainder = value % divisor;
    return remainder < 0 ? remainder + divisor : remainder;
}

inline ChunkAddress split_global_cell(Int3 global, int64_t size = 16) {
    const Int3 chunk{floor_div(global.x, size), floor_div(global.y, size), floor_div(global.z, size)};
    return {chunk, {global.x - chunk.x * size, global.y - chunk.y * size, global.z - chunk.z * size}};
}

inline Int3 join_global_cell(Int3 chunk, Int3 local, int64_t size = 16) {
    return {chunk.x * size + local.x, chunk.y * size + local.y, chunk.z * size + local.z};
}

// Data only: no Node, Object, Resource, Godot types or mesh generation.
class VoxelChunk {
public:
    static constexpr int SIZE = 16;
    static constexpr int CAPACITY = SIZE * SIZE * SIZE;
    Int3 coordinates{};
    int64_t seed = 1337;
    std::array<Voxel, CAPACITY> cells{};

    static bool contains(Int3 p) {
        return p.x >= 0 && p.x < SIZE && p.y >= 0 && p.y < SIZE && p.z >= 0 && p.z < SIZE;
    }
    static int index(Int3 p) { return int(p.x + SIZE * (p.z + SIZE * p.y)); }
    Voxel get(Int3 p) const { return contains(p) ? cells[index(p)] : Voxel::Empty; }
    void set(Int3 p, Voxel value) { if (contains(p)) { cells[index(p)] = value; } }
    Int3 world_cell(Int3 local) const {
        return {coordinates.x * SIZE + local.x, coordinates.y * SIZE + local.y, coordinates.z * SIZE + local.z};
    }
    int solid_count() const {
        return int(std::count_if(cells.begin(), cells.end(), [](Voxel v) { return v != Voxel::Empty; }));
    }
};

// Shared lattice math, independent of both rendering and terrain generation.
struct OctahedralLattice {
    static constexpr double SCALE = 0.3535533905932737622; // 0.5 / sqrt(2)
    static int parity(int64_t y) { return int(floor_mod(y, 2)); }
    static Vec3 center(Int3 global) {
        const double p = double(parity(global.y));
        return {SCALE * (4.0 * double(global.x) + 2.0 * p),
                SCALE * (2.0 * double(global.y)),
                SCALE * (4.0 * double(global.z) + 2.0 * p)};
    }
    static Vec3 noise_position(Int3 global) {
        const int p = parity(global.y);
        return {double(global.x) * 2 + p, double(global.y), double(global.z) * 2 + p};
    }
    static Int3 neighbor(Int3 cell, int face) {
        constexpr Int3 square[6] = {{1,0,0}, {-1,0,0}, {0,2,0}, {0,-2,0}, {0,0,1}, {0,0,-1}};
        if (face >= 0 && face < 6) { return cell + square[face]; }
        if (face < 6 || face >= 14) { return {-1,-1,-1}; }
        const int hex = face - 6;
        const int p = parity(cell.y);
        return cell + Int3{p - ((hex & 4) ? 1 : 0), (hex & 2) ? -1 : 1, p - ((hex & 1) ? 1 : 0)};
    }
};
} // namespace terrain
