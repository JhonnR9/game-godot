#include "truncated_octahedron.h"
#include "truncated_octahedron_geometry.h"

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <cmath>

using namespace godot;

void TruncatedOctahedron::_bind_methods() {
    ClassDB::bind_method(D_METHOD("rebuild_mesh"), &TruncatedOctahedron::rebuild_mesh);
    ClassDB::bind_method(D_METHOD("set_edge_length", "length"), &TruncatedOctahedron::set_edge_length);
    ClassDB::bind_method(D_METHOD("get_edge_length"), &TruncatedOctahedron::get_edge_length);
    ClassDB::bind_method(D_METHOD("set_color", "color"), &TruncatedOctahedron::set_color);
    ClassDB::bind_method(D_METHOD("get_color"), &TruncatedOctahedron::get_color);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "edge_length", PROPERTY_HINT_RANGE, "0.01,100,0.01"), "set_edge_length", "get_edge_length");
    ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), "set_color", "get_color");
}

void TruncatedOctahedron::_ready() { rebuild_mesh(); }

double TruncatedOctahedron::get_edge_length() const { return edge_length; }
Color TruncatedOctahedron::get_color() const { return color; }

void TruncatedOctahedron::set_edge_length(double p_length) {
    // Keep the previous valid mesh for invalid input, including NaN/Inf.
    if (!std::isfinite(p_length) || p_length < 0.01 || p_length > 100.0) {
        return;
    }
    edge_length = p_length;
    if (is_inside_tree()) { rebuild_mesh(); }
}

void TruncatedOctahedron::set_color(const Color &p_color) {
    color = p_color;
    if (is_inside_tree()) { rebuild_mesh(); }
}

void TruncatedOctahedron::rebuild_mesh() {
    namespace geometry = truncated_octahedron_geometry;
    const double scale = edge_length / std::sqrt(2.0);
    PackedVector3Array positions;
    PackedVector3Array normals;
    PackedInt32Array indices;

    auto vertex = [scale](int index) {
        const auto &v = geometry::vertices[index];
        return Vector3(v.x * scale, v.y * scale, v.z * scale);
    };

    for (const auto &face : geometry::faces) {
        const Vector3 a = vertex(face.indices[0]);
        const Vector3 b = vertex(face.indices[1]);
        const Vector3 c = vertex(face.indices[2]);
        // Clockwise front faces: reverse the conventional cross product
        // so the lighting normal points outwards.
        const Vector3 normal = (c - a).cross(b - a).normalized();
        const int base = positions.size();
        // Duplicate vertices at face boundaries: flat, crisp lighting.
        for (int i = 0; i < face.count; ++i) {
            positions.push_back(vertex(face.indices[i]));
            normals.push_back(normal);
        }
        // Convex polygon fan: square -> 2 triangles, hexagon -> 4.
        for (int i = 1; i < face.count - 1; ++i) {
            indices.push_back(base);
            indices.push_back(base + i);
            indices.push_back(base + i + 1);
        }
    }

    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);
    arrays[Mesh::ARRAY_VERTEX] = positions;
    arrays[Mesh::ARRAY_NORMAL] = normals;
    arrays[Mesh::ARRAY_INDEX] = indices;
    Ref<ArrayMesh> mesh;
    mesh.instantiate();
    mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    Ref<StandardMaterial3D> material;
    material.instantiate();
    material->set_albedo(color);
    material->set_roughness(0.7);
    mesh->surface_set_material(0, material);
    set_mesh(mesh);
}
