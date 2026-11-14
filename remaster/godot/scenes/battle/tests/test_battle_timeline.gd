extends SceneTree
## Headless tests for the rules 3 battle timeline.
##   godot --headless --path remaster/godot -s res://scenes/battle/tests/test_battle_timeline.gd

const FIXTURE := "res://scenes/battle/fixtures/rules3_battle.json"

var failures := 0
var passed := 0


func check(cond: bool, what: String) -> void:
	if cond:
		return
	failures += 1
	printerr("  FAIL  ", what)


func run(name: String, fn: Callable) -> void:
	var before := failures
	# A script error aborts the lambda and returns null: count that as a failure.
	var finished = fn.call()
	if finished != true:
		failures += 1
		printerr("  FAIL  ", name, " aborted")
	if failures == before:
		passed += 1
		print("  ok  ", name)


func _init() -> void:
	print("Battle timeline tests")
	var encounter: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(FIXTURE))
	check(encounter != null and encounter.get("rules") == 3.0, "fixture is a rules 3 encounter")
	var rounds: Array = encounter.rounds

	run("every event of every round is consumed", func():
		for r in rounds.size():
			var tl := _build(encounter, r)
			check(tl.all_consumed(), "round %d consumed all events: %s" % [r, tl.consumed])
			check(tl.consumed.size() == rounds[r].events.size(), "consumed size round %d" % r)
		return true)

	run("building is deterministic", func():
		for r in rounds.size():
			var a := _build(encounter, r)
			var b := _build(encounter, r)
			check(var_to_str(a.jobs) == var_to_str(b.jobs), "jobs equal round %d" % r)
			check(var_to_str(a.sounds) == var_to_str(b.sounds), "sounds equal round %d" % r)
			check(var_to_str(a.barks) == var_to_str(b.barks), "barks equal round %d" % r)
			for t in 60:
				for s in a.ships:
					check(s.pose_at(t * 0.1).equals(b.track(s.fleet_id).pose_at(t * 0.1)), "pose equal")
		return true)

	run("no pose jumps: ships move smoothly and stay at rest at segment ends", func():
		for r in rounds.size():
			var tl := _build(encounter, r)
			var dt := 1.0 / 240.0
			for s in tl.ships:
				var prev: BattleTimeline.Pose = s.pose_at(0.0)
				var t := dt
				while t <= tl.end_seconds:
					var p: BattleTimeline.Pose = s.pose_at(t)
					var step := Vector2(p.x - prev.x, p.y - prev.y).length()
					check(step < 0.1, "round %d ship %d moved %.3f cells in one frame at %.2f" % [r, s.fleet_id, step, t])
					check(absf(BattleTimeline.shortest_turn(prev.facing, p.facing)) < 4.0, "round %d ship %d facing jump" % [r, s.fleet_id])
					check(absf(p.bank - prev.bank) < 2.0, "round %d ship %d bank jump" % [r, s.fleet_id])
					prev = p
					t += dt
				for seg in s.segments:
					check(seg.t1 > seg.t0, "segment has length")
					check(absf(s.pose_at(seg.t1).bank) < 1e-6, "bank returns to zero")
				check(absf(s.pose_at(tl.end_seconds).bank) < 1e-6, "ends level")
		return true)

	run("rounds join up: a ship ends a round where the next begins", func():
		for r in rounds.size() - 1:
			var a := _build(encounter, r)
			var b := _build(encounter, r + 1)
			for s in b.ships:
				var prev := a.track(s.fleet_id)
				if prev == null or prev.hide_at >= 0.0:
					continue
				var pa: BattleTimeline.Pose = prev.pose_at(a.end_seconds)
				var pb: BattleTimeline.Pose = s.pose_at(0.0)
				check(Vector2(pa.x - pb.x, pa.y - pb.y).length() < 1e-6, "round %d->%d ship %d position" % [r, r + 1, s.fleet_id])
				check(absf(BattleTimeline.shortest_turn(pa.facing, pb.facing)) < 1e-6, "round %d->%d ship %d facing %.1f vs %.1f" % [r, r + 1, s.fleet_id, pa.facing, pb.facing])
		return true)

	run("jobs are sorted and weapon keys follow weapon.<Type>[.fac]", func():
		var fire_seen := false
		for r in rounds.size():
			var tl := _build(encounter, r)
			for i in range(1, tl.jobs.size()):
				check(tl.jobs[i - 1].start <= tl.jobs[i].start, "jobs sorted")
			for i in range(1, tl.sounds.size()):
				check(tl.sounds[i - 1].time <= tl.sounds[i].time, "sounds sorted")
			for snd in tl.sounds:
				if snd.cue == "BeamFire" or snd.cue == "TorpedoLaunch":
					fire_seen = true
					check(snd.keys[0].begins_with("weapon."), "weapon key %s" % snd.keys[0])
					check(snd.keys[snd.keys.size() - 1].begins_with("weapon.generic."), "generic fallback")
		check(fire_seen, "fixture fires weapons")
		return true)

	run("destroyed ships explode and vanish, cloakers fade", func():
		var explosions := 0
		var cloaks := 0
		for r in rounds.size():
			var tl := _build(encounter, r)
			for j in tl.jobs:
				if j.kind == BattleTimeline.Job.EXPLOSION:
					explosions += 1
					var s := tl.track(j.fleet_id)
					check(s.hide_at > j.start and s.visibility_at(s.hide_at + 0.01) == 0.0, "hidden after explosion")
					check(s.visibility_at(j.start) > 0.0 or s.starts_cloaked, "visible when it blows")
				if j.kind == BattleTimeline.Job.CLOAK and not tl.track(j.fleet_id).starts_cloaked:
					cloaks += 1
					check(tl.track(j.fleet_id).visibility_at(j.start + j.duration + 0.01) < 0.01, "cloaked after fade")
		check(explosions > 0, "fixture has explosions")
		return true)

	run("torpedo paths: hits end at the target, misses overshoot", func():
		var p := BattleTimeline.torpedo_point(Vector2.ZERO, Vector2(4, 0), 0.35, 0.0, 1.0)
		check(Vector2(p.x, p.y).distance_to(Vector2(4, 0)) < 1e-6 and absf(p.z) < 1e-6, "hit lands")
		var m := BattleTimeline.torpedo_point(Vector2.ZERO, Vector2(4, 0), 0.35, 2.5, 1.0)
		check(absf(m.x - 6.5) < 1e-6, "miss overshoots")
		var mid := BattleTimeline.torpedo_point(Vector2.ZERO, Vector2(4, 0), 0.35, 0.0, 0.5)
		check(absf(mid.y - 0.35) < 1e-6 and absf(mid.z - 0.35) < 1e-6, "arc bows")
		return true)

	run("synthetic events: slide banks, intercept, retreat, barks", func():
		var info := [{"fleetId": 1, "ownerId": 1, "shields": 100, "maxShields": 100, "x": 0, "y": 0, "facing": 0},
			{"fleetId": 2, "ownerId": 2, "shields": 100, "maxShields": 100, "x": 5, "y": 0, "facing": 180}]
		var ev := [
			{"tick": 0, "kind": "ManeuverStart", "fleetId": 1, "x": 0, "y": 0, "facing": 0, "maneuver": "Strafe"},
			{"tick": 0, "kind": "Retreated", "fleetId": 2, "x": 5, "y": 0, "facing": 180},
			{"tick": 4, "kind": "ManeuverEnd", "fleetId": 1, "x": 0, "y": 2, "facing": 0, "maneuver": "Strafe", "value": 2},
			{"tick": 5, "kind": "Fire", "fleetId": 2, "targetFleetId": 1, "mountIndex": 0, "weapon": "Disruptor", "family": "beam", "projectiles": 2, "hit": true, "impactTick": 5, "shieldDamage": 60, "hullDamage": 0},
			{"tick": 6, "kind": "Fire", "fleetId": 1, "targetFleetId": 2, "mountIndex": 1, "weapon": "PhotonTorpedo", "family": "torpedo", "projectiles": 3, "hit": true, "intercepted": true, "impactTick": 8}]
		var cfg := BattleTimeline.Config.new()
		cfg.player_empire = 1
		var tl := BattleTimeline.build(ev, info, cfg)
		check(tl.all_consumed(), "all consumed")
		var s := tl.track(1)
		check(s.segments.size() == 1 and s.segments[0].bank_peak != 0.0, "slide banks")
		check(absf(s.pose_at(1.0).facing) < 1e-6, "strafe keeps bow")
		var kinds := tl.jobs.map(func(j): return j.kind)
		check(kinds.count(BattleTimeline.Job.BEAM) == 2, "two beam emitters")
		check(kinds.count(BattleTimeline.Job.TORPEDO) == 3 and kinds.has(BattleTimeline.Job.INTERCEPT), "intercepted volley")
		check(kinds.has(BattleTimeline.Job.WARP) and tl.track(2).hide_at > 0.0, "warp out hides")
		check(tl.barks.any(func(b): return b.detail == "torpedo"), "torpedo bark")
		check(tl.barks.any(func(b): return b.detail == "shields.50"), "shield bark")
		check(BattleTimeline.bark_keys("shields.50", 1) == PackedStringArray(["vo.combat.shields.50.fed", "vo.combat.shields.fed"]), "bark keys")
		return true)

	run("director holds shots and cuts to explosions", func():
		for r in rounds.size():
			var tl := _build(encounter, r)
			var d := BattleTimeline.Director.new()
			var last_change := -10.0
			var t := 0.0
			while t < tl.end_seconds:
				var before := d.subject
				if d.update(tl, t):
					var urgent := false
					for j in tl.jobs:
						if j.kind == BattleTimeline.Job.EXPLOSION and j.start <= t and t <= j.start + j.duration and j.fleet_id == d.subject:
							urgent = true
					check(before == 0 or urgent or t - last_change >= d.min_shot_seconds - 1e-6, "shot held at %.2f" % t)
					last_change = t
				t += 1.0 / 30.0
		return true)

	# ---- 3D flight and true scale (ports of the matching client-logic-tests.cpp checks) ----

	run("unit_hash matches the C++ unitHash bits", func():
		# Reference values printed by BotfCombatTimeline.h unitHash.
		for ref in [[1, 1, 0.2298816442489624], [7, 1, 0.6673729419708252], [10, 101, 0.41475707292556763],
				[2000001, 7, 0.5847245454788208], [123456, 102, 0.69641011953353882], [99, 0, 0.42720115184783936]]:
			check(absf(BattleTimeline.unit_hash(ref[0], ref[1]) - ref[2]) < 1e-12, "unit_hash(%d, %d)" % [ref[0], ref[1]])
		return true)

	run("true scale: displayLengthForLength and the catalogue", func():
		check(absf(BattleShip.display_length_ue(350) - 120.0) < 1e-9, "350 m = 120 units")
		check(absf(BattleShip.display_length_ue(642) / BattleShip.display_length_ue(170) - 642.0 / 170.0) < 1e-9, "relative sizes kept")
		check(BattleShip.display_length_ue(15) == BattleShip.display_length_ue(60), "60 m floor")
		check(absf(BattleShip.display_length_ue(100000) - 1000.0) < 1e-9, "1000-unit cap")
		check(absf(BattleShip.display_length(642) / BattleScene.CELL - 642.0 * 120.0 / 350.0 / 170.0) < 1e-9, "Galaxy spans 1.3 cells")
		var ships := {}
		for s in encounter.ships:
			ships[int(s.id)] = s
		check(ShipFactory.canon_length_meters(ships[7]) == 642.0, "Galaxy 642 m")
		check(ShipFactory.canon_length_meters(ships[3]) == 110.0, "B'rel 110 m (apostrophes dropped from the key)")
		check(ShipFactory.canon_length_meters(ships[9]) == 228.0, "K't'inga 228 m")
		check(ShipFactory.canon_length_meters(ships[2000001]) == 0.0, "stations have no class length")
		check(ShipFactory.canon_length_meters({"id": 4, "owner": 1, "assetKey": "ship.fed.defiant"}) == 171.0, "roster assetKey first")
		check(ShipFactory.canon_length_meters({"id": 4, "owner": 9, "name": "Nobody"}) == 0.0, "unknown class")
		check(BattleShip.length_for(ships[7]) > BattleShip.length_for(ships[5]) * 2.5, "a Galaxy is 2.75 Mirandas long")
		var info := BattleTimeline.round_ship_info(encounter, 0)
		check(info.all(func(i): return i.has("lengthMeters")), "round info carries lengthMeters")
		return true)

	run("3D flight: home heights are deterministic, sides sit in layers, small craft spread more", func():
		var cfg := BattleTimeline.Config.new()
		check(BattleTimeline.home_height(7, 110, true) == BattleTimeline.home_height(7, 110, true), "deterministic")
		var cap_max := 0.0
		var small_max := 0.0
		var first_sum := 0.0
		var other_sum := 0.0
		for id in range(1, 401):
			cap_max = maxf(cap_max, absf(BattleTimeline.home_height(id, 650, true) - cfg.side_height_offset))
			small_max = maxf(small_max, absf(BattleTimeline.home_height(id, 110, true) - cfg.side_height_offset))
			first_sum += BattleTimeline.home_height(id, 650, true)
			other_sum += BattleTimeline.home_height(id, 650, false)
		check(cap_max <= cfg.capital_height_spread + 1e-9, "capital ships stay near the plane")
		check(small_max > 1.0, "small craft spread wider")
		check(first_sum > other_sum, "the first side flies above the other")
		# Fixture: both sides hold their own layer.
		var tl := _build(encounter, 0)
		var first_owner := int(BattleTimeline.round_ship_info(encounter, 0)[0].ownerId)
		var alt := {true: 0.0, false: 0.0}
		var n := {true: 0, false: 0}
		for s in tl.ships:
			alt[s.owner_id == first_owner] += s.initial.altitude
			n[s.owner_id == first_owner] += 1
		check(alt[true] / n[true] > alt[false] / n[false], "fixture layers")
		return true)

	run("3D flight: climbs pitch the nose, escorts bank harder, heights change smoothly", func():
		var cfg := BattleTimeline.Config.new()
		var ev := _flight_events()
		var small := BattleTimeline.build(ev, _flight_info(110.0), cfg)
		var s1 := small.track(1)
		check(s1.small_craft and s1.segments.size() == 1, "escort with one segment")
		var seg: BattleTimeline.Segment = s1.segments[0]
		check(s1.pose_at(0.0).altitude == seg.from.altitude and s1.pose_at(6.0).altitude == seg.to.altitude, "heights at the ends")
		check(s1.pose_at(6.0).pitch == 0.0 and absf(seg.pitch_peak) <= cfg.max_pitch_degrees, "pitch levels out")
		if absf(seg.to.altitude - seg.from.altitude) > 0.05:
			check((seg.pitch_peak > 0.0) == (seg.to.altitude > seg.from.altitude), "climbing noses up")
		check(s1.pose_at(0.0).plane_equals(BattleTimeline.Pose.new(0, 0, 0, 0)), "plane pose at start")
		check(s1.pose_at(2.0).plane_equals(BattleTimeline.Pose.new(2, 1, 45, 0)), "plane pose at the end")
		var max_climb := 0.0
		var t := 0.001
		while t <= 6.0:
			max_climb = maxf(max_climb, absf(s1.pose_at(t).altitude - s1.pose_at(t - 0.001).altitude))
			t += 0.001
		check(max_climb < 0.02, "smooth climb (%.4f)" % max_climb)
		var big := BattleTimeline.build(ev, _flight_info(650.0), cfg)
		check(absf(small.track(1).segments[0].bank_peak) > absf(big.track(1).segments[0].bank_peak), "escorts roll harder")
		var bseg: BattleTimeline.Segment = big.track(1).segments[0]
		check(bseg.from.altitude == bseg.to.altitude and bseg.pitch_peak == 0.0, "capital ships hold altitude through a maneuver")
		check(absf(bseg.bank_peak) > 0.0, "capital ships still bank into the turn")
		check(not big.track(1).small_craft and big.track(1).wing_leader == 0, "capital ships fly alone")
		check(absf(small.track(1).segments[0].bank_peak) <= cfg.max_bank_degrees * cfg.small_bank_scale, "bank limit")
		return true)

	run("wings: finger-four on the lowest id, wingmen peel off when the leader dies", func():
		var cfg := BattleTimeline.Config.new()
		var fleet := []
		for s in [[10, 1, 2, 5, 110], [11, 1, 2, 6, 120], [12, 1, 3, 4, 90], [13, 1, 1, 5, 150], [14, 1, 2, 7, 100],
				[15, 1, 12, 2, 110], [16, 2, 3, 5, 110], [17, 1, 2, 4, 650]]:
			fleet.append({"fleetId": s[0], "ownerId": s[1], "x": s[2], "y": s[3], "facing": 0, "lengthMeters": s[4], "maxShields": 10})
		var w := BattleTimeline.build([{"tick": 6, "kind": "Destroyed", "fleetId": 10, "x": 2, "y": 5, "facing": 0}], fleet, cfg)
		check(w.track(10).wing_leader == 0 and w.track(11).wing_leader == 10 and w.track(12).wing_leader == 10 \
			and w.track(13).wing_leader == 10, "wing on 10")
		check(w.track(14).wing_leader == 0, "the wing is full (4)")
		check(w.track(15).wing_leader == 0 and w.track(16).wing_leader == 0 and w.track(17).wing_leader == 0,
			"far-off, other side and capital fly alone")
		var lead := w.track(10).pose_at(0.5)
		var mate := w.pose_of(w.track(11), 0.5)
		check(absf(mate.x - (lead.x - cfg.wing_spacing_cells)) < 1e-9 and absf(mate.y - (lead.y + cfg.wing_spacing_cells)) < 1e-9,
			"station aft and to port")
		check(mate.altitude > lead.altitude and mate.facing == lead.facing, "station above, same heading")
		check(w.track(10).hide_at > 0.0, "leader destroyed")
		var after := w.track(10).hide_at + 3.0 * w.tick_seconds
		check(w.pose_of(w.track(11), after).plane_equals(w.track(11).pose_at(after)), "wingman back on its own track")
		# Peeling off is smooth.
		var prev := w.pose_of(w.track(11), 0.0)
		var t := 1.0 / 240.0
		while t <= w.end_seconds:
			var p := w.pose_of(w.track(11), t)
			check(Vector3(p.x - prev.x, p.y - prev.y, p.altitude - prev.altitude).length() < 0.1, "peel-off step at %.2f" % t)
			prev = p
			t += 1.0 / 240.0
		return true)

	run("spacing: overlapping hulls are pushed apart, mostly in height", func():
		var clear := BattleTimeline.spacing_offsets([_body(0, 0, 0, 0.5), _body(3, 0, 0, 0.5)])
		check(clear[0] == Vector3.ZERO and clear[1] == Vector3.ZERO, "clear hulls stay put")
		var level := [_body(0, 0, 0, 1.2), _body(1, 0, 0, 1.2)]
		var lv := BattleTimeline.spacing_offsets(level)
		check(_gap(level, lv, 0, 1) > -1e-6, "level pair separated")
		check(absf(lv[0].z) > 0.2 and absf(lv[1].z) > 0.2 and lv[0].z * lv[1].z < 0.0, "they stack")
		var same := [_body(2, 2, 0, 0.8), _body(2, 2, 0, 0.8)]
		var sm := BattleTimeline.spacing_offsets(same)
		check(_gap(same, sm, 0, 1) > -1e-6 and sm[0].x == 0.0 and sm[0].y == 0.0, "coincident hulls split vertically")
		var again := BattleTimeline.spacing_offsets(same)
		check(again[0].z == sm[0].z and again[1].z == sm[1].z, "deterministic")
		var mixed := [_body(0, 0, 0, 1.4), _body(0.5, 0.3, 0.1, 0.3)]
		var mx := BattleTimeline.spacing_offsets(mixed)
		check(_gap(mixed, mx, 0, 1) > -1e-6 and mx[1].length() > 2.0 * mx[0].length(), "the escort moves further")
		var crowd := []
		for i in 8:
			crowd.append(_body(i % 2, 0.2 * (i / 2), 0.1 * i, 1.0))
		var idle := _body(0, 0, 0, 1.0)
		idle.active = false
		crowd.append(idle)
		var cr := BattleTimeline.spacing_offsets(crowd, 40)
		var worst := 0.0
		for i in 8:
			for j in range(i + 1, 8):
				worst = minf(worst, _gap(crowd, cr, i, j))
		check(worst > -0.15, "eight capitals settle apart (%.3f)" % worst)
		check(cr[8] == Vector3.ZERO, "inactive hull ignored")
		return true)

	run("station-keeping: still at maneuver and round edges, steady while holding", func():
		var hold := BattleTimeline.ShipTrack.new()
		hold.fleet_id = 21
		var seg := BattleTimeline.Segment.new()
		seg.t0 = 0.0
		seg.t1 = 2.0
		hold.segments.append(seg)
		var escort := BattleTimeline.ShipTrack.new()
		escort.fleet_id = 21
		escort.small_craft = true
		escort.segments.append(seg)
		var mag := func(o: Vector3) -> float: return absf(o.x) + absf(o.y) + absf(o.z)
		check(mag.call(BattleTimeline.station_keeping(hold, 1.0, 6.0)) == 0.0 and mag.call(BattleTimeline.station_keeping(hold, 2.0, 6.0)) == 0.0, "none while flying")
		check(mag.call(BattleTimeline.station_keeping(hold, 6.0, 6.0)) == 0.0 and mag.call(BattleTimeline.station_keeping(hold, 0.0, 6.0)) == 0.0, "none at the round edges")
		check(mag.call(BattleTimeline.station_keeping(hold, 2.0 + 1e-4, 6.0)) < 1e-3 and mag.call(BattleTimeline.station_keeping(hold, 6.0 - 1e-4, 6.0)) < 1e-3, "fades in and out")
		# Simon, 8 Nov 2026 ("bob up and down like boats"): holding position keeps heading, bank, pitch and altitude;
		# only a barely visible altitude drift remains, the same for every hull size.
		var alt_peak := 0.0
		var turn_peak := 0.0
		var t := 2.0
		while t < 6.0:
			for tr in [hold, escort]:
				var o: Vector3 = BattleTimeline.station_keeping(tr, t, 6.0)
				alt_peak = maxf(alt_peak, absf(o.x))
				turn_peak = maxf(turn_peak, absf(o.y) + absf(o.z))
			t += 0.05
		check(turn_peak == 0.0, "no roll or pitch while holding (%.3f)" % turn_peak)
		check(alt_peak > 0.0 and alt_peak <= BattleTimeline.IDLE_DRIFT_CELLS + 1e-9 and alt_peak * BattleScene.CELL < 0.5, "drift barely visible (%.3f world units)" % (alt_peak * BattleScene.CELL))
		return true)

	run("no pose jumps in 3D: wings, heights, pitch and station-keeping (fixture)", func():
		for r in rounds.size():
			var tl := _build(encounter, r)
			var dt := 1.0 / 240.0
			for s in tl.ships:
				var prev := _shown(tl, s, 0.0)
				var t := dt
				while t <= tl.end_seconds:
					var p := _shown(tl, s, t)
					var step := Vector3(p.x - prev.x, p.y - prev.y, p.altitude - prev.altitude).length()
					check(step < 0.1, "round %d ship %d moved %.3f cells at %.2f" % [r, s.fleet_id, step, t])
					check(absf(BattleTimeline.shortest_turn(prev.facing, p.facing)) < 4.0, "round %d ship %d facing jump" % [r, s.fleet_id])
					check(absf(p.bank - prev.bank) < 2.0 and absf(p.pitch - prev.pitch) < 2.0, "round %d ship %d bank/pitch jump" % [r, s.fleet_id])
					prev = p
					t += dt
				check(absf(s.pose_at(tl.end_seconds).pitch) < 1e-6, "ends level")
		return true)

	# --- Cinematic playback (M1 battleplay; Simon, 9 Nov 2026) -------------------------------------------------------
	run("cinematic: consumed, deterministic, smooth, rounds join", func():
		for r in rounds.size():
			var a := _build_cine(encounter, r)
			var b := _build_cine(encounter, r)
			check(a.all_consumed(), "cinematic round %d consumed all events" % r)
			check(var_to_str(a.jobs) == var_to_str(b.jobs), "cinematic jobs equal round %d" % r)
			for s in a.ships:
				var prev: BattleTimeline.Pose = s.pose_at(0.0)
				var t := 1.0 / 240.0
				while t <= a.end_seconds:
					var p: BattleTimeline.Pose = s.pose_at(t)
					check(Vector2(p.x - prev.x, p.y - prev.y).length() < 0.1, "cinematic round %d ship %d jump at %.2f" % [r, s.fleet_id, t])
					prev = p
					t += 1.0 / 240.0
		for r in rounds.size() - 1:
			var a := _build_cine(encounter, r)
			var b := _build_cine(encounter, r + 1)
			for s in b.ships:
				var prev := a.track(s.fleet_id)
				if prev == null or prev.hide_at >= 0.0:
					continue
				var pa: BattleTimeline.Pose = prev.pose_at(a.end_seconds)
				var pb: BattleTimeline.Pose = s.pose_at(0.0)
				check(Vector2(pa.x - pb.x, pa.y - pb.y).length() < 1e-6, "cinematic round %d->%d ship %d joins" % [r, r + 1, s.fleet_id])
		return true)

	run("cinematic: shots spread over the round, not all at the start", func():
		var rounds_with_fire := 0
		for r in rounds.size():
			var tl := _build_cine(encounter, r)
			var starts := []
			var fires := {}
			for j in tl.jobs:
				if j.kind in [BattleTimeline.Job.BEAM, BattleTimeline.Job.PULSE, BattleTimeline.Job.TORPEDO]:
					starts.append(j.start)
					fires[j.event_index] = true
			if fires.size() < 2:
				continue
			rounds_with_fire += 1
			var early := starts.filter(func(x): return x <= 0.1 * tl.end_seconds).size()
			check(early < starts.size(), "round %d: not every shot in the first 10%% (%d of %d)" % [r, early, starts.size()])
			var launch := {}
			for j in tl.jobs:
				if j.kind in [BattleTimeline.Job.BEAM, BattleTimeline.Job.PULSE, BattleTimeline.Job.TORPEDO] and j.index == 0:
					launch[snappedf(j.start, 0.001)] = true
			check(launch.size() >= mini(fires.size(), 2), "round %d: %d shots launch at %d distinct times" % [r, fires.size(), launch.size()])
			starts.sort()
			check(starts[-1] - starts[0] >= 0.1 * tl.round_seconds, "round %d: fire spans %.2f s" % [r, starts[-1] - starts[0]])
		check(rounds_with_fire > 0, "fixture has rounds with fire")
		return true)

	run("cinematic: ships fly the whole round and fire on the move", func():
		var moving_shots := 0
		for r in rounds.size():
			var tl := _build_cine(encounter, r)
			for s in tl.ships:
				for seg in s.segments:
					if Vector2(seg.to.x - seg.from.x, seg.to.y - seg.from.y).length() > 0.0:
						check(seg.t1 >= 0.9 * tl.round_seconds, "round %d ship %d flies to %.2f of %.2f" % [r, s.fleet_id, seg.t1, tl.round_seconds])
						var mid := s.pose_at(0.5 * (seg.t0 + seg.t1))
						check(not mid.plane_equals(seg.from) and not mid.plane_equals(seg.to), "round %d ship %d is between poses mid-flight" % [r, s.fleet_id])
			for j in tl.jobs:
				if j.kind in [BattleTimeline.Job.BEAM, BattleTimeline.Job.PULSE, BattleTimeline.Job.TORPEDO]:
					var s := tl.track(j.fleet_id)
					var a := s.pose_at(j.start - 0.05)
					var b := s.pose_at(j.start + 0.05)
					if Vector2(b.x - a.x, b.y - a.y).length() > 1e-5:
						moving_shots += 1
		check(moving_shots > 0, "some shots leave a ship that is still flying (%d)" % moving_shots)
		return true)

	run("cinematic: torpedoes travel, then strike", func():
		var torps := 0
		for r in rounds.size():
			var tl := _build_cine(encounter, r)
			var cfg := BattleTimeline.Config.new()
			for j in tl.jobs:
				if j.kind == BattleTimeline.Job.TORPEDO:
					torps += 1
					if j.outcome == BattleTimeline.Outcome.HIT:
						check(j.duration >= cfg.min_torpedo_flight_seconds - 1e-6, "torpedo flies %.2f s" % j.duration)
						var landed := false
						for imp in tl.impact_log:
							if imp.event_index == j.event_index:
								landed = true
								check(imp.time >= j.start + j.duration - 1e-6, "impact after the torpedo arrives")
								check(imp.time > j.start + 1.0, "impact %.2f s after launch" % (imp.time - j.start))
						check(landed, "a torpedo hit lands")
		check(torps > 0, "fixture fires torpedoes")
		var ev := _flight_events() + [
			{"tick": 6, "kind": "Fire", "fleetId": 1, "targetFleetId": 2, "mountIndex": 1, "weapon": "QuantumTorpedo", "family": "torpedo", "projectiles": 3, "hit": true, "impactTick": 8, "shieldDamage": 30, "hullDamage": 0}]
		var c := BattleTimeline.Config.new()
		c.cinematic = true
		c.round_seconds = 8.0
		var tl2 := BattleTimeline.build(ev, _flight_info(400.0), c)
		var tubes := tl2.jobs.filter(func(j): return j.kind == BattleTimeline.Job.TORPEDO)
		check(tubes.size() >= 4 and tubes.size() <= 5, "a cruiser's salvo is 4-5 tubes (%d)" % tubes.size())
		if tubes.size() >= 3:
			for k in range(1, tubes.size()):
				var gap: float = tubes[k].start - tubes[k - 1].start
				check(gap >= 0.1 - 1e-6 and gap <= 0.15 + 1e-6, "tubes launch 0.10-0.15 s apart (%.3f)" % gap)
		return true)

	run("M4 combat: shields flare briefly, fire opens within 0.5 s, Dominion hulls resolve by race", func():
		var ev := _flight_events() + [
			{"tick": 5, "kind": "Fire", "fleetId": 1, "targetFleetId": 2, "mountIndex": 0, "weapon": "PhaserArray", "family": "beam", "projectiles": 1, "hit": true, "impactTick": 5, "shieldDamage": 30, "hullDamage": 0},
			{"tick": 6, "kind": "Fire", "fleetId": 2, "targetFleetId": 1, "mountIndex": 0, "weapon": "Disruptor", "family": "beam", "projectiles": 1, "hit": true, "impactTick": 6, "shieldDamage": 20, "hullDamage": 0}]
		var c := BattleTimeline.Config.new()
		c.cinematic = true
		c.round_seconds = 8.0
		var tl := BattleTimeline.build(ev, _flight_info(400.0), c)
		var shields := tl.jobs.filter(func(j): return j.kind == BattleTimeline.Job.SHIELD)
		check(not shields.is_empty(), "the round has shield impacts")
		for j in shields:
			check(j.duration <= 0.45 + 1e-9, "a shield flare is brief (%.2f s)" % j.duration)
		var first := INF
		for j in tl.jobs:
			if j.kind in [BattleTimeline.Job.BEAM, BattleTimeline.Job.PULSE, BattleTimeline.Job.TORPEDO]:
				first = minf(first, float(j.start))
		check(first <= 0.5, "fire opens within 0.5 s (%.2f)" % first)
		check(BattleTimeline.faction_prefix(6) == "dom" and BattleTimeline.faction_prefix(7) == "borg" and BattleTimeline.faction_prefix(8) == "breen", "Dominion, Borg and Breen prefixes")
		var keys := BattleShip.asset_keys_for({"owner": 6, "name": "Attack Ship — Jem'Hadar"})
		check(keys.has("ship.dom.attack_ship") and keys.has("study.dom"), "a Dominion ship resolves to ship.dom.<class>: %s" % [keys])
		return true)

	run("M4 combat 2: rules 1-2 rounds (no event stream) get presentation events from the round result", func():
		var rnd := {"index": 3, "events": [], "salvos": [
			{"from": 1, "to": 3, "beam": 20, "torpedo": 0, "ram": false, "absorbed": false, "ambush": false},
			{"from": 2, "to": 3, "beam": 0, "torpedo": 18, "ram": false, "absorbed": true, "ambush": false},
			{"from": 3, "to": 1, "beam": 12, "torpedo": 0, "ram": false, "absorbed": false, "ambush": false}],
			"ships": [
			{"id": 1, "owner": 1, "maneuver": "Engage", "target": 3, "from": {"x": 2, "y": 2}, "to": {"x": 4, "y": 2}, "yaw": 0, "hullBefore": 50, "hullAfter": 44, "shieldBefore": 40, "shieldAfter": 30, "destroyed": false},
			{"id": 2, "owner": 1, "maneuver": "Engage", "target": 3, "from": {"x": 2, "y": 4}, "to": {"x": 4, "y": 4}, "yaw": 0, "hullBefore": 50, "hullAfter": 50, "shieldBefore": 40, "shieldAfter": 40, "destroyed": false},
			{"id": 3, "owner": 2, "maneuver": "Engage", "target": 1, "from": {"x": 9, "y": 3}, "to": {"x": 7, "y": 3}, "yaw": 180, "hullBefore": 30, "hullAfter": 0, "shieldBefore": 20, "shieldAfter": 0, "destroyed": true}]}
		check(BattleSynth.needs_events(rnd), "a round with moves and no events needs them")
		var enc := {"rounds": [rnd], "ships": []}
		var out := BattleSynth.with_events(enc)
		check(rnd.events.is_empty(), "the original encounter is not touched")
		var ev: Array = out.rounds[0].events
		check(ev.any(func(e): return e.kind == "ManeuverStart") and ev.any(func(e): return e.kind == "ManeuverEnd"), "moves become maneuver events")
		var fires := ev.filter(func(e): return e.kind == "Fire")
		check(fires.size() >= 3, "each salvo becomes at least one shot (%d)" % fires.size())
		var hull_on_3 := 0
		var shield_on_3 := 0
		for f in fires:
			if int(f.targetFleetId) == 3:
				hull_on_3 += int(f.hullDamage)
				shield_on_3 += int(f.shieldDamage)
		check(absi(hull_on_3 - 30) <= 2 and absi(shield_on_3 - 20) <= 2, "damage on the target sums to what it lost (hull %d shields %d)" % [hull_on_3, shield_on_3])
		check(ev.any(func(e): return e.kind == "Destroyed" and int(e.fleetId) == 3), "a destroyed ship gets a Destroyed event")
		check(fires.any(func(e): return e.family == "torpedo") and fires.any(func(e): return e.family == "beam"), "beams and torpedoes")
		var info := BattleTimeline.round_ship_info(out, 0)
		var cfg := BattleTimeline.Config.new()
		cfg.cinematic = true
		cfg.round_seconds = 8.0
		var tl := BattleTimeline.build(ev, info, cfg)
		check(tl.all_consumed(), "the timeline consumes every synthesised event")
		check(tl.jobs.any(func(j): return j.kind == BattleTimeline.Job.BEAM) and tl.jobs.any(func(j): return j.kind == BattleTimeline.Job.TORPEDO), "the round plays beams and a torpedo salvo")
		check(not BattleSynth.needs_events(out.rounds[0]), "a round that has events is left alone")
		return true)

	run("M4 combat: class speeds, exact hand-over to the core's poses, beams held 0.6-1.5 s", func():
		var ships_seen := 0
		for r in rounds.size():
			var tl := _build_cine(encounter, r)
			for s in tl.ships:
				if s.hide_at >= 0.0 or s.kill_at >= 0.0:
					continue
				ships_seen += 1
				var a := tl.pose_of(s, 0.0)
				var c0 := s.pose_at(0.0)
				check(absf(a.x - c0.x) < 1e-3 and absf(a.y - c0.y) < 1e-3, "starts on the core's cell")
				var b := tl.pose_of(s, tl.round_seconds)
				var ce := s.pose_at(tl.round_seconds)
				check(absf(b.x - ce.x) < 1e-3 and absf(b.y - ce.y) < 1e-3 and absf(BattleTimeline.shortest_turn(b.facing, ce.facing)) < 0.5, "ends on the core's cell and facing")
				var prev := a
				var top := 0.0
				var t := 0.0
				while t < tl.round_seconds:
					t += 1.0 / 30.0
					var p := tl.pose_of(s, t)
					top = maxf(top, Vector3(p.x - prev.x, p.y - prev.y, p.altitude - prev.altitude).length() * 30.0)
					prev = p
				var limit: float = [1.0, 1.3, 2.0][maxi(0, mini(2, s.style))] if s.style >= 0 else 2.0
				check(top <= limit, "ship %d (style %d) top speed %.2f cells/s is scale-correct (limit %.1f; was 4-16)" % [s.fleet_id, s.style, top, limit])
			for j in tl.jobs:
				if j.kind == BattleTimeline.Job.BEAM:
					check(j.duration >= 0.6 - 1e-6 and j.duration <= 1.5 + 1e-6, "beam held %.2f s" % j.duration)
		check(ships_seen > 0, "ships checked")
		return true)

	run("M4 combat: escorts fly attack runs, an order changes the flight, a crippled hull breaks off", func():
		var ev := _flight_events() + [
			{"tick": 6, "kind": "Fire", "fleetId": 1, "targetFleetId": 2, "mountIndex": 0, "weapon": "PhaserArray", "family": "beam", "projectiles": 1, "hit": true, "impactTick": 6, "shieldDamage": 30, "hullDamage": 0}]
		var c := BattleTimeline.Config.new()
		c.cinematic = true
		c.round_seconds = 8.0
		var tl := BattleTimeline.build(ev, _flight_info(150.0), c)
		check(tl.track(1).style == 2 and tl.track(1).path.size() > 0, "an escort gets a flown path")
		check(tl.track(1).intent == "attack run", "intent: %s" % tl.track(1).intent)
		var hurt := _flight_info(150.0)
		hurt[0]["hull"] = 5
		hurt[0]["maxHull"] = 100
		var tl2 := BattleTimeline.build(ev, hurt, c)
		check(tl2.track(1).intent == "break off", "a crippled escort breaks off: %s" % tl2.track(1).intent)
		var info3 := _flight_info(700.0)
		var tl3 := BattleTimeline.build(ev, info3, c)
		check(tl3.track(1).style == 0 and tl3.track(1).path.size() == 0, "a capital ship keeps the core's path with a slow drift")
		check(BattleTimeline.shows_as_bolts("Disruptor", 2, 150.0) and not BattleTimeline.shows_as_bolts("Disruptor", 2, 700.0), "Klingon escorts fire bolts, capitals beams")
		return true)

	run("cinematic: impacts keep the core's order per ship, kills come last, final state is the core's", func():
		for r in rounds.size():
			var tl := _build_cine(encounter, r)
			var evs: Array = rounds[r].events
			_check_order(tl, evs, "round %d" % r)
			# Every ship ends on its last ManeuverEnd pose; destroyed / retreated ships are gone at the end.
			for s in tl.ships:
				var last := {}
				var gone := false
				for e in evs:
					if int(e.get("fleetId", 0)) == s.fleet_id and e.kind == "ManeuverEnd":
						last = e
					if int(e.get("fleetId", 0)) == s.fleet_id and e.kind in ["Destroyed", "Retreated"]:
						gone = true
				if not last.is_empty():
					var p := tl.pose_of(s, tl.end_seconds) # as drawn: wingmen too end on their own cells
					check(is_equal_approx(p.x, float(last.x)) and is_equal_approx(p.y, float(last.y)) and absf(BattleTimeline.shortest_turn(p.facing, float(last.facing))) < 1e-4, "round %d ship %d ends on the core's pose" % [r, s.fleet_id])
				check((s.visibility_at(tl.end_seconds + 0.01) == 0.0 and s.hide_at >= 0.0) == gone or s.starts_cloaked, "round %d ship %d gone at the end iff the core says so" % [r, s.fleet_id])
			# Damage shown over the round adds up to the core's per-event figures.
			var want := {}
			for e in evs:
				if (e.kind == "Fire" and e.get("hit", false) and not e.get("intercepted", false)) or e.kind == "Ram":
					var tgt := int(e.targetFleetId)
					want[tgt] = want.get(tgt, Vector2i.ZERO) + Vector2i(int(e.get("hullDamage", 0)), int(e.get("shieldDamage", 0)))
			for tgt in want:
				check(tl.damage_until(tgt, tl.end_seconds) == want[tgt], "round %d ship %d shown damage %s == core %s" % [r, tgt, tl.damage_until(tgt, tl.end_seconds), want[tgt]])
		# resolveOrder (rules 4 bridge) wins over the fallback order: two torpedoes arriving on one tick from different
		# fire ticks, the later-fired one resolved first; and a ship that dies on tick 5 still fires its tick-5 beam first.
		var ev := _flight_events() + [
			{"tick": 5, "kind": "Fire", "fleetId": 1, "targetFleetId": 2, "mountIndex": 0, "weapon": "PhotonTorpedo", "family": "torpedo", "projectiles": 1, "hit": true, "impactTick": 9, "shieldDamage": 10, "hullDamage": 0, "resolveOrder": 3},
			{"tick": 6, "kind": "Fire", "fleetId": 1, "targetFleetId": 2, "mountIndex": 1, "weapon": "PhotonTorpedo", "family": "torpedo", "projectiles": 1, "hit": true, "impactTick": 9, "shieldDamage": 5, "hullDamage": 0, "resolveOrder": 2},
			{"tick": 5, "kind": "Fire", "fleetId": 2, "targetFleetId": 1, "mountIndex": 0, "weapon": "Disruptor", "family": "beam", "projectiles": 1, "hit": true, "impactTick": 5, "shieldDamage": 4, "hullDamage": 0, "resolveOrder": 1},
			{"tick": 5, "kind": "Fire", "fleetId": 1, "targetFleetId": 2, "mountIndex": 2, "weapon": "PhaserArray", "family": "beam", "projectiles": 1, "hit": true, "impactTick": 5, "shieldDamage": 0, "hullDamage": 50, "destroyed": true, "resolveOrder": 0},
			{"tick": 5, "kind": "Destroyed", "fleetId": 2, "x": 5, "y": 0, "facing": 135}]
		var c := BattleTimeline.Config.new()
		c.cinematic = true
		c.round_seconds = 8.0
		var tl2 := BattleTimeline.build(ev, _flight_info(400.0), c)
		_check_order(tl2, ev, "synthetic")
		var t_a := -1.0
		var t_b := -1.0
		for imp in tl2.impact_log:
			if imp.event_index == 4: t_a = imp.time
			if imp.event_index == 5: t_b = imp.time
		check(t_b >= 0.0 and t_a > t_b, "resolveOrder: the tick-6 torpedo (rank 2) lands before the tick-5 one (rank 3)")
		# M4 combat (footage WotW 4805): a Klingon cruiser's disruptor is a red beam; its small craft fire bolts (below).
		var beam2 := tl2.jobs.filter(func(j): return j.kind == BattleTimeline.Job.BEAM and j.fleet_id == 2)
		check(not beam2.is_empty(), "a Klingon cruiser's disruptor shows as a beam")
		var tl2s := BattleTimeline.build(ev, _flight_info(150.0), c)
		var bolt := tl2s.jobs.filter(func(j): return j.kind == BattleTimeline.Job.PULSE and j.fleet_id == 2)
		check(not bolt.is_empty(), "a Klingon escort's disruptor shows as bolts")
		return true)

	print("%d passed, %d failures" % [passed, failures])
	quit(1 if failures > 0 else 0)


## Impacts on each ship land in the core's order (BattleTimeline.impact_key), and a ship destroyed this round launches
## all its shots before the blow that kills it, which is when it explodes.
func _check_order(tl: BattleTimeline, evs: Array, what: String) -> void:
	var by_target := {}
	for imp in tl.impact_log:
		if not by_target.has(imp.target):
			by_target[imp.target] = []
		by_target[imp.target].append(imp)
	for tgt in by_target:
		var list: Array = by_target[tgt]
		for k in range(1, list.size()):
			check(list[k - 1].time < list[k].time, "%s ship %d: impacts at distinct times" % [what, tgt])
			check(BattleTimeline.key_less(list[k - 1].key, list[k].key), "%s ship %d: impact %d before %d in the core's order" % [what, tgt, list[k - 1].event_index, list[k].event_index])
	for imp in tl.impact_log:
		if not imp.destroyed:
			continue
		var boom := tl.jobs.filter(func(j): return j.kind == BattleTimeline.Job.EXPLOSION and j.fleet_id == imp.target)
		check(boom.size() == 1 and is_equal_approx(boom[0].start, imp.time), "%s ship %d explodes at the killing blow" % [what, imp.target])
		for j in tl.jobs:
			if j.fleet_id == imp.target and j.kind in [BattleTimeline.Job.BEAM, BattleTimeline.Job.PULSE, BattleTimeline.Job.TORPEDO] and j.index == 0:
				check(j.start < imp.time, "%s ship %d fires (%.2f) before it dies (%.2f)" % [what, imp.target, j.start, imp.time])


func _build_cine(encounter: Dictionary, r: int) -> BattleTimeline:
	var info := BattleTimeline.round_ship_info(encounter, r)
	var cfg := BattleTimeline.Config.new()
	cfg.player_empire = 1
	cfg.cinematic = true
	cfg.round_seconds = 8.0
	return BattleTimeline.build(encounter.rounds[r].events, info, cfg)


## What the scene shows (before spacing): own flight or wing station plus station-keeping.
func _shown(tl: BattleTimeline, s: BattleTimeline.ShipTrack, t: float) -> BattleTimeline.Pose:
	var p := tl.pose_of(s, t)
	var idle := BattleTimeline.station_keeping(s, t, tl.round_seconds)
	p.altitude += idle.x
	p.bank += idle.y
	p.pitch += idle.z
	return p


## Ship 1 slides one cell while turning to 45 degrees; ship 2 turns from 180 to 135 (client-logic-tests' round).
func _flight_events() -> Array:
	return [
		{"tick": 0, "kind": "ManeuverStart", "fleetId": 1, "x": 0, "y": 0, "facing": 0, "maneuver": "Strafe"},
		{"tick": 0, "kind": "ManeuverStart", "fleetId": 2, "x": 6, "y": 0, "facing": 180, "maneuver": "Engage"},
		{"tick": 4, "kind": "ManeuverEnd", "fleetId": 1, "x": 2, "y": 1, "facing": 45, "maneuver": "Strafe", "value": 1},
		{"tick": 4, "kind": "ManeuverEnd", "fleetId": 2, "x": 5, "y": 0, "facing": 135, "maneuver": "Engage"}]


func _flight_info(length_meters: float) -> Array:
	return [{"fleetId": 1, "ownerId": 1, "x": 0, "y": 0, "facing": 0, "lengthMeters": length_meters, "maxShields": 10},
		{"fleetId": 2, "ownerId": 2, "x": 6, "y": 0, "facing": 180, "lengthMeters": length_meters, "maxShields": 10}]


func _body(x: float, y: float, alt: float, radius: float) -> Dictionary:
	return {"x": x, "y": y, "alt": alt, "radius": radius, "active": true}


func _gap(b: Array, o: Array, i: int, j: int) -> float:
	var d := Vector3(b[j].x + o[j].x - b[i].x - o[i].x, b[j].y + o[j].y - b[i].y - o[i].y,
		b[j].alt + o[j].z - b[i].alt - o[i].z)
	return d.length() - (b[i].radius + b[j].radius)


func _build(encounter: Dictionary, r: int) -> BattleTimeline:
	var info := BattleTimeline.round_ship_info(encounter, r)
	var cfg := BattleTimeline.Config.new()
	cfg.player_empire = 1
	return BattleTimeline.build(encounter.rounds[r].events, info, cfg)
