#pragma once

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/vector3i.hpp>
#include <cstdint>
#include <vector>

namespace godot {

// Body-centered cubic honeycomb, indexed as alternating horizontal layers.
// One voxel per (x,y,z), with fourteen face-neighbors instead of six.
class OctahedralVoxelChunk : public MeshInstance3D {
    GDCLASS(OctahedralVoxelChunk, MeshInstance3D)

    Vector3i dimensions = Vector3i(8, 4, 8);
    double edge_length = 0.5;
    Color color = Color(0.025, 0.25, 0.6);
    double color_variation = 0.18;
    std::vector<uint8_t> voxels = std::vector<uint8_t>(8 * 4 * 8, 1);
    bool dirty = true;
    bool rebuild_queued = false;
    int visible_face_count = 0;
    int triangle_count = 0;

    bool contains(const Vector3i &p_cell) const;
    int index_of(const Vector3i &p_cell) const;
    void request_rebuild();
    void rebuild_if_dirty();
    Vector3 lattice_center(const Vector3i &p_cell) const;

protected:
    static void _bind_methods();
    void _validate_property(PropertyInfo &p_property) const;

public:
    void _ready() override;
    void rebuild_mesh();

    void set_size_x(int p_size);
    int get_size_x() const;
    void set_size_y(int p_size);
    int get_size_y() const;
    void set_size_z(int p_size);
    int get_size_z() const;
    void set_chunk_size(const Vector3i &p_size);
    Vector3i get_chunk_size() const;
    void set_edge_length(double p_length);
    double get_edge_length() const;
    void set_color(const Color &p_color);
    Color get_color() const;
    void set_color_variation(double p_amount);
    double get_color_variation() const;

    bool get_voxel(const Vector3i &p_cell) const;
    void set_voxel(const Vector3i &p_cell, bool p_solid);
    void fill(bool p_solid);
    PackedByteArray get_voxel_data() const;
    void set_voxel_data(const PackedByteArray &p_data);
    Vector3 get_voxel_center(const Vector3i &p_cell) const;
    Vector3i get_neighbor(const Vector3i &p_cell, int p_face) const;
    int get_voxel_count() const;
    int get_visible_face_count() const;
    int get_triangle_count() const;
};

} // namespace godot
