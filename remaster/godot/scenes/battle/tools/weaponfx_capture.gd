extends SceneTree
## Weapon effects proof (M1 weaponfx, docs/WEAPON-FX.md): plays the core's combat sandbox in BattleView at 1x, as
## playback_capture does, and saves frames of one weapon effect, or the wide view, or measures the frame rate.
##   godot --path remaster/godot --resolution 1600x900 --audio-driver Dummy -s res://scenes/battle/tools/weaponfx_capture.gd -- \
##       <out_dir> [--shot=wide|phaser_shield|phaser_hull|photon_salvo|photon_impact|disruptor|beam_any] [--frames=8]
##       [--ships=24] [--seed=0] [--max-rounds=10] [--foe-owner=N] [--foe-asset=ship.rom.dderidex]
##       [--map=Disruptor:PolaronBeam,PhotonTorpedo:PlasmaTorpedo] [--fps] [--novsync]
## --shot picks the first round (playing the earlier ones at 8x) that has a matching shot, then films that shot with a
## close camera that follows it; wide keeps the battle's own camera on the busiest round. --fps plays the round with no
## frame saving and prints FPS lines (average, 1% low, worst frame) over its fire.
## --foe-owner / --foe-asset / --map dress the foe for the other empires' looks (presentation only: the core's ships,
## rolls and outcomes are unchanged; only the owner id, hull key and weapon type names the effects read are swapped).
## On the web build (no out_dir) each FRAME line holds the round still 1.5 s so the browser driver can screenshot it.

var out_dir := ""
var ships := 24
var seed := 0
var frames := 8
var max_rounds := 10
var shot := "wide"
var foe_owner := 0
var foe_asset := ""
var wmap := {}
var fps_mode := false
var core: Object
var view: Node3D
var _cam_job := {}
var _last_round := false
var _cam_on := false
var _cam: Camera3D
var _cam_eye := Vector3.ZERO
var _cam_look := Vector3.ZERO

const Job := BattleTimeline.Job


func _initialize() -> void:
	for a in OS.get_cmdline_user_args():
		if a.begins_with("--ships="): ships = int(a.split("=")[1])
		elif a.begins_with("--seed="): seed = int(a.split("=")[1])
		elif a.begins_with("--frames="): frames = int(a.split("=")[1])
		elif a.begins_with("--max-rounds="): max_rounds = int(a.split("=")[1])
		elif a.begins_with("--shot="): shot = a.split("=")[1]
		elif a.begins_with("--foe-owner="): foe_owner = int(a.split("=")[1])
		elif a.begins_with("--foe-asset="): foe_asset = a.split("=")[1]
		elif a.begins_with("--map="):
			for pair in a.split("=")[1].split(","):
				var kv := pair.split(":")
				if kv.size() == 2:
					wmap[kv[0]] = kv[1]
		elif a == "--fps": fps_mode = true
		elif a == "--novsync": DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
		elif not a.begins_with("--"): out_dir = a
	_run.call_deferred()


func _call(method: String, args := {}) -> Variant:
	var reply: Variant = core.request(method, args)
	if reply is Dictionary and reply.has("ok") and not reply.ok:
		printerr("bridge ", method, ": ", reply.get("error", ""))
		return null
	return reply.get("result", reply) if reply is Dictionary and reply.has("result") else reply


## Presentation-only dressing of the foe (see the header).
func _dress(enc: Dictionary) -> Dictionary:
	if foe_owner <= 0 and foe_asset == "" and wmap.is_empty():
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
		if foe_asset != "":
			s["assetKey"] = foe_asset
	for r in enc.get("rounds", []):
		if foe_owner > 0:
			for rs in r.get("ships", []):
				if foes.has(int(rs.get("id", 0))) and rs.has("owner"):
					rs["owner"] = foe_owner
		for e in r.get("events", []):
			if e.get("kind", "") == "Fire" and wmap.has(str(e.get("weapon", ""))):
				e["weapon"] = wmap[str(e.weapon)]
	return enc


## The job a close-up films, in the view's timeline (empty when the round has none).
func _find_job(tl: BattleTimeline) -> Dictionary:
	var me := _player_owner()
	if shot == "photon_salvo":
		# The sandbox's core fires one torpedo per mount per round, so a salvo on screen is a side's torpedoes on one
		# target: the largest such group (the player's side when it has one of 2 or more), filmed from beside its
		# shooters. A spread of n tubes from one ship (n > 1 in the core's Fire event) is in the group as n torpedoes.
		var groups := {}
		for j in tl.jobs:
			if j.kind == Job.TORPEDO:
				var key := "%d:%d" % [_owner(j.fleet_id), j.target_fleet_id]
				if not groups.has(key):
					groups[key] = []
				groups[key].append(j)
		var best_key := ""
		for key in groups:
			var mine: bool = int(key.split(":")[0]) == me
			var size: int = groups[key].size()
			var cur: int = groups[best_key].size() if best_key != "" else 0
			var cur_mine := best_key != "" and int(best_key.split(":")[0]) == me
			if (mine and not cur_mine and size >= 2) or (mine == cur_mine and size > cur) or best_key == "":
				best_key = key
		if best_key == "" or groups[best_key].size() < 6:
			return {}
		var g: Array = groups[best_key]
		var best: Dictionary = g[0].duplicate()
		best["salvo"] = g.size()
		best["salvo_fleets"] = g.map(func(x): return x.fleet_id)
		best["salvo_end"] = g.map(func(x): return x.start + x.duration).max()
		return best
	if shot == "phaser_hull" or shot == "photon_impact":
		# The clearest example: the player's shot first, then the one whose target has the fewest beams on it at once.
		var best := {}
		var best_score := INF
		for j in tl.jobs:
			var ok := false
			if shot == "phaser_hull":
				ok = j.kind == Job.BEAM and str(j.weapon).begins_with("Phaser") and j.outcome == BattleTimeline.Outcome.HIT and j.shield_damage <= 0 and j.hull_damage > 0
			else:
				ok = j.kind in [Job.SHIELD, Job.HULL] and int(j.get("family", 0)) == 2
			if not ok:
				continue
			var crowd := 0
			for o in tl.jobs:
				if o.kind == Job.BEAM and o.target_fleet_id == j.target_fleet_id and o.start < j.start + j.duration and o.start + o.duration > j.start:
					crowd += 1
			var src_fleet: int = j.fleet_id if shot == "phaser_hull" else j.source_fleet_id
			var dies := 0.0
			for o in tl.jobs:
				if o.kind == Job.EXPLOSION and o.fleet_id == j.target_fleet_id:
					dies = 50.0
			var score := float(crowd) + dies + (0.0 if _owner(src_fleet) == me else 100.0)
			if score < best_score:
				best_score = score
				best = j
		if shot == "photon_impact" and best_score >= 50.0 and fmod(best_score, 100.0) >= 50.0 and not _last_round:
			return {} # every torpedo hit this round is on a ship that dies: its kill explosion hides the impact
		return best
	if shot == "disruptor":
		# The closest Klingon burst (shooter to target), so the bolts and the Bird-of-Prey both fill the frame.
		var best := {}
		var best_d := INF
		for j in tl.jobs:
			if j.kind == Job.PULSE and j.index == 0 and j.count >= 2 and _owner(j.fleet_id) != me:
				var a: BattleShip = view.ship_node(j.fleet_id)
				var b: BattleShip = view.ship_node(j.target_fleet_id)
				if a != null and b != null:
					var d := a.global_position.distance_to(b.global_position)
					if d < best_d:
						best_d = d
						best = j
		return best
	for j in tl.jobs:
		var w := str(j.weapon)
		var phaser := w.begins_with("Phaser")
		match shot:
			"phaser_shield":
				if j.kind == Job.BEAM and phaser and j.outcome == BattleTimeline.Outcome.HIT and j.shield_damage > 0 and j.duration > 0.8:
					return j
			"phaser_hull":
				if j.kind == Job.BEAM and phaser and j.outcome == BattleTimeline.Outcome.HIT and j.shield_damage <= 0 and j.hull_damage > 0:
					return j
			"beam_any":
				if j.kind == Job.BEAM and _owner(j.fleet_id) != me and j.outcome == BattleTimeline.Outcome.HIT:
					return j
			"photon_impact":
				if j.kind in [Job.SHIELD, Job.HULL] and int(j.get("family", 0)) == 2:
					return j
			"disruptor":
				if j.kind == Job.PULSE and j.index == 0 and j.count >= 2 and _owner(j.fleet_id) != me:
					return j
	return {}


func _owner(fid: int) -> int:
	var s: BattleShip = view.ship_node(fid)
	return s.owner_id if s != null else 0


func _player_owner() -> int:
	for s in view.encounter.get("ships", []):
		if s.get("mine", false):
			return int(s.get("owner", 0))
	return 1


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
	root.add_child(view)
	await process_frame
	view.load_encounter(_dress(enc))
	for i in 60:
		await process_frame
	var picked := -1
	var job := {}
	var best_fires := -1
	for r in max_rounds:
		if enc.get("finished", false):
			break
		_call("tacticalOrder", {"sandbox": true, "all": true, "targetFleetId": 0, "maneuver": "Engage"})
		enc = _call("tacticalRound", {"sandbox": true})
		var idx: int = enc.rounds.size() - 1
		view.load_encounter(_dress(enc))
		var ok := false
		if OS.get_environment("WFX_DEBUG") != "":
			print("DEBUG round ", idx + 1, " jobs=", view.timeline.jobs.size() if view.timeline != null else -1, " finished=", enc.get("finished", false))
		if shot == "wide":
			ok = r >= 1 and _fires(enc.rounds[idx]) >= ships
		elif r >= 1: # round 1 is the decloak and opening torpedoes; the exchange starts in round 2
			_last_round = r == max_rounds - 1 or enc.get("finished", false)
			job = _find_job(view.timeline)
			ok = not job.is_empty()
		if ok or r == max_rounds - 1:
			picked = idx
			break
		view.playback_speed = 8.0
		while view.is_playing():
			await process_frame
		view.playback_speed = 1.0
	if picked < 0 or (shot != "wide" and job.is_empty()):
		printerr("no round has a shot for --shot=", shot)
		quit(1)
		return
	var tl: BattleTimeline = view.timeline
	var counts := {}
	for j in tl.jobs:
		var key := "%s:%s" % [BattleTimeline.JOB_NAMES[j.kind], j.weapon]
		counts[key] = counts.get(key, 0) + 1
	print("ROUND ", picked + 1, " jobs ", counts)
	var first := INF
	var last := 0.0
	for j in tl.jobs:
		if j.kind in [Job.BEAM, Job.PULSE, Job.TORPEDO]:
			first = minf(first, j.start)
			last = maxf(last, j.start + j.duration)
	if fps_mode:
		await _measure_fps(first, last)
		quit(0)
		return
	var t0 := maxf(0.2, minf(first, 0.6))
	var t1 := last
	if not job.is_empty():
		_cam_job = job
		_cam_on = true
		# A camera of our own for the close-up: the battle camera keeps filming but is no longer current. Set in the
		# frame's process_frame, before BattleFx sizes its screen floors against the current camera.
		_cam = Camera3D.new()
		_cam.fov = 40.0
		_cam.near = 0.5
		_cam.far = 20000.0
		root.add_child(_cam)
		_cam.current = true
		if "show_markers" in view.camera:
			view.camera.show_markers = false
		print("JOB kind=%s weapon=%s fleet=%d target=%d start=%.2f dur=%.2f shield=%d hull=%d count=%d" % [
			BattleTimeline.JOB_NAMES[job.kind], job.weapon, job.fleet_id, job.target_fleet_id, job.start, job.duration,
			job.shield_damage, job.hull_damage, job.count])
		match shot:
			"phaser_shield", "phaser_hull", "beam_any":
				t0 = job.start - 0.3
				t1 = job.start + job.duration * 0.9
			"photon_salvo":
				t0 = job.start + 0.3
				t1 = float(job.salvo_end) - 0.1
			"photon_impact":
				t0 = job.start - 0.15
				t1 = job.start + job.duration
			"disruptor":
				t0 = job.start
				t1 = job.start + 0.12 * job.count + job.duration
	if job.is_empty():
		t0 = _wide_start(tl, first, last)
	else:
		# Centre the 0.25 s strip on the filmed span.
		t0 = maxf(0.0, lerpf(t0, t1, 0.5) - 0.125 * float(frames - 1))
		if shot in ["phaser_shield", "phaser_hull", "beam_any", "disruptor", "photon_impact"]:
			t0 = maxf(0.0, float(job.start) - 0.3)
	print("STRIP t0=%.2f step=0.25 frames=%d" % [t0, frames])
	var k := 0
	while k < frames:
		var at := t0 + 0.25 * float(k)
		while view.time < at and view.is_playing():
			_place_camera()
			await process_frame
		_place_camera()
		await RenderingServer.frame_post_draw
		if out_dir != "":
			var img: Image = root.get_viewport().get_texture().get_image()
			img.save_png(out_dir.path_join("f_%02d.png" % k))
		print("FRAME %02d t=%.2f live=%s" % [k, view.time, _live()])
		if OS.has_feature("web"):
			view.playback_speed = 0.0
			await create_timer(1.5).timeout
			view.playback_speed = 1.0
		k += 1
		if not view.is_playing():
			break
	quit(0)


## Wide strip start: the window of frames (0.25 s apart) where phasers, torpedoes and disruptor bolts are all live
## in the most frames, ties broken by the most fire on screen.
func _wide_start(tl: BattleTimeline, first: float, last: float) -> float:
	var best := first
	var best_score := -1.0
	var t := maxf(0.0, first - 0.2)
	while t < last:
		var score := 0.0
		var seen := {}
		for k in frames:
			var at := t + 0.25 * float(k)
			var ph := 0
			var tp := 0
			var di := 0
			for j in tl.jobs:
				if at < j.start or at > j.start + j.duration:
					continue
				var w := str(j.weapon)
				if j.kind == Job.TORPEDO:
					tp += 1
				elif j.kind in [Job.BEAM, Job.PULSE] and w.begins_with("Phaser"):
					ph += 1
				elif j.kind in [Job.BEAM, Job.PULSE] and w.begins_with("Disruptor"):
					di += 1
			# The core's resolution order fires beams and bolts before torpedoes, so all three rarely share a frame:
			# score a frame that has all three highest, then a window whose frames cover all three.
			score += (100.0 if ph > 0 and tp > 0 and di > 0 else 0.0) + 0.01 * float(mini(ph, 12) + mini(tp, 12) + mini(di, 12))
			if ph > 2: seen["ph"] = seen.get("ph", 0) + 1
			if tp > 2: seen["tp"] = seen.get("tp", 0) + 1
			if di > 2: seen["di"] = seen.get("di", 0) + 1
		for key in seen:
			score += 10.0 + minf(float(seen[key]), 3.0)
		if score > best_score:
			best_score = score
			best = t
		t += 0.1
	return best


## Frame rate over the round's fire at 1x: average, 1% low (the 99th percentile frame time) and the worst frame.
func _measure_fps(first: float, last: float) -> void:
	while view.time < first and view.is_playing():
		await process_frame
	var times: Array[float] = []
	var prev := Time.get_ticks_usec()
	var live_max := 0
	while view.time < last and view.is_playing():
		await process_frame
		var now := Time.get_ticks_usec()
		times.append(float(now - prev) / 1000.0)
		prev = now
		live_max = maxi(live_max, view.fx._active.size())
	if times.size() < 10:
		print("FPS too few frames")
		return
	var total := 0.0
	for x in times:
		total += x
	var sorted := times.duplicate()
	sorted.sort()
	var p99: float = sorted[int(sorted.size() * 0.99) - 1]
	print("FPS frames=%d avg=%.1f fps 1%%low=%.1f fps (p99 %.2f ms) worst=%.2f ms max_live_effects=%d renderer=%s" % [
		times.size(), 1000.0 * times.size() / total, 1000.0 / p99, p99, sorted[-1], live_max,
		RenderingServer.get_current_rendering_method()])


## Close-up camera for the filmed job: set before the frame draws (and before BattleFx sizes its screen floors on the
## next frame), so it overrides the battle camera.
func _place_camera() -> void:
	if not _cam_on or view == null:
		return
	var cam := _cam
	if cam == null:
		return
	var j := _cam_job
	var shooter: BattleShip = view.ship_node(j.fleet_id if j.kind not in [Job.SHIELD, Job.HULL] else j.source_fleet_id)
	var target: BattleShip = view.ship_node(j.target_fleet_id)
	if target == null:
		return
	var tp := target.global_position
	var sp := shooter.global_position if shooter != null else tp + Vector3(40, 0, 0)
	if j.has("salvo_fleets"):
		# Beside the shooters' centre, looking along the salvo toward the target.
		var c := Vector3.ZERO
		var n := 0
		for fid in j.salvo_fleets:
			var s: BattleShip = view.ship_node(fid)
			if s != null:
				c += s.global_position
				n += 1
		if n > 0:
			sp = c / n
	var d := sp - tp
	var dist := d.length()
	var dir := d / maxf(dist, 1e-3)
	var side := dir.cross(Vector3.UP).normalized()
	var eye: Vector3
	var look: Vector3
	match shot:
		"phaser_shield", "phaser_hull", "beam_any", "photon_impact":
			# On the target, from the side, with the incoming fire crossing the frame toward it.
			var r := maxf(target.radius(), 4.0)
			eye = tp + (dir * 0.55 + side * 0.85 + Vector3.UP * 0.3).normalized() * r * 4.2
			look = tp + dir * r * 0.9
		"disruptor":
			# Behind and beside the Klingon shooter: its bolts leave the frame toward the target.
			var r := maxf(shooter.radius() if shooter != null else 4.0, 4.0)
			# Side-on to the line of fire, framing the shooter and the first half of the bolts' path.
			look = sp.lerp(tp, 0.35)
			eye = look + side * maxf(dist * 0.45, r * 5.0) + Vector3.UP * dist * 0.12 + dir * dist * 0.1
		_:
			# The volley side-on from near the shooter: it leaves the frame toward the target.
			look = sp.lerp(tp, 0.4)
			eye = look + side * dist * 0.42 + Vector3.UP * dist * 0.14
	if _cam_eye == Vector3.ZERO:
		_cam_eye = eye
		_cam_look = look
	_cam_eye = _cam_eye.lerp(eye, 0.2)
	_cam_look = _cam_look.lerp(look, 0.2)
	cam.look_at_from_position(_cam_eye, _cam_look, Vector3.UP)


func _live() -> String:
	var n := {}
	for j in view.timeline.jobs:
		if view.time >= j.start and view.time <= j.start + j.duration:
			var name: String = BattleTimeline.JOB_NAMES[j.kind]
			n[name] = n.get(name, 0) + 1
	return str(n)
