extends SceneTree

var failures := 0

func check(condition: bool, message: String) -> void:
    if not condition:
        failures += 1
        printerr("FAIL: " + message)

func _initialize() -> void:
    call_deferred("run")

func verify_mesh(solid: MeshInstance3D, edge: float) -> void:
    var mesh := solid.mesh
    check(mesh != null and mesh.get_surface_count() == 1, "one surface")
    var arrays := mesh.surface_get_arrays(0)
    var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
    var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
    var indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
    check(vertices.size() == 72, "72 face-local vertices")
    check(indices.size() == 132, "44 triangles")
    var unique := {}
    var faces := {}
    for i in vertices.size():
        if not unique.has(vertices[i]):
            unique[vertices[i]] = unique.size()
        if not faces.has(normals[i]):
            faces[normals[i]] = []
        faces[normals[i]].append(vertices[i])
        check(is_equal_approx(normals[i].length(), 1.0), "unit normal")
        check(normals[i].dot(vertices[i]) > 0, "outward normal")
    check(unique.size() == 24, "24 geometric vertices")
    check(faces.size() == 14, "14 planar faces")
    var edges := {}
    var squares := 0
    var hexagons := 0
    for normal in faces:
        var polygon: Array = faces[normal]
        squares += int(polygon.size() == 4)
        hexagons += int(polygon.size() == 6)
        for i in polygon.size():
            var a: Vector3 = polygon[i]
            var b: Vector3 = polygon[(i + 1) % polygon.size()]
            check(absf(a.distance_to(b) - edge) < edge * 0.00001, "equal edge lengths")
            check(absf((polygon[1] - polygon[0]).cross(polygon[2] - polygon[0]).normalized().dot(a - polygon[0])) < edge * 0.00001, "coplanar face")
            var ia: int = unique[a]
            var ib: int = unique[b]
            var key := Vector2i(mini(ia, ib), maxi(ia, ib))
            if not edges.has(key):
                edges[key] = [0, 0]
            edges[key][0] += 1
            edges[key][1] += 1 if ia < ib else -1
    check(squares == 6 and hexagons == 8, "6 squares and 8 hexagons")
    check(edges.size() == 36, "36 polygon edges")
    for incidence in edges.values():
        check(incidence == [2, 0], "closed manifold with opposite shared-edge directions")
    check(unique.size() - edges.size() + faces.size() == 2, "Euler characteristic")
    var area := 0.0
    var volume := 0.0
    for i in range(0, indices.size(), 3):
        var a := vertices[indices[i]]
        var b := vertices[indices[i + 1]]
        var c := vertices[indices[i + 2]]
        var cross := (b - a).cross(c - a)
        check(cross.dot(normals[indices[i]]) < 0, "clockwise triangle winding")
        area += cross.length() * 0.5
        volume -= a.dot(b.cross(c)) / 6.0
    var expected_area := (6.0 + 12.0 * sqrt(3.0)) * edge * edge
    var expected_volume := 8.0 * sqrt(2.0) * pow(edge, 3)
    check(absf(area - expected_area) < expected_area * 0.00001, "surface area")
    check(absf(volume - expected_volume) < expected_volume * 0.00001, "enclosed volume")
    print("edge=%.2f V=%d E=%d F=%d triangles=%d area=%.6f volume=%.6f" % [edge, unique.size(), edges.size(), faces.size(), indices.size() / 3, area, volume])

func run() -> void:
    check(ClassDB.class_exists("TruncatedOctahedron"), "native GDExtension registered")
    if failures:
        quit(1)
        return
    var solid := ClassDB.instantiate("TruncatedOctahedron") as MeshInstance3D
    root.add_child(solid)
    for length in [1.4, 0.01, 2.0, 100.0]:
        solid.set("edge_length", length)
        verify_mesh(solid, length)
    for invalid in [-1.0, 0.0, INF, NAN, 101.0]:
        solid.set("edge_length", invalid)
        check(solid.get("edge_length") == 100.0, "invalid size retains valid state")
    solid.set("color", Color.RED)
    var material := solid.mesh.surface_get_material(0) as StandardMaterial3D
    check(material.albedo_color == Color.RED, "color setter updates material")
    solid.free()
    var scene := load("res://truncated_octahedron.tscn") as PackedScene
    check(scene != null, "preview scene loads")
    if scene:
        var instance := scene.instantiate()
        root.add_child(instance)
        check(instance.get_node("Solid").mesh != null, "scene generates native mesh on ready")
        instance.free()
    print("PASS: geometry, native properties and scene" if failures == 0 else "FAILED: %d checks" % failures)
    quit(0 if failures == 0 else 1)
