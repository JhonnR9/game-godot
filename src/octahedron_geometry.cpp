#include "octahedron_geometry.h"
#include "truncated_octahedron_geometry.h"
#include <cmath>

namespace terrain {
void OctahedronGeometry::append_face(int face_index, Int3 center, Color color, Voxel material, MeshBuffers &out) {
    if (face_index < 0 || face_index >= 14) { return; }
    namespace shape = truncated_octahedron_geometry;
    const auto &face = shape::faces[face_index];
    const auto &a = shape::vertices[face.indices[0]];
    const auto &b = shape::vertices[face.indices[1]];
    const auto &c = shape::vertices[face.indices[2]];
    const Vec3 u{c.x-a.x,c.y-a.y,c.z-a.z}, v{b.x-a.x,b.y-a.y,b.z-a.z};
    Vec3 normal{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
    const double length=std::sqrt(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z);
    normal={normal.x/length,normal.y/length,normal.z/length};
    const double scale=EDGE_LENGTH/std::sqrt(2.0);
    const uint32_t base=uint32_t(out.positions.size());
    for (int i=0;i<face.count;++i) {
        const auto &p=shape::vertices[face.indices[i]];
        out.positions.push_back({(center.x+p.x)*scale,(center.y+p.y)*scale,(center.z+p.z)*scale});
        out.normals.push_back(normal);
        out.colors.push_back(color);
        out.materials.push_back(material);
    }
    for (int i=1;i<face.count-1;++i) {
        out.indices.insert(out.indices.end(), {base, base+uint32_t(i), base+uint32_t(i+1)});
    }
    ++out.face_count;
}
} // namespace terrain
