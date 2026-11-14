class_name BattleFlight
extends RefCounted
## Continuous flight-and-doctrine sim for the battle screen (M4 flight; Simon, 14 Nov 2026: "Yeah go for it").
##
## Ported from the LCARS battle screensaver (AI_stuff\lcars-theme\screensaver\v5\Battle.cs, read-only): a pure CPU sim in
## metres and seconds. Ships have a velocity, an acceleration, a class turn rate and an up vector; they bank into turns.
## Presentation only. The core still decides every outcome; the sim delivers it:
##   * the core's round result is the constraint set: who fires on whom, with which mount, the damage, who dies, who cloaks
##     or warps out (BattleTimeline turns the events into jobs; this sim only picks WHEN each shot launches and WHERE every
##     ship is);
##   * the core's grid poses are the ships' posts. A ship of the line holds station on its post as it moves from the
##     round's start cell to its end cell (the screensaver's fleet slot round a moving anchor, Battle.Hold), turning in
##     place to bring the mount that must fire to bear (LineFace: broadside by wing, bow on for torpedoes). Escorts and
##     strike craft fly as squadrons (Battle.StrikeLeader): they loiter at their post, form an attack wave in a V
##     (LaunchWave, UpdateWaves), split onto their own targets, make a strafing run, break away and come about, and
##     regroup on their post before the round ends. Wingmen hold their station on the leader;
##   * a shot launches when the sim's geometry gives the shooter its target inside that mount's arc and range (FireWeapons),
##     on the ship's own cadence; a shot still unfired at its deadline launches anyway (the core says it happened);
##   * damaged ships under 30% hull (capitals 20%) withdraw: stern to the enemy (damage control); a ship that warps out
##     this round turns away and runs before it goes.
## The state carries from round to round (commit), so a battle is one continuous flight: no ship stops at a round edge.
## Deterministic for a given encounter (seeded from it), so a replay looks the same.

const M := 592.0          ## Metres a tactical cell (BattleTimeline.METERS_PER_CELL).
const DT := 0.1           ## Sim step and path sample (BattleTimeline.PATH_DT).
const TAIL := 5.5         ## Seconds simulated past the round (torpedoes and explosions play on; the path must cover them).
const SNAP := 12          ## Floats in a state snapshot: pos 3, fwd 3, vel 3, speed, roll, in_slot.
const SMALL_SPEED := 0.85 ## Small craft fly at this share of the screensaver's speeds (the arena is half its width).

## Flight classes (BattleTimeline.ship_style plus strike craft): 0 capital, 1 cruiser, 2 escort, 3 strike craft.
class Ship:
	var fid := 0
	var side := 0
	var cls := 0
	var length := 300.0
	var radius := 150.0
	var pos := Vector3.ZERO
	var fwd := Vector3(1, 0, 0)
	var vel := Vector3.ZERO
	var speed := 0.0
	var roll := 0.0
	var in_slot := false
	var max_speed := 150.0
	var accel := 30.0
	var turn_rate := 0.14
	var immobile := false
	var orbit_sign := 1.0
	var h := 0.0               ## per-ship hash (0..1)
	# Round inputs.
	var track = null           ## BattleTimeline.ShipTrack
	var slot0 := Vector3.ZERO  ## the core's start cell (metres)
	var slot1 := Vector3.ZERO  ## the core's end cell
	var face_end := Vector3(1, 0, 0)
	var hull_frac := 1.0
	var withdraw := false
	var warp_at := -1.0
	var dead_at := -1.0
	var victim := false
	var shots: Array = []      ## {ev, tgt, arc: Vector2, family, span, deadline, range, fired, ram}
	var next_fire := 0.0
	var gap := 0.6
	var wing_leader := 0
	var wing_off := Vector3.ZERO ## (aft, port, up) metres
	# Squadron (Battle.StrikeLeader): 0 rally, 1 run, 2 break, 3 in a wave.
	var sq := 0
	var sq_t := 0.0
	var target := 0
	var break_point := Vector3.ZERO
	var break_t := 0.0
	var passes := 0
	var wave = null
	var wave_slot := Vector3.ZERO
	var returning := false
	# Scratch: what Hold / the doctrine asked for when it handed over to the flight model.
	var des := Vector3(1, 0, 0)
	var want := 0.0
	var snaps := PackedFloat32Array()


class Wave:
	var side := 0
	var pos := Vector3.ZERO
	var fwd := Vector3(1, 0, 0)
	var goal := Vector3.ZERO
	var speed := 0.0
	var age := 0.0
	var members: Array = []


var ships := {}            ## fid -> Ship
var seed := 0
var stats := {}            ## the last round's diagnostics (tools print them)
var _order: Array = []     ## Ship list in fid order (determinism)
var _waves: Array = []
var _steps := 0
var _arena := Vector2(16, 12)
var _R := 8.0
var _A := 0.1
var _B := 7.6
var _Tm := 7.5
var _centroid := {}        ## side -> Vector3
var _enemy_c := {}         ## side -> centroid of every other side's ships
var _side_n := {}
## M4 flight stage 2 (show footage: a fleet advances as one body on one heading): side -> the fleet heading (radians in
## the sim's x-y plane), turned slowly toward the enemy's centre. Ships of the line face it unless a shot needs another
## bearing; escorts with no run hold station ahead of their post on it. _head_snaps: side -> heading per step (commit).
const HEAD_TURN := 0.06
var _heading := {}
var _head_snaps := {}
var _debug_fid := int(OS.get_environment("FLIGHT_DEBUG")) if OS.get_environment("FLIGHT_DEBUG") != "" else -1


func reset() -> void:
	ships.clear()
	_order.clear()
	_waves.clear()
	_heading.clear()
	_head_snaps.clear()
	_steps = 0


## Takes the state at round time t of the last planned round as where every ship starts the next one.
func commit(t: float) -> void:
	if _steps <= 0:
		return
	var k := clampi(int(round(t / DT)), 0, _steps - 1)
	for s: Ship in _order:
		var o := k * SNAP
		if s.snaps.size() < o + SNAP:
			continue
		var sn: PackedFloat32Array = s.snaps
		s.pos = Vector3(sn[o], sn[o + 1], sn[o + 2])
		s.fwd = Vector3(sn[o + 3], sn[o + 4], sn[o + 5]).normalized()
		s.vel = Vector3(sn[o + 6], sn[o + 7], sn[o + 8])
		s.speed = sn[o + 9]
		s.roll = sn[o + 10]
		s.in_slot = sn[o + 11] > 0.5
	for side in _head_snaps:
		var hs: PackedFloat32Array = _head_snaps[side]
		if k < hs.size():
			_heading[side] = hs[k]
	_steps = 0


static func encounter_seed(encounter: Dictionary) -> int:
	var ids := []
	for s in encounter.get("ships", []):
		ids.append(int(s.get("id", 0)))
	ids.sort()
	return hash([ids, encounter.get("sectorX", 0), encounter.get("sectorY", 0), encounter.get("turn", 0)]) & 0x7FFFFFFF


func _hash(a: int, b: int) -> float:
	return BattleTimeline.unit_hash(a ^ (seed & 0xFFFF), b + ((seed >> 16) & 0x7FFF))


# ------------------------------------------------------------------------------------------------------------------
# Round planning
# ------------------------------------------------------------------------------------------------------------------

## Flies the round: sets every known ship's path on the timeline and returns {event index: launch time} for every Fire
## and Ram event (BattleTimeline._schedule keeps the core's order per target from there).
func plan_round(tl: BattleTimeline, events: Array, info_by_id: Dictionary, cfg, encounter: Dictionary) -> Dictionary:
	if seed == 0:
		seed = encounter_seed(encounter)
	_R = tl.round_seconds
	_A = cfg.fire_from * _R
	_B = cfg.fire_to * _R
	_Tm = cfg.flight_to * _R
	_arena = Vector2(float(encounter.get("arenaWidth", 16)), float(encounter.get("arenaHeight", 12)))
	var enc_ships := {}
	for es in encounter.get("ships", []):
		enc_ships[int(es.get("id", 0))] = es
	# Ships: keep the ones still in the fight, add newcomers on their core pose.
	var present := {}
	for tr in tl.ships:
		if not info_by_id.has(tr.fleet_id):
			continue
		present[tr.fleet_id] = true
		var s: Ship = ships.get(tr.fleet_id)
		var p0: BattleTimeline.Pose = tr.initial
		if s == null:
			s = _new_ship(tr, info_by_id[tr.fleet_id], enc_ships.get(tr.fleet_id, {}))
			s.pos = Vector3(p0.x * M, p0.y * M, p0.altitude * M)
			var f := deg_to_rad(p0.facing)
			s.fwd = Vector3(cos(f), sin(f), 0.0)
			s.speed = s.max_speed * (0.25 if s.cls <= 1 else 0.5)
			s.vel = s.fwd * s.speed
			ships[tr.fleet_id] = s
		s.track = tr
		s.side = tr.owner_id
		var inf: Dictionary = info_by_id[tr.fleet_id]
		var mx := float(inf.get("maxHull", 0))
		s.hull_frac = clampf(float(inf.get("hull", 0)) / mx, 0.0, 1.0) if mx > 0.0 and inf.has("hull") else 1.0
		s.slot0 = Vector3(p0.x * M, p0.y * M, p0.altitude * M)
		var p1: BattleTimeline.Pose = tr.segments.back().to if not tr.segments.is_empty() else p0
		s.slot1 = Vector3(p1.x * M, p1.y * M, p1.altitude * M)
		var fe := deg_to_rad(p1.facing)
		s.face_end = Vector3(cos(fe), sin(fe), 0.0)
		s.withdraw = s.cls <= 1 and (s.hull_frac < (0.2 if s.cls == 0 else 0.3) or tr.maneuver_last == "Retreat")
		s.warp_at = -1.0
		s.dead_at = -1.0
		s.victim = false
		s.shots = []
		s.sq = 0
		s.sq_t = 0.0
		s.passes = 0
		s.wave = null
		s.returning = false
		s.wing_leader = tr.wing_leader
		s.wing_off = Vector3(tr.wing_back, tr.wing_side, tr.wing_up) * M
		s.snaps = PackedFloat32Array()
		s.next_fire = 0.0
		# Same style and envelope the timeline's reactions read (kicks turn the bow inside choreo_end).
		tr.style = BattleTimeline.ship_style(tr.length_meters, cfg)
		tr.choreo_end = _Tm
		tr.sway_phase = BattleTimeline.unit_hash(tr.fleet_id, 41) * TAU
		tr.hull_max = float(inf.get("hull", 0))
		tr.hull_frac = s.hull_frac
	for fid in ships.keys():
		if not present.has(fid):
			ships.erase(fid)
	_order = ships.values()
	_order.sort_custom(func(a, b): return a.fid < b.fid)
	_waves.clear()

	# Constraints from the round's events.
	var W := tl.window_starts
	for i in events.size():
		var e: Dictionary = events[i]
		var fid := int(BattleTimeline._num(e, "fleetId"))
		var s: Ship = ships.get(fid)
		if s == null:
			continue
		match str(e.kind):
			"Destroyed":
				s.victim = true
			"Retreated":
				s.warp_at = float(W[clampi(int(e.tick), 0, BattleTimeline.TICKS_PER_ROUND)])
			"Disengage":
				s.withdraw = true
			"Fire", "Ram":
				var ram: bool = e.kind == "Ram"
				var tgt := int(BattleTimeline._num(e, "targetFleetId"))
				if not ships.has(tgt):
					continue
				var family := BattleTimeline.family_index(BattleTimeline._num(e, "family", 0))
				var arc := BattleTimeline._mount_arc(info_by_id.get(fid, {}), int(BattleTimeline._num(e, "mountIndex", -1)))
				var nominal := 4800.0 if family == 0 else (3900.0 if family == 1 else 7800.0)
				var t_s: Ship = ships[tgt]
				var core_d := maxf(s.slot0.distance_to(t_s.slot0), s.slot1.distance_to(t_s.slot1))
				s.shots.append({"ev": i, "tgt": tgt, "arc": arc, "family": family, "ram": ram, "fired": false,
					"span": BattleTimeline.shot_span(e, i, info_by_id, cfg), "tick": int(e.tick),
					"mount": int(BattleTimeline._num(e, "mountIndex", -1)),
					"range": (s.radius + t_s.radius) * 1.3 if ram else maxf(nominal, core_d * 1.3 + 600.0)})
	# Cadence and deadlines: a ship's shots in the core's order, each with the latest time it may launch.
	var launches := {}
	for s: Ship in _order:
		var n: int = s.shots.size()
		if n == 0:
			continue
		s.shots.sort_custom(func(a, b): return a.tick < b.tick or (a.tick == b.tick and (a.mount < b.mount or (a.mount == b.mount and a.ev < b.ev))))
		var lo := _A
		var hi := _B
		s.gap = lerpf(0.4, 1.2, s.h) if s.cls <= 1 else lerpf(0.3, 0.55, s.h)
		if s.cls == 0 and n > 2:
			s.gap = maxf(0.3, minf(s.gap, (hi - lo - 0.4) / float(n - 1)))
		if s.victim:
			hi = minf(hi, 0.55 * _R)
			s.gap = minf(s.gap, 0.45)
		s.gap = minf(s.gap, maxf(0.15, (hi - lo - 0.1) / float(maxi(1, n - 1))))
		for k in n:
			s.shots[k].deadline = hi - float(n - 1 - k) * maxf(0.15, minf(s.gap, 0.35))
		# Ships of the line open fire in turn (the crews, not the geometry, set the first beat): a seeded share of the
		# first third of the fire window, the line ships with torpedoes first. Escorts wait for their run.
		var first := 0.0
		if s.cls <= 1:
			first = _hash(s.fid, 12) * 0.3 * (hi - lo)
		s.next_fire = lo + first
	# Squadron waves at the round's start (Battle.LaunchWave): every escort leader of a side with a run to make this
	# round, when there are at least two of them and the targets are well off, forms one V and goes in together.
	_side_stats()
	_launch_waves()

	# Fly.
	var n_steps := int(ceil((_R + TAIL) / DT)) + 1
	_steps = n_steps
	var forced := 0
	var fired := 0
	for s: Ship in _order:
		s.snaps.resize(n_steps * SNAP)
		if s.track != null:
			var p := PackedFloat32Array()
			p.resize(n_steps * 6)
			s.track.path = p
	var prev_yaw := {}
	_head_snaps.clear()
	for k in n_steps:
		var t := float(k) * DT
		if k > 0:
			_side_stats()
			_update_heading(DT)
			_update_waves(DT)
			for s: Ship in _order:
				_step(s, t, DT)
		else:
			_update_heading(0.0)
		for side in _heading:
			if not _head_snaps.has(side):
				var hs := PackedFloat32Array()
				hs.resize(n_steps)
				hs.fill(float(_heading[side]))
				_head_snaps[side] = hs
			(_head_snaps[side] as PackedFloat32Array)[k] = float(_heading[side])
		# Fire: each ship's next shot when its target is in that mount's arc and in range.
		for s: Ship in _order:
			if s.shots.is_empty() or t < s.next_fire or (s.dead_at >= 0.0 and t > s.dead_at):
				continue
			var pick := -1
			var force := false
			for j in s.shots.size():
				var sh: Dictionary = s.shots[j]
				if sh.fired:
					continue
				if t >= float(sh.deadline):
					pick = j
					force = true
					break
				if pick < 0 and _can_fire(s, sh, t):
					pick = j
			if pick < 0:
				continue
			var shot: Dictionary = s.shots[pick]
			if force and not _can_fire(s, shot, t):
				forced += 1
			shot.fired = true
			fired += 1
			launches[shot.ev] = t
			s.next_fire = t + s.gap * (0.85 + 0.3 * _hash(s.fid * 31 + pick, 13))
			# A victim dies (for the sim: stops flying) once every shot aimed at it has launched and landed.
			var tg: Ship = ships.get(int(shot.tgt))
			if tg != null and tg.victim:
				var all_out := true
				var land := t + float(shot.span)
				for o: Ship in _order:
					for sh2 in o.shots:
						if int(sh2.tgt) == tg.fid:
							if not sh2.fired:
								all_out = false
							elif launches.has(sh2.ev):
								land = maxf(land, float(launches[sh2.ev]) + float(sh2.span))
				if all_out:
					tg.dead_at = land
		# Record.
		for s: Ship in _order:
			_record(s, k, t, prev_yaw)
	# Every shot has a launch (deadlines force the stragglers inside the round).
	for s: Ship in _order:
		for sh in s.shots:
			if not launches.has(sh.ev):
				launches[sh.ev] = minf(float(sh.deadline), _B)
				forced += 1
	# Diagnostics: how near the core's end poses the ships settle, how many shots had to be forced.
	var err_sum := 0.0
	var err_max := 0.0
	var turn_max := 0.0
	var n_err := 0
	var k_end := clampi(int(round(_R / DT)), 0, n_steps - 1)
	for s: Ship in _order:
		if s.track == null or s.dead_at >= 0.0 or s.warp_at >= 0.0:
			continue
		var o := k_end * SNAP
		var at := Vector3(s.snaps[o], s.snaps[o + 1], s.snaps[o + 2])
		var e := Vector2(at.x - s.slot1.x, at.y - s.slot1.y).length() / M
		err_sum += e
		err_max = maxf(err_max, e)
		var fa := rad_to_deg(atan2(s.snaps[o + 4], s.snaps[o + 3]))
		turn_max = maxf(turn_max, absf(BattleTimeline.shortest_turn(fa, rad_to_deg(atan2(s.face_end.y, s.face_end.x)))))
		n_err += 1
	stats = {"ships": _order.size(), "shots": fired, "forced": forced, "end_err_mean": err_sum / maxf(1.0, n_err),
		"end_err_max": err_max, "end_facing_max": turn_max}
	return launches


func _new_ship(tr, inf: Dictionary, es: Dictionary) -> Ship:
	var s := Ship.new()
	s.fid = tr.fleet_id
	s.side = tr.owner_id
	s.h = BattleTimeline.unit_hash(tr.fleet_id, 977)
	var L: float = tr.length_meters if tr.length_meters > 0.0 else 600.0
	s.length = L
	s.radius = 0.5 * L
	var style := BattleTimeline.ship_style(L)
	s.cls = 3 if style == 2 and L <= BattleTimeline.STRIKE_CRAFT_METERS else style
	var h1 := BattleTimeline.unit_hash(tr.fleet_id, 978)
	# Battle.MakeShip: class speed (m/s), acceleration (m/s2) and turn rate (rad/s).
	match s.cls:
		3:
			s.max_speed = lerpf(700, 820, h1) * SMALL_SPEED; s.accel = 420; s.turn_rate = lerpf(1.4, 1.8, s.h)
		2:
			s.max_speed = lerpf(470, 540, h1) * SMALL_SPEED; s.accel = 220; s.turn_rate = lerpf(0.9, 1.1, s.h)
		1:
			s.max_speed = lerpf(230, 290, h1); s.accel = 70; s.turn_rate = minf(0.55, 0.38 * (380.0 / L))
		_:
			s.max_speed = lerpf(140, 165, h1); s.accel = 30; s.turn_rate = lerpf(0.14, 0.18, s.h)
	s.orbit_sign = -1.0 if BattleTimeline.unit_hash(tr.fleet_id, 979) < 0.5 else 1.0
	s.immobile = bool(es.get("immobile", false))
	if s.immobile:
		s.max_speed = 0.0
		s.turn_rate = 0.0
	return s


func _side_stats() -> void:
	_centroid.clear()
	_side_n.clear()
	var all := Vector3.ZERO
	var n_all := 0
	for s: Ship in _order:
		if s.dead_at >= 0.0:
			continue
		_centroid[s.side] = _centroid.get(s.side, Vector3.ZERO) + s.pos
		_side_n[s.side] = int(_side_n.get(s.side, 0)) + 1
		all += s.pos
		n_all += 1
	_enemy_c.clear()
	for side in _centroid:
		var n_o := n_all - int(_side_n[side])
		_enemy_c[side] = (all - _centroid[side]) / float(n_o) if n_o > 0 else Vector3(_arena.x * 0.5 * M, _arena.y * 0.5 * M, 0)
		_centroid[side] = _centroid[side] / float(_side_n[side])


## Each side's fleet heading eases toward the enemy's centre at HEAD_TURN rad/s (dt 0: a new side starts on it).
func _update_heading(dt: float) -> void:
	for side in _centroid:
		var to: Vector3 = _enemy_c.get(side, Vector3.ZERO) - _centroid[side]
		if Vector2(to.x, to.y).length() < 1.0:
			continue
		var want := atan2(to.y, to.x)
		if not _heading.has(side) or dt <= 0.0:
			if not _heading.has(side):
				_heading[side] = want
			continue
		var cur: float = _heading[side]
		_heading[side] = cur + clampf(wrapf(want - cur, -PI, PI), -HEAD_TURN * dt, HEAD_TURN * dt)


func _head(side: int, fallback: Vector3) -> Vector3:
	if not _heading.has(side):
		return fallback
	var a: float = _heading[side]
	return Vector3(cos(a), sin(a), 0.0)


# ------------------------------------------------------------------------------------------------------------------
# Fire (Battle.FireWeapons): in arc and in range
# ------------------------------------------------------------------------------------------------------------------

func _can_fire(s: Ship, sh: Dictionary, t: float) -> bool:
	var tg: Ship = ships.get(int(sh.tgt))
	if tg == null or (tg.dead_at >= 0.0 and t > tg.dead_at):
		return false
	var to := tg.pos - s.pos
	var d := to.length()
	if d > float(sh.range) + tg.radius:
		return false
	if sh.ram:
		return true
	var arc: Vector2 = sh.arc
	var face := rad_to_deg(atan2(s.fwd.y, s.fwd.x))
	var bearing := rad_to_deg(atan2(to.y, to.x))
	var rel := absf(BattleTimeline.shortest_turn(face, bearing))
	if maxf(0.0, absf(rel - arc.x) - arc.y) > 12.0:
		return false
	var late := t > float(sh.deadline) - 1.5
	if s.cls >= 2:
		# Escorts fire on their pass (Battle: a strafing run), not from their post across the arena.
		return late or d < tg.radius + 2200.0 or s.sq == 1 and d < tg.radius + 3200.0
	if arc.y >= 150.0 and not late and d > 1600.0:
		# An all-round mount on a ship of the line fires from the broadside it turned to show (beams), the bow tubes
		# bow-on (torpedoes).
		if int(sh.family) == 2:
			return rel < 65.0
		return rel > 22.0 and rel < 158.0
	return true


# ------------------------------------------------------------------------------------------------------------------
# Doctrine and flight
# ------------------------------------------------------------------------------------------------------------------

func _slot(s: Ship, t: float) -> Vector3:
	return s.slot0.lerp(s.slot1, clampf(t / _Tm, 0.0, 1.0))


func _slot_vel(s: Ship, t: float) -> Vector3:
	return (s.slot1 - s.slot0) / _Tm if t < _Tm else Vector3.ZERO


func _pending(s: Ship) -> Dictionary:
	for sh in s.shots:
		if not sh.fired:
			return sh
	return {}


func _flat(v: Vector3) -> Vector3:
	var f := Vector3(v.x, v.y, 0.0)
	return f.normalized() if f.length_squared() > 1e-6 else Vector3.ZERO


func _step(s: Ship, t: float, dt: float) -> void:
	if s.immobile:
		s.vel = Vector3.ZERO
		s.speed = 0.0
		return
	if s.dead_at >= 0.0 and t > s.dead_at:
		# Crippled: it keeps its way on and slows (the timeline draws the tumble and the break-up).
		s.vel *= pow(0.8, dt)
		s.pos += s.vel * dt
		return
	if s.warp_at >= 0.0 and t > s.warp_at - 3.5:
		# Warping out this round: turn away from the enemy and run before the jump.
		var away := _flat(s.pos - _enemy_c.get(s.side, s.pos - s.fwd))
		if away == Vector3.ZERO:
			away = _flat(s.fwd)
		_fly(s, (away + Vector3(0, 0, 0.1)).normalized(), s.max_speed, dt)
		return
	var sh := _pending(s)
	if not sh.is_empty() and sh.ram:
		var tg: Ship = ships.get(int(sh.tgt))
		if tg != null:
			_fly(s, (tg.pos - s.pos).normalized(), s.max_speed * 1.4, dt)
			return
	if s.cls <= 1:
		_line(s, t, dt)
	else:
		_strike(s, t, dt)


## A ship of the line holds its post (the core's cell, moving from the round's start cell to its end cell) and turns in
## place to bring its arrays to bear (Battle.Hold, Battle.LineFace). Away from the post it flies to it bow first.
func _line(s: Ship, t: float, dt: float) -> void:
	var ph := s.h * 6.2832
	# PostPos: formations hold tight but not frozen.
	var wander := Vector3(sin(t * 0.07 + ph) * 40.0, cos(t * 0.05 + ph * 1.7) * 50.0, sin(t * 0.06 + ph * 2.3) * 90.0)
	var slot := _slot(s, t) + wander
	var face := _line_face(s, t)
	if _hold(s, slot, _slot_vel(s, t), face, dt):
		return
	_fly(s, s.des, s.want, dt)


func _line_face(s: Ship, t: float) -> Vector3:
	var enemy := _flat(_enemy_c.get(s.side, s.pos + s.fwd) - s.pos)
	if enemy == Vector3.ZERO:
		enemy = _flat(s.fwd)
	if s.withdraw:
		return -enemy
	var face := _head(s.side, enemy)
	var sh := _pending(s)
	var cur := rad_to_deg(atan2(s.fwd.y, s.fwd.x))
	# Time left to swing round to the core's end facing (plan view and orders read it).
	var end_deg := rad_to_deg(atan2(s.face_end.y, s.face_end.x))
	var need := absf(BattleTimeline.shortest_turn(cur, end_deg)) / maxf(1.0, rad_to_deg(s.turn_rate)) + 0.8
	if sh.is_empty():
		if t >= _Tm - need or t >= _B:
			return s.face_end
		return face
	var tg: Ship = ships.get(int(sh.tgt))
	if tg == null:
		return face
	var to := tg.pos - s.pos
	var b := _flat(to)
	if b == Vector3.ZERO:
		return face
	var arc: Vector2 = sh.arc
	var close := to.length() < 1500.0
	# Stage 2: keep station on the fleet heading while the shot already bears from it (the line advances as one body
	# and fires from it, as the show's fleets do); turn out of line only for a shot that does not.
	if not close and _heading.has(s.side):
		var rel_h := absf(BattleTimeline.shortest_turn(rad_to_deg(float(_heading[s.side])), rad_to_deg(atan2(b.y, b.x))))
		var bears := false
		if arc.y >= 150.0:
			bears = rel_h < 50.0 if int(sh.family) == 2 else (rel_h > 30.0 and rel_h < 150.0)
		else:
			bears = absf(rel_h - arc.x) <= arc.y - 4.0
		if bears:
			return face
	# Each wing turns its broadside its own way (Battle.FleetSlots: Block by Post.X): the side of the formation it is on,
	# looking at the enemy.
	var lat := enemy.cross(s.pos - _centroid.get(s.side, s.pos)).z
	var wing := 1.0 if lat >= 0.0 else -1.0
	if absf(lat) < 1.0:
		wing = 1.0 if s.h < 0.5 else -1.0
	var off := 0.0
	if close or (arc.y < 150.0 and arc.x < 40.0) or (arc.y >= 150.0 and int(sh.family) == 2):
		off = 0.0                            # bow on: forward mounts, torpedo tubes, an enemy on top of it
	elif arc.y >= 150.0:
		off = (55.0 if s.cls == 0 else 70.0) # broadside to show its arrays
	else:
		off = clampf(arc.x, 0.0, 180.0)      # put the target on that mount's bearing
	var want_deg := rad_to_deg(atan2(b.y, b.x)) + off * wing
	if off > 0.0 and off < 180.0:
		# The other broadside, if the ship is already nearer to it.
		var alt := rad_to_deg(atan2(b.y, b.x)) - off * wing
		if absf(BattleTimeline.shortest_turn(cur, alt)) + 25.0 < absf(BattleTimeline.shortest_turn(cur, want_deg)):
			want_deg = alt
	var r := deg_to_rad(want_deg)
	return Vector3(cos(r), sin(r), clampf(to.z / maxf(to.length(), 1.0), -0.3, 0.3)).normalized()


## Squadrons (Battle.StrikeLeader): wait at the post, go in with the wave, make the run, break away together, regroup.
func _strike(s: Ship, t: float, dt: float) -> void:
	s.sq_t += dt
	var slot := _slot(s, t)
	var home_d := s.pos.distance_to(s.slot1)
	var need := home_d / (0.7 * s.max_speed) + 1.6 + PI / maxf(0.3, s.turn_rate)
	var sh := _pending(s)
	if s.returning or t >= _Tm - need or t >= _B:
		# Home before the round ends: back onto the post, then hold station facing the core's way.
		s.returning = true
		if s.wave != null:
			s.wave.members.erase(s)
			s.wave = null
		var face := s.face_end if t > _Tm - 2.0 or sh.is_empty() else _strike_face(s, sh)
		if _hold(s, slot, _slot_vel(s, t), face, dt):
			return
		_fly(s, s.des, minf(s.want * 1.25, s.max_speed), dt)
		return
	# Wingman: hold the slot on the leader, unless it has a shot of its own on another ship.
	var lead: Ship = ships.get(s.wing_leader) if s.wing_leader != 0 else null
	if lead != null and (lead.dead_at < 0.0 or t < lead.dead_at) and not lead.returning \
			and (sh.is_empty() or int(sh.tgt) == lead.target or lead.target == 0):
		var lf := _flat(lead.fwd)
		var left := Vector3(-lf.y, lf.x, 0.0)
		var spot := lead.pos - lead.fwd * s.wing_off.x + left * s.wing_off.y + Vector3(0, 0, s.wing_off.z)
		var to := spot - s.pos
		_fly(s, (lead.fwd * 600.0 + to).normalized(), minf(s.max_speed * 1.15, lead.speed + to.length() * 0.6), dt)
		return
	if s.sq == 3:
		var w = s.wave
		if w == null:
			s.sq = 0
		else:
			var wf := _flat(w.fwd)
			var right := Vector3(wf.y, -wf.x, 0.0)
			var spot2: Vector3 = w.pos + right * s.wave_slot.x + w.fwd * s.wave_slot.y + Vector3(0, 0, s.wave_slot.z)
			var to2 := spot2 - s.pos
			_fly(s, (w.fwd * 500.0 + to2).normalized(), minf(s.max_speed * 1.15, w.speed + to2.length() * 0.5), dt)
			return
	if s.sq == 0:
		if not sh.is_empty() and ships.has(int(sh.tgt)):
			_begin_run(s, int(sh.tgt))
		else:
			# Stage 2: hold station ahead of the post on the fleet heading (the screen the show's escorts fly in front of
			# the line), where the screensaver's rally circled the post: a circling escort read as random headings.
			var hd := _head(s.side, _flat(s.fwd))
			var ph := s.h * 6.2832
			var spot := slot + hd * 500.0 + Vector3(sin(t * 0.11 + ph) * 60.0, cos(t * 0.09 + ph) * 60.0, sin(t * 0.07 + ph * 2.0) * 80.0)
			if _hold(s, spot, _slot_vel(s, t), hd, dt):
				return
			_fly(s, s.des, s.want, dt)
			return
	var tg: Ship = ships.get(s.target)
	if tg == null or (tg.dead_at >= 0.0 and t > tg.dead_at) or s.sq_t > 14.0:
		s.sq = 0
		s.sq_t = 0.0
		s.target = 0
		_fly(s, s.fwd, s.max_speed * 0.7, dt)
		return
	var tv := tg.pos - s.pos
	var td := tv.length()
	var tn := tv / maxf(td, 1.0)
	if s.sq == 1:
		var pass_d := tg.radius * (1.5 if tg.radius > 600.0 else 1.0) + (350.0 if s.cls == 3 else 600.0)
		if td < pass_d:
			# Break away: peel off a big ship back the way it came; overshoot a small one (Battle.StrikeLeader).
			s.sq = 2
			s.passes += 1
			s.break_t = lerpf(2.0, 3.5, _hash(s.fid * 7 + s.passes, 81))
			var side := Vector3(tn.y, -tn.x, 0.0) * (1.0 if _hash(s.fid * 7 + s.passes, 82) < 0.5 else -1.0)
			var up := Vector3(0, 0, lerpf(-1.0, 1.0, _hash(s.fid * 7 + s.passes, 83)) * 700.0)
			if tg.radius > 150.0:
				s.break_point = s.pos + side * lerpf(1000, 1700, _hash(s.fid, 84)) - tn * lerpf(700, 1400, _hash(s.fid, 85)) + up
			else:
				s.break_point = tg.pos + tn * lerpf(1100, 1900, _hash(s.fid, 84)) + side * lerpf(400, 1000, _hash(s.fid, 85)) + up
			s.break_point = _inside(s.break_point, 400.0)
		else:
			var lead_pt := tg.pos + tg.fwd * tg.speed * minf(2.0, td / 1500.0)
			# Come in on a strafing line past the target's beam, not into its hull.
			var nrm := Vector3(-tn.y, tn.x, 0.0) * s.orbit_sign
			var aim := lead_pt + nrm * minf(pass_d * 0.8, td * 0.3)
			_fly(s, (aim - s.pos).normalized(), s.max_speed, dt)
			return
	# Breaking.
	s.break_t -= dt
	if s.break_t <= 0.0 or s.pos.distance_to(s.break_point) < 300.0:
		var again := false
		for sh2 in s.shots:
			if not sh2.fired and int(sh2.tgt) == s.target:
				again = true
		if again and t < _Tm - need - 2.5:
			s.sq = 1
			s.sq_t = 0.0
		else:
			s.sq = 0
			s.sq_t = 0.0
			s.target = 0
	_fly(s, (s.break_point - s.pos).normalized(), s.max_speed, dt)


func _strike_face(s: Ship, sh: Dictionary) -> Vector3:
	var tg: Ship = ships.get(int(sh.tgt))
	if tg == null:
		return s.face_end
	var b := _flat(tg.pos - s.pos)
	return b if b != Vector3.ZERO else s.face_end


func _begin_run(s: Ship, tgt: int) -> void:
	s.target = tgt
	s.sq = 1
	s.sq_t = 0.0


## Battle.LaunchWave: the squadrons with a run to make form one V at their centre and fly it in together.
func _launch_waves() -> void:
	var by_side := {}
	for s: Ship in _order:
		if s.cls < 2 or s.immobile or s.warp_at >= 0.0:
			continue
		var sh := _pending(s)
		if sh.is_empty():
			continue
		var lead: Ship = ships.get(s.wing_leader) if s.wing_leader != 0 else null
		if lead != null:
			continue # wingmen fly on their leader
		if not by_side.has(s.side):
			by_side[s.side] = []
		by_side[s.side].append(s)
	for side in by_side:
		var list: Array = by_side[side]
		if list.size() < 2:
			continue
		var c := Vector3.ZERO
		var goal := Vector3.ZERO
		var speed := INF
		for s: Ship in list:
			c += s.pos
			var tg: Ship = ships[int(_pending(s).tgt)]
			goal += tg.pos
			speed = minf(speed, s.max_speed * 0.85)
		c /= list.size()
		goal /= list.size()
		if c.distance_to(goal) < 3200.0:
			continue
		var w := Wave.new()
		w.side = side
		w.pos = c
		w.goal = goal
		w.fwd = (goal - c).normalized()
		w.speed = speed
		list.sort_custom(func(a, b): return a.length > b.length or (a.length == b.length and a.fid < b.fid))
		for k in list.size():
			var s: Ship = list[k]
			var row := (k + 1) / 2
			s.wave = w
			s.sq = 3
			s.wave_slot = Vector3((1.0 if k % 2 == 0 else -1.0) * row * 380.0, -row * 340.0, float((k % 3) - 1) * 140.0)
			w.members.append(s)
		_waves.append(w)


func _update_waves(dt: float) -> void:
	for i in range(_waves.size() - 1, -1, -1):
		var w: Wave = _waves[i]
		w.age += dt
		if w.members.is_empty():
			_waves.remove_at(i)
			continue
		var to: Vector3 = w.goal - w.pos
		var d := to.length()
		if d > 1.0:
			var want := to / d
			var ang := w.fwd.angle_to(want)
			var axis := w.fwd.cross(want)
			if axis.length_squared() > 1e-8 and ang > 1e-4:
				w.fwd = w.fwd.rotated(axis.normalized(), minf(ang, 0.5 * dt)).normalized()
		w.pos += w.fwd * w.speed * (0.4 if w.age < 1.2 else 1.0) * dt
		# Split onto targets at about 2.5 km (Battle: 3 km on a 22 km wall), or when the round is running out.
		if d < 2500.0 or w.age > 0.4 * _R:
			for s: Ship in w.members:
				s.wave = null
				s.sq = 0
			_waves.remove_at(i)


## Battle.Hold: station-keeping on a moving slot. Away from it the ship makes for it along its heading (returns false and
## leaves the heading and speed in s.des / s.want for _fly); in it, the ship moves with the slot and turns in place.
func _hold(s: Ship, slot: Vector3, slot_vel: Vector3, face: Vector3, dt: float) -> bool:
	var err := slot - s.pos
	var d := err.length()
	var lock_in := 300.0 + s.radius * 0.5
	var lock_out := 1200.0 + s.radius
	if (d > lock_out) if s.in_slot else (d > lock_in):
		var go := slot_vel + err * 0.4
		var gs := go.length()
		if gs > s.max_speed:
			go *= s.max_speed / gs
			gs = s.max_speed
		s.des = go / gs if gs > 1.0 else face
		s.want = gs
		s.in_slot = false
		return false
	s.in_slot = true
	var push := _separation(s, true)
	var want := slot_vel + err * 0.35 + push * (s.max_speed * 0.6)
	var ws := want.length()
	var cap := s.max_speed * 1.1
	if ws > cap:
		want *= cap / ws
	var dv := want - s.vel
	var a := maxf(s.accel, 40.0) * dt
	if dv.length() > a:
		dv *= a / dv.length()
	s.vel += dv
	s.pos += s.vel * dt
	var prev := s.fwd
	var f := face
	f.z = clampf(f.z, -0.5, 0.5)
	_turn_to(s, f.normalized() if f.length_squared() > 1e-6 else s.fwd, dt)
	s.speed = maxf(0.0, s.vel.dot(s.fwd))
	_bank(s, prev, dt)
	return true


## Battle.Steer (the flight model): separation and soft walls, then turn toward the heading at the class turn rate, slow
## in hard turns, accelerate at the class rate, move along the bow and bank into the turn.
func _fly(s: Ship, desired: Vector3, want_speed: float, dt: float) -> void:
	var push := _separation(s, false)
	var p := s.pos
	var inside := _inside(p, 60.0)
	push.x += (inside.x - p.x) / 1200.0
	push.y += (inside.y - p.y) / 1000.0
	push.z += (inside.z - p.z) / 600.0
	var des := desired + push
	if des.length_squared() < 1e-8:
		des = s.fwd
	des = des.normalized()
	des.z = clampf(des.z, -0.7, 0.7)
	des = des.normalized()
	var prev := s.fwd
	var c := _turn_to(s, des, dt)
	var target := want_speed * (0.55 + 0.45 * maxf(0.0, c))
	s.speed += clampf(target - s.speed, -s.accel * dt, s.accel * dt)
	s.speed = maxf(0.0, s.speed)
	s.pos += s.fwd * s.speed * dt
	s.vel = s.fwd * s.speed
	s.in_slot = false
	_bank(s, prev, dt)


## Turns toward a heading at the class turn rate; returns the cosine of the angle it still had to turn (Battle.TurnTo).
## The turn is split into yaw (round the vertical) and pitch (capped at MAX_PITCH), so a ship coming about turns round
## like a ship, never loops over the top (the drawn pose is yaw, pitch and roll).
const MAX_PITCH := 0.6

static func _turn_to(s: Ship, desired: Vector3, dt: float) -> float:
	var c := clampf(s.fwd.dot(desired), -1.0, 1.0)
	var step := s.turn_rate * dt
	if step <= 0.0:
		return c
	var yaw := atan2(s.fwd.y, s.fwd.x)
	var pitch := asin(clampf(s.fwd.z, -1.0, 1.0))
	var flat := Vector2(desired.x, desired.y)
	var want_yaw := atan2(desired.y, desired.x) if flat.length() > 1e-4 else yaw
	var want_pitch := clampf(asin(clampf(desired.z, -1.0, 1.0)), -MAX_PITCH, MAX_PITCH)
	var dy := wrapf(want_yaw - yaw, -PI, PI)
	var dp := want_pitch - pitch
	var arc := sqrt(dy * dy * cos(pitch) * cos(pitch) + dp * dp)
	if arc > 1e-5:
		var k := minf(1.0, step / arc)
		yaw += dy * k
		pitch += dp * k
	s.fwd = Vector3(cos(pitch) * cos(yaw), cos(pitch) * sin(yaw), sin(pitch))
	return c


## Bank into turns (Battle.Bank): the roll follows the yaw rate, eased; capital ships lumber, fighters roll hard.
static func _bank(s: Ship, prev: Vector3, dt: float) -> void:
	var yaw_rate := prev.cross(s.fwd).z / maxf(dt, 1e-4)
	var max_roll := 1.1 if s.cls == 3 else (0.7 if s.cls == 2 else 0.3)
	var want := clampf(-yaw_rate * (1.3 if s.cls >= 2 else 2.5), -max_roll, max_roll)
	s.roll += (want - s.roll) * minf(1.0, dt * 2.5)


## Neighbours push apart (Battle.Separate); ships holding station in their slots don't push their own side.
func _separation(s: Ship, holding: bool) -> Vector3:
	var push := Vector3.ZERO
	for o: Ship in _order:
		if o == s or (o.dead_at >= 0.0):
			continue
		if holding and o.in_slot and o.side == s.side:
			continue
		var dv := s.pos - o.pos
		var mn := (s.radius + o.radius) * 1.2 + 60.0
		var d2 := dv.length_squared()
		if d2 < mn * mn and d2 > 1.0:
			var dd := sqrt(d2)
			push += dv / dd * (1.0 - dd / mn) * (o.radius / (s.radius + o.radius)) * 2.0
			if o.radius > 600.0 and dd < o.radius * 1.2:
				push += dv / dd * 5.0
	return push


## The nearest point inside the arena (margin metres inside its edge), height kept to +-4 cells.
func _inside(p: Vector3, margin: float) -> Vector3:
	var lo_x := -0.5 * M + margin
	var hi_x := (_arena.x - 0.5) * M - margin
	var lo_y := -0.5 * M + margin
	var hi_y := (_arena.y - 0.5) * M - margin
	return Vector3(clampf(p.x, lo_x, hi_x), clampf(p.y, lo_y, hi_y), clampf(p.z, -4.0 * M, 4.0 * M))


func _record(s: Ship, k: int, t: float, prev_yaw: Dictionary) -> void:
	var o := k * SNAP
	var sn := s.snaps
	sn[o] = s.pos.x; sn[o + 1] = s.pos.y; sn[o + 2] = s.pos.z
	sn[o + 3] = s.fwd.x; sn[o + 4] = s.fwd.y; sn[o + 5] = s.fwd.z
	sn[o + 6] = s.vel.x; sn[o + 7] = s.vel.y; sn[o + 8] = s.vel.z
	sn[o + 9] = s.speed; sn[o + 10] = s.roll; sn[o + 11] = 1.0 if s.in_slot else 0.0
	s.snaps = sn
	if _debug_fid == s.fid:
		print("DBG t=%.1f pos=(%.0f,%.0f,%.0f) fwd=(%.2f,%.2f,%.2f) v=%.0f sq=%d slot=%s ret=%s wave=%s" % [t, s.pos.x, s.pos.y, s.pos.z, s.fwd.x, s.fwd.y, s.fwd.z, s.speed, s.sq, s.in_slot, s.returning, s.wave != null])
	if s.track == null:
		return
	var path: PackedFloat32Array = s.track.path
	var q := k * 6
	path[q] = s.pos.x / M
	path[q + 1] = s.pos.y / M
	path[q + 2] = s.pos.z / M
	path[q + 3] = BattleTimeline.wrap_degrees(rad_to_deg(atan2(s.fwd.y, s.fwd.x)))
	path[q + 4] = clampf(rad_to_deg(s.roll), -70.0, 70.0)
	path[q + 5] = clampf(rad_to_deg(asin(clampf(s.fwd.z, -1.0, 1.0))), -40.0, 40.0)
	s.track.path = path
