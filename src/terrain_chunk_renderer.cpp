#include "terrain_chunk_renderer.h"
#include "terrain_generator.h"
#include "chunk_mesher.h"
#include "ambient_occlusion.h"
#include "planar_uv.h"
#include <cmath>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>

#include <array>

using namespace godot;

namespace {
const char *material_path(terrain::Voxel voxel) {
    switch(voxel) {
        case terrain::Voxel::Grass: return "res://materials/blocks/grass.tres";
        case terrain::Voxel::Dirt: return "res://materials/blocks/dirt.tres";
        case terrain::Voxel::Stone: return "res://materials/blocks/stone.tres";
        default: return "";
    }
}
}

void TerrainChunkRenderer::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_seed", "value"), &TerrainChunkRenderer::set_seed);
    ClassDB::bind_method(D_METHOD("get_seed"), &TerrainChunkRenderer::get_seed);
    ClassDB::bind_method(D_METHOD("set_chunk_coordinates", "value"), &TerrainChunkRenderer::set_chunk_coordinates);
    ClassDB::bind_method(D_METHOD("get_chunk_coordinates"), &TerrainChunkRenderer::get_chunk_coordinates);
    ClassDB::bind_method(D_METHOD("regenerate"), &TerrainChunkRenderer::regenerate);
    ClassDB::bind_method(D_METHOD("_regenerate_if_dirty"), &TerrainChunkRenderer::regenerate_if_dirty);
    ClassDB::bind_method(D_METHOD("get_voxel_count"), &TerrainChunkRenderer::get_voxel_count);
    ClassDB::bind_method(D_METHOD("get_visible_face_count"), &TerrainChunkRenderer::get_visible_face_count);
    ClassDB::bind_method(D_METHOD("get_triangle_count"), &TerrainChunkRenderer::get_triangle_count);
    ClassDB::bind_method(D_METHOD("get_voxel_data"), &TerrainChunkRenderer::get_voxel_data);
    ADD_GROUP("Terrain Generation", "");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3I, "chunk_coordinates"), "set_chunk_coordinates", "get_chunk_coordinates");
    ClassDB::bind_method(D_METHOD("set_connect_aligned_surfaces", "value"), &TerrainChunkRenderer::set_connect_aligned_surfaces);
    ClassDB::bind_method(D_METHOD("get_connect_aligned_surfaces"), &TerrainChunkRenderer::get_connect_aligned_surfaces);
    ADD_GROUP("Surface", "");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "connect_aligned_surfaces"), "set_connect_aligned_surfaces", "get_connect_aligned_surfaces");
    ClassDB::bind_method(D_METHOD("set_ambient_occlusion_strength", "value"), &TerrainChunkRenderer::set_ambient_occlusion_strength);
    ClassDB::bind_method(D_METHOD("get_ambient_occlusion_strength"), &TerrainChunkRenderer::get_ambient_occlusion_strength);
    ClassDB::bind_method(D_METHOD("set_ambient_occlusion_radius", "value"), &TerrainChunkRenderer::set_ambient_occlusion_radius);
    ClassDB::bind_method(D_METHOD("get_ambient_occlusion_radius"), &TerrainChunkRenderer::get_ambient_occlusion_radius);
    ADD_GROUP("Ambient Occlusion", "ambient_occlusion_");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ambient_occlusion_strength", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_ambient_occlusion_strength", "get_ambient_occlusion_strength");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ambient_occlusion_radius", PROPERTY_HINT_RANGE, "0.05,5,0.05"), "set_ambient_occlusion_radius", "get_ambient_occlusion_radius");
    ADD_SIGNAL(MethodInfo("mesh_rebuilt"));
}
void TerrainChunkRenderer::_validate_property(PropertyInfo &property) const {
    if (property.name == StringName("mesh")) {
        property.usage &= ~PROPERTY_USAGE_STORAGE;
        property.usage |= PROPERTY_USAGE_READ_ONLY;
    }
}
void TerrainChunkRenderer::_ready() {
    if(!terrain_body) {
        terrain_body=memnew(StaticBody3D);
        terrain_body->set_name("TerrainCollisionBody");
        terrain_body->set_collision_layer(1);
        terrain_body->set_collision_mask(1);
        add_child(terrain_body);
        terrain_collision=memnew(CollisionShape3D);
        terrain_collision->set_name("TerrainCollisionShape");
        terrain_body->add_child(terrain_collision);
    }
    regenerate();
}
void TerrainChunkRenderer::set_seed(int64_t value) {
    if (seed==value) { return; }
    seed=value;
    request_regeneration();
}
int64_t TerrainChunkRenderer::get_seed() const { return seed; }
void TerrainChunkRenderer::set_chunk_coordinates(const Vector3i &value) {
    if (chunk_coordinates==value) { return; }
    chunk_coordinates=value;
    request_regeneration();
}
Vector3i TerrainChunkRenderer::get_chunk_coordinates() const { return chunk_coordinates; }
void TerrainChunkRenderer::set_connect_aligned_surfaces(bool value) {
    if (value==connect_aligned_surfaces) { return; }
    connect_aligned_surfaces=value;
    request_regeneration();
}
bool TerrainChunkRenderer::get_connect_aligned_surfaces() const { return connect_aligned_surfaces; }
void TerrainChunkRenderer::set_ambient_occlusion_strength(double value) {
    if (!std::isfinite(value) || value<0 || value>1 || value==ambient_occlusion_strength) return;
    ambient_occlusion_strength=value;
    request_regeneration();
}
double TerrainChunkRenderer::get_ambient_occlusion_strength() const { return ambient_occlusion_strength; }
void TerrainChunkRenderer::set_ambient_occlusion_radius(double value) {
    if (!std::isfinite(value) || value<0.05 || value>5 || value==ambient_occlusion_radius) return;
    ambient_occlusion_radius=value;
    request_regeneration();
}
double TerrainChunkRenderer::get_ambient_occlusion_radius() const { return ambient_occlusion_radius; }
void TerrainChunkRenderer::request_regeneration() {
    dirty=true;
    if (is_inside_tree() && !queued) {
        queued=true;
        call_deferred("_regenerate_if_dirty");
    }
}
void TerrainChunkRenderer::regenerate_if_dirty() {
    queued=false;
    if (dirty && is_inside_tree()) { regenerate(); }
}
void TerrainChunkRenderer::regenerate() {
    // Explicit stages: generate data -> build CPU geometry -> upload to Godot.
    chunk=terrain::TerrainGenerator(seed).generate({chunk_coordinates.x,chunk_coordinates.y,chunk_coordinates.z});
    const terrain::MeshBuffers buffers=terrain::ChunkMesher().build(chunk,connect_aligned_surfaces);
    // AO sees the actual final triangles, including bridges in all directions.
    occlusion = ambient_occlusion_strength>0
        ? terrain::AmbientOcclusion::calculate(buffers,ambient_occlusion_radius)
        : std::vector<float>();
    apply_triangles(buffers);
    face_count=buffers.face_count;
    triangle_count=int(buffers.indices.size()/3);
    dirty=false;
    emit_signal("mesh_rebuilt");
}
void TerrainChunkRenderer::apply_triangles(const terrain::MeshBuffers &buffers) {
    if (buffers.indices.empty()) {
        set_mesh(Ref<Mesh>());
        if(terrain_collision) terrain_collision->set_shape(Ref<Shape3D>());
        return;
    }
    std::array<PackedVector3Array,4> vertices,normals;
    std::array<PackedVector2Array,4> uvs;
    std::array<PackedColorArray,4> colors;
    std::array<PackedInt32Array,4> indices;
    constexpr double period=4.0*terrain::OctahedralLattice::SCALE;
    const terrain::Vec3 world_offset{
        double(chunk_coordinates.x)*64.0*terrain::OctahedralLattice::SCALE,
        double(chunk_coordinates.y)*32.0*terrain::OctahedralLattice::SCALE,
        double(chunk_coordinates.z)*64.0*terrain::OctahedralLattice::SCALE};
    for(size_t tri=0;tri+2<buffers.indices.size();tri+=3) {
        const uint32_t source[3]={buffers.indices[tri],buffers.indices[tri+1],buffers.indices[tri+2]};
        const size_t material=static_cast<size_t>(buffers.materials[source[0]]);
        if(material>=vertices.size()) continue;
        const auto n0=buffers.normals[source[0]];
        const terrain::PlanarUVProjection projection(n0,world_offset,period);
        for(const uint32_t index:source) {
            const auto &p=buffers.positions[index];
            const auto &normal=buffers.normals[index];
            const Vector3 position(p.x,p.y,p.z);
            const auto uv=projection.project(p);
            const double shade=index<occlusion.size()
                ? 1.0-ambient_occlusion_strength*(1.0-occlusion[index]) : 1.0;
            const int32_t destination=vertices[material].size();
            vertices[material].append(position);
            normals[material].append(Vector3(normal.x,normal.y,normal.z));
            uvs[material].append(Vector2(real_t(uv.u),real_t(uv.v)));
            colors[material].append(Color(shade,shade,shade,1));
            indices[material].append(destination);
        }
    }
    Ref<ArrayMesh> mesh;
    mesh.instantiate();
    for(size_t material=1;material<vertices.size();++material) {
        if(indices[material].is_empty()) continue;
        Array arrays;
        arrays.resize(Mesh::ARRAY_MAX);
        arrays[Mesh::ARRAY_VERTEX]=vertices[material];
        arrays[Mesh::ARRAY_NORMAL]=normals[material];
        arrays[Mesh::ARRAY_TEX_UV]=uvs[material];
        arrays[Mesh::ARRAY_COLOR]=colors[material];
        arrays[Mesh::ARRAY_INDEX]=indices[material];
        const int surface=mesh->get_surface_count();
        mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES,arrays);
        const char *path=material_path(terrain::Voxel(material));
        Ref<Resource> resource=ResourceLoader::get_singleton()->load(path);
        Ref<Material> shared=resource;
        if(shared.is_valid()) mesh->surface_set_material(surface,shared);
    }
    set_mesh(mesh);
    if(terrain_collision) {
        Ref<ConcavePolygonShape3D> collision=mesh->create_trimesh_shape();
        terrain_collision->set_shape(collision);
    }
}
int TerrainChunkRenderer::get_voxel_count() const { return chunk.solid_count(); }
int TerrainChunkRenderer::get_visible_face_count() const { return face_count; }
int TerrainChunkRenderer::get_triangle_count() const { return triangle_count; }
PackedByteArray TerrainChunkRenderer::get_voxel_data() const {
    PackedByteArray data;
    data.resize(terrain::VoxelChunk::CAPACITY);
    for (int i=0;i<terrain::VoxelChunk::CAPACITY;++i) { data.set(i,uint8_t(chunk.cells[i])); }
    return data;
}
