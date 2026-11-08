extends SceneTree
## Renders UI screenshots (needs a display: run under xvfb-run with --rendering-driver opengl3).
##   godot --path remaster/godot --rendering-driver opengl3 --resolution 1920x1080 \
##     -s res://scenes/ui/tools/screenshot.gd -- <out_dir> [shot ...]
## Shots: title new galaxy galaxy-menu system research fleets empire diplomacy intel database combat briefing events victory end settings saves
## Battle: battle-r1 battle-r3, and the camera modes battle-overview battle-chase battle-ship battle-target battle-topdown
## battle-cinematic battle-free (BattleCamera), and the close-ups battle-closeup (the player's flagship fills the frame;
## SHOT_CLASS=<ship class name> frames the first ship of that class instead, e.g. Galaxy or "Negh'Var").
## Galaxy: galaxy-open (as it opens), galaxy-features (one of each feature type across a revealed chart), and
## galaxy-nebula galaxy-ion galaxy-rift galaxy-wormhole galaxy-blackhole galaxy-neutron (zoomed on that feature).
## SHOT_FPS=1 prints a "FPS" line per battle shot (avg fps, p99 and worst frame, over the whole drill and over the last
## 100 frames); SHOT_NOVSYNC=1 turns vsync off so the average is the real frame cost, not the display refresh.

var app: App
var out_dir := "res://docs/screens"
var shots: Array = []
var _ft: Array = []   ## frame times (ms) recorded while SHOT_FPS is set
var _last_us := 0
var _recording := false

func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() > 0:
		out_dir = args[0]
	shots = args.slice(1) if args.size() > 1 else ["title", "new", "galaxy", "galaxy-menu", "system", "research", "fleets", "empire", "diplomacy", "intel", "database", "combat", "briefing", "events", "victory", "end", "settings", "saves"]
	if OS.get_environment("SHOT_NOVSYNC") != "":
		DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
	process_frame.connect(_on_frame)
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(out_dir) if out_dir.begins_with("res://") else out_dir)
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred()

func _on_frame() -> void:
	var now := Time.get_ticks_usec()
	if _recording and _last_us > 0:
		_ft.append((now - _last_us) / 1000.0)
	_last_us = now

static func fps_line(ft: Array) -> String:
	if ft.is_empty():
		return "no frames"
	var sorted := ft.duplicate()
	sorted.sort()
	var total := 0.0
	for v in ft:
		total += v
	return "%d frames, %.0f fps avg, p99 %.1f ms, worst %.1f ms" % [ft.size(), 1000.0 * ft.size() / total,
		sorted[mini(sorted.size() - 1, int(sorted.size() * 0.99))], sorted[sorted.size() - 1]]

func _report_fps(label: String) -> void:
	if not _recording:
		return
	for i in _ft.size():
		if _ft[i] > 80.0:
			print("FPS spike ", label, " frame ", i, ": ", snappedf(_ft[i], 0.1), " ms")
	print("FPS ", label, ": ", fps_line(_ft), " | last 100: ", fps_line(_ft.slice(maxi(0, _ft.size() - 100))))
	print("FPS render ", label, ": draw calls ", int(Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME)),
		", objects ", int(Performance.get_monitor(Performance.RENDER_TOTAL_OBJECTS_IN_FRAME)),
		", primitives ", int(Performance.get_monitor(Performance.RENDER_TOTAL_PRIMITIVES_IN_FRAME)),
		", video mem ", int(Performance.get_monitor(Performance.RENDER_VIDEO_MEM_USED) / 1048576.0), " MB")
	_recording = false

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
		_report_fps(s)
		var img := root.get_viewport().get_texture().get_image()
		if (s == "battle-closeup" or (s.begins_with("battle-volley") or s.begins_with("battle-strip") or s == "battle-campaign")) and app._current != null and app._current.get("host") != null and app._current.host.view_node != null:
			img = app._current.host.view_node.get_viewport().get_texture().get_image() # the 3D view alone, no panels
		# Committed screenshots are kept small: 960x540.
		if OS.get_environment("SHOT_FULL") == "":
			img.resize(960, 540, Image.INTERPOLATE_LANCZOS)
		var path := "%s/ui-%s.png" % [out_dir, s]
		img.save_png(path)
		print("saved ", path)
	quit(0)

## The first ship in the battle view whose name starts with `klass` ("Galaxy" finds "Galaxy — Command Cruiser").
func _find_ship(klass: String) -> BattleShip:
	var view: Node = app._current.host.view_node
	for fid in view._ships:
		var ship: BattleShip = view._ships[fid]
		if ship.ship_name.begins_with(klass):
			return ship
	return null

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
			_recording = OS.get_environment("SHOT_FPS") != ""
			_ft.clear()
			app.start_drill("newBattleDrill", "Starbase drill", drill)
			app.end_turn()
			await _wait(4)
			app.close_all_modals() # the deferred "Enemy contact" choice; Take command
			for n in (1 if s == "battle-r1" else 3):
				app._current._round()
				await _wait(4)
			await _wait(int(OS.get_environment("SHOT_BATTLE_FRAMES")) if OS.get_environment("SHOT_BATTLE_FRAMES") != "" else 150)
		"battle-overview", "battle-chase", "battle-ship", "battle-target", "battle-topdown", "battle-cinematic", "battle-free", "battle-closeup":
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
				"battle-cinematic": 5, "battle-free": 0, "battle-closeup": 0}[s]
			cam.set_mode(mode)
			if s == "battle-free":
				# Zoomed toward a point left of centre, then orbited round: the free camera.
				await _wait(60)
				cam.zoom_at(4.0, Vector2(0.35, 0.55))
				cam.add_orbit(-40.0, -15.0)
			if s == "battle-closeup":
				# A named class fills the frame: the free camera sits 1.15 hull lengths from it at a three-quarter angle.
				# Saved from the battle's own viewport (no LCARS panels around it). SHOT_CLASS defaults to Galaxy.
				await _wait(60)
				var want := OS.get_environment("SHOT_CLASS") if OS.get_environment("SHOT_CLASS") != "" else "Galaxy"
				var target := _find_ship(want)
				if target != null:
					app._current.host.view_node.playback_speed = 0.0 # hold the moment: the ship must stay in frame
					await _wait(4)
					cam.detach_to_free_cam()
					cam.free_distance = maxf(target.length * 1.15, 6.0)
					cam.add_orbit(-35.0, -4.0)
					for n in 30:
						cam.free_focus = target.global_position
						cam.transition = 0.0
						cam.snap()
						await _wait(1)
				else:
					push_warning("battle-closeup: no ship of class %s in the battle" % want)
			if s != "battle-closeup":
				await _wait(int(OS.get_environment("SHOT_BATTLE_FRAMES")) if OS.get_environment("SHOT_BATTLE_FRAMES") != "" else 120)
		"battle-volley", "battle-volley-close", "battle-campaign":
			# Mid-volley: the starbase drill (SHOT_DRILL_SHIPS a side) plays a round until the most weapon jobs are in
			# flight, then holds that instant. -close parks the free camera beside a ship that is firing.
			await _volley_drill(s == "battle-campaign")
			var view: Node = app._current.host.view_node
			var cam: BattleCamera = view.camera
			if s == "battle-volley-close":
				var shot := _live_shot(view)
				var shooter: BattleShip = view.ship_node(int(shot.get("fleet_id", 0))) if not shot.is_empty() else null
				if shooter != null:
					var target: BattleShip = view.ship_node(int(shot.get("target_fleet_id", 0)))
					cam.detach_to_free_cam()
					cam.free_distance = maxf(shooter.length * 2.2, 10.0)
					cam.add_orbit(-30.0, -6.0)
					for n in 20:
						var focus: Vector3 = shooter.global_position
						if target != null:
							focus = focus.lerp(target.global_position, 0.25)
						cam.free_focus = focus
						cam.transition = 0.0
						cam.snap()
						await _wait(1)
			await _wait(6)
			print("VOLLEY ", s, ": ", _volley_count(view), " weapon jobs live at t=", snappedf(view.time, 0.01))
			if OS.get_environment("SHOT_FXDEBUG") != "":
				var fx: BattleFx = view.fx
				print("FXDBG active=", fx._active.size(), " children=", fx.get_child_count(), " cam=", view.camera.global_position)
				for k in fx._active:
					var j: Dictionary = view.timeline.jobs[k]
					var nodes: Array = fx._active[k].nodes
					for nd in nodes:
						if nd is Node3D and is_instance_valid(nd):
							var extra = nd.get_instance_shader_parameter("beam_intensity") if nd is GeometryInstance3D else null
							print("  job ", k, " kind ", j.kind, " u ", snappedf((view.time - j.start) / j.duration, 0.01), " ", nd.global_position, " scale ", nd.global_basis.get_scale(), " vis ", nd.is_visible_in_tree(), " bi ", extra)
		"battle-strip", "battle-strip-close":
			# Motion check: 10 frames 0.25 s of round time apart around the busiest volley, saved from the 3D view as
			# ui-<shot>-NN.png (the final ui-<shot>.png is the last frame). -close holds a free camera near a shooter.
			await _volley_drill(OS.get_environment("SHOT_CAMPAIGN") != "")
			var view: Node = app._current.host.view_node
			var cam: BattleCamera = view.camera
			var t0: float = maxf(0.0, view.time - 1.25)
			if OS.get_environment("SHOT_KILL") != "":
				# Kill strip: resolve rounds until one destroys a ship, then frame the explosion.
				var kt := -1.0
				for r in 12:
					if view.timeline != null:
						for j in view.timeline.jobs:
							if j.kind == BattleTimeline.Job.EXPLOSION:
								kt = j.start
								break
					if kt >= 0.0 or app.encounter.get("finished", false):
						break
					app._current._round()
					await _wait(6)
				print("KILL at ", kt)
				if kt >= 0.0:
					t0 = maxf(0.0, kt - 0.5)
			var shot := _live_shot(view)
			view.force_cutaway = s == "battle-strip-close"
			view.seek(t0)
			# -close: the director's own two-ship framing of the busiest beam exchange (no free camera).
			view.playback_speed = 1.0
			for k in 10:
				while view.time < t0 + 0.25 * k:
					await _wait(1)
				var im: Image = view.get_viewport().get_texture().get_image()
				im.resize(640, 360, Image.INTERPOLATE_LANCZOS)
				im.save_png("%s/ui-%s-%02d.png" % [out_dir, s, k])
			print("STRIP ", s, " t ", snappedf(t0, 0.01), " .. ", snappedf(view.time, 0.01), " mode ", cam.mode, " director ", view.director.kind, " subj ", view.director.subject, " playing ", view.is_playing())
		"galaxy-open":
			app.view = {}
			_campaign()
			app.goto("galaxy")
		"galaxy-features", "galaxy-nebula", "galaxy-ion", "galaxy-rift", "galaxy-wormhole", "galaxy-blackhole", "galaxy-neutron":
			_campaign()
			app.goto("galaxy")
			await _wait(4)
			var types := ["Nebula", "IonStorm", "SubspaceRift", "Wormhole", "BlackHole", "NeutronStar"]
			var pick := {"galaxy-nebula": 0, "galaxy-ion": 1, "galaxy-rift": 2, "galaxy-wormhole": 3, "galaxy-blackhole": 4, "galaxy-neutron": 5}
			var gm: GalaxyMap = app.current_screen().map
			var gsz := gm.grid_size()
			var cx := int(gsz.x / 2)
			var cy := int(gsz.y / 2)
			var feats := []
			for k in types.size():
				feats.append({"id": k + 1, "type": types[k], "x": cx - 5 + 2 * k, "y": cy, "surveyed": true})
			feats[3]["pairId"] = 9
			feats[3]["exit"] = {"x": cx + 4, "y": cy + 3}
			app.view["features"] = feats
			app.view["fog"] = "2".repeat(gsz.x * gsz.y)
			gm.rebuild()
			if pick.has(s):
				gm.focus_cell(int(feats[pick[s]]["x"]), cy, 2.2, false)
			else:
				gm.frame_known()
			await _wait(4)
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


## Starts the starbase drill (or, with campaign=true, plays the reference campaign until a battle), takes command,
## resolves rounds until one has weapon fire, and holds playback at that round's busiest instant.
func _volley_drill(campaign := false) -> void:
	if campaign:
		app.view = {}
		# A small, crowded quadrant: empires meet within a few turns (a real campaign contact, not a drill).
		app.start_campaign({"faction": 1, "seed": 424242, "era": 3, "ruleset": "reference", "width": 10, "height": 8, "systems": 14, "preset": "remastered", "events": true, "aiAggression": true})
		var st := ""
		for n in 80:
			_force_contact()
			st = app.end_turn()
			if OS.get_environment("SHOT_FXDEBUG") != "" and n % 5 == 0:
				var e0: Dictionary = app.view.get("empires", [{}])[0] if app.view.get("empires", []).size() > 0 else {}
				print("TURN ", n, " st=", st, " contacts=", app.view.get("contacts", []).size(), " battles=", app.view.get("battles", []).size(), " fleets=", app.view.get("fleets", []).map(func(f): return [f.x, f.y]), " empire0=", str(e0).left(300))
			await _wait(1)
			if st == "battle":
				break
			app.close_all_modals()
		print("CAMPAIGN battle status=", st, " turn=", app.view.get("meta", {}).get("turn", 0))
	else:
		var drill := {"station": true}
		if OS.get_environment("SHOT_DRILL_SHIPS") != "":
			drill["ships"] = int(OS.get_environment("SHOT_DRILL_SHIPS"))
		if OS.get_environment("SHOT_FOE_STATION") != "":
			drill["foeStation"] = true
		app.start_drill("newBattleDrill", "Starbase drill", drill)
		app.end_turn()
	await _wait(4)
	app.close_all_modals()
	var view: Node = app._current.host.view_node
	var best := -1
	var best_t := 0.0
	var best_end := 0.0
	for r in 8:
		if app.encounter.get("finished", false):
			break
		app._current._round()
		await _wait(4)
		if view.timeline == null:
			continue
		var want_beam := OS.get_environment("SHOT_WANT_BEAM") != "" and r < 7
		var has_beam := false
		for j in view.timeline.jobs:
			has_beam = has_beam or j.kind in [BattleTimeline.Job.BEAM, BattleTimeline.Job.PULSE]
		if want_beam and not has_beam:
			continue
		best = -1
		for k in 120:
			var t: float = view.timeline.round_seconds * k / 120.0
			var n := 0
			for j in view.timeline.jobs:
				if j.kind in [BattleTimeline.Job.BEAM, BattleTimeline.Job.PULSE, BattleTimeline.Job.TORPEDO] and t >= j.start + 0.25 * j.duration and t <= j.start + 0.7 * j.duration:
					n += (20 if OS.get_environment("SHOT_WANT_BEAM") != "" else 3) if j.kind == BattleTimeline.Job.BEAM else 1
			if n > best:
				best = n
				best_t = t
				best_end = t
			elif n == best:
				best_end = t
		if best > 0:
			var kinds := {}
			for j in view.timeline.jobs:
				kinds[BattleTimeline.JOB_NAMES[j.kind]] = kinds.get(BattleTimeline.JOB_NAMES[j.kind], 0) + 1
			print("VOLLEY round ", r + 1, " jobs ", kinds, " best ", best, " at ", best_t, "-", best_end)
			if OS.get_environment("SHOT_FXDEBUG") != "":
				for j in view.timeline.jobs:
					if j.kind == BattleTimeline.Job.BEAM:
						print("  beam start ", snappedf(j.start, 0.01), " dur ", snappedf(j.duration, 0.01))
			best_t = lerpf(best_t, best_end, 0.5) # mid-volley: shots in flight, not just leaving the hull
			break
	var guard := 0
	while view.time < best_t - 0.4 and guard < 2000:
		guard += 1
		await _wait(1)
	view.playback_speed = 0.0
	view.seek(best_t)
	await _wait(2)

func _volley_count(view: Node) -> int:
	var n := 0
	for j in view.timeline.jobs:
		if j.kind in [BattleTimeline.Job.BEAM, BattleTimeline.Job.PULSE, BattleTimeline.Job.TORPEDO] and view.time >= j.start and view.time <= j.start + j.duration:
			n += 1
	return n

func _live_shot(view: Node) -> Dictionary:
	for j in view.timeline.jobs:
		if j.kind in [BattleTimeline.Job.BEAM, BattleTimeline.Job.PULSE] and view.time >= j.start + j.duration * 0.15 and view.time <= j.start + j.duration * 0.7:
			return j
	return {}


## Campaign proof route: send every fleet of the player toward the nearest system another empire owns (or, with no
## foreign system known, the far side of the map), so a real campaign contact happens within a few dozen turns.
func _force_contact() -> void:
	var me := app.player()
	# At war with every empire we have met, so a meeting is a battle (BOTF powers start at peace).
	for e in app.view.get("empires", []):
		var id := int(e.get("empire", e.get("id", 0)))
		if e.get("atWar", false) or not e.get("contact", false):
			continue
		if id != 0 and id != me and id <= 5:
			app.core.invoke("submit", {"command": {"type": "DeclareWar", "targetEmpire": id}, "cancelIds": []})
	var fleets: Array = app.view.get("fleets", [])
	var targets := []
	for c in app.view.get("contacts", []):
		targets.append(Vector2i(int(c.get("x", 0)), int(c.get("y", 0))))
	for sy in app.view.get("systems", []):
		var o := int(sy.get("owner", 0))
		if o != 0 and o != me:
			targets.append(Vector2i(int(sy.get("x", 0)), int(sy.get("y", 0))))
	for f in fleets:
		if int(f.get("owner", me)) != me:
			continue
		var at := Vector2i(int(f.get("x", 0)), int(f.get("y", 0)))
		var best := Vector2i(app.width() - 1 - at.x, at.y)
		var bd := INF
		for t in targets:
			var d := Vector2(t - at).length()
			if d < bd:
				bd = d
				best = t
		var r: Dictionary = app.core.invoke("submit", {"command": {"type": "MoveFleet", "fleetId": int(f.get("id", 0)), "targetX": best.x, "targetY": best.y}, "cancelIds": []})
		if not r.get("ok", false) and OS.get_environment("SHOT_FXDEBUG") != "":
			print("move refused: ", r.get("error", ""))
