class_name WebGpuWarm
extends Node
## WebGPU build only. The browser creates each render pipeline the first time something draws with it, synchronously, so
## the first battle round (weapons, shields, hits, explosions, trails) stalled 2.5-3.8 s even with every shader baked
## (WebGPU trial, 19 Nov). Before the first battle opens, App shows its loading panel ("Preparing battle graphics") and
## this node plays the first rounds of the fixture battle, silent, in an off-screen viewport with the battle view's own
## settings, so those pipelines exist before the player's battle draws. Once per session. Frame-counted in _process.

signal finished

const FIXTURE := "res://scenes/battle/fixtures/rules3_battle.json"
const BATTLE_SCENE := "res://scenes/battle/battle_view.tscn"
const ROUNDS := 2
const FRAMES_PER_ROUND := 90

static var done := false

var _sv: SubViewport
var _view: Node
var _enc: Dictionary
var _round := -1
var _frame := 0
var _t0 := 0

## True when this session still has to warm the battle pipelines (WebGPU only).
static func needed() -> bool:
	return not done and GpuReadback.webgpu()

## Plays the warm-up under `host` and returns when it is over.
static func run(host: Node, settings: Dictionary) -> void:
	done = true
	var enc: Variant = JSON.parse_string(FileAccess.get_file_as_string(FIXTURE)) if FileAccess.file_exists(FIXTURE) else null
	if not enc is Dictionary or (enc.get("rounds", []) as Array).is_empty() or not AppLoader.exists(BATTLE_SCENE):
		print("webgpu warm: no fixture battle")
		return
	var w := WebGpuWarm.new()
	w.name = "WebGpuWarm"
	w._enc = enc
	w._t0 = Time.get_ticks_msec()
	w._sv = SubViewport.new()
	w._sv.size = Vector2i(320, 180)
	w._sv.own_world_3d = true
	w._sv.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	DisplaySettings.apply_viewport(w._sv, settings) # the battle host's MSAA and scale: the same pipelines
	w._view = (AppLoader.resource(BATTLE_SCENE) as PackedScene).instantiate()
	if "play_audio" in w._view:
		w._view.play_audio = false # off-screen: the player must not hear it
	w._sv.add_child(w._view)
	w._sv.add_child(w)
	host.add_child(w._sv)
	await w.finished

func _process(_delta: float) -> void:
	if not is_instance_valid(_view):
		_finish()
		return
	if _frame % FRAMES_PER_ROUND == 0:
		_round += 1
		if _round >= mini(ROUNDS, (_enc.get("rounds", []) as Array).size()):
			_finish()
			return
		print("webgpu warm: round %d at frame %d, %d ms" % [_round, _frame, Time.get_ticks_msec() - _t0])
		_view.play_round(_enc, _round)
		if "playback_speed" in _view:
			_view.playback_speed = 4.0
	_frame += 1

func _finish() -> void:
	set_process(false)
	print("webgpu warm: %d battle rounds drawn off-screen in %d ms" % [maxi(_round, 0), Time.get_ticks_msec() - _t0])
	finished.emit()
	if is_instance_valid(_sv):
		_sv.queue_free()
