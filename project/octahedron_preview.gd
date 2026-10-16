extends Node3D

var azimuth := 0.25
var elevation := 0.38
var distance := 10.5
@onready var camera: Camera3D = $Camera3D

func _ready() -> void:
	update_camera()
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--capture="):
			capture(argument.trim_prefix("--capture="))

func update_camera() -> void:
	var target := Vector3(0, 2.0, 0)
	camera.position = target + distance * Vector3(sin(azimuth) * cos(elevation), sin(elevation), cos(azimuth) * cos(elevation))
	camera.look_at(target)

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion and Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT):
		azimuth -= event.relative.x * 0.008
		elevation = clampf(elevation + event.relative.y * 0.008, -0.05, 1.4)
	elif event is InputEventMouseButton and event.pressed:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			distance = maxf(5.0, distance * 0.9)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			distance = minf(30.0, distance * 1.1)
	elif event is InputEventKey and event.pressed and event.keycode == KEY_R:
		azimuth = 0.25
		elevation = 0.38
		distance = 10.5
	update_camera()

func capture(path: String) -> void:
	for frame in range(12):
		await RenderingServer.frame_post_draw
	var error := get_viewport().get_texture().get_image().save_png(path)
	if error != OK:
		push_error("Capture failed: %s" % error)
	get_tree().quit(0 if error == OK else 1)
