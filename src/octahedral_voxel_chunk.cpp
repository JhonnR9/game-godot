#include "octahedral_voxel_chunk.h"
#include "truncated_octahedron_geometry.h"

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <algorithm>
#include <cmath>

using namespace godot;

void OctahedralVoxelChunk::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_size_x", "size"), &OctahedralVoxelChunk::set_size_x);
    ClassDB::bind_method(D_METHOD("get_size_x"), &OctahedralVoxelChunk::get_size_x);
    ClassDB::bind_method(D_METHOD("set_size_y", "size"), &OctahedralVoxelChunk::set_size_y);
    ClassDB::bind_method(D_METHOD("get_size_y"), &OctahedralVoxelChunk::get_size_y);
    ClassDB::bind_method(D_METHOD("set_size_z", "size"), &OctahedralVoxelChunk::set_size_z);
    ClassDB::bind_method(D_METHOD("get_size_z"), &OctahedralVoxelChunk::get_size_z);
    ClassDB::bind_method(D_METHOD("set_chunk_size", "size"), &OctahedralVoxelChunk::set_chunk_size);
    ClassDB::bind_method(D_METHOD("get_chunk_size"), &OctahedralVoxelChunk::get_chunk_size);
    ClassDB::bind_method(D_METHOD("set_edge_length", "length"), &OctahedralVoxelChunk::set_edge_length);
    ClassDB::bind_method(D_METHOD("get_edge_length"), &OctahedralVoxelChunk::get_edge_length);
    ClassDB::bind_method(D_METHOD("set_color", "color"), &OctahedralVoxelChunk::set_color);
    ClassDB::bind_method(D_METHOD("get_color"), &OctahedralVoxelChunk::get_color);
    ClassDB::bind_method(D_METHOD("set_color_variation", "amount"), &OctahedralVoxelChunk::set_color_variation);
    ClassDB::bind_method(D_METHOD("get_color_variation"), &OctahedralVoxelChunk::get_color_variation);
    ClassDB::bind_method(D_METHOD("get_voxel", "cell"), &OctahedralVoxelChunk::get_voxel);
    ClassDB::bind_method(D_METHOD("set_voxel", "cell", "solid"), &OctahedralVoxelChunk::set_voxel);
    ClassDB::bind_method(D_METHOD("fill", "solid"), &OctahedralVoxelChunk::fill);
    ClassDB::bind_method(D_METHOD("get_voxel_data"), &OctahedralVoxelChunk::get_voxel_data);
    ClassDB::bind_method(D_METHOD("set_voxel_data", "data"), &OctahedralVoxelChunk::set_voxel_data);
    ClassDB::bind_method(D_METHOD("get_voxel_center", "cell"), &OctahedralVoxelChunk::get_voxel_center);
    ClassDB::bind_method(D_METHOD("get_neighbor", "cell", "face"), &OctahedralVoxelChunk::get_neighbor);
    ClassDB::bind_method(D_METHOD("get_voxel_count"), &OctahedralVoxelChunk::get_voxel_count);
    ClassDB::bind_method(D_METHOD("get_visible_face_count"), &OctahedralVoxelChunk::get_visible_face_count);
    ClassDB::bind_method(D_METHOD("get_triangle_count"), &OctahedralVoxelChunk::get_triangle_count);
    ClassDB::bind_method(D_METHOD("rebuild_mesh"), &OctahedralVoxelChunk::rebuild_mesh);
    ClassDB::bind_method(D_METHOD("_rebuild_if_dirty"), &OctahedralVoxelChunk::rebuild_if_dirty);

    ADD_GROUP("Chunk Dimensions", "");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "size_x", PROPERTY_HINT_RANGE, "1,64,1"), "set_size_x", "get_size_x");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "size_y", PROPERTY_HINT_RANGE, "1,64,1"), "set_size_y", "get_size_y");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "size_z", PROPERTY_HINT_RANGE, "1,64,1"), "set_size_z", "get_size_z");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "edge_length", PROPERTY_HINT_RANGE, "0.01,100,0.01"), "set_edge_length", "get_edge_length");
    ADD_GROUP("Voxel Appearance", "");
    ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), "set_color", "get_color");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "color_variation", PROPERTY_HINT_RANGE, "0,0.5,0.01"), "set_color_variation", "get_color_variation");
    // Store the occupancy, not the generated mesh. Order matters: sizes load first.
    ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "voxel_data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE), "set_voxel_data", "get_voxel_data");
    ADD_SIGNAL(MethodInfo("mesh_rebuilt"));
}

void OctahedralVoxelChunk::_validate_property(PropertyInfo &p_property) const {
    if (p_property.name == StringName("mesh")) {
        p_property.usage &= ~PROPERTY_USAGE_STORAGE;
        p_property.usage |= PROPERTY_USAGE_READ_ONLY;
    }
}

void OctahedralVoxelChunk::_ready() { rebuild_mesh(); }

bool OctahedralVoxelChunk::contains(const Vector3i &p_cell) const {
    return p_cell.x >= 0 && p_cell.y >= 0 && p_cell.z >= 0 &&
            p_cell.x < dimensions.x && p_cell.y < dimensions.y && p_cell.z < dimensions.z;
}

int OctahedralVoxelChunk::index_of(const Vector3i &p_cell) const {
    return p_cell.x + dimensions.x * (p_cell.z + dimensions.z * p_cell.y);
}

void OctahedralVoxelChunk::request_rebuild() {
    dirty = true;
    // Coalesce Inspector edits or many voxel writes into one mesh per frame.
    if (is_inside_tree() && !rebuild_queued) {
        rebuild_queued = true;
        call_deferred("_rebuild_if_dirty");
    }
}

void OctahedralVoxelChunk::rebuild_if_dirty() {
    rebuild_queued = false;
    if (dirty && is_inside_tree()) { rebuild_mesh(); }
}

void OctahedralVoxelChunk::set_chunk_size(const Vector3i &p_size) {
    if (p_size.x < 1 || p_size.y < 1 || p_size.z < 1 ||
            p_size.x > 64 || p_size.y > 64 || p_size.z > 64 || p_size == dimensions) {
        return;
    }
    dimensions = p_size;
    // Resizing deliberately regenerates a completely filled chunk.
    voxels.assign(dimensions.x * dimensions.y * dimensions.z, 1);
    request_rebuild();
}

void OctahedralVoxelChunk::set_size_x(int p_size) { set_chunk_size(Vector3i(p_size, dimensions.y, dimensions.z)); }
int OctahedralVoxelChunk::get_size_x() const { return dimensions.x; }
void OctahedralVoxelChunk::set_size_y(int p_size) { set_chunk_size(Vector3i(dimensions.x, p_size, dimensions.z)); }
int OctahedralVoxelChunk::get_size_y() const { return dimensions.y; }
void OctahedralVoxelChunk::set_size_z(int p_size) { set_chunk_size(Vector3i(dimensions.x, dimensions.y, p_size)); }
int OctahedralVoxelChunk::get_size_z() const { return dimensions.z; }
Vector3i OctahedralVoxelChunk::get_chunk_size() const { return dimensions; }

void OctahedralVoxelChunk::set_edge_length(double p_length) {
    if (!std::isfinite(p_length) || p_length < 0.01 || p_length > 100.0 || p_length == edge_length) { return; }
    edge_length = p_length;
    request_rebuild();
}
double OctahedralVoxelChunk::get_edge_length() const { return edge_length; }
void OctahedralVoxelChunk::set_color(const Color &p_color) {
    if (p_color == color) { return; }
    color = p_color;
    request_rebuild();
}
Color OctahedralVoxelChunk::get_color() const { return color; }
void OctahedralVoxelChunk::set_color_variation(double p_amount) {
    if (!std::isfinite(p_amount) || p_amount < 0 || p_amount > 0.5 || p_amount == color_variation) { return; }
    color_variation = p_amount;
    request_rebuild();
}
double OctahedralVoxelChunk::get_color_variation() const { return color_variation; }

bool OctahedralVoxelChunk::get_voxel(const Vector3i &p_cell) const {
    return contains(p_cell) && voxels[index_of(p_cell)] != 0;
}

void OctahedralVoxelChunk::set_voxel(const Vector3i &p_cell, bool p_solid) {
    if (!contains(p_cell) || get_voxel(p_cell) == p_solid) { return; }
    voxels[index_of(p_cell)] = p_solid ? 1 : 0;
    request_rebuild();
}

void OctahedralVoxelChunk::fill(bool p_solid) {
    std::fill(voxels.begin(), voxels.end(), p_solid ? 1 : 0);
    request_rebuild();
}

PackedByteArray OctahedralVoxelChunk::get_voxel_data() const {
    PackedByteArray data;
    data.resize(static_cast<int64_t>(voxels.size()));
    std::copy(voxels.begin(), voxels.end(), data.ptrw());
    return data;
}

void OctahedralVoxelChunk::set_voxel_data(const PackedByteArray &p_data) {
    if (p_data.size() != static_cast<int64_t>(voxels.size())) { return; }
    for (size_t i = 0; i < voxels.size(); ++i) { voxels[i] = p_data[i] ? 1 : 0; }
    request_rebuild();
}

Vector3 OctahedralVoxelChunk::lattice_center(const Vector3i &p_cell) const {
    const int parity = p_cell.y & 1;
    // In unscaled geometry units: square-neighbor offsets are +/-4 on
    // an axis, hexagonal-neighbor offsets are all combinations of (+/-2)^3.
    return Vector3(4 * p_cell.x + 2 * parity, 2 * p_cell.y, 4 * p_cell.z + 2 * parity);
}

Vector3 OctahedralVoxelChunk::get_voxel_center(const Vector3i &p_cell) const {
    const int odd_layer_offset = dimensions.y > 1 ? 1 : 0;
    const Vector3 origin(2 * (dimensions.x - 1) + odd_layer_offset, -2,
            2 * (dimensions.z - 1) + odd_layer_offset);
    return (lattice_center(p_cell) - origin) * (edge_length / std::sqrt(2.0));
}

Vector3i OctahedralVoxelChunk::get_neighbor(const Vector3i &p_cell, int p_face) const {
    // Order matches truncated_octahedron_geometry::faces exactly.
    static const Vector3i square_offsets[6] = {
        Vector3i(1, 0, 0), Vector3i(-1, 0, 0), Vector3i(0, 2, 0),
        Vector3i(0, -2, 0), Vector3i(0, 0, 1), Vector3i(0, 0, -1)
    };
    if (p_face >= 0 && p_face < 6) { return p_cell + square_offsets[p_face]; }
    if (p_face < 6 || p_face >= 14) { return Vector3i(-1, -1, -1); }
    const int hex = p_face - 6;
    const int parity = p_cell.y & 1;
    return p_cell + Vector3i(parity - ((hex & 4) ? 1 : 0),
            (hex & 2) ? -1 : 1, parity - ((hex & 1) ? 1 : 0));
}

int OctahedralVoxelChunk::get_voxel_count() const {
    return static_cast<int>(std::count(voxels.begin(), voxels.end(), uint8_t(1)));
}
int OctahedralVoxelChunk::get_visible_face_count() const { return visible_face_count; }
int OctahedralVoxelChunk::get_triangle_count() const { return triangle_count; }

void OctahedralVoxelChunk::rebuild_mesh() {
    namespace geometry = truncated_octahedron_geometry;
    const double scale = edge_length / std::sqrt(2.0);
    const int odd_layer_offset = dimensions.y > 1 ? 1 : 0;
    const Vector3 origin(2 * (dimensions.x - 1) + odd_layer_offset, -2,
            2 * (dimensions.z - 1) + odd_layer_offset);
    PackedVector3Array positions;
    PackedVector3Array normals;
    PackedColorArray colors;
    PackedInt32Array indices;
    visible_face_count = 0;
    triangle_count = 0;

    // Compute face normals once, independent of voxel position and scale.
    Vector3 face_normals[14];
    for (int f = 0; f < 14; ++f) {
        const auto &face = geometry::faces[f];
        const auto &a = geometry::vertices[face.indices[0]];
        const auto &b = geometry::vertices[face.indices[1]];
        const auto &c = geometry::vertices[face.indices[2]];
        face_normals[f] = Vector3(c.x - a.x, c.y - a.y, c.z - a.z)
                .cross(Vector3(b.x - a.x, b.y - a.y, b.z - a.z)).normalized();
    }

    for (int y = 0; y < dimensions.y; ++y) {
        for (int z = 0; z < dimensions.z; ++z) {
            for (int x = 0; x < dimensions.x; ++x) {
                const Vector3i cell(x, y, z);
                if (!get_voxel(cell)) { continue; }
                const Vector3 center = lattice_center(cell) - origin;
                const uint32_t hash = uint32_t(x) * 73856093u ^ uint32_t(y) * 19349663u ^ uint32_t(z) * 83492791u;
                const double brightness = 1.0 - color_variation * double(hash % 1024) / 1023.0;
                const Color tint(color.r * brightness, color.g * brightness, color.b * brightness, color.a);
                for (int f = 0; f < 14; ++f) {
                    if (get_voxel(get_neighbor(cell, f))) { continue; }
                    const auto &face = geometry::faces[f];
                    const int base = positions.size();
                    for (int i = 0; i < face.count; ++i) {
                        const auto &v = geometry::vertices[face.indices[i]];
                        // Add integer lattice coordinates before scaling, so a
                        // shared vertex has identical floating-point coordinates.
                        positions.push_back(Vector3((center.x + v.x) * scale,
                                (center.y + v.y) * scale, (center.z + v.z) * scale));
                        normals.push_back(face_normals[f]);
                        colors.push_back(tint);
                    }
                    for (int i = 1; i < face.count - 1; ++i) {
                        indices.push_back(base);
                        indices.push_back(base + i);
                        indices.push_back(base + i + 1);
                    }
                    ++visible_face_count;
                    triangle_count += face.count - 2;
                }
            }
        }
    }

    if (positions.is_empty()) {
        set_mesh(Ref<Mesh>());
    } else {
        Array arrays;
        arrays.resize(Mesh::ARRAY_MAX);
        arrays[Mesh::ARRAY_VERTEX] = positions;
        arrays[Mesh::ARRAY_NORMAL] = normals;
        arrays[Mesh::ARRAY_COLOR] = colors;
        arrays[Mesh::ARRAY_INDEX] = indices;
        Ref<ArrayMesh> mesh;
        mesh.instantiate();
        mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
        Ref<StandardMaterial3D> material;
        material.instantiate();
        material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
        material->set_roughness(0.8);
        mesh->surface_set_material(0, material);
        set_mesh(mesh);
    }
    dirty = false;
    emit_signal("mesh_rebuilt");
}
