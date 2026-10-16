extends SceneTree

var failures := 0

func check(condition: bool, message: String) -> void:
    if not condition:
        failures += 1
        printerr("FAIL: " + message)

func _initialize() -> void:
    call_deferred("run")

# Independent oracle: compare physical center displacements instead of using
# the implementation's parity-dependent neighbor table.
func verify(chunk: MeshInstance3D, label: String) -> void:
    chunk.rebuild_mesh()
    var size: Vector3i = chunk.get_chunk_size()
    var centers: Array[Vector3] = []
    var edge: float = chunk.get("edge_length")
    var unit := edge / sqrt(2.0)
    for y in size.y:
        for z in size.z:
            for x in size.x:
                var cell := Vector3i(x, y, z)
                if chunk.get_voxel(cell):
                    centers.append(chunk.get_voxel_center(cell))
    var square_pairs := 0
    var hex_pairs := 0
    for i in centers.size():
        for j in range(i + 1, centers.size()):
            var delta := (centers[j] - centers[i]).abs() / unit
            if delta.is_equal_approx(Vector3(4, 0, 0)) or delta.is_equal_approx(Vector3(0, 4, 0)) or delta.is_equal_approx(Vector3(0, 0, 4)):
                square_pairs += 1
            elif delta.is_equal_approx(Vector3(2, 2, 2)):
                hex_pairs += 1
    var n := centers.size()
    var expected_faces := n * 14 - 2 * (square_pairs + hex_pairs)
    var expected_triangles := n * 44 - 4 * square_pairs - 8 * hex_pairs
    check(chunk.get_voxel_count() == n, label + ": occupancy")
    check(chunk.get_visible_face_count() == expected_faces, label + ": only boundary faces")
    check(chunk.get_triangle_count() == expected_triangles, label + ": culled triangles")
    if n == 0:
        check(chunk.mesh == null, label + ": empty mesh removed")
        return
    check(chunk.mesh != null and chunk.mesh.get_surface_count() == 1, label + ": single surface")
    var arrays := chunk.mesh.surface_get_arrays(0)
    var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
    var indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
    var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
    check(indices.size() == expected_triangles * 3, label + ": mesh index count")
    var volume := 0.0
    var area := 0.0
    var edges := {}
    var unique := {}
    var triangles := {}
    for point in vertices:
        if not unique.has(point):
            unique[point] = unique.size()
    for i in range(0, indices.size(), 3):
        var a := vertices[indices[i]]
        var b := vertices[indices[i + 1]]
        var c := vertices[indices[i + 2]]
        var cross := (b - a).cross(c - a)
        check(cross.dot(normals[indices[i]]) < 0, label + ": outward winding")
        area += cross.length() * 0.5
        volume -= a.dot(b.cross(c)) / 6.0
        var ids: Array[int] = [unique[a], unique[b], unique[c]]
        var sorted_ids := ids.duplicate()
        sorted_ids.sort()
        var triangle_key := Vector3i(sorted_ids[0], sorted_ids[1], sorted_ids[2])
        check(not triangles.has(triangle_key), label + ": no duplicate triangles")
        triangles[triangle_key] = true
        for k in 3:
            var ia: int = ids[k]
            var ib: int = ids[(k + 1) % 3]
            var key := Vector2i(mini(ia, ib), maxi(ia, ib))
            if not edges.has(key):
                edges[key] = [0, 0]
            edges[key][0] += 1
            edges[key][1] += 1 if ia < ib else -1
    for incidence in edges.values():
        check(incidence == [2, 0], label + ": watertight surface")
    var expected_volume := n * 8.0 * sqrt(2.0) * pow(edge, 3)
    var expected_area := (n * (6.0 + 12.0 * sqrt(3.0)) - 2.0 * square_pairs - 3.0 * sqrt(3.0) * hex_pairs) * edge * edge
    check(absf(volume - expected_volume) < expected_volume * 0.0001, label + ": volume equals sum of voxels")
    check(absf(area - expected_area) < expected_area * 0.0001, label + ": boundary area")
    print("PASS geometry %s: voxels=%d faces=%d triangles=%d" % [label, n, expected_faces, expected_triangles])

func run() -> void:
    check(ClassDB.class_exists("OctahedralVoxelChunk"), "native class registered")
    if failures:
        quit(1)
        return
    var chunk := ClassDB.instantiate("OctahedralVoxelChunk") as MeshInstance3D
    root.add_child(chunk)
    var properties := {}
    for property in chunk.get_property_list():
        properties[property.name] = property
    for name in ["size_x", "size_y", "size_z", "edge_length", "color", "color_variation"]:
        check(properties.has(name) and properties[name].usage & PROPERTY_USAGE_EDITOR != 0, name + " is exposed in Inspector")
    check(properties.mesh.usage & PROPERTY_USAGE_STORAGE == 0, "generated mesh is not serialized")
    for size in [Vector3i(1, 1, 1), Vector3i(2, 1, 1), Vector3i(1, 2, 1), Vector3i(1, 3, 1), Vector3i(3, 4, 2), Vector3i(8, 4, 8)]:
        chunk.set_chunk_size(size)
        verify(chunk, str(size))
    chunk.set_chunk_size(Vector3i(4, 5, 4))
    chunk.set_voxel(Vector3i(1, 2, 1), false)
    verify(chunk, "interior cavity")
    chunk.set_voxel(Vector3i(1, 2, 1), true)
    verify(chunk, "restored voxel")
    chunk.set_voxel(Vector3i(0, 0, 0), false)
    verify(chunk, "corner removed")
    chunk.set("edge_length", 1.7)
    verify(chunk, "rescaled")
    var old_size: Vector3i = chunk.get_chunk_size()
    for bad_size in [Vector3i(0, 2, 2), Vector3i(2, -1, 2), Vector3i(65, 2, 2)]:
        chunk.set_chunk_size(bad_size)
        check(chunk.get_chunk_size() == old_size, "invalid dimensions ignored")
    for invalid in [0.0, -1.0, INF, NAN, 101.0]:
        chunk.set("edge_length", invalid)
        check(chunk.get("edge_length") == 1.7, "invalid edge ignored")
    var count: int = chunk.get_voxel_count()
    chunk.set_voxel(Vector3i(-1, 0, 0), true)
    check(chunk.get_voxel_count() == count, "out of bounds write ignored")
    check(not chunk.get_voxel(Vector3i(100, 0, 0)), "out of bounds neighbor is empty")
    var packed := PackedScene.new()
    check(packed.pack(chunk) == OK, "pack native chunk")
    var path := "user://octahedral_chunk_roundtrip_test.tscn"
    check(ResourceSaver.save(packed, path) == OK, "save chunk")
    var loaded := load(path) as PackedScene
    var restored := loaded.instantiate() as MeshInstance3D
    root.add_child(restored)
    check(restored.get_chunk_size() == old_size, "dimensions survive save/load")
    check(restored.get_voxel_data() == chunk.get_voxel_data(), "voxel edits survive save/load")
    check(is_equal_approx(restored.get("edge_length"), 1.7), "edge survives save/load")
    verify(restored, "serialized chunk")
    restored.free()
    DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
    chunk.fill(false)
    verify(chunk, "empty")
    chunk.set_voxel(Vector3i(0, 1, 0), true)
    verify(chunk, "single odd-layer voxel")
    var updated := [0]
    chunk.connect("mesh_rebuilt", func(): updated[0] += 1)
    chunk.set("size_x", 5)
    chunk.set("size_y", 3)
    chunk.set("size_z", 6)
    await process_frame
    await process_frame
    check(updated[0] == 1, "Inspector setters coalesce into one automatic rebuild")
    check(chunk.get_chunk_size() == Vector3i(5, 3, 6), "all dimension setters applied")
    check(chunk.get_voxel_count() == 90, "resize regenerates filled chunk")
    chunk.set("color", Color.RED)
    chunk.set("color_variation", 0.0)
    await process_frame
    await process_frame
    var colors: PackedColorArray = chunk.mesh.surface_get_arrays(0)[Mesh.ARRAY_COLOR]
    check(colors[0].is_equal_approx(Color.RED), "vertex color property updates")
    var started := Time.get_ticks_msec()
    chunk.set_chunk_size(Vector3i(64, 64, 64))
    chunk.rebuild_mesh()
    check(chunk.get_voxel_count() == 262144, "maximum chunk capacity")
    check(chunk.get_visible_face_count() == 97666, "maximum chunk boundary face count")
    check(chunk.get_triangle_count() == 325128, "maximum chunk boundary triangle count")
    print("PASS maximum 64^3 chunk: %d ms" % (Time.get_ticks_msec() - started))
    chunk.free()
    var scene := load("res://voxel_chunk.tscn") as PackedScene
    check(scene != null, "demo scene loads")
    if scene:
        var demo := scene.instantiate()
        root.add_child(demo)
        check(demo.get_node("Chunk").mesh != null, "demo native chunk generates")
        demo.free()
    print("PASS: chunk geometry, culling, Inspector properties, deferred rebuild, occupancy and persistence" if failures == 0 else "FAILED: %d checks" % failures)
    quit(0 if failures == 0 else 1)
