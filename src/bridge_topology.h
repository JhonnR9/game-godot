#pragma once

#include "voxel_chunk.h"
#include <array>

namespace terrain {

struct BridgeFrame {
    Vec3 u,n,v;
    Int3 du,dv,forward;
    constexpr Vec3 rotate(Vec3 p) const {
        return {u.x*p.x+n.x*p.y+v.x*p.z,
                u.y*p.x+n.y*p.y+v.y*p.z,
                u.z*p.x+n.z*p.y+v.z*p.z};
    }
};

// Ordered frames match the six approved axis-aligned bridge directions.
inline constexpr std::array<BridgeFrame,6> BRIDGE_FRAMES={{{{1,0,0},{0,1,0},{0,0,1},{1,0,0},{0,0,1},{0,2,0}},
    {{1,0,0},{0,-1,0},{0,0,1},{1,0,0},{0,0,1},{0,-2,0}},
    {{0,1,0},{1,0,0},{0,0,1},{0,2,0},{0,0,1},{1,0,0}},
    {{0,1,0},{-1,0,0},{0,0,1},{0,2,0},{0,0,1},{-1,0,0}},
    {{1,0,0},{0,0,1},{0,1,0},{1,0,0},{0,2,0},{0,0,1}},
    {{1,0,0},{0,0,-1},{0,1,0},{1,0,0},{0,2,0},{0,0,-1}}}};

inline constexpr std::array<Vec3,12> BRIDGE_VERTICES={{{1,2,0},{3,2,0},{4,2,1},{4,2,3},
    {3,2,4},{1,2,4},{0,2,3},{0,2,1},{2,1,0},{4,1,2},{2,1,4},{0,1,2}}};

struct BridgePlaneDefinition { Vec3 normal; double distance; };
inline constexpr std::array<BridgePlaneDefinition,10> BRIDGE_PLANES={{{{0,1,0},2},{{0,-1,0},-1},
    {{-1,0,0},0},{{1,0,0},4},{{0,0,-1},0},{{0,0,1},4},
    {{-1,-1,-1},-3},{{1,-1,-1},1},{{1,-1,1},5},{{-1,-1,1},1}}};

} // namespace terrain
