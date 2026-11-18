extends SceneTree
## Headless smoke test of BattleScene: every fixture round plays to round_finished, emits weapon / sfx cues with
## the expected keys, and ship nodes follow the timeline without jumps, within a round and from one round to the next
## (true-scale spacing and 3D heights included).
##   godot --headless --path remaster/godot -s res://scenes/battle/tests/test_battle_scene.gd

const FIXTURE := "res://scenes/battle/fixtures/rules3_battle.json"

var failures := 0


func check(cond: bool, what: String) -> void:
	if not cond:
		failures += 1
		printerr("  FAIL  ", what)


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	print("Battle scene tests")
	var encounter: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(FIXTURE))
	var scene := BattleScene.new()
	scene.play_audio = false
	root.add_child(scene)
	await process_frame
	var cues: Array = []
	var finished: Array = []
	scene.cue.connect(func(key, keys, priority, fleet): cues.append([key, keys, priority, fleet]))
	scene.round_finished.connect(func(i): finished.append(i))
	scene.playback_speed = 8.0
	var last := {}
	for r in encounter.rounds.size():
		cues.clear()
		scene.play_round(encounter, r)
		check(scene.is_playing(), "round %d playing" % r)
		var frames := 0
		var prev_t := 0.0
		while scene.is_playing() and frames < 2000:
			await process_frame
			frames += 1
			var last_t := prev_t
			prev_t = scene.time
			for fid in [5, 7, 8, 10]:
				var s := scene.ship_node(fid)
				if s == null or not s.visible:
					last.erase(fid)
					continue
				if last.has(fid):
					# A jump is measured against the timeline time the frame advanced: at 8x a capped 0.05 s frame is 0.4 s of
					# flight, and a fast escort covers more than half a cell in that honestly (M1: the fixed half-cell bound
					# failed on main at 18-41 units). Allowed: half a cell plus 3 cells per timeline second.
					var step_t: float = maxf(0.0, scene.time - last_t)
					var bound: float = BattleScene.CELL * BattleScene.SPREAD * (0.5 + 3.0 * step_t)
					check(s.global_position.distance_to(last[fid]) < bound, "round %d ship %d jumped %.1f units in %.2f s" % [r, fid, s.global_position.distance_to(last[fid]), step_t])
				last[fid] = s.global_position
		check(finished.has(r), "round %d finished (frames %d)" % [r, frames])
		var fires: Array = encounter.rounds[r].events.filter(func(e): return e.kind == "Fire")
		if not fires.is_empty():
			check(cues.any(func(c): return c[0].begins_with("weapon.")), "round %d weapon cue" % r)
			for c in cues:
				if c[0].begins_with("weapon."):
					check(c[0].count(".") == 2, "faction-specific weapon key %s" % c[0])
					check(c[1][-1].begins_with("weapon.generic."), "generic fallback for %s" % c[0])
		if encounter.rounds[r].events.any(func(e): return e.kind == "Destroyed"):
			check(cues.any(func(c): return c[0] == "sfx.explosion.large"), "round %d explosion cue" % r)
	check(scene.camera != null, "camera exists")
	_check_director(scene, encounter)
	scene.camera.set_mode(BattleCamera.Mode.ORBIT)
	scene.seek(2.0)
	await process_frame
	scene.queue_free()
	await process_frame
	print("scene tests: %d failures" % failures)
	quit(1 if failures > 0 else 0)


## M4 flight stage 2 (show-style camera): the shot director, driven frame by frame at 30 fps through the whole battle at
## real speed with a short gap between rounds. Every shot but the last holds 2.5-6.5 s of real time unless its subject
## has gone (destroyed or warped out); the first is an establisher and there are more; every other shot has a live
## subject; consecutive cuts are at least 2.5 s apart except when a subject goes.
func _check_director(scene: BattleScene, encounter: Dictionary) -> void:
	scene.camera.set_mode(BattleCamera.Mode.CINEMATIC)
	scene.playback_speed = 1.0
	var dt := 1.0 / 30.0
	var shots: Array = []     # [n, type, subject, start clock, end clock, subject gone at the cut]
	var last_n := -1
	var kill_frames := 0
	var kill_framed := 0
	for r in encounter.rounds.size():
		scene.play_round(encounter, r)
		var frames := 0
		while scene.is_playing() and frames < 30 * 40:
			scene._process(dt)
			frames += 1
			var d: Dictionary = scene._dir
			var kw: Dictionary = scene._kill_window(scene.time)
			if not kw.is_empty() and scene.time >= float(kw.boom):
				kill_frames += 1
				if str(d.get("type", "")) == "victim" and int(d.get("subject", 0)) == int(kw.fid):
					kill_framed += 1
			if int(d.get("n", -1)) != last_n and str(d.get("type", "")) != "":
				if not shots.is_empty():
					var prev: Array = shots.back()
					prev[4] = scene._dir_clock - dt
					var sid := int(prev[2])
					var gone := sid != 0 and (scene.ship_node(sid) == null or not scene.ship_node(sid).visible)
					var tr := scene.timeline.track(sid) if sid != 0 else null
					if tr != null and tr.hide_at >= 0.0 and scene.time >= tr.hide_at:
						gone = true
					prev[5] = gone or d.has("boom") # a kill cuts in whatever is running (BattleScene.KILL_LEAD)
				last_n = int(d.n)
				shots.append([last_n, str(d.type), int(d.get("subject", 0)), scene._dir_clock, -1.0, false])
				if str(d.type) != "establish":
					check(int(d.get("subject", 0)) != 0 and scene.ship_node(int(d.subject)) != null, "director shot %d (%s) has a live subject" % [last_n, d.type])
				check(str(d.type) in ["establish", "track", "push", "push_rev", "lead", "approach", "victim"], "director shot type %s" % d.type)
		for k in 15: # the player's turn: a short gap (inside GAP_GRACE), the shot carries over
			scene._process(dt)
	check(kill_frames == 0 or kill_framed == kill_frames, "the director holds on every breakup (%d of %d frames)" % [kill_framed, kill_frames])
	check(shots.size() >= 4, "director made %d shots over the battle" % shots.size())
	if shots.is_empty():
		return
	check(shots[0][1] == "establish", "the battle opens on an establisher (%s)" % shots[0][1])
	check(shots.filter(func(x): return x[1] == "establish").size() >= 2, "establishers recur")
	for k in shots.size() - 1:
		var sh: Array = shots[k]
		var held: float = float(sh[4]) - float(sh[3])
		if not bool(sh[5]):
			check(held >= 2.5 - 1e-3 and held <= 6.5, "director shot %d (%s) held %.2f s" % [sh[0], sh[1], held])
	print("  director: %d shots, types %s" % [shots.size(), shots.map(func(x): return x[1])])
