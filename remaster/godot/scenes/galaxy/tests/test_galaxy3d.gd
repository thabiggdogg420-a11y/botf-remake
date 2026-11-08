extends SceneTree
## 3D galaxy map: deterministic disc layout and star types, the three zoom levels (galaxy, sector, system close-up
## with orbiting planets), smooth fly-to, picking (stars, fleets, open space, off the chart), the mouse gestures
## (right-click menu vs right-drag orbit, wheel zoom to cursor, double-click fly-to) and the layer toggles.
##   godot --headless --audio-driver Dummy --path remaster/godot -s res://scenes/galaxy/tests/test_galaxy3d.gd
## Exit code 0 on success, 1 on any failure.

var failures := 0
var app: App

func check(cond: bool, what: String) -> void:
	if cond:
		print("  ok  ", what)
	else:
		failures += 1
		printerr("  FAIL ", what)

func frames(n := 3) -> void:
	for k in n:
		await process_frame

func _initialize() -> void:
	_run.call_deferred()

func mouse(map: GalaxyMap, button: MouseButton, pressed: bool, pos: Vector2, double := false) -> void:
	var e := InputEventMouseButton.new()
	e.button_index = button
	e.pressed = pressed
	e.position = pos
	e.double_click = double
	map._gui_input(e)

func motion(map: GalaxyMap, pos: Vector2, rel: Vector2) -> void:
	var e := InputEventMouseMotion.new()
	e.position = pos
	e.relative = rel
	map._gui_input(e)

func _run() -> void:
	App.forced_backend = "fixture"
	PlanetTextures.disabled = true
	app = load("res://scenes/ui/app.tscn").instantiate()
	app.size = Vector2(1920, 1080)
	root.add_child(app)
	await frames()
	app.start_campaign({"faction": 1, "seed": 424242, "era": 3, "ruleset": "reference", "width": 30, "height": 20, "systems": 80, "preset": "remastered", "events": true, "aiAggression": true})
	app.close_all_modals()
	app.goto("galaxy")
	await frames(6)
	var gs := app.current_screen()
	var map: GalaxyMap = gs.map
	var g := map.grid_size()
	app.view["fog"] = "2".repeat(g.x * g.y)
	map.rebuild()
	await frames(3)

	# ---- Layout: deterministic, every star inside its own sector, a thin disc, all star types used
	var seed_ := int(app.view["meta"]["seed"])
	check(GalaxyLayout.system_position(3, 4, g, seed_) == GalaxyLayout.system_position(3, 4, g, seed_), "layout is deterministic")
	check(GalaxyLayout.system_position(3, 4, g, seed_) != GalaxyLayout.system_position(3, 4, g, seed_ + 1), "layout follows the galaxy seed")
	var lo := INF
	var hi := -INF
	var inside := true
	var types := {}
	for y in 20:
		for x in 30:
			var p := GalaxyLayout.system_position(x, y, Vector2i(30, 20), seed_)
			inside = inside and int(floor(p.x)) == x and int(floor(p.z)) == y and absf(p.y) <= GalaxyLayout.DISC_HALF
			lo = minf(lo, p.y)
			hi = maxf(hi, p.y)
			types[GalaxyLayout.star_type(x, y, seed_)] = true
	check(inside, "every star stays inside its sector and the disc")
	check(lo < -0.3 and hi > 0.3, "stars sit above and below the plane (%.2f .. %.2f)" % [lo, hi])
	check(types.size() == GalaxyLayout.STAR_TYPES.size(), "all %d star types appear on a Huge grid" % types.size())
	var systems: Array = app.view["systems"]
	var all_placed := systems.size() > 0
	for s in systems:
		var e: Dictionary = map.systems3d.get(int(s["id"]), {})
		all_placed = all_placed and not e.is_empty() and e["pos"] == GalaxyLayout.system_position(int(s["x"]), int(s["y"]), g, seed_)
	check(all_placed, "every system has a 3D star at its layout position (%d)" % systems.size())

	# ---- Zoom levels and picking at each
	var s0: Dictionary = systems[0]
	for s in systems:
		if not s.get("planets", []).is_empty():
			s0 = s
			break
	var sid := int(s0["id"])
	var cell0 := int(s0["y"]) * g.x + int(s0["x"])
	var pos0: Vector3 = map.systems3d[sid]["pos"]
	var last_d := INF
	for pair in [[0.4, "galaxy"], [GalaxyMap.FLY_SECTOR, "sector"], [GalaxyMap.FLY_SYSTEM, "system"]]:
		map.focus_cell(int(s0["x"]), int(s0["y"]), pair[0], false)
		await frames(2)
		check(map.zoom_level() == pair[1], "zoom %.1f is the %s level" % [pair[0], pair[1]])
		var d := map.camera_position().distance_to(pos0)
		check(d < last_d, "%s view is closer than the previous level (%.2f)" % [pair[1], d])
		last_d = d
		var sp := map.world_to_screen(pos0)
		check(sp.distance_to(map.size / 2.0) < 40.0, "%s view is centred on the star (%s)" % [pair[1], sp])
		var pk := map.pick(sp)
		check(pk["kind"] == "system" and int(pk["cell"]) == cell0 and int(pk["id"]) == sid, "%s level: clicking the star picks it (%s)" % [pair[1], pk])
		check(map.cell_at(sp + Vector2(6, 6)) == cell0, "%s level: a click a few pixels off still picks the star" % pair[1])
	# System close-up: photosphere, planets in orbit, labels.
	var e0: Dictionary = map.systems3d[sid]
	check((e0["sphere"] as Node3D).visible, "the star's sphere shows at system zoom")
	var pr: Node3D = e0["planets"]
	check(pr != null and pr.visible and pr.get_child_count() == s0["planets"].size(), "%d planets orbit at system zoom" % s0["planets"].size())
	if pr:
		var piv: Node3D = pr.get_child(0)
		var a0 := piv.rotation.y
		await frames(10)
		check(not is_equal_approx(a0, piv.rotation.y), "planets move along their orbits")
		var body := piv.get_child(0) as Node3D
		var bp := map.world_to_screen(body.global_position)
		check(bp.x > 0 and bp.y > 0 and bp.x < map.size.x and bp.y < map.size.y, "the inner planet is on screen at system zoom (%s)" % bp)
	map.focus_cell(int(s0["x"]), int(s0["y"]), 0.4, false)
	await frames(2)
	check(not (e0["sphere"] as Node3D).visible and not (e0["planets"] as Node3D).visible, "sphere and planets hide at galaxy zoom")

	# ---- Smooth fly-to: double-click steps galaxy -> sector -> system without jumps
	var sp0 := map.world_to_screen(pos0)
	mouse(map, MOUSE_BUTTON_LEFT, true, sp0, true)
	mouse(map, MOUSE_BUTTON_LEFT, false, sp0)
	var zooms: Array = [map.zoom]
	for k in 600:
		await process_frame
		zooms.append(map.zoom)
		if not map._animating:
			break
	var smooth := true
	for k in range(1, zooms.size()):
		smooth = smooth and zooms[k] >= zooms[k - 1] - 1e-4 and zooms[k] / maxf(zooms[k - 1], 1e-4) < 1.6
	check(smooth, "fly-to zooms in smoothly (%.2f -> %.2f)" % [zooms[0], zooms[zooms.size() - 1]])
	check(map.zoom_level() == "sector" and absf(map.zoom - GalaxyMap.FLY_SECTOR) < 0.05, "first double-click lands at sector zoom (%.2f)" % map.zoom)
	var sp1 := map.world_to_screen(pos0)
	mouse(map, MOUSE_BUTTON_LEFT, true, sp1, true)
	mouse(map, MOUSE_BUTTON_LEFT, false, sp1)
	for k in 600:
		await process_frame
		if not map._animating:
			break
	check(map.zoom_level() == "system" and map.is_focused_on(int(s0["x"]), int(s0["y"])), "second double-click lands on the system close-up (%.2f)" % map.zoom)

	# ---- Picking open space and off the chart
	map.frame_known()
	await frames(2)
	var empty := -1
	var occupied := {}
	for s in systems:
		occupied[int(s["y"]) * g.x + int(s["x"])] = true
	for f in app.view.get("fleets", []) + app.view.get("contacts", []) + app.view.get("stations", []):
		occupied[int(f["y"]) * g.x + int(f["x"])] = true
	for c in g.x * g.y:
		var cx := c % g.x
		var cy := c / g.x
		if occupied.has(c) or cx == 0 or cy == 0 or cx == g.x - 1 or cy == g.y - 1:
			continue
		var near := false
		for dy in range(-1, 2):
			for dx in range(-1, 2):
				near = near or occupied.has((cy + dy) * g.x + cx + dx)
		if not near:
			empty = c
			break
	check(empty >= 0, "found an empty sector")
	if empty >= 0:
		map.focus_cell(empty % g.x, empty / g.x, 1.0, false)
		await frames(2)
		var ep := map.map_to_screen(map.cell_center(empty % g.x, empty / g.x))
		var pk2 := map.pick(ep)
		check(pk2["kind"] == "space" and int(pk2["cell"]) == empty, "clicking open space picks its sector (%s)" % pk2)
		var round_trip := map.screen_to_map(ep)
		check(round_trip.distance_to(map.cell_center(empty % g.x, empty / g.x)) < 1.0, "screen_to_map inverts map_to_screen")
	map.focus_cell(0, 0, 1.0, false)
	await frames(2)
	check(map.cell_at(map.map_to_screen(Vector2(-2.5, -2.5) * GalaxyMap.CELL)) == -1, "a click off the charted grid picks nothing")

	# ---- Picking a fleet icon
	var fleets: Array = app.view.get("fleets", [])
	check(fleets.size() > 0, "player has fleets")
	if fleets.size() > 0:
		var f: Dictionary = fleets[0]
		map.focus_cell(int(f["x"]), int(f["y"]), GalaxyMap.FLY_SECTOR, false)
		await frames(3)
		var icon := {}
		for c in map._overlay.pickables:
			if c["kind"] == "fleet" and int(c["cell"]) == int(f["y"]) * g.x + int(f["x"]):
				icon = c
		check(not icon.is_empty(), "the fleet has a screen-space icon")
		if not icon.is_empty():
			var pk3 := map.pick(icon["pos"])
			check(pk3["kind"] == "fleet" and int(pk3["cell"]) == int(f["y"]) * g.x + int(f["x"]), "clicking the fleet icon picks the fleet (%s)" % pk3)

	# ---- Gestures: right-click opens the menu, right-drag orbits instead, wheel zooms to the cursor
	map.frame_known()
	await frames(2)
	var ctx := []
	map.cell_context.connect(func(c, p): ctx.append(c))
	var at := map.world_to_screen(pos0)
	mouse(map, MOUSE_BUTTON_RIGHT, true, at)
	mouse(map, MOUSE_BUTTON_RIGHT, false, at)
	check(ctx.size() == 1 and int(ctx[0]) == cell0, "right-click without a drag asks for the order menu on that sector")
	var yaw0 := map.yaw
	var pitch0 := map.pitch
	mouse(map, MOUSE_BUTTON_RIGHT, true, at)
	for k in 6:
		motion(map, at + Vector2(10 * (k + 1), 4 * (k + 1)), Vector2(10, 4))
	mouse(map, MOUSE_BUTTON_RIGHT, false, at + Vector2(60, 24))
	check(ctx.size() == 1, "right-drag does not open the menu")
	check(not is_equal_approx(map.yaw, yaw0) and not is_equal_approx(map.pitch, pitch0), "right-drag orbits the camera (yaw %.2f, pitch %.2f)" % [map.yaw, map.pitch])
	var cursor := Vector2(700, 400)
	var before := map.screen_to_map(cursor)
	map.zoom_at(cursor, 1.15)
	check(map.screen_to_map(cursor).distance_to(before) < 2.0, "wheel zoom keeps the point under the cursor")
	var cam0 := map.cam
	mouse(map, MOUSE_BUTTON_LEFT, true, cursor)
	for k in 5:
		motion(map, cursor + Vector2(-8 * (k + 1), 0), Vector2(-8, 0))
	mouse(map, MOUSE_BUTTON_LEFT, false, cursor + Vector2(-40, 0))
	check(map.cam.distance_to(cam0) > 5.0, "left-drag pans")
	map.frame_known()
	check(is_equal_approx(map.yaw, 0.0) and is_equal_approx(map.pitch, GalaxyMap.PITCH_DEFAULT), "F (frame) resets the orbit")

	# ---- Anomalies follow the renderer; layers toggle
	var types6 := ["Nebula", "IonStorm", "SubspaceRift", "Wormhole", "BlackHole", "NeutronStar"]
	var feats := []
	for k in types6.size():
		feats.append({"id": k + 1, "type": types6[k], "x": 3 + 2 * k, "y": 4, "surveyed": true})
	app.view["features"] = feats
	map.rebuild()
	await frames(2)
	var vol := RenderingServer.get_current_rendering_method() == "forward_plus"
	check(map.is_volumetric() == vol, "anomalies are volumetric only on Forward+ (%s)" % RenderingServer.get_current_rendering_method())
	var mesh_ok := map._features.get_child_count() == 5
	for n in map._features.get_children():
		var mi := n as MeshInstance3D
		mesh_ok = mesh_ok and (mi.mesh is SphereMesh if vol else mi.mesh is QuadMesh)
	check(mesh_ok, "five anomaly %s" % ("volumes" if vol else "billboards"))
	check(map._decor.get_child_count() == 1, "the neutron star is a pulsing star, not a cloud")
	map.set_layer("territory", false)
	map.set_layer("fog", false)
	check(not map._territory.visible and not map._fog.visible, "territory and fog layers hide")
	map.set_layer("territory", true)
	map.set_layer("fog", true)
	check(map._territory.visible and map._fog.visible, "territory and fog layers show again")
	print("%d failures" % failures)
	quit(1 if failures > 0 else 0)
