extends SceneTree
var failures := 0

func check(condition: bool, message: String) -> void:
    if not condition:
        failures += 1
        printerr("FAIL: " + message)

func _initialize() -> void:
    call_deferred("run")

func run() -> void:
    check(ClassDB.class_exists("TerrainChunkRenderer"), "renderer registered")
    check(not ClassDB.class_exists("OctahedralVoxelChunk"), "old chunk Node removed")
    check(not ClassDB.class_exists("VoxelChunk"), "logical chunk is not a Godot class")
    var renderer := ClassDB.instantiate("TerrainChunkRenderer") as MeshInstance3D
    root.add_child(renderer)
    var properties := {}
    for p in renderer.get_property_list():
        properties[p.name] = p
    for name in ["seed", "chunk_coordinates", "ambient_occlusion_strength", "ambient_occlusion_radius"]:
        check(properties.has(name) and properties[name].usage & PROPERTY_USAGE_EDITOR != 0, name + " visible in Inspector")
    for name in ["size_x", "size_y", "size_z", "chunk_size", "voxel_data"]:
        check(not properties.has(name), name + " removed from Inspector/storage")
    check(properties.mesh.usage & PROPERTY_USAGE_STORAGE == 0, "derived mesh not serialized")
    var original: PackedByteArray = renderer.get_voxel_data()
    check(original.size() == 4096, "fixed chunk 16 cubed")
    check(renderer.get_voxel_count() == 1946, "default seed matches standalone C++ data")
    check(renderer.get_triangle_count() > 0, "joined geometry uploaded")
    check(renderer.mesh != null and renderer.mesh.get_surface_count() == 3, "separate shared material surfaces for grass, dirt, and stone")
    var uploaded_indices := 0
    for surface in renderer.mesh.get_surface_count():
        var surface_arrays: Array = renderer.mesh.surface_get_arrays(surface)
        uploaded_indices += (surface_arrays[Mesh.ARRAY_INDEX] as PackedInt32Array).size()
        var surface_uvs: PackedVector2Array = surface_arrays[Mesh.ARRAY_TEX_UV]
        var surface_vertices: PackedVector3Array = surface_arrays[Mesh.ARRAY_VERTEX]
        var surface_colors: PackedColorArray = surface_arrays[Mesh.ARRAY_COLOR]
        check(surface_uvs.size() == surface_vertices.size(), "UV per uploaded vertex")
        var ao_is_grayscale := true
        for color in surface_colors:
            if abs(color.r - color.g) >= 0.001 or abs(color.g - color.b) >= 0.001:
                ao_is_grayscale = false
                break
        check(ao_is_grayscale, "AO occupies a grayscale vertex channel separate from block albedo")
        check(renderer.mesh.surface_get_material(surface) is ShaderMaterial, "textured shared material assigned")
    check(uploaded_indices == renderer.get_triangle_count() * 3, "all material surfaces preserve triangle count")
    check(renderer.get_dropped_triangle_count() == 0, "no CPU triangles dropped before ArrayMesh upload")
    var collision_node := renderer.get_node_or_null("TerrainCollisionBody/TerrainCollisionShape") as CollisionShape3D
    check(collision_node != null and collision_node.shape is ConcavePolygonShape3D, "collision adapter follows rendered terrain")
    if collision_node != null and collision_node.shape is ConcavePolygonShape3D:
        var collision_faces := (collision_node.shape as ConcavePolygonShape3D).get_faces()
        check(collision_faces.size() == renderer.get_triangle_count() * 3, "collision triangles match final visible mesh")
        await physics_frame
        var mesh_arrays: Array = renderer.mesh.surface_get_arrays(0)
        var mesh_vertices: PackedVector3Array = mesh_arrays[Mesh.ARRAY_VERTEX]
        var mesh_normals: PackedVector3Array = mesh_arrays[Mesh.ARRAY_NORMAL]
        var mesh_indices: PackedInt32Array = mesh_arrays[Mesh.ARRAY_INDEX]
        var i0 := mesh_indices[0]
        var i1 := mesh_indices[1]
        var i2 := mesh_indices[2]
        var ray_origin := (mesh_vertices[i0] + mesh_vertices[i1] + mesh_vertices[i2]) / 3.0
        var ray_normal := mesh_normals[i0].normalized()
        var ray_query := PhysicsRayQueryParameters3D.create(ray_origin + ray_normal * 2.0, ray_origin - ray_normal * 2.0)
        var ray_hit := renderer.get_world_3d().direct_space_state.intersect_ray(ray_query)
        check(not ray_hit.is_empty() and ray_hit.collider == collision_node.get_parent(), "physics ray hits matching terrain collider")
    for block in ["grass", "dirt", "stone", "sand", "snow", "wood", "leaves", "bedrock"]:
        var block_material := load("res://materials/blocks/" + block + ".tres") as ShaderMaterial
        check(block_material != null and block_material.get_shader_parameter("albedo_texture") is Texture2D, block + " texture material loads")
        if block_material != null:
            var block_texture := block_material.get_shader_parameter("albedo_texture") as Texture2D
            var block_image := block_texture.get_image()
            check(block_image.get_width() == 64 and block_image.get_height() == 64, block + " stays at native pixel dimensions")
            check(block_image.get_mipmap_count() == 0, block + " imports without blurry mip levels")
            check(block_material.shader.code.contains("filter_nearest") and block_material.shader.code.contains("repeat_enable"), block + " samples nearest and repeats")
    var indices: PackedInt32Array = renderer.mesh.surface_get_arrays(0)[Mesh.ARRAY_INDEX]
    check(uploaded_indices == renderer.get_triangle_count() * 3, "actual uploaded triangles")
    var shaded_colors: PackedColorArray = renderer.mesh.surface_get_arrays(0)[Mesh.ARRAY_COLOR]
    renderer.set("ambient_occlusion_strength", 0.0)
    await process_frame
    await process_frame
    var base_colors: PackedColorArray = renderer.mesh.surface_get_arrays(0)[Mesh.ARRAY_COLOR]
    var darkened := 0
    for i in base_colors.size():
        check(shaded_colors[i].r <= base_colors[i].r + 0.005, "AO only darkens local shading")
        if shaded_colors[i].r < base_colors[i].r - 0.005:
            darkened += 1
    check(darkened > 0, "final mesh contains occluded vertices")
    check(renderer.get_voxel_data() == original, "AO preserves voxel data")
    check(renderer.mesh.surface_get_arrays(0)[Mesh.ARRAY_INDEX] == indices, "AO preserves final geometry")
    for invalid in [-1.0, 2.0, INF, NAN]:
        renderer.set("ambient_occlusion_strength", invalid)
        check(renderer.get("ambient_occlusion_strength") == 0.0, "invalid AO strength ignored")
    renderer.set("ambient_occlusion_strength", 0.8)
    await process_frame
    await process_frame
    check(renderer.mesh.surface_get_arrays(0)[Mesh.ARRAY_COLOR] == shaded_colors, "AO regeneration deterministic")
    var joined_triangles: int = renderer.get_triangle_count()
    renderer.set("connect_aligned_surfaces", false)
    await process_frame
    await process_frame
    check(renderer.get_voxel_data() == original, "surface joining preserves logical occupancy")
    check(renderer.get_triangle_count() == 21492, "original faceted surface restored (%d triangles)" % renderer.get_triangle_count())
    check(renderer.get_dropped_triangle_count() == 0, "faceted upload does not drop triangles")
    renderer.set("connect_aligned_surfaces", true)
    await process_frame
    await process_frame
    check(renderer.get_triangle_count() == joined_triangles, "aligned connection switch rebuilds mesh (%d vs %d)" % [renderer.get_triangle_count(), joined_triangles])
    check(renderer.get_dropped_triangle_count() == 0, "joined upload does not drop triangles")
    var rebuilds := [0]
    renderer.connect("mesh_rebuilt", func(): rebuilds[0] += 1)
    renderer.set("seed", 9821)
    renderer.set("chunk_coordinates", Vector3i(-2, 0, 3))
    await process_frame
    await process_frame
    check(rebuilds[0] == 1, "seed and coordinates coalesce into one rebuild")
    check(renderer.get_voxel_data() != original, "Inspector changes regenerate terrain")
    var changed: PackedByteArray = renderer.get_voxel_data()
    renderer.regenerate()
    check(renderer.get_voxel_data() == changed, "generation repeatable")
    renderer.set("seed", 1337)
    renderer.set("chunk_coordinates", Vector3i.ZERO)
    await process_frame
    await process_frame
    check(renderer.get_voxel_data() == original, "returning to same seed/coordinates restores data")
    renderer.set("chunk_coordinates", Vector3i(0, 1, 0))
    await process_frame
    await process_frame
    check(renderer.mesh == null and renderer.get_voxel_count() == 0 and renderer.get_triangle_count() == 0, "sky clears previous geometry")
    check(collision_node.shape == null, "empty chunk clears stale collision")
    renderer.set("chunk_coordinates", Vector3i(0, -1, 0))
    await process_frame
    await process_frame
    check(renderer.mesh != null and renderer.get_voxel_count() > 0, "negative Y generates underground caves")
    renderer.set("seed", -419)
    renderer.set("chunk_coordinates", Vector3i(-2, 0, -3))
    await process_frame
    await process_frame
    var packed := PackedScene.new()
    check(packed.pack(renderer) == OK, "pack renderer settings")
    var file := "user://terrain_settings_test.tscn"
    check(ResourceSaver.save(packed, file) == OK, "save only generator parameters")
    var serialized := FileAccess.get_file_as_string(file)
    check(not serialized.contains("ArrayMesh") and not serialized.contains("PackedByteArray"), "no baked geometry or occupancy in scene")
    var saved := load(file) as PackedScene
    var restored := saved.instantiate() as MeshInstance3D
    root.add_child(restored)
    check(restored.get("seed") == -419 and restored.get("chunk_coordinates") == Vector3i(-2, 0, -3), "parameters survive scene reload")
    check(restored.get_voxel_data() == renderer.get_voxel_data(), "data regenerated from saved parameters")
    restored.free()
    DirAccess.remove_absolute(ProjectSettings.globalize_path(file))
    renderer.free()
    var demo: Node = load("res://voxel_chunk.tscn").instantiate()
    root.add_child(demo)
    check(demo.get_node("TerrainRenderer").mesh != null, "preview loads new renderer")
    demo.free()
    print("PASS: renderer upload, fixed dimensions, Inspector seed/coordinates, automatic rebuild, empty chunk, persistence" if failures == 0 else "FAILED: %d checks" % failures)
    quit(0 if failures == 0 else 1)
