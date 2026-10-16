@tool
extends Node3D

func _ready() -> void:
    if not Engine.is_editor_hint() or not OS.get_cmdline_user_args().has("--verify-chunk-editor"):
        return
    call_deferred("verify_editor")

func verify_editor() -> void:
    await get_tree().process_frame
    var chunk := $Chunk
    var ok: bool = chunk.mesh != null and chunk.get_voxel_count() == 256
    chunk.set("size_x", 3)
    chunk.set("size_y", 2)
    chunk.set("size_z", 4)
    chunk.set("edge_length", 0.75)
    await get_tree().process_frame
    await get_tree().process_frame
    ok = ok and chunk.get_voxel_count() == 24 and chunk.get_triangle_count() > 0
    ok = ok and chunk.get_chunk_size() == Vector3i(3, 2, 4)
    ok = ok and is_equal_approx(chunk.get("edge_length"), 0.75)
    print("PASS: native chunk generates and Inspector properties rebuild in editor mode" if ok else "FAIL: editor chunk rebuild")
    get_tree().quit(0 if ok else 1)
