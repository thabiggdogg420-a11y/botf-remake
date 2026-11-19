extends "res://scenes/battle/battle_scene.gd"
## Adapter between the UI's BattleHost contract (load_encounter / play_round(round, encounter))
## and BattleScene.play_round(encounter, round_index).
##
## The combat screen keeps this view alive across refreshes and calls load_encounter after every order, pick and
## round, so both entry points are idempotent: a round that is already playing or has played stays as it is, and
## only a new round (or a different encounter) starts playback.

var _encounter_key := ""
const DEPLOY_PER_FRAME := 2   ## Hero-model hulls built per frame while the fleets deploy (each can cost tens of ms)
var _stage := {}              ## Staged deployment: {e, ships (the deployment's ship rows), n (rows shown so far)}
var _pending_deploy := {}   ## the encounter whose deployment waits for its hero models (parsed on worker threads)


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
			if BattleAssets.loading():
				# Hero models still parsing on worker threads: building the hulls now would block this frame until they
				# finish (a 1.5-4 s stall on the first battle). Show the sky and planet, and deploy the fleets the frame
				# after the last model lands (M1-BETAFIX).
				_pending_deploy = e
			else:
				_show_deployment(e)
		# Compile the weapon effects while the fleets deploy, not on the first round's first frame (R3.5: 370 ms).
		if not _prewarmed:
			_prewarmed = true
			fx.prewarm.call_deferred()
			BattleShip.flame_flipbook.call_deferred() # damage-fire textures are generated once, here
			BattleShip.puff_texture.call_deferred()
		return
	_play_index(e, rounds.size() - 1, key)


func _process(delta: float) -> void:
	super._process(delta)
	if not _pending_round.is_empty():
		# F2: a round asked for while the hulls were still parsing starts once they have landed and are built.
		if not BattleAssets.loading() and _stage.is_empty() and _pending_deploy.is_empty():
			var pr := _pending_round
			_pending_round = {}
			_play_index(pr.e, int(pr.idx), str(pr.key))
	if not _pending_deploy.is_empty() and not BattleAssets.loading():
		var e := _pending_deploy
		_pending_deploy = {}
		var t := LoadTrace.start()
		_show_deployment(e)
		LoadTrace.done("battle: show deployment", t)
	elif not _stage.is_empty():
		# F2: the hulls are built a couple a frame (hidden), and the deployment is posed once, when the last is built
		# (posing every step rebuilt the deployment timeline each frame: 28-34 ms frames while a fleet arrived).
		var n0 := int(_stage.n)
		_stage.n = mini(n0 + DEPLOY_PER_FRAME, _stage.ships.size())
		var t := LoadTrace.start()
		_build_hulls(_stage.ships.slice(n0, _stage.n))
		t = LoadTrace.done("battle: build hulls %d-%d" % [n0, int(_stage.n)], t)
		if int(_stage.n) >= _stage.ships.size():
			var st := _stage
			_stage = {}
			_deploy_rows(st.e, st.ships)
			LoadTrace.done("battle: deploy rows", t)


var _pending_round := {}   ## F2: {e, idx, key} of a round asked for before the hulls were ready


## Builds the ship nodes for deployment rows without posing them (BattleScene._sync_ships builds the rest).
func _build_hulls(rows: Array) -> void:
	var by_id := {}
	for s in _stage.e.get("ships", []):
		by_id[int(s.id)] = s
	for r in rows:
		var fid := int(r.get("id", 0))
		if _ships.has(fid):
			continue
		var ship := BattleShip.new()
		ship.visible = false
		add_child(ship)
		ship.setup(by_id.get(fid, {"id": fid, "owner": int(r.get("owner", 0))}), assets)
		_ships[fid] = ship


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
	if BattleAssets.loading() or not _stage.is_empty() or not _pending_deploy.is_empty():
		# F2: the hulls are still parsing or being built: building the rest on this frame cost 320-470 ms (the first-open
		# hitch). The round waits for them (a second or two at most on a cold first battle) and then plays in full.
		_pending_round = {"e": e, "idx": idx, "key": key}
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
			# A sensor ghost reports hull -1; round_ship_info drops hull <= 0, so it was never built here and round 1
			# instanced the whole cloaked side on its first frame (M1 profile, 24v24: 70 ms sync, 334 ms worst frame).
			ships.append({"id": s.get("id", 0), "owner": s.get("owner", 0), "hullBefore": maxi(1, int(s.get("hull", 1))),
				"shieldBefore": maxi(0, int(s.get("shields", 0))), "from": at, "to": at, "yaw": 0.0, "startsCloaked": true})
	if ships.is_empty():
		return
	if assets != null and not assets.heroes.is_empty() and ships.size() > DEPLOY_PER_FRAME:
		# Hero-model hulls cost tens of ms each to instance: build a couple per frame instead of the whole fleet on one.
		_stage = {"e": e, "ships": ships, "n": 0}
		return
	_deploy_rows(e, ships)


func _deploy_rows(e: Dictionary, ships: Array) -> void:
	var staged := e.duplicate()
	staged["rounds"] = [{"index": 0, "events": [], "ships": ships}]
	var _t0 := Time.get_ticks_usec()
	super.play_round(staged, 0)
	if OS.get_environment("ROUND_TIMING") != "":
		print("ROUND_TIMING deploy %d rows %d ms" % [ships.size(), (Time.get_ticks_usec() - _t0) / 1000])
	round_index = -1 # the real round 1 must still start playback
