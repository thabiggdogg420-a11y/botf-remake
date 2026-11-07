extends SceneTree
## Galaxy chart: it opens framed (a neighbourhood of the known sectors, inside the galaxy, clear of the panels) and every
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
	check(map.zoom >= 0.35 and map.zoom <= 1.6, "zoom %.2f within the framed range (no 2.4x single-sector view)" % map.zoom)
	var world := Vector2(g) * GalaxyMap.CELL
	var avail := Vector2(map.size.x - map.view_insets.x - map.view_insets.z, map.size.y - map.view_insets.y - map.view_insets.w)
	var shown := avail / map.zoom
	check(shown.x >= minf(world.x, 9.0 * GalaxyMap.CELL) - 1.0, "the chart shows at least 9 sectors across (%.1f)" % (shown.x / GalaxyMap.CELL))
	# The visible rectangle sits inside the galaxy (or the galaxy is centred when it is smaller than the view).
	var centre := map.cam + Vector2(map.view_insets.x - map.view_insets.z, map.view_insets.y - map.view_insets.w) * 0.5 / map.zoom
	for axis in 2:
		if shown[axis] < world[axis]:
			check(centre[axis] - shown[axis] * 0.5 >= -1.0 and centre[axis] + shown[axis] * 0.5 <= world[axis] + 1.0, "view inside the galaxy on axis %d" % axis)
		else:
			check(absf(centre[axis] - world[axis] * 0.5) < 1.0, "galaxy centred on axis %d" % axis)
	# A known sector is on screen, clear of the inspector panel on the right.
	var fog: String = app.view["fog"]
	var known := -1
	for i in fog.length():
		if fog[i] != "0":
			known = i
			break
	check(known >= 0, "a known sector exists")
	if known >= 0:
		var sp := map.map_to_screen(map.cell_center(known % g.x, known / g.x))
		check(sp.x > map.view_insets.x and sp.x < map.size.x - map.view_insets.z and sp.y > 0.0 and sp.y < map.size.y - map.view_insets.w,
			"the first known sector is inside the visible area (%s)" % sp)
	# Home / F re-frames.
	map.zoom = 0.4
	map.frame_known()
	check(map.zoom >= 0.35 and map.zoom <= 1.6 and not is_equal_approx(map.zoom, 0.4), "frame_known re-frames")

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
		var m := (r as ColorRect).material as ShaderMaterial
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
