extends SceneTree
## Headless smoke test of BattleScene: every fixture round plays to round_finished, emits weapon / sfx cues with
## the expected keys, and ship nodes follow the timeline without jumps.
##   godot --headless --path remaster/godot -s res://scenes/battle/tests/test_battle_scene.gd

const FIXTURE := "res://scenes/battle/fixtures/rules3_battle.json"

var failures := 0


func check(cond: bool, what: String) -> void:
	if not cond:
		failures += 1
		printerr("  FAIL  ", what)


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	print("Battle scene tests")
	var encounter: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(FIXTURE))
	var scene := BattleScene.new()
	scene.play_audio = false
	root.add_child(scene)
	await process_frame
	var cues: Array = []
	var finished: Array = []
	scene.cue.connect(func(key, keys, priority, fleet): cues.append([key, keys, priority, fleet]))
	scene.round_finished.connect(func(i): finished.append(i))
	scene.playback_speed = 8.0
	for r in encounter.rounds.size():
		cues.clear()
		scene.play_round(encounter, r)
		check(scene.is_playing(), "round %d playing" % r)
		var last := {}
		var frames := 0
		while scene.is_playing() and frames < 2000:
			await process_frame
			frames += 1
			for fid in [5, 7, 8, 10]:
				var s := scene.ship_node(fid)
				if s == null or not s.visible:
					continue
				if last.has(fid):
					check(s.global_position.distance_to(last[fid]) < BattleScene.CELL * 0.5, "round %d ship %d jumped" % [r, fid])
				last[fid] = s.global_position
		check(finished.has(r), "round %d finished (frames %d)" % [r, frames])
		var fires: Array = encounter.rounds[r].events.filter(func(e): return e.kind == "Fire")
		if not fires.is_empty():
			check(cues.any(func(c): return c[0].begins_with("weapon.")), "round %d weapon cue" % r)
			for c in cues:
				if c[0].begins_with("weapon."):
					check(c[0].count(".") == 2, "faction-specific weapon key %s" % c[0])
					check(c[1][-1].begins_with("weapon.generic."), "generic fallback for %s" % c[0])
		if encounter.rounds[r].events.any(func(e): return e.kind == "Destroyed"):
			check(cues.any(func(c): return c[0] == "sfx.explosion.large"), "round %d explosion cue" % r)
	check(scene.camera != null, "camera exists")
	scene.camera.set_mode(BattleCamera.Mode.ORBIT)
	scene.seek(2.0)
	await process_frame
	scene.queue_free()
	await process_frame
	print("scene tests: %d failures" % failures)
	quit(1 if failures > 0 else 0)
