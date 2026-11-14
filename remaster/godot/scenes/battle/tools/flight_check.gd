extends SceneTree
## Flight proof (M4 combat): builds the cinematic timeline of each round of a combat-sandbox battle and samples every ship
## at 30 Hz. Reports, per class, the top speed (cells a second and metres a second at 592 m a cell), the top turn rate, the
## largest frame-to-frame jump of speed (a teleport or a jitter shows here), and whether every ship starts and ends the
## round on the core's pose. Prints the intent the planner gave each ship.
##   godot --headless --path remaster/godot --audio-driver Dummy -s res://scenes/battle/tools/flight_check.gd -- [--ships=24] [--seed=0] [--rounds=5]

var ships := 24
var seed := 0
var rounds := 5
var use_flight := false
var core: Object
var flight := BattleFlight.new()


func _initialize() -> void:
	for a in OS.get_cmdline_user_args():
		if a.begins_with("--ships="): ships = int(a.split("=")[1])
		elif a.begins_with("--seed="): seed = int(a.split("=")[1])
		elif a.begins_with("--rounds="): rounds = int(a.split("=")[1])
		elif a == "--flight": use_flight = true
	_run.call_deferred()


func _call(method: String, args := {}) -> Variant:
	var reply: Variant = core.request(method, args)
	return reply.get("result", reply) if reply is Dictionary and reply.has("result") else reply


func _run() -> void:
	core = Engine.get_singleton("BotfCore")
	var enc: Variant = _call("combatSandbox", {"ships": ships, "seed": seed})
	var worst := {}
	var bad_ends := 0
	var checked := 0
	var prev_tl: BattleTimeline = null
	for r in rounds:
		if enc.get("finished", false):
			break
		_call("tacticalOrder", {"sandbox": true, "all": true, "targetFleetId": 0, "maneuver": "Engage"})
		enc = _call("tacticalRound", {"sandbox": true})
		var idx: int = enc.rounds.size() - 1
		var info := BattleTimeline.round_ship_info(enc, idx)
		var cfg := BattleTimeline.Config.new()
		cfg.cinematic = true
		cfg.round_seconds = 8.0
		if use_flight:
			cfg.flight = flight
			cfg.encounter = enc
			if prev_tl != null:
				flight.commit(prev_tl.end_seconds + 0.25)
		var t0 := Time.get_ticks_usec()
		var tl := BattleTimeline.build(enc.rounds[idx].events, info, cfg)
		var build_ms := float(Time.get_ticks_usec() - t0) / 1000.0
		prev_tl = tl
		if use_flight:
			print("FLIGHT round ", idx + 1, " ", flight.stats)
		var intents := {}
		for s in tl.ships:
			var it := str(s.get("intent")) if s.get("intent") != null and str(s.get("intent")) != "" else "wing/none"
			intents[it] = int(intents.get(it, 0)) + 1
		print("ROUND ", idx + 1, " ships ", tl.ships.size(), " build ", "%.1f ms" % build_ms, " intents ", intents)
		# Fire mix (stage 3): torpedoes drawn, beams held at once, shot starts per second over the round's fire span.
		var torps := 0
		var salvos := {}
		var starts: Array = []
		var beam_iv: Array = []
		var ev_seen := {}
		for j in tl.jobs:
			match int(j.kind):
				BattleTimeline.Job.TORPEDO:
					torps += 1
					salvos["%d" % int(j.fleet_id)] = int(salvos.get("%d" % int(j.fleet_id), 0)) + 1
					if not ev_seen.has(j.event_index):
						ev_seen[j.event_index] = true
						starts.append(float(j.start))
				BattleTimeline.Job.BEAM:
					beam_iv.append([float(j.start), float(j.start) + float(j.duration)])
					if not ev_seen.has(j.event_index):
						ev_seen[j.event_index] = true
						starts.append(float(j.start))
				BattleTimeline.Job.PULSE:
					if not ev_seen.has(j.event_index):
						ev_seen[j.event_index] = true
						starts.append(float(j.start))
		starts.sort()
		var span := (float(starts.back()) - float(starts[0])) if starts.size() > 1 else 1.0
		var bmax := 0
		var bsum := 0.0
		var samples := 0
		var tq := float(starts[0]) if not starts.is_empty() else 0.0
		while not starts.is_empty() and tq <= float(starts.back()):
			var c := 0
			for iv in beam_iv:
				if tq >= float(iv[0]) and tq < float(iv[1]):
					c += 1
			bmax = maxi(bmax, c)
			bsum += c
			samples += 1
			tq += 0.1
		var most := 0
		for k in salvos:
			most = maxi(most, int(salvos[k]))
		print("FIRE round ", idx + 1, " torpedoes ", torps, " ships_firing_torps ", salvos.size(), " max_torps_one_ship ", most,
			" shot_starts_per_s ", "%.2f" % (starts.size() / maxf(span, 0.5)), " beams_mean ", "%.2f" % (bsum / maxf(1.0, samples)), " beams_max ", bmax)
		for s in tl.ships:
			var cls: int = s.style if s.style >= 0 else -1
			if cls == 2 and s.length_meters > 0.0 and s.length_meters <= 120.0:
				cls = 3
			var name: String = ["capital", "cruiser", "escort", "strike", "wingman"][cls if cls >= 0 else 4]
			var prev: BattleTimeline.Pose = null
			var prev_speed := -1.0
			var top := 0.0
			var top_turn := 0.0
			var top_jump := 0.0
			var top_at := 0.0
			var top_turn_at := 0.0
			var t := 0.0
			var end := s.hide_at if s.hide_at >= 0.0 else tl.round_seconds
			while t <= minf(end, tl.round_seconds):
				var p := tl.pose_of(s, t)
				if prev != null:
					var v := Vector3(p.x - prev.x, p.y - prev.y, p.altitude - prev.altitude).length() * 30.0
					if v > top:
						top = v
						top_at = t
					top_turn_at = t if absf(BattleTimeline.shortest_turn(prev.facing, p.facing)) * 30.0 > top_turn else top_turn_at
					top_turn = maxf(top_turn, absf(BattleTimeline.shortest_turn(prev.facing, p.facing)) * 30.0)
					if prev_speed >= 0.0:
						top_jump = maxf(top_jump, absf(v - prev_speed) * 30.0)
					prev_speed = v
				prev = p
				t += 1.0 / 30.0
			if top > 1.6 * float(BattleTimeline.CLASS_SPEED[maxi(cls, 0) if cls >= 0 else 3]) or top_turn > 200.0:
				print("OFFENDER id=", s.fleet_id, " ", name, " intent=", s.get("intent"), " speed ", "%.2f" % top, " at ", "%.2f" % top_at, " turn ", "%.0f" % top_turn, " at ", "%.2f" % top_turn_at, " kill_at ", s.kill_at, " hide_at ", s.hide_at, " wing ", s.wing_leader, " segs ", s.segments.size())
			if OS.get_environment("FLIGHT_TRACE") != "" and r == int(OS.get_environment("FLIGHT_TRACE")):
				var pts := []
				var tt := 0.0
				while tt <= tl.round_seconds:
					var q := tl.pose_of(s, tt)
					pts.append("%.2f,%.2f,%.0f" % [q.x, q.y, q.facing])
					tt += 0.25
				print("TRACE ", s.fleet_id, " ", s.owner_id, " ", name, " ", s.get("intent"), " ", ";".join(pts))
			var w: Dictionary = worst.get(name, {"speed": 0.0, "turn": 0.0, "accel": 0.0})
			w.speed = maxf(w.speed, top)
			w.turn = maxf(w.turn, top_turn)
			w.accel = maxf(w.accel, top_jump)
			worst[name] = w
			var p0 := tl.pose_of(s, 0.0)
			var pe := tl.pose_of(s, tl.end_seconds)
			var c0 := s.pose_at(0.0)
			var ce := s.pose_at(tl.round_seconds)
			if s.hide_at < 0.0:
				checked += 1
				if absf(p0.x - c0.x) > 1e-3 or absf(p0.y - c0.y) > 1e-3 or absf(pe.x - ce.x) > 1e-3 or absf(pe.y - ce.y) > 1e-3 \
						or absf(BattleTimeline.shortest_turn(pe.facing, ce.facing)) > 0.5:
					bad_ends += 1
	for k in worst:
		var w: Dictionary = worst[k]
		print("CLASS %-8s top speed %.2f cells/s (%.0f m/s)  top turn %.0f deg/s  top accel step %.2f cells/s2" % [k, w.speed, w.speed * BattleTimeline.METERS_PER_CELL, w.turn, w.accel])
	print("ENDS bad ", bad_ends, " of ", checked)
	quit(0)
