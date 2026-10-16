@tool
extends Node3D

func _ready() -> void:
    if not Engine.is_editor_hint() or not OS.get_cmdline_user_args().has("--verify-terrain-editor"):
        return
    call_deferred("verify_editor")

func verify_editor() -> void:
    await get_tree().process_frame
    var renderer := $TerrainRenderer
    var initial: PackedByteArray = renderer.get_voxel_data()
    var ok: bool = renderer.mesh != null and initial.size() == 4096
    var ao_colors: PackedColorArray = renderer.mesh.surface_get_arrays(0)[Mesh.ARRAY_COLOR]
    renderer.set("ambient_occlusion_strength", 0.0)
    await get_tree().process_frame
    await get_tree().process_frame
    var unoccluded_colors: PackedColorArray = renderer.mesh.surface_get_arrays(0)[Mesh.ARRAY_COLOR]
    ok = ok and ao_colors != unoccluded_colors and renderer.get_voxel_data() == initial
    renderer.set("ambient_occlusion_strength", 0.8)
    await get_tree().process_frame
    await get_tree().process_frame
    ok = ok and renderer.mesh.surface_get_arrays(0)[Mesh.ARRAY_COLOR] == ao_colors
    var joined_count: int = renderer.get_triangle_count()
    renderer.set("connect_aligned_surfaces", false)
    await get_tree().process_frame
    await get_tree().process_frame
    ok = ok and renderer.get_voxel_data() == initial
    ok = ok and renderer.get_triangle_count() == 21492
    renderer.set("connect_aligned_surfaces", true)
    await get_tree().process_frame
    await get_tree().process_frame
    ok = ok and renderer.get_triangle_count() == joined_count
    renderer.set("seed", -1234)
    renderer.set("chunk_coordinates", Vector3i(-2, 0, 1))
    await get_tree().process_frame
    await get_tree().process_frame
    ok = ok and renderer.get_voxel_data() != initial and renderer.mesh != null
    renderer.set("seed", 1337)
    renderer.set("chunk_coordinates", Vector3i.ZERO)
    await get_tree().process_frame
    await get_tree().process_frame
    ok = ok and renderer.get_voxel_data() == initial
    print("PASS: seed, coordinates, final geometry and ambient occlusion update in editor mode" if ok else "FAIL: editor generation")
    # Let the editor finish startup/documentation callbacks before teardown.
    await get_tree().create_timer(1.0).timeout
    get_tree().quit(0 if ok else 1)
