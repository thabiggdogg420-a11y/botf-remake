extends SceneTree
## Galaxy chart: it opens framed on the whole galaxy (every star inside the free area, clear of the panels) and every
## feature type (nebula, ion storm, subspace rift, wormhole, black hole, neutron star) gets drawn.
##   godot --headless --path remaster/godot -s res://scenes/galaxy/tests/test_galaxy_map.gd
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

func _run() -> void:
	App.forced_backend = "fixture"
	app = load("res://scenes/ui/app.tscn").instantiate()
	app.size = Vector2(1920, 1080)
	root.add_child(app)
	await frames()
	app.start_campaign({"faction": 1, "seed": 424242, "era": 3, "ruleset": "reference", "width": 22, "height": 16, "systems": 48, "preset": "remastered", "events": true, "aiAggression": true})
	app.close_all_modals()
	app.goto("galaxy")
	await frames(6)
	var gs := app.current_screen()
	var map: GalaxyMap = gs.map
	var g := map.grid_size()
	check(g.x > 1 and g.y > 1, "galaxy grid %dx%d" % [g.x, g.y])

	# ---- Framing on open
	check(gs._framed, "the chart framed itself when the screen opened")
	check(map.zoom >= GalaxyMap.ZOOM_MIN and map.zoom <= 1.6, "zoom %.2f within the framed range" % map.zoom)
	check(is_equal_approx(map.yaw, 0.0) and is_equal_approx(map.pitch, GalaxyMap.PITCH_DEFAULT), "the opening view is north up at the default tilt")
	# The whole galaxy is framed: the four corners of the tilted disc and every star sit inside the free rectangle,
	# clear of the right-hand inspector and the layer bar (and the nav rail and top bar, which lie outside the map).
	var free := Rect2(Vector2(map.view_insets.x, map.view_insets.y), map.size - Vector2(map.view_insets.x + map.view_insets.z, map.view_insets.y + map.view_insets.w))
	var corners := [Vector3(0, 0, 0), Vector3(g.x, 0, 0), Vector3(g.x, 0, g.y), Vector3(0, 0, g.y)]
	for c in corners:
		var cp := map.world_to_screen(c)
		check(free.grow(2.0).has_point(cp), "galaxy corner %s is inside the free area %s" % [cp, free])
	var out_of_frame := 0
	for sid in map.systems3d:
		if not free.grow(2.0).has_point(map.world_to_screen(map.systems3d[sid]["pos"])):
			out_of_frame += 1
	check(out_of_frame == 0, "every star is inside the free area (%d outside)" % out_of_frame)
	# F (frame_known) re-frames from any camera.
	var z0 := map.zoom
	map.zoom = 3.0
	map.cam = Vector2.ZERO
	map.frame_known()
	check(absf(map.zoom - z0) < 0.01 and map.cam != Vector2.ZERO, "frame_known re-frames the whole galaxy")

	# ---- Every feature type is drawn
	var types := ["Nebula", "IonStorm", "SubspaceRift", "Wormhole", "BlackHole", "NeutronStar"]
	var feats := []
	for k in types.size():
		feats.append({"id": k + 1, "type": types[k], "x": 3 + 2 * k, "y": 4, "surveyed": true})
	feats[3]["pairId"] = 9
	feats[3]["exit"] = {"x": 15, "y": 11}
	app.view["features"] = feats
	var all_known := ""
	for i in g.x * g.y:
		all_known += "2"
	app.view["fog"] = all_known
	map.rebuild()
	await frames(4)
	var by_type := {}
	for r in map._features.get_children():
		var m := GalaxyMap.feature_material(r)
		by_type[r.get_index()] = m
	# The shader layer draws the five volumetric types; the neutron star has its glyph in the drawer only.
	check(map._features.get_child_count() == 5, "five volumetric feature quads (%d)" % map._features.get_child_count())
	var ion: ShaderMaterial = by_type[1]
	var rift: ShaderMaterial = by_type[2]
	var worm: ShaderMaterial = by_type[3]
	var hole: ShaderMaterial = by_type[4]
	check(float(ion.get_shader_parameter("crackle")) > 0.0, "ion storm crackles")
	check(float(rift.get_shader_parameter("swirl")) > 0.0 and rift.get_shader_parameter("tint") != ion.get_shader_parameter("tint"), "subspace rift swirls in its own colour")
	check(float(worm.get_shader_parameter("swirl")) > 0.0, "wormhole swirls")
	check(float(hole.get_shader_parameter("swirl")) > 0.0 and hole.get_shader_parameter("tint") != worm.get_shader_parameter("tint"), "black hole has its own tint")
	check(by_type[0].get_shader_parameter("swirl") == 0.0, "nebula is a plain cloud")
	print("%d failures" % failures)
	quit(1 if failures > 0 else 0)
