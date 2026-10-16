#pragma once
#include "octahedron_geometry.h"
#include <vector>

namespace terrain {
// Deterministic, geometry-based vertex AO. No Godot or occupancy assumptions.
// Visibility is evaluated only after the final planar connections are built.
class AmbientOcclusion {
public:
    static constexpr int SAMPLES = 32;
    static std::vector<float> calculate(const MeshBuffers &mesh, double radius);
};
}
