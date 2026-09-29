extends Node3D
## Standalone demo: plays the fixture battle round after round, in a loop.
## Keys: C camera mode, Space pause, Left/Right previous/next round, 1-4 playback speed.
##
## Headless screenshots (see scenes/battle/tools/capture.sh):
##   godot --path remaster/godot --fixed-fps 30 res://scenes/battle/battle_demo.tscn -- \
##       --capture=/tmp/shots --shots=4:7.2,5:6.5 [--size=1280x720] [--gallery] [--orbit]

const DEFAULT_FIXTURE := "res://scenes/battle/fixtures/rules3_battle.json"

@export_file("*.json") var fixture_path := DEFAULT_FIXTURE
@export var loop_pause := 1.0

@onready var battle: BattleScene = $BattleScene
@onready var hud: Label = $Hud/Label

var encounter: Dictionary
var current := 0
var _paused_until := -1.0


func _ready() -> void:
	var args := _user_args()
	encounter = _load_fixture(fixture_path)
	if encounter.is_empty():
		hud.text = "No fixture at %s" % fixture_path
		return
	battle.round_finished.connect(_on_round_finished)
	battle.cue.connect(func(key, _keys, _priority, _fleet): _last_cue = key)
	if args.has("capture"):
		hud.visible = args.has("hud")
		_capture(args)
		return
	battle.play_round(encounter, current)


var _last_cue := ""


func _process(_delta: float) -> void:
	if encounter.is_empty() or not hud.visible:
		return
	var rounds: Array = encounter.rounds
	hud.text = "%s  •  Round %d / %d  •  %.1fs  •  camera: %s\n%s\nC camera   Space pause   ← → round   1-4 speed" % [
		str(encounter.get("location", "")), current + 1, rounds.size(), battle.time,
		"cinematic" if battle.camera.mode == BattleCamera.Mode.CINEMATIC else "orbit", _last_cue]


func _unhandled_input(event: InputEvent) -> void:
	if not (event is InputEventKey) or not event.pressed or event.echo:
		return
	match event.keycode:
		KEY_SPACE:
			battle.playback_speed = 0.0 if battle.playback_speed > 0.0 else 1.0
		KEY_RIGHT:
			_goto(current + 1)
		KEY_LEFT:
			_goto(current - 1)
		KEY_1, KEY_2, KEY_3, KEY_4:
			battle.playback_speed = [0.25, 0.5, 1.0, 2.0][event.keycode - KEY_1]


func _goto(i: int) -> void:
	var n: int = encounter.rounds.size()
	current = posmod(i, n)
	battle.play_round(encounter, current)


func _on_round_finished(_i: int) -> void:
	await get_tree().create_timer(loop_pause).timeout
	_goto(current + 1)


static func _load_fixture(path: String) -> Dictionary:
	if not FileAccess.file_exists(path):
		return {}
	var data = JSON.parse_string(FileAccess.get_file_as_string(path))
	# Accept a raw encounter or a recorded botf_call reply {ok, result}.
	if data is Dictionary and data.has("result") and data.result is Dictionary:
		data = data.result
	return data if data is Dictionary and data.has("rounds") else {}


static func _user_args() -> Dictionary:
	var out := {}
	for a in OS.get_cmdline_user_args():
		var kv := a.trim_prefix("--").split("=", true, 1)
		out[kv[0]] = kv[1] if kv.size() > 1 else ""
	return out


# ------------------------------------------------------------------------------------------------------------------
# Headless capture
# ------------------------------------------------------------------------------------------------------------------

func _capture(args: Dictionary) -> void:
	var dir: String = args.capture
	DirAccess.make_dir_recursive_absolute(dir)
	if args.has("size"):
		var wh: PackedStringArray = str(args.size).split("x")
		get_window().size = Vector2i(int(wh[0]), int(wh[1]))
	battle.play_audio = false
	if args.has("gallery"):
		await _capture_gallery(dir)
		get_tree().quit()
		return
	if args.has("orbit"):
		battle.camera.set_mode(BattleCamera.Mode.ORBIT)
	var shots: PackedStringArray = str(args.get("shots", "4:7.0")).split(",")
	for shot in shots:
		var parts := shot.split(":")
		var r := int(parts[0])
		var t := float(parts[1])
		battle.play_round(encounter, r)
		current = r
		# Skip ahead, then play the last stretch in real frames so particles and trails are alive.
		battle.seek(maxf(0.0, t - 1.6))
		while battle.time < t:
			await get_tree().process_frame
		battle.playback_speed = 0.0
		for i in 3:
			await get_tree().process_frame
		await RenderingServer.frame_post_draw
		var path := dir.path_join("battle_r%d_t%s.png" % [r, str(t).replace(".", "_")])
		get_viewport().get_texture().get_image().save_png(path)
		print("captured ", path)
		battle.playback_speed = 1.0
	get_tree().quit()


func _capture_gallery(dir: String) -> void:
	# One of each faction hull plus a starbase, parked in a row.
	var ships := []
	var info_ships := []
	var specs := [[1, "Galaxy — Cruiser", 208, 1], [2, "B'rel — Bird-of-Prey", 60, 3], [3, "D'deridex — Warbird", 220, 5],
		[4, "Galor — Cruiser", 150, 7], [5, "D'Kora — Marauder", 160, 9]]
	for s in specs:
		ships.append({"id": s[3], "owner": s[0], "name": s[1], "maxHull": s[2], "shipClass": "Destroyer", "referenceShipId": 1})
		info_ships.append({"id": s[3], "owner": s[0], "hullBefore": s[2], "shieldBefore": 100, "from": {"x": 4.0 + s[3] * 0.45, "y": 5.0 + (s[3] % 2) * 0.35}, "yaw": 160})
	ships.append({"id": 2000001, "owner": 1, "name": "Starbase", "maxHull": 300, "referenceShipId": -1})
	info_ships.append({"id": 2000001, "owner": 1, "hullBefore": 300, "shieldBefore": 100, "from": {"x": 9.5, "y": 3.5}, "yaw": 0})
	var enc := {"arenaWidth": 16, "arenaHeight": 12, "ships": ships, "rounds": [{"index": 2, "ships": info_ships, "events": []}]}
	battle.formation_spread = 0.0
	battle.play_round(enc, 0)
	battle.camera.set_mode(BattleCamera.Mode.ORBIT)
	battle.camera.orbit_focus = battle.cell_to_world(6.3, 5.1)
	battle.camera.orbit_distance = 27.0
	battle.camera.orbit_yaw = 170.0
	battle.camera.orbit_pitch = -18.0
	battle.camera.auto_orbit_speed = 0.0
	for i in 20:
		await get_tree().process_frame
	await RenderingServer.frame_post_draw
	get_viewport().get_texture().get_image().save_png(dir.path_join("battle_gallery.png"))
	print("captured gallery")
