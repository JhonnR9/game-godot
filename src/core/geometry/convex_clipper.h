#pragma once

#include "surface_contracts.h"

namespace terrain::core {

using ConvexPolygon = std::vector<Vec3>;
struct ClipPlane { Vec3 normal; double distance = 0; };
struct ClipBounds { Vec3 low, high; };
struct ClipSolid { std::vector<ClipPlane> planes; ClipBounds bounds; PrimitiveId id; };

class ConvexClipper {
public:
    static ClipBounds bounds(const ConvexPolygon &polygon);
    static bool overlaps(const ClipBounds &a,const ClipBounds &b);
    static bool valid(const ConvexPolygon &polygon);
    static std::vector<ConvexPolygon> subtract(const ConvexPolygon &polygon,Vec3 normal,
                                               const PrimitiveId &owner,const ClipSolid &cutter);
};

} // namespace terrain::core
