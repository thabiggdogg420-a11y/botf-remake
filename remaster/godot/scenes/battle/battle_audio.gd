class_name BattleAudio
extends Node
## Plays BattleScene cues. With the BotfAudio autoload present (always, in the game) every cue goes through it:
## the cue scheduler (cooldowns, voice queue), the Effects and Voice buses, ducking, and local files or generated
## tones. Without it (a scene run on its own) the old local path below plays: the first key with a local file
## (BattleAssets), else a short generated tone for sfx and weapon cues when `generated_tones` is on. Voice barks
## (vo.*) play only from files. Up to `voices` sounds at once; the lowest priority is dropped first, and a key
## repeats at most every `repeat_gap` seconds.

@export var generated_tones := true
@export var voices := 16
@export var repeat_gap := 0.06
@export var volume_db := -6.0

var assets: BattleAssets
var _players: Array[AudioStreamPlayer] = []
var _priority: Dictionary = {}   ## player -> priority
var _last: Dictionary = {}       ## key -> time
var _tones: Dictionary = {}

const RATE := 22050


func _ready() -> void:
	if BotfAudioSystem.instance != null:
		return   # the autoload plays everything; no local pool needed
	for i in voices:
		var p := AudioStreamPlayer.new()
		p.volume_db = volume_db
		add_child(p)
		_players.append(p)


## Plays a cue. keys are tried in order; returns the key that played ("" when nothing).
func play(keys: PackedStringArray, priority := 1) -> String:
	if keys.is_empty():
		return ""
	if BotfAudioSystem.instance != null:
		return BotfAudioSystem.instance.play_keys(keys, priority, assets, generated_tones)
	if _players.is_empty():
		return ""
	var now := Time.get_ticks_msec() / 1000.0
	if now - float(_last.get(keys[0], -10.0)) < repeat_gap:
		return ""
	_last[keys[0]] = now
	var stream: AudioStream = assets.stream_for(keys) if assets != null else null
	var played := keys[0]
	if stream == null:
		if not generated_tones or keys[0].begins_with("vo."):
			return ""
		stream = _tone(keys[0])
		if stream == null:
			return ""
	var player := _free_player(priority)
	if player == null:
		return ""
	player.stream = stream
	player.pitch_scale = randf_range(0.95, 1.05)
	player.play()
	_priority[player] = priority
	return played


func _free_player(priority: int) -> AudioStreamPlayer:
	var victim: AudioStreamPlayer = null
	for p in _players:
		if not p.playing:
			return p
		if victim == null or _priority.get(p, 0) < _priority.get(victim, 0):
			victim = p
	if victim != null and _priority.get(victim, 0) <= priority:
		victim.stop()
		return victim
	return null


func _tone(key: String) -> AudioStreamWAV:
	var kind := BattleAudio.battle_kind_for(key)
	if kind == "":
		return null
	if not _tones.has(kind):
		_tones[kind] = BattleAudio.synth(kind)
	return _tones[kind]


## The generated-effect kind for a battle sound key ("" when it has none).
static func battle_kind_for(key: String) -> String:
	var kind := ""
	if key.begins_with("weapon."):
		var t := key.get_slice(".", 1)
		kind = "torpedo" if BattleTimeline.TORPEDO_TYPES.has(t) or t == "generic" and key.ends_with("torpedo") 			else ("pulse" if t in ["PhaseCannon", "PulsePhaser", "DisruptorCannon", "PlasmaCannon"] else "beam")
		if t in ["Disruptor", "DisruptorCannon", "PlasmaCannon"]:
			kind += "_d"
	elif key.begins_with("sfx.shield"):
		kind = "shield"
	elif key == "sfx.explosion.small":
		kind = "hull"
	elif key.begins_with("sfx.explosion"):
		kind = "explosion"
	elif key.begins_with("sfx.cloak") or key.begins_with("sfx.decloak"):
		kind = "cloak"
	elif key.begins_with("sfx.warp"):
		kind = "warp"
	return kind


## Small synthesised effects so the scene is audible without any local audio.
static func synth(kind: String) -> AudioStreamWAV:
	var dur: float = {"beam": 0.55, "beam_d": 0.45, "pulse": 0.18, "pulse_d": 0.2, "torpedo": 0.7, "torpedo_d": 0.7,
		"shield": 0.35, "hull": 0.45, "explosion": 1.8, "cloak": 1.0, "warp": 0.9}.get(kind, 0.4)
	var n := int(dur * RATE)
	var data := PackedByteArray()
	data.resize(n * 2)
	var rng := RandomNumberGenerator.new()
	rng.seed = hash(kind)
	var phase := 0.0
	var lp := 0.0
	for i in n:
		var t := float(i) / RATE
		var u := t / dur
		var s := 0.0
		match kind:
			"beam", "beam_d":
				var f := (880.0 if kind == "beam" else 520.0) - 200.0 * u
				phase += TAU * f / RATE
				s = (sin(phase) * 0.6 + sin(phase * 1.5) * 0.25 + rng.randf_range(-0.1, 0.1)) * minf(t * 60.0, 1.0) * pow(1.0 - u, 1.5)
			"pulse", "pulse_d":
				var f := (1200.0 if kind == "pulse" else 700.0) * (1.0 - 0.6 * u)
				phase += TAU * f / RATE
				s = sign(sin(phase)) * 0.35 * exp(-u * 5.0)
			"torpedo", "torpedo_d":
				var f := 180.0 + 500.0 * u
				phase += TAU * f / RATE
				lp = lerpf(lp, rng.randf_range(-1, 1), 0.2)
				s = (sin(phase) * 0.5 + lp * 0.5) * minf(t * 40.0, 1.0) * exp(-u * 3.0)
			"shield":
				phase += TAU * (300.0 + 150.0 * sin(t * 90.0)) / RATE
				s = sin(phase) * 0.5 * exp(-u * 6.0) + rng.randf_range(-0.2, 0.2) * exp(-u * 12.0)
			"hull":
				lp = lerpf(lp, rng.randf_range(-1, 1), 0.3)
				s = lp * 1.4 * exp(-u * 6.0)
			"explosion":
				lp = lerpf(lp, rng.randf_range(-1, 1), 0.08 + 0.1 * exp(-u * 4.0))
				phase += TAU * (55.0 - 20.0 * u) / RATE
				s = (lp * 2.2 + sin(phase) * 0.4) * minf(t * 80.0, 1.0) * pow(1.0 - u, 2.0)
			"cloak":
				phase += TAU * (200.0 + 600.0 * (u if kind == "cloak" else 1.0 - u)) / RATE
				s = sin(phase) * sin(t * 40.0) * 0.4 * sin(PI * u)
			"warp":
				phase += TAU * (150.0 + 1400.0 * u * u) / RATE
				s = sin(phase) * 0.5 * sin(PI * u) + rng.randf_range(-0.1, 0.1) * u
		var v := clampi(int(clampf(s, -1.0, 1.0) * 26000.0), -32768, 32767)
		data.encode_s16(i * 2, v)
	var w := AudioStreamWAV.new()
	w.format = AudioStreamWAV.FORMAT_16_BITS
	w.mix_rate = RATE
	w.stereo = false
	w.data = data
	return w
