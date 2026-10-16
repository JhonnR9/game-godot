#pragma once
#include "voxel_chunk.h"
#include "octahedron_geometry.h"
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/variant/vector3i.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

namespace godot {
// Engine adapter, not the chunk: owns the logical data and uploads CPU buffers.
class TerrainChunkRenderer : public MeshInstance3D {
    GDCLASS(TerrainChunkRenderer, MeshInstance3D)
    int64_t seed = 1337;
    bool connect_aligned_surfaces = true;
    double ambient_occlusion_strength = 0.8;
    double ambient_occlusion_radius = 1.25;
    std::vector<float> occlusion;
    Vector3i chunk_coordinates;
    terrain::VoxelChunk chunk;
    int face_count = 0;
    int triangle_count = 0;
    int dropped_triangle_count = 0;
    bool dirty = true;
    bool queued = false;
    StaticBody3D *terrain_body = nullptr;
    CollisionShape3D *terrain_collision = nullptr;
    void request_regeneration();
    void regenerate_if_dirty();
    void apply_triangles(const terrain::MeshBuffers &buffers);
protected:
    static void _bind_methods();
    void _validate_property(PropertyInfo &p_property) const;
public:
    void _ready() override;
    void regenerate();
    void set_seed(int64_t value);
    int64_t get_seed() const;
    void set_connect_aligned_surfaces(bool value);
    bool get_connect_aligned_surfaces() const;
    void set_ambient_occlusion_strength(double value);
    double get_ambient_occlusion_strength() const;
    void set_ambient_occlusion_radius(double value);
    double get_ambient_occlusion_radius() const;
    void set_chunk_coordinates(const Vector3i &value);
    Vector3i get_chunk_coordinates() const;
    int get_voxel_count() const;
    int get_visible_face_count() const;
    int get_triangle_count() const;
    int get_dropped_triangle_count() const;
    PackedByteArray get_voxel_data() const;
};
} // namespace godot
