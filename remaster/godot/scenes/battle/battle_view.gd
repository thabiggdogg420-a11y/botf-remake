extends "res://scenes/battle/battle_scene.gd"
## Adapter between the UI's BattleHost contract (load_encounter / play_round(round, encounter))
## and BattleScene.play_round(encounter, round_index).
##
## The combat screen keeps this view alive across refreshes and calls load_encounter after every order, pick and
## round, so both entry points are idempotent: a round that is already playing or has played stays as it is, and
## only a new round (or a different encounter) starts playback.

var _encounter_key := ""


static func encounter_key(e: Dictionary) -> String:
	var ids := []
	for s in e.get("ships", []):
		ids.append(int(s.get("id", 0)))
	ids.sort()
	return "%s|%s|%s|%s|%s" % [e.get("turn", ""), e.get("location", ""), e.get("sectorX", ""), e.get("sectorY", ""), str(ids)]


func load_encounter(e: Dictionary) -> void:
	var rounds: Array = e.get("rounds", [])
	var key := encounter_key(e)
	if key != _encounter_key and assets != null:
		assets.warm(BattleAssets.encounter_keys(e)) # every hull parses in parallel, not one after another on this frame
	if rounds.is_empty():
		if key != _encounter_key:
			_encounter_key = key
			_show_deployment(e)
		# Compile the weapon effects while the fleets deploy, not on the first round's first frame (R3.5: 370 ms).
		if not _prewarmed:
			_prewarmed = true
			fx.prewarm.call_deferred()
			BattleShip.flame_flipbook.call_deferred() # damage-fire textures are generated once, here
			BattleShip.puff_texture.call_deferred()
		return
	_play_index(e, rounds.size() - 1, key)


func play_round(r: Variant, e: Variant = null) -> void:
	# BattleScene-style call: play_round(encounter, index).
	if r is Dictionary and (e is int or e is float):
		super.play_round(r, int(e))
		return
	var enc: Dictionary = e if e is Dictionary else {}
	var rounds: Array = enc.get("rounds", [])
	var idx := rounds.size() - 1
	for i in rounds.size():
		if rounds[i] == r or (r is Dictionary and rounds[i].get("index") == r.get("index")):
			idx = i
			break
	if idx >= 0:
		_play_index(enc, idx, encounter_key(enc))


func _play_index(e: Dictionary, idx: int, key: String) -> void:
	if key == _encounter_key and idx == round_index:
		return
	_encounter_key = key
	super.play_round(e, idx)


## Before round 1 the encounter has no rounds; pose every ship at its deployment cell with an empty round, so the
## view shows the two fleets facing each other instead of an empty sky.
func _show_deployment(e: Dictionary) -> void:
	# Each side faces the other side's centre (facing 0 points along +x of the arena, 90 along +y).
	var sum := {true: Vector2.ZERO, false: Vector2.ZERO}
	var n := {true: 0, false: 0}
	for s in e.get("ships", []):
		var p: Variant = s.get("position")
		if not p is Dictionary:
			continue  # hidden contacts carry no position
		var mine: bool = s.get("mine", false)
		sum[mine] += Vector2(float(p.x), float(p.y))
		n[mine] += 1
	var ships := []
	for s in e.get("ships", []):
		if not s.get("alive", true) or s.get("hidden", false) or not s.get("position") is Dictionary:
			continue
		var pos: Dictionary = s["position"]
		var mine: bool = s.get("mine", false)
		var facing := 0.0 if mine else 180.0
		if n[not mine] > 0:
			var d: Vector2 = sum[not mine] / n[not mine] - Vector2(float(pos.x), float(pos.y))
			if d.length() > 0.01:
				facing = rad_to_deg(atan2(d.y, d.x))
		ships.append({"id": s.get("id", 0), "owner": s.get("owner", 0), "hullBefore": s.get("hull", 1),
			"shieldBefore": s.get("shields", 0), "from": pos, "to": pos, "yaw": facing})
	# Cloaked contacts carry no position, but their hulls are built now too (cloaked, invisible), so round 1 does not
	# instance half a 24v24 fleet on its first frame (R3.5: 186 ms of round 1's 330-400 ms stall).
	for s in e.get("ships", []):
		if s.get("alive", true) and (s.get("hidden", false) or not s.get("position") is Dictionary):
			var side: bool = s.get("mine", false)
			var c: Vector2 = sum[side] / maxf(1, n[side])
			var at := {"x": c.x, "y": c.y}
			ships.append({"id": s.get("id", 0), "owner": s.get("owner", 0), "hullBefore": s.get("hull", 1),
				"shieldBefore": s.get("shields", 0), "from": at, "to": at, "yaw": 0.0, "startsCloaked": true})
	if ships.is_empty():
		return
	var staged := e.duplicate()
	staged["rounds"] = [{"index": 0, "events": [], "ships": ships}]
	super.play_round(staged, 0)
	round_index = -1 # the real round 1 must still start playback
