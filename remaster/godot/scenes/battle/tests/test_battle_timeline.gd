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

	run("station-keeping: still at maneuver and round edges, gentle while holding", func():
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
		var cap_peak := 0.0
		var small_peak := 0.0
		var t := 2.0
		while t < 6.0:
			cap_peak = maxf(cap_peak, absf(BattleTimeline.station_keeping(hold, t, 6.0).y))
			small_peak = maxf(small_peak, absf(BattleTimeline.station_keeping(escort, t, 6.0).y))
			t += 0.05
		check(cap_peak > 0.5 and cap_peak <= 2.4 + 1e-9 and small_peak > cap_peak, "small craft move more (%.2f / %.2f)" % [cap_peak, small_peak])
		check(absf(BattleTimeline.station_keeping(escort, 4.0, 6.0).x) <= 0.12 + 1e-9, "bob amplitude")
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

	print("%d passed, %d failures" % [passed, failures])
	quit(1 if failures > 0 else 0)


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
