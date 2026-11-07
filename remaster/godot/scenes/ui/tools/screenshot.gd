extends SceneTree
## Renders UI screenshots (needs a display: run under xvfb-run with --rendering-driver opengl3).
##   godot --path remaster/godot --rendering-driver opengl3 --resolution 1920x1080 \
##     -s res://scenes/ui/tools/screenshot.gd -- <out_dir> [shot ...]
## Shots: title new galaxy galaxy-menu system research fleets empire diplomacy intel database combat briefing events victory end settings saves
## Battle: battle-r1 battle-r3, and the camera modes battle-overview battle-chase battle-ship battle-target battle-topdown
## battle-cinematic battle-free (BattleCamera).

var app: App
var out_dir := "res://docs/screens"
var shots: Array = []

func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() > 0:
		out_dir = args[0]
	shots = args.slice(1) if args.size() > 1 else ["title", "new", "galaxy", "galaxy-menu", "system", "research", "fleets", "empire", "diplomacy", "intel", "database", "combat", "briefing", "events", "victory", "end", "settings", "saves"]
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(out_dir) if out_dir.begins_with("res://") else out_dir)
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred()

func _wait(frames: int = 8) -> void:
	for n in frames:
		await process_frame

func _run() -> void:
	await _wait(4)
	for s in shots:
		await _prepare(s)
		for t in app.get_node("Toasts").get_children():
			t.queue_free()
		await _wait(10)
		var img := root.get_viewport().get_texture().get_image()
		# Committed screenshots are kept small: 960x540.
		if OS.get_environment("SHOT_FULL") == "":
			img.resize(960, 540, Image.INTERPOLATE_LANCZOS)
		var path := "%s/ui-%s.png" % [out_dir, s]
		img.save_png(path)
		print("saved ", path)
	quit(0)

func _campaign() -> void:
	if app.view.is_empty() or app.view.get("session", {}).get("active", false):
		app.start_campaign({"faction": 1, "seed": 424242, "era": 3, "ruleset": "reference", "width": 22, "height": 16, "systems": 48, "preset": "remastered", "events": true, "aiAggression": true})
		# With the real core, play a few turns so the map has something on it.
		if app.core.is_native():
			for n in 12:
				app.end_turn()
				app.close_all_modals()
	app.close_all_modals()

func _prepare(s: String) -> void:
	match s:
		"title":
			app.view = {}
			app.goto("title")
		"new":
			app.view = {}
			app.goto("new")
		"saves":
			app.goto("saves")
		"galaxy":
			_campaign()
			app.goto("galaxy")
			var cols: Array = app.view.get("fleets", [])
			if cols.size() > 0:
				app.select_fleet(int(cols[0]["id"]))
		"galaxy-menu":
			_campaign()
			app.goto("galaxy")
			await _wait(2)
			var scr := app.current_screen()
			var f: Dictionary = app.view["fleets"][0]
			app.select_fleet(int(f["id"]))
			if scr.has_method("open_context_menu_at_cell"):
				scr.open_context_menu_at_cell(int(f["x"]) + 2, int(f["y"]) + 1)
		"frontier":
			app.start_drill("newFrontierDrill", "Frontier drill")
			app.close_all_modals()
			var st: Array = app.view.get("stations", [])
			if st.size() > 0:
				app.select_cell(int(st[0]["y"]) * app.width() + int(st[0]["x"]))
		"combat":
			app.start_drill("newBattleDrill", "Tactical drill")
			app.end_turn()
		"battle-r1", "battle-r3":
			# Starbase drill (Remastered rules 3): take command and resolve rounds, then let the 3D view play.
			# SHOT_DRILL_SHIPS sets the ships a side (default 3, max 24) for the large-battle shots.
			var drill := {"station": true}
			if OS.get_environment("SHOT_DRILL_SHIPS") != "":
				drill["ships"] = int(OS.get_environment("SHOT_DRILL_SHIPS"))
			app.start_drill("newBattleDrill", "Starbase drill", drill)
			app.end_turn()
			await _wait(4)
			app.close_all_modals() # the deferred "Enemy contact" choice; Take command
			for n in (1 if s == "battle-r1" else 3):
				app._current._round()
				await _wait(4)
			await _wait(int(OS.get_environment("SHOT_BATTLE_FRAMES")) if OS.get_environment("SHOT_BATTLE_FRAMES") != "" else 150)
		"battle-overview", "battle-chase", "battle-ship", "battle-target", "battle-topdown", "battle-cinematic", "battle-free":
			# The battle camera modes on the starbase drill after round 2 (BattleCamera; the mode bar shows the mode).
			if not (app._current != null and app._current.get("host") != null and app._current.host.view_node != null):
				app.start_drill("newBattleDrill", "Starbase drill", {"station": true})
				app.end_turn()
				await _wait(4)
				app.close_all_modals()
				for n in 2:
					app._current._round()
					await _wait(4)
			var cam: BattleCamera = app._current.host.view_node.camera
			var mode: int = {"battle-overview": 0, "battle-chase": 1, "battle-ship": 2, "battle-target": 3, "battle-topdown": 4,
				"battle-cinematic": 5, "battle-free": 0}[s]
			cam.set_mode(mode)
			if s == "battle-free":
				# Zoomed toward a point left of centre, then orbited round: the free camera.
				await _wait(60)
				cam.zoom_at(4.0, Vector2(0.35, 0.55))
				cam.add_orbit(-40.0, -15.0)
			await _wait(int(OS.get_environment("SHOT_BATTLE_FRAMES")) if OS.get_environment("SHOT_BATTLE_FRAMES") != "" else 120)
		"briefing", "events", "victory", "end", "settings":
			_campaign()
			app.goto("galaxy")
			if s == "briefing":
				app.briefing = app.core.request("briefing")
			if s == "end":
				preload("res://scenes/ui/screens/overlays.gd").end_screen(app, "victory")
			else:
				app.open_overlay(s)
		_:
			_campaign()
			app.goto(s)
