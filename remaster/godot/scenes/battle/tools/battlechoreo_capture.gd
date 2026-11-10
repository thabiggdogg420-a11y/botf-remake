extends SceneTree
## Battle choreography proof (M1 battlechoreo, BATTLE-REFERENCE.md): plays one whole round of the core's combat sandbox
## in BattleView at 1x and saves a frame every --step seconds (default 0.25: 32 frames over an 8 s round), from the
## wide view (--cam=wide, the overview camera) or the cinematic camera with film cuts (--cam=cine). Prints per frame the
## beams, bolts and torpedoes in the air, and at the end what the strip counts need: ships that moved more than one hull
## length during the round, and ships whose drawn end pose is the core's plotted cell and facing.
##   godot --path remaster/godot --resolution 1600x900 --audio-driver Dummy -s res://scenes/battle/tools/battlechoreo_capture.gd -- \
##       <out_dir> [--ships=24] [--seed=0] [--cam=wide|cine] [--frames=32] [--step=0.25] [--max-rounds=8] [--no-save]
##       [--foe-owner=N] [--map=Disruptor:PlasmaCannon]
## --no-save plays the round without saving frames and prints FPS (average, 1% low) over the round instead.

var out_dir := ""
var ships := 24
var seed := 0
var cam := "wide"
var frames := 32
var step := 0.25
var max_rounds := 8
var save := true
var foe_owner := 0
var wmap := {}
var core: Object
var view: Node3D

const Job := BattleTimeline.Job


func _initialize() -> void:
	for a in OS.get_cmdline_user_args():
		if a.begins_with("--ships="): ships = int(a.split("=")[1])
		elif a.begins_with("--seed="): seed = int(a.split("=")[1])
		elif a.begins_with("--cam="): cam = a.split("=")[1]
		elif a.begins_with("--frames="): frames = int(a.split("=")[1])
		elif a.begins_with("--step="): step = float(a.split("=")[1])
		elif a.begins_with("--max-rounds="): max_rounds = int(a.split("=")[1])
		elif a.begins_with("--foe-owner="): foe_owner = int(a.split("=")[1])
		elif a.begins_with("--map="):
			for pair in a.split("=")[1].split(","):
				var kv := pair.split(":")
				if kv.size() == 2:
					wmap[kv[0]] = kv[1]
		elif a == "--no-save": save = false
		elif a == "--novsync": DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
		elif not a.begins_with("--"): out_dir = a
	_run.call_deferred()


func _call(method: String, args := {}) -> Variant:
	var reply: Variant = core.request(method, args)
	if reply is Dictionary and reply.has("ok") and not reply.ok:
		printerr("bridge ", method, ": ", reply.get("error", ""))
		return null
	return reply.get("result", reply) if reply is Dictionary and reply.has("result") else reply


## Presentation-only dressing of the foe (owner id and weapon type names the effects read; outcomes unchanged).
func _dress(enc: Dictionary) -> Dictionary:
	if foe_owner <= 0 and wmap.is_empty():
		return enc
	enc = enc.duplicate(true)
	var mine := -1
	for s in enc.get("ships", []):
		if s.get("mine", false):
			mine = int(s.get("owner", 0))
	var foes := {}
	for s in enc.get("ships", []):
		if int(s.get("owner", 0)) == mine:
			continue
		foes[int(s.get("id", 0))] = true
		if foe_owner > 0:
			s["owner"] = foe_owner
	for r in enc.get("rounds", []):
		for rs in r.get("ships", []):
			if foes.has(int(rs.get("id", 0))) and foe_owner > 0:
				rs["owner"] = foe_owner
		for e in r.get("events", []):
			if foes.has(int(e.get("fleetId", 0))) and wmap.has(str(e.get("weapon", ""))):
				e["weapon"] = wmap[str(e.get("weapon", ""))]
	return enc


static func _fires(rnd: Dictionary) -> int:
	var n := 0
	for e in rnd.get("events", []):
		if e.kind == "Fire":
			n += 1
	return n


func _run() -> void:
	core = Engine.get_singleton("BotfCore")
	if core == null:
		printerr("BotfCore missing")
		quit(1)
		return
	if out_dir != "":
		DirAccess.make_dir_recursive_absolute(out_dir)
	var enc: Variant = _call("combatSandbox", {"ships": ships, "seed": seed})
	if not enc is Dictionary:
		quit(1)
		return
	view = load("res://scenes/battle/battle_view.tscn").instantiate()
	view.play_audio = false
	view.film_cuts = cam == "cine"
	root.add_child(view)
	await process_frame
	view.load_encounter(_dress(enc))
	for i in 60:
		await process_frame
	var picked := -1
	for r in max_rounds:
		if enc.get("finished", false):
			break
		_call("tacticalOrder", {"sandbox": true, "all": true, "targetFleetId": 0, "maneuver": "Engage"})
		enc = _call("tacticalRound", {"sandbox": true})
		var idx: int = enc.rounds.size() - 1
		var fires := _fires(enc.rounds[idx])
		# Round 1 is the opening; film a round of the exchange with fire from most of the fleet.
		if r >= 1 and fires >= maxi(2, ships) or r == max_rounds - 1 or (enc.get("finished", false) and fires > 0):
			picked = idx
			break
		view.load_encounter(_dress(enc))
		view.playback_speed = 8.0
		while view.is_playing():
			await process_frame
		view.playback_speed = 1.0
	if picked < 0:
		printerr("no round to film")
		quit(1)
		return
	view.camera.set_mode(BattleCamera.Mode.OVERVIEW if cam == "wide" else BattleCamera.Mode.CINEMATIC)
	if cam == "wide":
		view.camera.zoom_factor = 0.6 # the overview frames the whole arena; come in to the fleets
	if "show_markers" in view.camera:
		view.camera.show_markers = false
	view.load_encounter(_dress(enc))
	var tl: BattleTimeline = view.timeline
	var counts := {}
	for j in tl.jobs:
		var key := "%s:%s" % [BattleTimeline.JOB_NAMES[j.kind], j.weapon]
		counts[key] = counts.get(key, 0) + 1
	print("ROUND ", picked + 1, " ships ", tl.ships.size(), " jobs ", counts)
	if not save:
		await _measure_fps(tl)
		quit(0)
		return
	var per_frame := []
	var k := 0
	while k < frames:
		var at := step * float(k)
		while view.time < at and view.is_playing():
			await process_frame
		await RenderingServer.frame_post_draw
		if out_dir != "":
			root.get_viewport().get_texture().get_image().save_png(out_dir.path_join("f_%02d.png" % k))
		var live := _live(tl, view.time)
		per_frame.append(live)
		print("FRAME %02d t=%.2f beams=%d bolts=%d torpedoes=%d kills=%d" % [k, view.time, live.beam, live.pulse, live.torpedo, live.explosion])
		k += 1
		if not view.is_playing():
			break
	for key in ["beam", "pulse", "torpedo"]:
		var vals := []
		for f in per_frame:
			vals.append(int(f[key]))
		vals.sort()
		print("COUNT %s max=%d median=%d" % [key, vals[-1] if not vals.is_empty() else 0, vals[vals.size() / 2] if not vals.is_empty() else 0])
	var all := []
	for f in per_frame:
		all.append(int(f.beam) + int(f.pulse) + int(f.torpedo))
	all.sort()
	print("COUNT shots_in_air max=%d median=%d" % [all[-1], all[all.size() / 2]])
	_motion_report(tl, enc.rounds[picked].get("events", []))
	quit(0)


func _live(tl: BattleTimeline, t: float) -> Dictionary:
	var n := {"beam": 0, "pulse": 0, "torpedo": 0, "explosion": 0}
	for j in tl.jobs:
		if t >= j.start and t <= j.start + j.duration:
			var name: String = BattleTimeline.JOB_NAMES[j.kind]
			if n.has(name):
				n[name] += 1
	return n


## Ships that moved more than a hull length (largest distance from the round-start pose along the drawn path), and ships
## whose drawn pose at the round's end is the core's last ManeuverEnd cell and facing.
func _motion_report(tl: BattleTimeline, events: Array) -> void:
	var last := {}
	for e in events:
		if e.kind == "ManeuverEnd":
			last[int(e.fleetId)] = e
	var moved := 0
	var on_cell := 0
	var with_end := 0
	var lines := []
	for s in tl.ships:
		var p0 := tl.pose_of(s, 0.0)
		var far := 0.0
		var turned := 0.0
		var t := 0.0
		var end := s.hide_at if s.hide_at >= 0.0 else tl.round_seconds
		while t <= end:
			var p := tl.pose_of(s, t)
			far = maxf(far, Vector3(p.x - p0.x, p.y - p0.y, p.altitude - p0.altitude).length())
			turned = maxf(turned, absf(BattleTimeline.shortest_turn(p0.facing, p.facing)))
			t += 0.1
		var hull := maxf(s.length_meters, 100.0) / BattleTimeline.METERS_PER_CELL
		if far > hull:
			moved += 1
		lines.append("%d:%s far=%.2f hull=%.2f turn=%.0f" % [s.fleet_id, ["cap", "cru", "esc"][maxi(0, s.style)] if s.style >= 0 else "wing", far, hull, turned])
		if last.has(s.fleet_id):
			with_end += 1
			var pe := tl.pose_of(s, tl.end_seconds)
			var e: Dictionary = last[s.fleet_id]
			if is_equal_approx(pe.x, float(e.x)) and is_equal_approx(pe.y, float(e.y)) and absf(BattleTimeline.shortest_turn(pe.facing, float(e.facing))) < 1e-3:
				on_cell += 1
	print("MOTION ", " ".join(lines))
	print("COUNT moved_over_hull=%d of %d; end_on_core_cell=%d of %d" % [moved, tl.ships.size(), on_cell, with_end])


func _measure_fps(tl: BattleTimeline) -> void:
	var times: Array[float] = []
	var proc: Array[float] = []
	var parts := [0.0, 0.0, 0.0, 0.0]
	var prev := Time.get_ticks_usec()
	while view.time < tl.round_seconds and view.is_playing():
		await process_frame
		var now := Time.get_ticks_usec()
		times.append(float(now - prev) / 1000.0)
		proc.append(float(view.last_apply_usec) / 1000.0)
		for q in 4:
			parts[q] += float(view.prof[q]) / 1000.0
		if float(view.last_apply_usec) > 12000.0:
			print("SLOW t=%.2f apply=%.1f poses=%.1f spacing=%.1f ships=%.1f fx=%.1f frame=%.1f" % [view.time, view.last_apply_usec / 1000.0, view.prof[0] / 1000.0, view.prof[1] / 1000.0, view.prof[2] / 1000.0, view.prof[3] / 1000.0, times[-1]])
		prev = now
	var total := 0.0
	for x in times:
		total += x
	var sorted := times.duplicate()
	sorted.sort()
	var p99: float = sorted[int(sorted.size() * 0.99) - 1]
	var ps := proc.duplicate()
	ps.sort()
	var ptot := 0.0
	for x in proc:
		ptot += x
	print("APPLY parts avg ms poses=%.2f spacing=%.2f ships=%.2f fx=%.2f (camera = rest)" % [parts[0] / proc.size(), parts[1] / proc.size(), parts[2] / proc.size(), parts[3] / proc.size()])
	print("APPLY (battle scene script) avg=%.2f ms p99=%.2f ms max=%.2f ms" % [ptot / maxf(1, proc.size()), ps[int(ps.size() * 0.99) - 1], ps[-1]])
	print("FPS cam=%s frames=%d avg=%.1f fps 1%%low=%.1f fps (p99 %.2f ms) worst=%.2f ms renderer=%s" % [cam,
		times.size(), 1000.0 * times.size() / total, 1000.0 / p99, p99, sorted[-1], RenderingServer.get_current_rendering_method()])
