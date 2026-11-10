extends SceneTree
## Real-campaign galaxy proof frames (M1 galaxy lane): starts a new Federation game of a given size with the real core,
## plays N turns of autoplay (end turn, close modals), opens the galaxy screen and writes one PNG. No client-side stand-ins.
##   godot --path remaster/godot --resolution 1920x1080 --audio-driver Dummy -s res://scenes/galaxy/tools/galaxy_proof.gd -- \
##     <out.png> <small|medium|large|huge> <turns> <open|sector|uncharted|order> [fps]
## open: the screen as the game opens it. sector: flown to the home sector. uncharted: an uncharted star near home selected
## (inspector shows its card). order: own ship selected and the order menu open on that uncharted star.
## Add the 5th argument "fps" to print the frame rate of the opening view. Needs a window (GPU).
var app: App

func wait(k := 2) -> void:
	for i in k:
		await process_frame

func _initialize() -> void:
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred()

func _fps(label: String, seconds: float) -> void:
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
	print("PROOF fps %s: %.0f fps avg, p99 %.1f ms, worst %.1f ms (%s)" % [label, 1000.0 * times.size() / maxf(total, 1.0),
		sorted[mini(sorted.size() - 1, int(sorted.size() * 0.99))], sorted[sorted.size() - 1], RenderingServer.get_current_rendering_method()])

## A simple player autopilot so a 30-turn frame shows a played empire: the colony runs on its governor, the scout charts the
## nearest uncharted star, the colony ship settles the nearest charted free system. Orders go through the real core.
func _play_turn() -> void:
	var v: Dictionary = app.view
	for c in v.get("colonies", []):
		if str(c.get("governor", {}).get("workforce", "Manual")) == "Manual":
			app.issue({"type": "SetColonyGovernor", "systemId": int(c["systemId"]), "governorWorkforce": "Balanced", "governorQueue": "Balanced"}, [], "")
	for f in v.get("fleets", []):
		var here := Vector2(int(f["x"]), int(f["y"]))
		if bool(f.get("colonyShip", false)):
			var sy := app.system_at(int(f["x"]), int(f["y"]))
			if not sy.is_empty() and int(sy.get("owner", 0)) == 0 and app.issue({"type": "Colonize", "fleetId": int(f["id"])}, [], ""):
				continue
			if Vector2(int(f.get("targetX", f["x"])), int(f.get("targetY", f["y"]))) != here:
				continue
			var best := {}
			var bd := INF
			for s in app.session.charted_systems():
				if int(s.get("owner", 0)) != 0 or Vector2(int(s["x"]), int(s["y"])) == here:
					continue
				var d := Vector2(int(s["x"]), int(s["y"])).distance_to(here)
				if d < bd:
					bd = d
					best = s
			if not best.is_empty():
				app.issue({"type": "MoveFleet", "fleetId": int(f["id"]), "targetX": int(best["x"]), "targetY": int(best["y"])}, [], "")
		elif Vector2(int(f.get("targetX", f["x"])), int(f.get("targetY", f["y"]))) == here:
			var bu := {}
			var bud := INF
			for s in app.view["systems"]:
				if AppSession.is_charted(s):
					continue
				var d2 := Vector2(int(s["x"]), int(s["y"])).distance_to(here)
				if d2 < bud:
					bud = d2
					bu = s
			if not bu.is_empty():
				app.issue({"type": "MoveFleet", "fleetId": int(f["id"]), "targetX": int(bu["x"]), "targetY": int(bu["y"])}, [], "")

func _run() -> void:
	var a := OS.get_cmdline_user_args()
	var out: String = a[0]
	var size_id: String = a[1]
	var turns := int(a[2])
	var what: String = a[3]
	var sz: Dictionary = NewCampaignVM.SIZES.filter(func(s): return s["id"] == size_id)[0]
	await wait(10)
	app.start_campaign({"faction": 1, "seed": 424242, "era": 1, "ruleset": "reference", "width": sz["width"], "height": sz["height"],
		"systems": sz["systems"], "preset": "remastered", "events": true, "aiAggression": true})
	for k in turns:
		_play_turn()
		app.end_turn()
		app.close_all_modals()
	app.close_all_modals()
	app.goto("galaxy")
	await wait(30)
	# The "Campaign begins" toast was raised at start_campaign and lives 3.4 s of real time; the turns above run inside a
	# single frame, so wait it out before any capture (otherwise it shows at turn 30 as a capture artefact).
	await create_timer(4.2).timeout
	await wait(4)
	var gs := app.current_screen()
	var map: GalaxyMap = gs.map
	var g := map.grid_size()
	var systems: Array = app.view["systems"]
	var charted := app.session.charted_systems().size()
	print("PROOF %s %dx%d, %d systems listed, %d charted, turn %d, zoom %.3f, renderer %s" % [size_id, g.x, g.y, systems.size(), charted,
		int(app.view["meta"]["turn"]), map.zoom, RenderingServer.get_current_rendering_method()])
	var fog_s := str(app.view["fog"])
	var known_cells := fog_s.length() - fog_s.count("0")
	print("PROOF counters: %d of %d sectors known (%.1f%%), %d ships, %d contacts, %d colonies, rivals met: %d" % [known_cells, fog_s.length(),
		100.0 * known_cells / maxf(1.0, fog_s.length()), app.view["fleets"].size(), app.view["contacts"].size(), app.view["colonies"].size(),
		app.view["empires"].filter(func(e): return e.get("contact", false)).size()])
	var home: Dictionary = app.find_by_id("systems", int(app.view["colonies"][0]["systemId"]))
	var hx := int(home["x"])
	var hy := int(home["y"])
	if what == "sector":
		map.focus_cell(hx, hy, GalaxyMap.FLY_SECTOR, false)
		await wait(20)
	elif what == "uncharted" or what == "order":
		var best := {}
		var bd := INF
		for s in systems:
			if AppSession.is_charted(s):
				continue
			var d := Vector2(int(s["x"]), int(s["y"])).distance_to(Vector2(hx, hy))
			if d < bd:
				bd = d
				best = s
		print("PROOF uncharted pick: %s at %d.%d" % [best["name"], int(best["x"]), int(best["y"])])
		map.focus_cell(int(best["x"]), int(best["y"]), 2.2, false)
		await wait(10)
		if what == "order":
			var fl: Array = app.view["fleets"]
			app.select_fleet(int(fl[0]["id"]))
			gs.open_context_menu_at_cell(int(best["x"]), int(best["y"]))
		else:
			app.select_cell(int(best["y"]) * g.x + int(best["x"]))
		await wait(20)
	var t0 := Time.get_ticks_msec()
	while map.textures_pending() and Time.get_ticks_msec() - t0 < 15000:
		await process_frame
	await wait(10)
	await RenderingServer.frame_post_draw
	root.get_texture().get_image().save_png(out)
	print("PROOF wrote %s" % out)
	if a.size() > 4 and a[4] == "fps":
		await _fps("%s %s" % [size_id, what], 5.0)
	quit(0)
