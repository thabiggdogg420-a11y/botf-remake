class_name BattleSynth
extends RefCounted
## Presentation events for rounds the core resolved without an event stream (M4 combat 2, Simon 14 Nov: the Tactical drill and
## the Classic rules, tactical rules 1-2, fire no weapons at all). The round result still carries what happened: each ship's
## move (from/to), the salvos (who hit whom, beam and torpedo damage, whether shields absorbed it) and the hull and shield
## figures before and after. This turns them into the same Fire / Maneuver / Destroyed events the timeline reads for rules 3
## and 4, so a Classic battle plays with held beams, staggered torpedo salvos and the same cameras. Nothing here changes what
## happened: damage is only spread over the shots the core reported (the sums match), and rounds that already carry events
## are returned untouched. No core change, no randomness (shot order follows the core's salvo order).

const TICKS := 12
const BEAM_WEAPON := {1: "PhaserArray", 2: "Disruptor", 3: "Disruptor", 4: "Disruptor", 5: "Disruptor", 6: "PolaronBeam",
	7: "CuttingBeam", 8: "EnergyDampener"}
const TORPEDO_WEAPON := {1: "PhotonTorpedo", 2: "PhotonTorpedo", 3: "PlasmaTorpedo", 4: "PhotonTorpedo", 5: "PhotonTorpedo",
	6: "QuantumTorpedo", 7: "PhotonTorpedo", 8: "PhotonTorpedo"}


## True when the round has a move or a shot to show but no event stream of its own.
static func needs_events(rnd: Dictionary) -> bool:
	if not rnd.get("events", []).is_empty():
		return false
	for rs in rnd.get("ships", []):
		if rs.has("from") and rs.has("to"):
			return true
	return false


## A copy of the encounter in which every round without events has synthesised ones (the original is not touched).
static func with_events(encounter: Dictionary) -> Dictionary:
	var rounds: Array = encounter.get("rounds", [])
	var need := false
	for rnd in rounds:
		if needs_events(rnd):
			need = true
			break
	if not need:
		return encounter
	var out := encounter.duplicate(false)
	var new_rounds := []
	for rnd in rounds:
		if needs_events(rnd):
			var copy: Dictionary = rnd.duplicate(false)
			copy["events"] = events_for_round(rnd, encounter)
			copy["synthesised"] = true
			new_rounds.append(copy)
		else:
			new_rounds.append(rnd)
	out["rounds"] = new_rounds
	return out


static func _facing_to(a: Vector2, b: Vector2, fallback: float) -> float:
	var d := b - a
	if d.length() < 1e-3:
		return fallback
	return rad_to_deg(atan2(d.y, d.x))


static func events_for_round(rnd: Dictionary, encounter: Dictionary) -> Array:
	var ev := []
	var ships: Array = rnd.get("ships", [])
	var by_id := {}
	var from_pos := {}
	var to_pos := {}
	var owner_of := {}
	for rs in ships:
		var id := int(rs.get("id", 0))
		by_id[id] = rs
		owner_of[id] = int(rs.get("owner", 0))
		var f: Dictionary = rs.get("from", {})
		var t: Dictionary = rs.get("to", f)
		from_pos[id] = Vector2(float(f.get("x", 0)), float(f.get("y", 0)))
		to_pos[id] = Vector2(float(t.get("x", f.get("x", 0))), float(t.get("y", f.get("y", 0))))
	# Where each ship points when the move ends: at the enemy it is shooting, else the nearest enemy, else along its move.
	var target_of := {}
	for sv in rnd.get("salvos", []):
		var a := int(sv.get("from", 0))
		if not target_of.has(a):
			target_of[a] = int(sv.get("to", 0))
	var end_facing := {}
	for id in by_id:
		var rs: Dictionary = by_id[id]
		var tgt := int(target_of.get(id, 0))
		var face := float(rs.get("yaw", 0))
		if tgt != 0 and to_pos.has(tgt):
			face = _facing_to(to_pos[id], to_pos[tgt], face)
		else:
			var best := INF
			for other in by_id:
				if owner_of[other] != owner_of[id] and int(by_id[other].get("hullBefore", 1)) > 0:
					var d: float = to_pos[id].distance_to(to_pos[other])
					if d < best:
						best = d
						face = _facing_to(to_pos[id], to_pos[other], face)
		end_facing[id] = face
	for rs in ships:
		var id := int(rs.get("id", 0))
		if int(rs.get("hullBefore", 1)) <= 0:
			continue
		var man := str(rs.get("maneuver", "Engage"))
		var f: Vector2 = from_pos[id]
		var t: Vector2 = to_pos[id]
		if bool(rs.get("decloaked", false)):
			ev.append({"tick": 0, "kind": "Cloak", "fleetId": id, "targetFleetId": 0, "x": int(f.x), "y": int(f.y), "facing": int(float(rs.get("yaw", 0)))})
			ev.append({"tick": 1, "kind": "Decloak", "fleetId": id, "targetFleetId": 0, "x": int(f.x), "y": int(f.y), "facing": int(float(rs.get("yaw", 0)))})
		ev.append({"tick": 0, "kind": "ManeuverStart", "fleetId": id, "targetFleetId": int(rs.get("target", 0)), "maneuver": man, "value": 0,
			"x": int(f.x), "y": int(f.y), "facing": int(float(rs.get("yaw", 0)))})
		ev.append({"tick": 4, "kind": "ManeuverEnd", "fleetId": id, "targetFleetId": int(rs.get("target", 0)), "maneuver": man, "value": 0,
			"x": int(t.x), "y": int(t.y), "facing": int(end_facing[id])})
	# Damage each target took, shared out over the shots that hit it.
	var power_to := {}
	var hull_power_to := {}   ## the same, over the salvos that were not absorbed by shields (only those can reach the hull)
	for sv in rnd.get("salvos", []):
		var to := int(sv.get("to", 0))
		var pw := float(sv.get("beam", 0)) + float(sv.get("torpedo", 0))
		power_to[to] = float(power_to.get(to, 0.0)) + pw
		if not bool(sv.get("absorbed", false)):
			hull_power_to[to] = float(hull_power_to.get(to, 0.0)) + pw
	var last_hit_of := {}
	var salvos: Array = rnd.get("salvos", [])
	var mount_of := {}
	var n := 0
	var shots := []
	for sv in salvos:
		var a := int(sv.get("from", 0))
		var to := int(sv.get("to", 0))
		var dmg_beam := int(sv.get("beam", 0))
		var dmg_torp := int(sv.get("torpedo", 0))
		if bool(sv.get("ram", false)) or (dmg_beam <= 0 and dmg_torp <= 0) or not by_id.has(a) or not by_id.has(to):
			continue
		var power := maxf(1.0, float(dmg_beam + dmg_torp))
		var share := power / maxf(1.0, float(power_to.get(to, power)))
		var absorbed := bool(sv.get("absorbed", false))
		var s_part := int(round(float(_initial_loss(rnd, to, true)) * share))
		var h_part := 0
		if float(hull_power_to.get(to, 0.0)) > 0.0:
			if not absorbed:
				h_part = int(round(float(_initial_loss(rnd, to, false)) * power / float(hull_power_to[to])))
		else:
			h_part = int(round(float(_initial_loss(rnd, to, false)) * share))
		var owner: int = owner_of[a]
		var nb := 0
		if dmg_beam > 0:
			nb = clampi(int(ceil(float(dmg_beam) / 14.0)), 1, 3)
		var parts := []
		if dmg_beam > 0:
			parts.append(["beam", dmg_beam, nb])
		if dmg_torp > 0:
			parts.append(["torpedo", dmg_torp, clampi(int(round(float(dmg_torp) / 9.0)), 1, 5)])
		var total_p := float(dmg_beam + dmg_torp)
		for p in parts:
			var frac: float = float(p[1]) / maxf(1.0, total_p)
			var count: int = p[2]
			for k in count:
				var mount := int(mount_of.get(a, 0))
				mount_of[a] = mount + 1
				var is_torp: bool = p[0] == "torpedo"
				var tick := (4 + (n % 3)) if is_torp else (3 + ((n + k) % 4))
				var e := {"tick": tick, "kind": "Fire", "fleetId": a, "targetFleetId": to, "mountIndex": mount,
					"weapon": (TORPEDO_WEAPON if is_torp else BEAM_WEAPON).get(owner, "PhotonTorpedo" if is_torp else "PhaserArray"),
					"family": "torpedo" if is_torp else "beam", "projectiles": count if is_torp else 1, "hit": true,
					"impactTick": mini(TICKS - 1, tick + (3 if is_torp else 0)),
					"shieldDamage": int(round(float(s_part) * frac / float(maxi(1, 1 if is_torp else count)))),
					"hullDamage": int(round(float(h_part) * frac / float(maxi(1, 1 if is_torp else count)))),
					"synth": true}
				if is_torp:
					e["projectiles"] = count
					e["shieldDamage"] = int(round(float(s_part) * frac))
					e["hullDamage"] = int(round(float(h_part) * frac))
				shots.append(e)
				last_hit_of[to] = shots.size() - 1
				if is_torp:
					break
		n += 1
	for i in shots.size():
		ev.append(shots[i])
	# A ship the round destroyed: its last hit says so, and a Destroyed event follows it.
	for id in by_id:
		var rs: Dictionary = by_id[id]
		if bool(rs.get("destroyed", false)):
			var tick := TICKS - 3
			if last_hit_of.has(id):
				var e: Dictionary = shots[int(last_hit_of[id])]
				e["destroyed"] = true
				tick = mini(TICKS - 2, int(e.impactTick) + 1)
			var t: Vector2 = to_pos[id]
			ev.append({"tick": tick, "kind": "Destroyed", "fleetId": id, "targetFleetId": 0, "x": int(t.x), "y": int(t.y), "facing": int(end_facing[id])})
	# Stable by tick: Array.sort_custom is not, and a tick's events keep the order they were made in.
	var keyed := []
	for i in ev.size():
		keyed.append([int(ev[i].tick) * 100000 + i, ev[i]])
	keyed.sort_custom(func(a, b): return a[0] < b[0])
	var sorted := []
	for k in keyed:
		sorted.append(k[1])
	return sorted


## What a ship lost in the round: shield points (shield true) or hull points.
static func _initial_loss(rnd: Dictionary, id: int, shield: bool) -> int:
	for rs in rnd.get("ships", []):
		if int(rs.get("id", 0)) == id:
			if shield:
				return maxi(0, int(rs.get("shieldBefore", 0)) - int(rs.get("shieldAfter", 0)))
			return maxi(0, int(rs.get("hullBefore", 0)) - int(rs.get("hullAfter", 0)))
	return 0
