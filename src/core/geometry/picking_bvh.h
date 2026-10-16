#pragma once

#include "surface_build_service.h"
#include "convex_clipper.h"

namespace terrain::core {

struct PickingBvhNode {
    ClipBounds bounds;
    uint32_t first = 0;
    uint32_t count = 0;
    uint32_t left = 0;
    uint32_t right = 0;
    bool leaf = false;
};

struct PickingBvh {
    struct Triangle {
        uint32_t surface = 0;
        uint32_t index_offset = 0;
        PrimitiveId primitive;
        BlockId material = BlockId::Air;
        ClipBounds bounds;
        Vec3 centroid;
    };
    std::vector<Triangle> triangles;
    std::vector<uint32_t> order;
    std::vector<PickingBvhNode> nodes;

    static PickingBvh build(const SurfaceMeshPayload &mesh, uint32_t leaf_capacity = 8);
};

struct RayHit {
    bool hit = false;
    double distance = 0;
    Vec3 position;
    PrimitiveId primitive;
    BlockId material = BlockId::Air;
};

RayHit raycast(const SurfaceMeshPayload &mesh,const PickingBvh &bvh,Vec3 origin,Vec3 direction,
               double max_distance = 5.0);

} // namespace terrain::core
