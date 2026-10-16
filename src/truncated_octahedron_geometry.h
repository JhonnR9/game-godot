#pragma once

#include <array>

// Truncate the octahedron |x| + |y| + |z| <= 3 at x,y,z = +/-2.
// Its 24 vertices are the signed permutations of (0, 1, 2).
// Base edge = sqrt(2); multiply coordinates by edge_length / sqrt(2).
namespace truncated_octahedron_geometry {
struct Vertex { double x, y, z; };
struct Face { int count; std::array<int, 6> indices; };
inline constexpr std::array<Vertex, 24> vertices = {{
    {0, 1, 2}, // 0
    {0, 1, -2}, // 1
    {0, -1, 2}, // 2
    {0, -1, -2}, // 3
    {0, 2, 1}, // 4
    {0, 2, -1}, // 5
    {0, -2, 1}, // 6
    {0, -2, -1}, // 7
    {1, 0, 2}, // 8
    {1, 0, -2}, // 9
    {-1, 0, 2}, // 10
    {-1, 0, -2}, // 11
    {2, 0, 1}, // 12
    {2, 0, -1}, // 13
    {-2, 0, 1}, // 14
    {-2, 0, -1}, // 15
    {1, 2, 0}, // 16
    {1, -2, 0}, // 17
    {-1, 2, 0}, // 18
    {-1, -2, 0}, // 19
    {2, 1, 0}, // 20
    {2, -1, 0}, // 21
    {-2, 1, 0}, // 22
    {-2, -1, 0}, // 23
}};

// Six squares, then eight hexagons. Clockwise when seen from outside,
// as required by Godot. Unused square slots are ignored.
inline constexpr std::array<Face, 14> faces = {{
    {4, {13, 21, 12, 20, 0, 0}},
    {4, {15, 22, 14, 23, 0, 0}},
    {4, {5, 16, 4, 18, 0, 0}},
    {4, {7, 19, 6, 17, 0, 0}},
    {4, {2, 10, 0, 8, 0, 0}},
    {4, {3, 9, 1, 11, 0, 0}},
    {6, {20, 12, 8, 0, 4, 16}},
    {6, {20, 16, 5, 1, 9, 13}},
    {6, {21, 17, 6, 2, 8, 12}},
    {6, {21, 13, 9, 3, 7, 17}},
    {6, {22, 18, 4, 0, 10, 14}},
    {6, {22, 15, 11, 1, 5, 18}},
    {6, {23, 14, 10, 2, 6, 19}},
    {6, {23, 19, 7, 3, 11, 15}},
}};
} // namespace truncated_octahedron_geometry
