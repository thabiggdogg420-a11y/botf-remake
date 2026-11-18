extends RefCounted
## Round playback checks shared by pc_smoke and playback_capture (M1 battleplay; Simon, 9 Nov 2026: "all ships seem to
## fire their weapons at the same time and only seem to move between turns"). check_round() measures one round's
## cinematic timeline against the core's round record and returns {"stats": {...}, "problems": [String]}:
##   - shots spread over the timeline: not all launch in the first 10% of it, and not all at one moment;
##   - torpedoes have travel time (every torpedo that strikes flies at least MIN_TORPEDO_FLIGHT seconds);
##   - ships move during playback: a ship the core moved a cell is between its start and end, or still under way, mid-round, and some
##     shots leave a ship that is still flying;
##   - the playback's final state is the core's: each surviving ship ends on the core's end cell, the hull damage
##     shown by the end of the round is hullBefore - hullAfter, and a ship is gone at the end iff the core lost it.

## M4 flight: how far (cells) a flown ship may settle from the core's end cell (stage 3: up to ~1.2 after a 2.8-cell move).
const FLOWN_END_CELLS := 1.5

const MIN_TORPEDO_FLIGHT := 1.0
const SHOTS := [BattleTimeline.Job.BEAM, BattleTimeline.Job.PULSE, BattleTimeline.Job.TORPEDO]


static func check_round(tl: BattleTimeline, rnd: Dictionary) -> Dictionary:
	var problems: Array[String] = []
	var end := tl.end_seconds
	var starts := []
	var volleys := {}
	var torps := []
	for j in tl.jobs:
		if j.kind in SHOTS:
			starts.append(j.start)
			if j.index == 0:
				volleys[snappedf(j.start, 0.01)] = true
		if j.kind == BattleTimeline.Job.TORPEDO and j.outcome == BattleTimeline.Outcome.HIT:
			torps.append(j.duration)
	starts.sort()
	var early := starts.filter(func(x): return x <= 0.1 * end).size()
	if starts.size() >= 2 and early == starts.size():
		problems.append("every shot launches in the first 10%% of the round (%d)" % early)
	if starts.size() >= 2 and volleys.size() < 2:
		problems.append("all %d shots launch at one moment" % starts.size())
	for d in torps:
		if d < MIN_TORPEDO_FLIGHT:
			problems.append("a torpedo strikes after only %.2f s" % d)
	# Motion.
	var by_id := {}
	for rs in rnd.get("ships", []):
		by_id[int(rs.get("id", 0))] = rs
	var moved := 0
	var mid_ok := 0
	for s in tl.ships:
		var a := tl.pose_of(s, 0.0)
		var b := tl.pose_of(s, tl.round_seconds)
		# Moved means the core moved it a cell this round (a segment whose cells differ). Since fleets deploy inside beam range
		# (Simon, 18 Nov 2026) many ships hold their cell all round, and their flight settles a few hundredths of a cell off
		# where it started: that drift is not a core move and has no "between" to be in.
		var core_moved := false
		for seg in s.segments:
			if absf(seg.to.x - seg.from.x) + absf(seg.to.y - seg.from.y) > 0.5:
				core_moved = true
				break
		if not core_moved:
			continue
		moved += 1
		var m := tl.pose_of(s, 0.5 * tl.round_seconds)
		# A ship still under way mid-round passes too: at short range an attack run can back off and cross its own start
		# point at the half (24v24 round 3, ship 18: 0.010 cells from its start at 4.0 s, flying 0.1 cells/s).
		var m0 := tl.pose_of(s, 0.45 * tl.round_seconds)
		var m1 := tl.pose_of(s, 0.55 * tl.round_seconds)
		var under_way := Vector2(m1.x - m0.x, m1.y - m0.y).length() > 0.01
		if under_way or (Vector2(m.x - a.x, m.y - a.y).length() > 0.01 and Vector2(b.x - m.x, b.y - m.y).length() > 0.01):
			mid_ok += 1
		else:
			problems.append("ship %d is not between its start and end mid-round" % s.fleet_id)
	var moving_shots := 0
	for j in tl.jobs:
		if j.kind in SHOTS and j.index == 0:
			var s := tl.track(j.fleet_id)
			if s != null:
				var p0 := tl.pose_of(s, j.start - 0.05)
				var p1 := tl.pose_of(s, j.start + 0.05)
				if Vector2(p1.x - p0.x, p1.y - p0.y).length() > 1e-4:
					moving_shots += 1
	if moved > 0 and starts.size() >= 2 and moving_shots == 0:
		problems.append("no shot leaves a moving ship")
	# Final state against the core.
	var lost := {}
	for id in rnd.get("lost", []):
		lost[int(id)] = true
	var retreated := {}
	for id in rnd.get("retreatedIds", []):
		retreated[int(id)] = true
	var final_ok := 0
	for s in tl.ships:
		var rs: Dictionary = by_id.get(s.fleet_id, {})
		if rs.is_empty():
			continue
		var gone := s.hide_at >= 0.0 and s.hide_at <= end + 1e-6
		var core_gone := lost.has(s.fleet_id) or retreated.has(s.fleet_id) or int(rs.get("hullAfter", 1)) <= 0
		if gone != core_gone:
			problems.append("ship %d: playback gone=%s, core lost/retreated=%s" % [s.fleet_id, gone, core_gone])
			continue
		var shown := tl.damage_until(s.fleet_id, end)
		var hull_loss := int(rs.get("hullBefore", 0)) - int(rs.get("hullAfter", 0))
		if shown.x != hull_loss and not core_gone:
			problems.append("ship %d: hull shown lost %d, core %d -> %d" % [s.fleet_id, shown.x, int(rs.get("hullBefore", 0)), int(rs.get("hullAfter", 0))])
			continue
		if not core_gone and rs.has("to"):
			var p := tl.pose_of(s, end)
			var to: Dictionary = rs.to
			# M4 flight: a flown ship (the flight sim's path) settles near its cell, not on it, and starts the next round
			# from there; the plan view keeps the core's cell. Within FLOWN_END_CELLS of it is the core's pose as drawn.
			var off := Vector2(p.x - float(to.x), p.y - float(to.y)).length()
			var flown := not s.path.is_empty()
			if (flown and off > FLOWN_END_CELLS) or (not flown and not (is_equal_approx(p.x, float(to.x)) and is_equal_approx(p.y, float(to.y)))):
				problems.append("ship %d ends at (%.2f, %.2f), core (%s, %s)" % [s.fleet_id, p.x, p.y, to.x, to.y])
				continue
		final_ok += 1
	return {"problems": problems, "stats": {"end": end, "shots": starts.size(), "first10pct": early,
		"spread": [starts[0] if not starts.is_empty() else -1.0, starts[-1] if not starts.is_empty() else -1.0],
		"volleys": volleys.size(), "torpedo_hits": torps.size(), "torp_flight": torps.slice(0, 6).map(func(x): return snappedf(x, 0.01)),
		"moved": moved, "mid_between": mid_ok, "shots_while_moving": moving_shots, "final_ok": final_ok, "ships": tl.ships.size()}}
