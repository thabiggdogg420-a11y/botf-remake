class_name WebGpuWarm
extends Node
## WebGPU build only. The browser creates each render pipeline the first time something draws with it, synchronously, so
## the first battle round (weapons, shields, hits, explosions, trails) stalled 2.5-3.8 s even with every shader baked
## (WebGPU trial, 19 Nov). Before the first battle opens, App shows its loading panel ("Preparing battle graphics") and
## this node plays the first rounds of the fixture battle, silent, in an off-screen viewport with the battle view's own
## settings, so those pipelines exist before the player's battle draws. Once per session. Frame-counted in _process.
##
## Round 3 (patched fork template, "async-all"): the freeze itself came from the GPU process compiling each pipeline
## (DXC, 0.2-3.9 s each) on the thread that also presents frames. The patched driver compiles every pipeline through
## the async API instead and skips a pending pipeline's draws, so nothing freezes; globalThis.godotWebgpuPending says
## how many are still compiling. So the warm-up now starts in the background as soon as the galaxy screen is up
## (start()), waits until the GPU process has finished every pipeline it asked for, then turns async-all off
## (globalThis.godotWebgpuAsyncAll = false) so one-shot renders after that are never skipped. Battle open waits only
## for whatever is still compiling (run()).

signal finished

const FIXTURE := "res://scenes/battle/fixtures/rules3_battle.json"
const BATTLE_SCENE := "res://scenes/battle/battle_view.tscn"
const ROUNDS := 2
const FRAMES_PER_ROUND := 90

static var done := false
static var busy := false
static var _current: WebGpuWarm = null

const SETTLE_FRAMES := 20     ## frames in a row with nothing compiling before the warm-up counts as finished
const SETTLE_CAP_MS := 120000 ## never wait longer than this for the GPU process

var _sv: SubViewport
var _view: Node
var _enc: Dictionary
var _round := -1
var _frame := 0
var _t0 := 0

## True when this session still has to warm the battle pipelines (WebGPU only).
static func needed() -> bool:
	return not done and GpuReadback.webgpu()

## Pipelines the patched driver still has compiling (0 on an unpatched template, which publishes nothing).
static func pending() -> int:
	if not OS.has_feature("web"):
		return 0
	return int(JavaScriptBridge.eval("(globalThis.godotWebgpuPending|0) + (globalThis.godotWebgpuQueued|0)"))

## Starts the warm-up in the background under `host` (galaxy screen); the frame keeps running.
static func start(host: Node, settings: Dictionary) -> void:
	if done or busy or not GpuReadback.webgpu():
		return
	_begin(host, settings)

## Battle open: starts the warm-up if it has not started, and returns once it is over.
static func run(host: Node, settings: Dictionary) -> void:
	if not done and not busy:
		_begin(host, settings)
	if busy and is_instance_valid(_current):
		await _current.finished

static func _begin(host: Node, settings: Dictionary) -> void:
	busy = true
	print("webgpu warm: start, %d pipelines compiling" % pending())
	var enc: Variant = JSON.parse_string(FileAccess.get_file_as_string(FIXTURE)) if FileAccess.file_exists(FIXTURE) else null
	if not enc is Dictionary or (enc.get("rounds", []) as Array).is_empty() or not AppLoader.exists(BATTLE_SCENE):
		print("webgpu warm: no fixture battle")
		_settle_only(host)
		return
	# Web smoothness pass (19 Nov): the fixture's hero hulls load a step per frame first (BattleAssets sliced loads);
	# _process waits for them, so no frame parses a whole glb.
	BattleAssets.shared().warm(BattleAssets.encounter_keys(enc))
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
	_current = w

## No fixture: still wait for the pipelines already asked for.
static func _settle_only(host: Node) -> void:
	var w := WebGpuWarm.new()
	w.name = "WebGpuWarm"
	w._t0 = Time.get_ticks_msec()
	w._settling = true
	host.add_child(w)
	_current = w

var _settling := false
var _quiet := 0
var _ts := 0

func _process(_delta: float) -> void:
	if _settling:
		_quiet = _quiet + 1 if pending() == 0 else 0
		if _quiet >= SETTLE_FRAMES or Time.get_ticks_msec() - _ts > SETTLE_CAP_MS:
			_done()
		return
	if not is_instance_valid(_view):
		_finish()
		return
	if _round < 0 and BattleAssets.loading():
		return # hero hulls still loading a step per frame
	# Round 8: a fixture hull whose pack landed after _begin was not queued then, and the first round parsed it whole in one
	# frame (2.6-3.0 s at galaxy open). Queue any such hull now and wait for it, a step per frame, like the others.
	if _round < 0 and BattleAssets.shared().warm(BattleAssets.encounter_keys(_enc)) > 0:
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
	print("webgpu warm: %d battle rounds drawn off-screen in %d ms, %d pipelines still compiling" % [maxi(_round, 0), Time.get_ticks_msec() - _t0, pending()])
	if is_instance_valid(_view):
		_view.process_mode = Node.PROCESS_MODE_DISABLED
	if is_instance_valid(_sv):
		_sv.render_target_update_mode = SubViewport.UPDATE_DISABLED
	_settling = true
	_ts = Time.get_ticks_msec()

func _done() -> void:
	set_process(false)
	print("webgpu warm: done in %d ms (%d still compiling)" % [Time.get_ticks_msec() - _t0, pending()])
	if OS.has_feature("web"):
		JavaScriptBridge.eval("globalThis.godotWebgpuAsyncAll = false;")
	done = true
	busy = false
	finished.emit()
	if is_instance_valid(_sv):
		_sv.queue_free()
	else:
		queue_free()
