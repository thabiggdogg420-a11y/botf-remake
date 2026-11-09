extends SceneTree
## Round playback proof (M1 battleplay): plays the core's combat sandbox in BattleView at 1x and saves frames spread
## across one round's playback, plus a STATS line on the timeline (fire spread, torpedo flight, ship motion).
##   godot --path remaster/godot --resolution 1600x900 --audio-driver Dummy -s res://scenes/battle/tools/playback_capture.gd -- \
##       <out_dir> [--ships=4] [--seed=0] [--want=torpedo|busy|kill] [--frames=10] [--max-rounds=8] [--camera=cinematic|overview]
##       [--asset=ship.fed.nebula] [--lod-floor=1.0]
## --asset dresses every ship of the first owner in that hull (presentation only; the core's ships are unchanged).
## --lod-floor sets HeroLook.min_lod_ratio: 1.0 (default) is the fix, 0.0 is the old behaviour (decimated LODs at range).
## --want picks the round to film: the first with a torpedo hit (torpedo), the most Fire events in the first rounds
## (busy), or the first with a kill (kill). Earlier rounds play at 8x so the scene state carries over as in a game.
## Frames: <out_dir>/f_NN.png at round times spread evenly from the first to the last job; STATS on stdout.

var out_dir := ""
var ships := 4
var seed := 0
var want := "torpedo"
var frames := 10
var max_rounds := 8
var camera := "cinematic"
var asset := ""
var core: Object

const PlaybackChecks := preload("res://scenes/battle/tools/playback_checks.gd")


func _initialize() -> void:
	for a in OS.get_cmdline_user_args():
		if a.begins_with("--ships="): ships = int(a.split("=")[1])
		elif a.begins_with("--seed="): seed = int(a.split("=")[1])
		elif a.begins_with("--want="): want = a.split("=")[1]
		elif a.begins_with("--frames="): frames = int(a.split("=")[1])
		elif a.begins_with("--max-rounds="): max_rounds = int(a.split("=")[1])
		elif a.begins_with("--camera="): camera = a.split("=")[1]
		elif a.begins_with("--asset="): asset = a.split("=")[1]
		elif a.begins_with("--lod-floor="): HeroLook.min_lod_ratio = float(a.split("=")[1])
		elif not a.begins_with("--"): out_dir = a
	_run.call_deferred()


func _call(method: String, args := {}) -> Variant:
	var reply: Variant = core.request(method, args)
	if reply is Dictionary and reply.has("ok") and not reply.ok:
		printerr("bridge ", method, ": ", reply.get("error", ""))
		return null
	return reply.get("result", reply) if reply is Dictionary and reply.has("result") else reply


static func _round_matches(rnd: Dictionary, what: String) -> bool:
	for e in rnd.get("events", []):
		if what == "torpedo" and e.kind == "Fire" and str(e.get("family", "")) == "torpedo" and e.get("hit", false):
			return true
		if what == "kill" and e.kind == "Destroyed":
			return true
	return false


func _dress(enc: Dictionary) -> Dictionary:
	if asset == "" or not enc.has("ships") or enc.ships.is_empty():
		return enc
	var owner0 := int(enc.ships[0].get("owner", 0))
	for s in enc.ships:
		if int(s.get("owner", 0)) == owner0:
			s["assetKey"] = asset
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
	var view: Node3D = load("res://scenes/battle/battle_view.tscn").instantiate()
	view.play_audio = false
	root.add_child(view)
	await process_frame
	view.load_encounter(_dress(enc))
	for i in 30:
		await process_frame
	var picked := -1
	for r in max_rounds:
		if enc.get("finished", false):
			break
		_call("tacticalOrder", {"sandbox": true, "all": true, "targetFleetId": 0, "maneuver": "Engage"})
		enc = _call("tacticalRound", {"sandbox": true})
		var idx: int = enc.rounds.size() - 1
		var rnd: Dictionary = enc.rounds[idx]
		var hit := _round_matches(rnd, want) if want != "busy" else (_fires(rnd) >= ships and r >= 1)
		view.load_encounter(_dress(enc))
		if hit or r == max_rounds - 1:
			picked = idx
			break
		view.playback_speed = 8.0
		while view.is_playing():
			await process_frame
		view.playback_speed = 1.0
	if picked < 0:
		printerr("no round matched --want=", want)
		quit(1)
		return
	if camera == "overview":
		view.camera.set_mode(BattleCamera.Mode.OVERVIEW)
	var tl: BattleTimeline = view.timeline
	_stats(view, tl, picked)
	var res: Dictionary = PlaybackChecks.check_round(tl, enc.rounds[picked])
	print("CHECK ", res.stats)
	for p in res.problems:
		print("CHECK FAIL ", p)
	if res.problems.is_empty():
		print("CHECK ok")
	# Frames spread from the first job to the end of the last (a few tenths in, so the first is not the static pose).
	var first := INF
	var last := 0.0
	for j in tl.jobs:
		first = minf(first, j.start)
		last = maxf(last, j.start + j.duration)
	first = maxf(0.2, minf(first, 0.6))
	var k := 0
	view.playback_speed = 1.0
	while k < frames:
		var at := lerpf(first, last, float(k) / maxf(1.0, frames - 1))
		while view.time < at and view.is_playing():
			await process_frame
		await RenderingServer.frame_post_draw
		if out_dir != "":
			var img: Image = root.get_viewport().get_texture().get_image()
			img.save_png(out_dir.path_join("f_%02d.png" % k))
		print("FRAME %02d t=%.2f live=%s" % [k, view.time, _live(view)])
		k += 1
		if not view.is_playing():
			break
	quit(0 if res.problems.is_empty() else 1)


## Jobs drawing at the view's time, by kind name.
static func _live(view: Node) -> String:
	var n := {}
	for j in view.timeline.jobs:
		if view.time >= j.start and view.time <= j.start + j.duration:
			var name: String = BattleTimeline.JOB_NAMES[j.kind]
			n[name] = n.get(name, 0) + 1
	return str(n)


func _stats(view: Node, tl: BattleTimeline, idx: int) -> void:
	var starts := []
	var torps := []
	for j in tl.jobs:
		if j.kind in [BattleTimeline.Job.BEAM, BattleTimeline.Job.PULSE, BattleTimeline.Job.TORPEDO]:
			starts.append(j.start)
		if j.kind == BattleTimeline.Job.TORPEDO:
			torps.append(j.duration)
	starts.sort()
	var end := tl.end_seconds
	var early := 0
	for s in starts:
		if s <= 0.1 * end:
			early += 1
	var moved := 0
	var moving_at_fire := 0
	for s in tl.ships:
		var a := tl.pose_of(s, 0.0)
		var b := tl.pose_of(s, tl.round_seconds)
		if Vector2(b.x - a.x, b.y - a.y).length() > 0.01 or absf(BattleTimeline.shortest_turn(a.facing, b.facing)) > 0.5:
			moved += 1
	for j in tl.jobs:
		if j.kind in [BattleTimeline.Job.BEAM, BattleTimeline.Job.PULSE, BattleTimeline.Job.TORPEDO]:
			var s := tl.track(j.fleet_id)
			if s != null:
				var p0 := tl.pose_of(s, j.start - 0.05)
				var p1 := tl.pose_of(s, j.start + 0.05)
				if Vector2(p1.x - p0.x, p1.y - p0.y).length() > 1e-4:
					moving_at_fire += 1
	print("STATS round=%d ships=%d end=%.2f shots=%d first10pct=%d spread=%.2f..%.2f distinct_starts=%d torpedoes=%d torp_flight=%s moved=%d shots_while_moving=%d" % [
		idx + 1, tl.ships.size(), end, starts.size(), early, starts[0] if not starts.is_empty() else -1.0,
		starts[-1] if not starts.is_empty() else -1.0, _distinct(starts), torps.size(),
		str(torps.slice(0, 6).map(func(x): return snappedf(x, 0.01))), moved, moving_at_fire])


static func _distinct(a: Array) -> int:
	var d := {}
	for x in a:
		d[snappedf(x, 0.01)] = true
	return d.size()
