extends SceneTree
## Tactical rules 4 driver for the battle view: builds the bridge's combat sandbox (BotfCore "combatSandbox"),
## shows the plan phase for the first player ship (forecast, arcs, range bands, move preview, subsystem pips), commits
## aimed orders and plays rounds through BattleView, profiling the director's group shots and saving frame strips.
##   godot --path remaster/godot --resolution 1920x1080 --audio-driver Dummy -s res://scenes/battle/tools/wego_capture.gd -- \
##       <out_dir> [--ships=6] [--rounds=6] [--seed=0] [--aim=engines] [--every=0.75] [--probe]
## --probe: no screenshots, just the director statistics (runs headless too).

var out_dir := ""
var ships := 6
var rounds := 6
var seed := 0
var aim := "engines"
var every := 0.75
var probe := false
var core: Object


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	for a in args:
		if a.begins_with("--ships="): ships = int(a.split("=")[1])
		elif a.begins_with("--rounds="): rounds = int(a.split("=")[1])
		elif a.begins_with("--seed="): seed = int(a.split("=")[1])
		elif a.begins_with("--aim="): aim = a.split("=")[1]
		elif a.begins_with("--every="): every = float(a.split("=")[1])
		elif a == "--probe": probe = true
		elif not a.begins_with("--"): out_dir = a
	_run.call_deferred()


func _call(method: String, args := {}) -> Variant:
	var reply: Variant = core.request(method, args)
	if reply is Dictionary and reply.has("ok") and not reply.ok:
		printerr("bridge ", method, ": ", reply.get("error", ""))
		return null
	return reply.get("result", reply) if reply is Dictionary and reply.has("result") else reply


func _shot(name: String) -> void:
	if probe or out_dir == "":
		return
	await RenderingServer.frame_post_draw
	var img := root.get_viewport().get_texture().get_image()
	img.save_png(out_dir.path_join(name + ".png"))
	print("shot ", name)


func _run() -> void:
	if not Engine.has_singleton("BotfCore"):
		printerr("BotfCore missing")
		quit(1)
		return
	core = Engine.get_singleton("BotfCore")
	if out_dir != "":
		DirAccess.make_dir_recursive_absolute(out_dir)
	var enc: Variant = _call("combatSandbox", {"ships": ships, "seed": seed})
	if not enc is Dictionary:
		quit(1)
		return
	var view: Node3D = load("res://scenes/battle/battle_view.tscn").instantiate()
	view.play_audio = false
	root.add_child(view)
	await process_frame
	var t0 := Time.get_ticks_msec()
	view.load_encounter(enc)
	print("deployment load_encounter %d ms" % (Time.get_ticks_msec() - t0))
	for i in 20:
		await process_frame
	view.force_cutaway = true
	var subsystem_kills := 0
	var group_frames := 0
	for r in rounds:
		if enc.get("finished", false):
			break
		# Plan phase: the first active player ship against the weakest enemy, aimed at `aim`.
		var mine := {}
		var target := 0
		var weakest := INF
		for s in enc.ships:
			if s.get("mine", false) and s.get("active", false) and mine.is_empty():
				mine = s
			elif not s.get("mine", false) and s.get("active", false) and not s.get("hidden", false):
				var ehp := float(s.get("hull", 0)) + float(s.get("shields", 0))
				if ehp < weakest:
					weakest = ehp
					target = int(s.id)
		if mine.is_empty():
			break
		var fc: Variant = _call("tacticalForecast", {"sandbox": true, "fleetId": mine.id, "targetFleetId": target,
			"maneuver": "Engage", "subsystem": aim})
		if fc is Dictionary:
			view.show_plan(mine, fc)
			print("plan r%d ship %d -> %d: expected %d, mounts firing %d, path %d poses, exact %s" % [r + 1, int(mine.id),
				int(fc.get("target", 0)), int(fc.get("expectedDamage", 0)), int(fc.get("mountsFiring", 0)),
				fc.get("path", []).size(), str(fc.get("exact", false))])
			view.camera.select_ship(int(mine.id))
			for i in 30:
				await process_frame
			await _shot("plan_r%d" % (r + 1))
			view.clear_plan()
		_call("tacticalOrder", {"sandbox": true, "all": true, "targetFleetId": target, "maneuver": "Engage", "subsystem": aim})
		var t1 := Time.get_ticks_msec()
		enc = _call("tacticalRound", {"sandbox": true})
		var idx: int = enc.rounds.size() - 1
		for e in enc.rounds[idx].get("events", []):
			if e.get("subsystemDestroyed", false):
				subsystem_kills += 1
		view.load_encounter(enc)
		var worst := 0
		var last := Time.get_ticks_msec()
		var next_shot := every
		var k := 0
		while view.is_playing():
			await process_frame
			var now := Time.get_ticks_msec()
			worst = maxi(worst, now - last)
			last = now
			if not view.camera._cut_pose.is_empty():
				group_frames += 1
			if not probe and view.time >= next_shot:
				next_shot += every
				k += 1
				await _shot("r%d_%02d%s" % [r + 1, k, "_group" if not view.camera._cut_pose.is_empty() else ""])
		print("round %d: %d ms, worst frame %d ms, %d events" % [r + 1, Time.get_ticks_msec() - t1, worst, enc.rounds[idx].events.size()])
	print("director group shots: ", view.camera.group_stats)
	print("frames on a group shot: %d; subsystem kills: %d" % [group_frames, subsystem_kills])
	quit(0)
