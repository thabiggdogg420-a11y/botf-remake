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

	print("%d passed, %d failures" % [passed, failures])
	quit(1 if failures > 0 else 0)


func _build(encounter: Dictionary, r: int) -> BattleTimeline:
	var info := BattleTimeline.round_ship_info(encounter, r)
	var cfg := BattleTimeline.Config.new()
	cfg.player_empire = 1
	return BattleTimeline.build(encounter.rounds[r].events, info, cfg)
