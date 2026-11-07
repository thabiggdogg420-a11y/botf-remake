extends SceneTree
## Headless checks of CameraRig, ported from the camera rig block of remaster/game/Tools/client-logic-tests.cpp:
## an orthonormal view looking at the focus from above the plane; projection and screen rays agree; a framed sphere
## fills the frame; picking finds the nearest hull, or a speck near the cursor; easing converges along the short way
## round and arcs out on long moves. Plus the BattleCamera controls on a live scene (modes, pick, follow, fly-to).
##   godot --headless --path remaster/godot -s res://scenes/battle/tests/test_camera_rig.gd

const FIXTURE := "res://scenes/battle/fixtures/rules3_battle.json"

var checks := 0
var failures := 0


func check(cond: bool, what: String) -> void:
	checks += 1
	if not cond:
		failures += 1
		printerr("  FAIL  ", what)


static func approx(a: float, b: float, eps := 1e-6) -> bool:
	return absf(a - b) < eps


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	print("Camera rig tests")
	_rig_maths()
	await _camera_controls()
	await _host_input()
	print("camera rig: %d checks, %d failures" % [checks, failures])
	quit(1 if failures > 0 else 0)


func _rig_maths() -> void:
	var fov := 48.0
	var aspect := 16.0 / 9.0
	var rig := CameraRig.Rig.new(Vector3(100, 20, -50), 1500.0, -90.0, 58.0)
	var v := CameraRig.view_of(rig, fov, aspect)
	check(approx(v.forward.length(), 1) and approx(v.right.length(), 1) and approx(v.up.length(), 1), "unit axes")
	check(approx(v.forward.dot(v.right), 0) and approx(v.forward.dot(v.up), 0) and approx(v.right.dot(v.up), 0), "orthogonal axes")
	check(v.eye.y > rig.focus.y and v.eye.z < rig.focus.z and approx((v.eye - rig.focus).length(), 1500, 1e-3), "eye above, on the azimuth side, at the distance")
	check(v.up.y > 0.0, "screen up leans away from the plane")
	var b := v.basis()
	check(approx(b.determinant(), 1.0, 1e-5), "camera basis is a rotation")
	check((b * Vector3(0, 0, -1)).distance_to(v.forward) < 1e-5, "camera looks down -Z")
	var centre := CameraRig.project(v, rig.focus)
	check(centre.visible and approx(centre.u, 0.5) and approx(centre.v, 0.5), "focus projects to the centre")
	check(CameraRig.project(v, rig.focus + v.right * 50.0).u > 0.5 and CameraRig.project(v, rig.focus + v.up * 50.0).v < 0.5, "right is right, up is up")
	check(not CameraRig.project(v, v.eye - v.forward * 10.0).visible, "behind the camera is not visible")
	var somewhere := Vector3(180, -30, 40)
	var sp := CameraRig.project(v, somewhere)
	var along := CameraRig.ray_through(v, sp.u, sp.v)
	check(along.cross((somewhere - v.eye).normalized()).length() < 1e-5, "ray through a projected point hits it")
	var ground: Variant = CameraRig.ray_plane(v, 0.5, 0.5, rig.focus.y)
	check(ground is Vector3 and approx(ground.x, rig.focus.x, 1e-3) and approx(ground.z, rig.focus.z, 1e-3), "centre ray meets the plane at the focus")
	# The same projection as Godot's own camera.
	var cam := Camera3D.new()
	cam.fov = fov
	cam.transform = Transform3D(v.basis(), v.eye)
	check(approx(CameraRig.rig_from_eye(v.eye, rig.focus).azimuth, -90.0, 1e-3) and approx(CameraRig.rig_from_eye(v.eye, rig.focus).elevation, 58.0, 1e-3), "rig_from_eye inverts view_of")
	cam.free()
	# Framing: at frame_distance a sphere's projected radius is half the image height over the margin.
	var d := CameraRig.frame_distance(40.0, fov, aspect, 1.0)
	var framed := CameraRig.Rig.new(Vector3.ZERO, d)
	var fp := CameraRig.project(CameraRig.view_of(framed, fov, aspect), framed.focus, 40.0)
	check(absf(fp.radius - 0.5) < 0.05, "framed sphere fills the frame height (%.3f)" % fp.radius)
	var tall := CameraRig.project(CameraRig.view_of(CameraRig.Rig.new(Vector3.ZERO, CameraRig.frame_distance(40.0, fov, 0.5, 1.0)), fov, 0.5), Vector3.ZERO, 40.0)
	check(absf(tall.radius - 0.25) < 0.03, "portrait frame fits the width (%.3f)" % tall.radius) # width = 0.5 * height
	check(CameraRig.frame_distance(400.0, fov, aspect) > 9.0 * CameraRig.frame_distance(40.0, fov, aspect), "bigger sphere, farther")
	# Picking: the nearer of two hulls on the ray; a speck by screen distance; nothing in empty space.
	var toward := (rig.focus - v.eye).normalized()
	var line := [{"id": 7, "center": v.eye + toward * 1200.0, "radius": 30.0}, {"id": 9, "center": v.eye + toward * 600.0, "radius": 30.0}]
	check(CameraRig.pick(v, 0.5, 0.5, line) == 9, "pick the nearer hull")
	var speck := [{"id": 4, "center": rig.focus + v.right * 60.0, "radius": 1.0}]
	var sk := CameraRig.project(v, speck[0].center, 1.0)
	check(CameraRig.pick(v, sk.u + 0.004, sk.v, speck) == 4 and CameraRig.pick(v, sk.u + 0.2, sk.v, speck) == 0, "speck within slack, not beyond")
	var all := CameraRig.bounds_of([{"id": 1, "center": Vector3(0, 0, 0), "radius": 10.0}, {"id": 2, "center": Vector3(100, 0, 0), "radius": 20.0}])
	check(approx(all.center.x, 55) and approx(all.radius, 65), "bounds of two bodies")
	# Easing.
	check(approx(CameraRig.angle_delta(170, -170), 20) and approx(CameraRig.angle_delta(-170, 170), -20), "angle delta folds")
	check(approx(CameraRig.approach_angle(170, -170, 1.0, 1.0), 180) and CameraRig.approach(0, 10, 0.0, 1.0) == 0.0, "short way round")
	var smooth := CameraRig.Smoother.new()
	var a := CameraRig.Rig.new(Vector3.ZERO, 100.0)
	smooth.snap(a)
	var bb := a.copy()
	bb.focus = Vector3(1000, 0, 0)
	bb.distance = 400.0
	var first := smooth.step(bb, 0.1, 0.3)
	check(first.distance > 400.0 * 0.5 + 0.35 * 500.0, "lifted while the focus travels (%.1f)" % first.distance)
	for i in 200:
		smooth.step(bb, 0.05, 0.3)
	var settled := smooth.step(bb, 0.05, 0.3)
	check(approx(settled.focus.x, 1000, 1e-3) and approx(settled.distance, 400, 1e-3), "converges")
	check(CameraRig.zoom_factor_after(1.0, 2) < CameraRig.zoom_factor_after(1.0, 1) and CameraRig.zoom_factor_after(1.0, -1) > 1.0, "wheel zoom")


## BattleCamera on the fixture battle: every mode films, picks and follows.
func _camera_controls() -> void:
	var encounter: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(FIXTURE))
	var scene := BattleScene.new()
	scene.play_audio = false
	root.add_child(scene)
	await process_frame
	scene.play_round(encounter, 0)
	scene.seek(1.0)
	scene.playback_speed = 0.0
	await process_frame
	var cam := scene.camera
	var picked: Array = []
	scene.ship_picked.connect(func(id): picked.append(id))
	for m in [BattleCamera.Mode.OVERVIEW, BattleCamera.Mode.CHASE, BattleCamera.Mode.SHIP, BattleCamera.Mode.TARGET,
			BattleCamera.Mode.TOP_DOWN, BattleCamera.Mode.CINEMATIC]:
		cam.set_mode(m)
		for i in 5:
			await process_frame
		check(cam.mode == m and cam.view != null, "mode %s films" % BattleCamera.MODE_NAMES[m])
		check(cam.global_transform.basis.is_finite() and cam.global_position.is_finite(), "mode %s transform finite" % BattleCamera.MODE_NAMES[m])
	# A moment with both fleets in view.
	cam.set_mode(BattleCamera.Mode.OVERVIEW)
	var both := false
	for r in encounter.rounds.size():
		scene.play_round(encounter, r)
		scene.playback_speed = 0.0
		scene.seek(3.0)
		cam.smoother.primed = false
		await process_frame
		if cam.bodies.any(func(b): return b.mine) and cam.bodies.any(func(b): return not b.mine):
			both = true
			break
	check(both, "both fleets in view in some round")
	check(not cam.bodies.is_empty(), "camera sees the ships")
	# Click a ship: it is picked. (The one nearest the camera: nothing stands in front of it.)
	var body: Dictionary = cam.bodies[0]
	for b in cam.bodies:
		if b.center.distance_to(cam.view.eye) < body.center.distance_to(cam.view.eye):
			body = b
	var p := CameraRig.project(cam.view, body.center, body.radius)
	check(cam.pick_at(Vector2(p.u, p.v)) == int(body.id), "pick the ship under its projection")
	check(cam.pick_at(Vector2(0.02, 0.02)) == 0, "nothing in the corner of the sky")
	var size := cam.get_viewport().get_visible_rect().size
	var at := Vector2(p.u, p.v) * size
	_click(cam, at, false)
	await process_frame
	check(picked.has(int(body.id)), "click emits ship_picked")
	# Double-click it: the camera follows it.
	_click(cam, at, true)
	check(cam.mode in [BattleCamera.Mode.SHIP, BattleCamera.Mode.TARGET], "double-click follows the ship")
	check((int(body.id) == cam.selected_id) or (int(body.id) == cam.target_id), "followed ship is the selection")
	# Home frames the battle; F follows the selection; G the target.
	var mine: Array = cam.bodies.filter(func(b): return b.mine)
	var foes: Array = cam.bodies.filter(func(b): return not b.mine)
	check(not mine.is_empty() and not foes.is_empty(), "both sides on screen")
	cam.set_selection(int(mine[0].id), int(foes[0].id))
	_key(cam, KEY_HOME)
	check(cam.mode == BattleCamera.Mode.OVERVIEW and not cam.free_cam, "Home frames the battle")
	_key(cam, KEY_G)
	check(cam.mode == BattleCamera.Mode.TARGET and cam.target_id == int(foes[0].id), "G follows the target")
	_key(cam, KEY_F)
	check(cam.mode == BattleCamera.Mode.SHIP and cam.selected_id == int(mine[0].id), "F follows the selected ship")
	await create_timer(2.0).timeout
	var followed := CameraRig.project(cam.view, cam.body(int(mine[0].id)).center)
	check(followed.visible and absf(followed.u - 0.5) < 0.2 and absf(followed.v - 0.5) < 0.2, "followed ship near the centre")
	_key(cam, KEY_C)
	check(cam.mode == BattleCamera.Mode.CINEMATIC, "C to cinematic")
	_key(cam, KEY_C)
	check(cam.mode == BattleCamera.Mode.SHIP, "C back to the previous mode")
	# Wheel toward a point in overview frees the camera and keeps that point under the cursor.
	cam.set_mode(BattleCamera.Mode.OVERVIEW)
	await create_timer(2.0).timeout
	var d0: float = cam.smoother.current.distance
	var corner := Vector2(0.3, 0.6)
	cam.view = CameraRig.view_of(cam.smoother.current, cam.fov, cam.view.aspect) # without the travel lift
	var before: Variant = CameraRig.ray_plane(cam.view, corner.x, corner.y, cam.smoother.current.focus.y)
	cam.zoom_at(2.0, corner)
	check(cam.free_cam and cam.free_distance < d0, "wheel zooms in and frees the camera")
	cam.smoother.snap(cam.target_rig())
	cam.view = CameraRig.view_of(cam.smoother.current, cam.fov, cam.view.aspect)
	var after: Variant = CameraRig.ray_plane(cam.view, corner.x, corner.y, cam.smoother.current.focus.y)
	check(before is Vector3 and after is Vector3 and (before as Vector3).distance_to(after) < 0.05, "point under the cursor stays put")
	# Double-click empty sky: fly there.
	var f0: Vector3 = cam.free_focus
	cam.fly_to(Vector2(0.5, 0.75))
	check(cam.free_cam and cam.free_focus.distance_to(f0) > 0.1, "fly-to moves the focus")
	# Right-drag pans.
	var f1: Vector3 = cam.free_focus
	cam.pan(Vector2(0.1, 0.0))
	check(cam.free_focus.distance_to(f1) > 0.1, "pan moves the focus")
	# Legacy free orbit.
	cam.set_mode(BattleCamera.Mode.ORBIT)
	check(cam.mode == BattleCamera.Mode.OVERVIEW and cam.free_cam, "ORBIT = free overview")
	scene.queue_free()
	await process_frame


## Through BattleHost's SubViewportContainer, as the combat screen mounts the view: the mode bar, keys and clicks
## reach the camera, and a clicked ship comes out as BattleHost.ship_picked.
func _host_input() -> void:
	var encounter: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(FIXTURE))
	root.size = Vector2i(1280, 720)
	var host := BattleHost.new()
	host.set_anchors_preset(Control.PRESET_FULL_RECT)
	root.add_child(host)
	await process_frame
	await process_frame
	check(host.view_node != null and "camera" in host.view_node, "host mounts the 3D view")
	check(ResourceLoader.exists(BattleHost.BATTLE_SCENE) and not ("placeholder" in host), "no 2D placeholder fallback in the host")
	var cam: BattleCamera = host.view_node.camera
	host.view_node.play_audio = false
	host.load_encounter(encounter)
	host.view_node.playback_speed = 0.0
	var bar := host.get_node_or_null("CameraBar")
	check(bar != null and bar.get_child_count() >= 7, "mode bar")
	if bar != null:
		(bar.get_node("CamTOPDOWN") as Button).pressed.emit()
		check(cam.mode == BattleCamera.Mode.TOP_DOWN, "TOP-DOWN button")
		(bar.get_node("CamReset") as Button).pressed.emit()
		check(cam.mode == BattleCamera.Mode.OVERVIEW, "RESET button frames the battle")
	await create_timer(1.5).timeout
	var key := InputEventKey.new()
	key.keycode = KEY_C
	key.physical_keycode = KEY_C
	key.pressed = true
	root.push_input(key)
	await process_frame
	check(cam.mode == BattleCamera.Mode.CINEMATIC, "C key reaches the camera through the SubViewport")
	if bar != null:
		check((bar.get_node("CamCINEMATIC") as Button).button_pressed, "mode bar follows the camera")
	# C is a toggle: pressed again it leaves cinematic for the mode it came from, and the CINEMATIC button returns to it.
	var key_up := InputEventKey.new()
	key_up.keycode = KEY_C
	key_up.physical_keycode = KEY_C
	key_up.pressed = false
	root.push_input(key_up)
	root.push_input(key)
	await process_frame
	check(cam.mode == BattleCamera.Mode.OVERVIEW, "second C leaves cinematic (toggle, not a one-way switch)")
	if bar != null:
		check((bar.get_node("CamOVERVIEW") as Button).button_pressed and not (bar.get_node("CamCINEMATIC") as Button).button_pressed,
			"mode bar follows the C toggle back")
		(bar.get_node("CamCINEMATIC") as Button).pressed.emit()
		check(cam.mode == BattleCamera.Mode.CINEMATIC, "CINEMATIC button selects cinematic")
	cam.set_mode(BattleCamera.Mode.OVERVIEW)
	await create_timer(1.5).timeout
	# Click the ship nearest the camera through the container.
	var picked := []
	host.ship_picked.connect(func(id): picked.append(id))
	check(not cam.bodies.is_empty(), "host view sees ships")
	if not cam.bodies.is_empty():
		var body: Dictionary = cam.bodies[0]
		for b in cam.bodies:
			if b.center.distance_to(cam.view.eye) < body.center.distance_to(cam.view.eye):
				body = b
		var p := CameraRig.project(cam.view, body.center, body.radius)
		var svc := host.get_child(0) as Control
		var at := root.get_final_transform() * (svc.get_global_rect().position + Vector2(p.u, p.v) * svc.size) # window pixels
		for pressed in [true, false]:
			var mb := InputEventMouseButton.new()
			mb.button_index = MOUSE_BUTTON_LEFT
			mb.pressed = pressed
			mb.position = at
			mb.global_position = at
			root.push_input(mb)
			await process_frame
		check(picked.has(int(body.id)), "click through the container picks the ship")
		var wheel := InputEventMouseButton.new()
		wheel.button_index = MOUSE_BUTTON_WHEEL_UP
		wheel.pressed = true
		wheel.factor = 1.0
		wheel.position = svc.get_global_rect().get_center()
		wheel.global_position = wheel.position
		root.push_input(wheel)
		await process_frame
		check(cam.free_cam, "wheel through the container zooms the camera")
	host.queue_free()
	await process_frame


func _click(cam: BattleCamera, at: Vector2, double: bool) -> void:
	for pressed in [true, false]:
		var e := InputEventMouseButton.new()
		e.button_index = MOUSE_BUTTON_LEFT
		e.pressed = pressed
		e.double_click = double and pressed
		e.position = at
		cam.handle_input(e)


func _key(cam: BattleCamera, key: Key) -> void:
	var e := InputEventKey.new()
	e.keycode = key
	e.pressed = true
	cam.handle_input(e)
