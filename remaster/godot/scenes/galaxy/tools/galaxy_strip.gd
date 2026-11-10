extends SceneTree
## Captures the 3D galaxy zoom strip on a Huge galaxy (34x24 sectors, 120 systems) with the real core: the whole
## galaxy, the flight in to a sector, then to the home system's close-up with its planets, plus a feature sweep
## (one of each anomaly) and a frame-rate measurement at each level. Needs a window (GPU).
##   Forward+:       godot --path remaster/godot --resolution 1920x1080 --audio-driver Dummy \
##                     -s res://scenes/galaxy/tools/galaxy_strip.gd -- <out_dir>
##   Compatibility:  add --rendering-driver opengl3 --rendering-method gl_compatibility
## Set BOTF_PLANET_DIR to LocalAssets/planets to use the lane L7 planet textures. STRIP_NOVSYNC=1 measures uncapped.
## Prints "STRIP fps <level>: ..." lines and writes <out_dir>/strip-NN-<label>.png.

var app: App
var out_dir := "user://galaxy_strip"
var n := 0

func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() > 0:
		out_dir = args[0]
	if OS.get_environment("STRIP_NOVSYNC") != "":
		DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
	DirAccess.make_dir_recursive_absolute(out_dir)
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred()

func wait(k := 2) -> void:
	for i in k:
		await process_frame

func shot(label: String) -> void:
	await RenderingServer.frame_post_draw
	var im: Image = root.get_texture().get_image()
	im.save_png("%s/strip-%02d-%s.png" % [out_dir, n, label])
	n += 1

func measure(label: String, seconds: float) -> void:
	var t0 := Time.get_ticks_usec()
	var last := t0
	var times: Array = []
	while Time.get_ticks_usec() - t0 < int(seconds * 1e6):
		await process_frame
		var now := Time.get_ticks_usec()
		times.append((now - last) / 1000.0)
		last = now
	var sorted := times.duplicate()
	sorted.sort()
	var total := 0.0
	for v in times:
		total += v
	print("STRIP fps %s: %.0f fps avg, p99 %.1f ms, worst %.1f ms (%s)" % [label, 1000.0 * times.size() / maxf(total, 1.0),
		sorted[mini(sorted.size() - 1, int(sorted.size() * 0.99))], sorted[sorted.size() - 1], RenderingServer.get_current_rendering_method()])

## Flies to (x, y) at zoom z, capturing `count` frames spread over the flight.
func fly(map: GalaxyMap, x: int, y: int, z: float, label: String, count: int) -> void:
	map.focus_cell(x, y, z)
	for k in count:
		await wait(7)
		await shot("%s-%d" % [label, k])
	while map._animating:
		await process_frame

func _run() -> void:
	await wait(10)
	app.start_campaign({"faction": 1, "seed": 424242, "era": 3, "ruleset": "reference", "width": 34, "height": 24, "systems": 120, "preset": "remastered", "events": true, "aiAggression": true})
	for k in 10:
		app.end_turn()
		app.close_all_modals()
	app.close_all_modals()
	app.goto("galaxy")
	await wait(10)
	var gs := app.current_screen()
	var map: GalaxyMap = gs.map
	var g := map.grid_size()
	print("STRIP galaxy %dx%d, %d systems known, renderer %s, volumetric %s" % [g.x, g.y, app.view["systems"].size(), RenderingServer.get_current_rendering_method(), map.is_volumetric()])
	# A plotted course so the fleet path and ETA show.
	var fleets: Array = app.view.get("fleets", [])
	if fleets.size() > 0:
		var f: Dictionary = fleets[0]
		app.select_fleet(int(f["id"]))
		for d in [Vector2i(3, 2), Vector2i(-3, 2), Vector2i(3, -2), Vector2i(2, 0)]:
			var tx: int = clampi(int(f["x"]) + d.x, 0, g.x - 1)
			var ty: int = clampi(int(f["y"]) + d.y, 0, g.y - 1)
			if GalaxyOrders.plot_course(app, f, tx, ty):
				break
		await wait(4)
	# Whole galaxy: the opening frame, exactly as the game frames it (every star is on the chart from turn 1).
	map.frame_known()
	await wait(12)
	await shot("galaxy")
	await measure("Huge 120 systems, whole galaxy", 5.0)
	var home := {}
	var cols: Array = app.view.get("colonies", [])
	if cols.size() > 0:
		home = app.find_by_id("systems", int(cols[0]["systemId"]))
	if home.is_empty():
		home = app.view["systems"][0]
	var hx := int(home["x"])
	var hy := int(home["y"])
	await fly(map, hx, hy, GalaxyMap.FLY_SECTOR, "to-sector", 4)
	await wait(6)
	await shot("sector")
	await measure("sector", 3.0)
	await fly(map, hx, hy, GalaxyMap.FLY_SYSTEM, "to-system", 4)
	var t0 := Time.get_ticks_msec()
	while map.textures_pending() and Time.get_ticks_msec() - t0 < 20000:
		await process_frame
	await wait(20)
	await shot("system")
	await measure("system close-up", 3.0)
	map.orbit(0.9, -0.45)
	map.zoom = 30.0
	map._apply_camera()
	await wait(10)
	await shot("system-orbit")
	# The real galaxy again, a sector at the centre, for the frame rate and look (no stand-in systems: the core lists them all).
	map.focus_cell(g.x / 2, g.y / 2, GalaxyMap.FLY_SECTOR, false)
	await wait(12)
	await shot("huge-sector")
	await measure("Huge 120 systems, sector", 3.0)
	map.orbit(0.7, -0.3)
	map.focus_cell(g.x / 2, g.y / 2, 0.55, false)
	await wait(12)
	await shot("huge-orbit")
	# Feature sweep: one of each anomaly near the centre, revealed.
	var types := ["Nebula", "IonStorm", "SubspaceRift", "Wormhole", "BlackHole", "NeutronStar"]
	var feats := []
	var cx := g.x / 2
	var cy := g.y / 2
	for k in types.size():
		feats.append({"id": k + 1, "type": types[k], "x": cx - 5 + 2 * k, "y": cy, "surveyed": true, "name": types[k], "effect": ""})
	feats[3]["pairId"] = 9
	feats[3]["exit"] = {"x": cx + 4, "y": cy + 4}
	app.view["features"] = feats
	map.rebuild()
	map.focus_cell(cx, cy, 1.1, false)
	await wait(12)
	await shot("anomalies")
	await measure("anomalies", 3.0)
	map.focus_cell(cx + 3, cy, 4.5, false)
	map.orbit(0.5, -0.35)
	await wait(12)
	await shot("anomalies-close")
	quit(0)
