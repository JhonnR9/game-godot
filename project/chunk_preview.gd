@tool
extends Node3D

var azimuth := 0.65
var elevation := 0.45
var distance := 30.0
var radius := 1.0
var target := Vector3.ZERO
@onready var renderer: MeshInstance3D = $TerrainRenderer
@onready var camera: Camera3D = $Camera3D

func _ready() -> void:
    if not renderer.is_connected("mesh_rebuilt", refresh_chunk):
        renderer.connect("mesh_rebuilt", refresh_chunk)
    refresh_chunk()
    if not Engine.is_editor_hint():
        for argument in OS.get_cmdline_user_args():
            if argument.begins_with("--capture="):
                capture(argument.trim_prefix("--capture="))

func refresh_chunk() -> void:
    var coordinates: Vector3i = renderer.get("chunk_coordinates")
    var count: int = renderer.get_voxel_count()
    $CanvasLayer/Help.text = "TERRENO PROCEDURAL · CHUNK 16 × 16 × 16\nSeed %d · coordenadas %s · %d voxels · %d triângulos" % [renderer.get("seed"), str(coordinates), count, renderer.get_triangle_count()]
    if count == 0:
        $CanvasLayer/Help.text += "\nRegião vazia — selecione Y = 0 para visualizar a superfície."
    # Reset the frame even for an empty chunk; never keep stale previous bounds.
    target = renderer.position + Vector3(0, 5.0, 0)
    radius = 17.5
    if renderer.mesh:
        var bounds := renderer.get_aabb()
        target = renderer.position + bounds.get_center()
        radius = maxf(bounds.size.length() * 0.5, 0.01)
    distance = radius / sin(deg_to_rad(camera.fov * 0.5)) * 1.08
    update_camera()

func update_camera() -> void:
    camera.position = target + distance * Vector3(sin(azimuth) * cos(elevation), sin(elevation), cos(azimuth) * cos(elevation))
    camera.near = maxf(radius * 0.001, 0.001)
    camera.far = maxf(100.0, distance + radius * 5.0)
    camera.look_at(target)

func _unhandled_input(event: InputEvent) -> void:
    if Engine.is_editor_hint():
        return
    if event is InputEventMouseMotion and Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT):
        azimuth -= event.relative.x * 0.008
        elevation = clampf(elevation + event.relative.y * 0.008, 0.05, 1.4)
    elif event is InputEventMouseButton and event.pressed:
        if event.button_index == MOUSE_BUTTON_WHEEL_UP:
            distance = maxf(radius * 1.2, distance * 0.9)
        elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
            distance = minf(radius * 20.0, distance * 1.1)
    elif event is InputEventKey and event.pressed and event.keycode == KEY_R:
        azimuth = 0.65
        elevation = 0.45
        refresh_chunk()
    update_camera()

func capture(path: String) -> void:
    for frame in range(12):
        await RenderingServer.frame_post_draw
    var error := get_viewport().get_texture().get_image().save_png(path)
    if error != OK:
        push_error("Capture failed: %s" % error)
    get_tree().quit(0 if error == OK else 1)
