extends SceneTree
## Clip recorder (M4 combat): plays a combat-sandbox battle round after round in BattleView, as the game does (cinematic
## playback, film cuts), for --seconds of game time. Run it with Godot's Movie Maker so every frame is rendered at a fixed
## step: godot --path remaster/godot --resolution 1280x720 --audio-driver Dummy --write-movie out.avi --fixed-fps 30
##   -s res://scenes/battle/tools/battle_clip.gd -- [--ships=6] [--seed=0] [--seconds=30] [--foe-owner=N]
##   [--map=Disruptor:PolaronBeam] [--plan-pause=1.0] [--speed=1.0]
## The sandbox fleets are Federation against Klingon; --foe-owner and --map re-dress the foe's weapons for the look of
## another race (outcomes unchanged). Prints one CLIP line per round with the jobs launched.

var ships := 6
var seed := 0
var seconds := 30.0
var foe_owner := 0
var wmap := {}
var mine_owner := 0
var mine_map := {}
var names_mine := PackedStringArray()
var names_foe := PackedStringArray()
var plan_pause := 1.0
var speed := 1.0
var core: Object
var view: Node3D


func _initialize() -> void:
	for a in OS.get_cmdline_user_args():
		if a.begins_with("--ships="): ships = int(a.split("=")[1])
		elif a.begins_with("--seed="): seed = int(a.split("=")[1])
		elif a.begins_with("--seconds="): seconds = float(a.split("=")[1])
		elif a.begins_with("--foe-owner="): foe_owner = int(a.split("=")[1])
		elif a.begins_with("--plan-pause="): plan_pause = float(a.split("=")[1])
		elif a.begins_with("--speed="): speed = float(a.split("=")[1])
		elif a.begins_with("--mine-owner="): mine_owner = int(a.split("=")[1])
		elif a.begins_with("--names-mine="): names_mine = a.split("=")[1].split(",")
		elif a.begins_with("--names-foe="): names_foe = a.split("=")[1].split(",")
		elif a.begins_with("--mine-map="):
			for pair in a.split("=")[1].split(","):
				var kv := pair.split(":")
				if kv.size() == 2:
					mine_map[kv[0]] = kv[1]
		elif a.begins_with("--map="):
			for pair in a.split("=")[1].split(","):
				var kv := pair.split(":")
				if kv.size() == 2:
					wmap[kv[0]] = kv[1]
	_run.call_deferred()


func _call(method: String, args := {}) -> Variant:
	var reply: Variant = core.request(method, args)
	if reply is Dictionary and reply.has("ok") and not reply.ok:
		printerr("bridge ", method, ": ", reply.get("error", ""))
		return null
	return reply.get("result", reply) if reply is Dictionary and reply.has("result") else reply


func _dress(enc: Dictionary) -> Dictionary:
	if foe_owner <= 0 and wmap.is_empty() and mine_owner <= 0 and mine_map.is_empty() and names_mine.is_empty() and names_foe.is_empty():
		return enc
	enc = enc.duplicate(true)
	var mine := -1
	for s in enc.get("ships", []):
		if s.get("mine", false):
			mine = int(s.get("owner", 0))
	var foes := {}
	var ours := {}
	var ni := 0
	var nf := 0
	for s in enc.get("ships", []):
		var id := int(s.get("id", 0))
		if int(s.get("owner", 0)) == mine:
			ours[id] = true
			if mine_owner > 0:
				s["owner"] = mine_owner
			if not names_mine.is_empty():
				s["name"] = names_mine[ni % names_mine.size()] + " - Cruiser"
				ni += 1
		else:
			foes[id] = true
			if foe_owner > 0:
				s["owner"] = foe_owner
			if not names_foe.is_empty():
				s["name"] = names_foe[nf % names_foe.size()] + " - Cruiser"
				nf += 1
	for r in enc.get("rounds", []):
		for rs in r.get("ships", []):
			var rid := int(rs.get("id", 0))
			if foes.has(rid) and foe_owner > 0:
				rs["owner"] = foe_owner
			if ours.has(rid) and mine_owner > 0:
				rs["owner"] = mine_owner
		for e in r.get("events", []):
			var fid := int(e.get("fleetId", 0))
			if foes.has(fid) and wmap.has(str(e.get("weapon", ""))):
				e["weapon"] = wmap[str(e.get("weapon", ""))]
			if ours.has(fid) and mine_map.has(str(e.get("weapon", ""))):
				e["weapon"] = mine_map[str(e.get("weapon", ""))]
	return enc


func _run() -> void:
	core = Engine.get_singleton("BotfCore")
	var enc: Variant = _call("combatSandbox", {"ships": ships, "seed": seed})
	if not enc is Dictionary:
		quit(1)
		return
	view = load("res://scenes/battle/battle_view.tscn").instantiate()
	view.play_audio = false
	view.film_cuts = true
	root.add_child(view)
	await process_frame
	view.load_encounter(_dress(enc))
	var clock := 0.0
	# Establishing: the deployed fleets for a moment, as the game shows before orders.
	for i in 45:
		await process_frame
	clock += 1.5
	var round_n := 0
	while clock < seconds and not enc.get("finished", false):
		_call("tacticalOrder", {"sandbox": true, "all": true, "targetFleetId": 0, "maneuver": "Engage"})
		enc = _call("tacticalRound", {"sandbox": true})
		round_n += 1
		view.playback_speed = speed
		view.load_encounter(_dress(enc))
		var tl: BattleTimeline = view.timeline
		var counts := {}
		for j in tl.jobs:
			var key: String = BattleTimeline.JOB_NAMES[j.kind]
			counts[key] = counts.get(key, 0) + 1
		print("CLIP round ", round_n, " ships ", tl.ships.size(), " jobs ", counts, " end ", tl.end_seconds)
		var frames := 0
		while view.is_playing():
			await process_frame
			frames += 1
			if frames > 30 * 60:
				break
		clock += float(frames) / 30.0
		print("CLIP round ", round_n, " played ", frames, " frames ", clock, " s")
		for i in int(plan_pause * 30.0):
			await process_frame
		clock += plan_pause
	print("CLIP done rounds ", round_n, " seconds ", clock, " finished ", enc.get("finished", false))
	quit(0)
