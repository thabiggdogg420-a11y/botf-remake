class_name BattleScene
extends Node3D
## 3D tactical battle that plays one rules 3 round at a time from the core's event stream.
##
## Host API:
##   play_round(encounter, round_index)  encounter = botf_call("tactical"/"tacticalRound") result,
##                                        round_index = index into encounter.rounds.
##   signal round_finished(round_index)   after the last effect of the round has played.
##   signal cue(key, keys, priority, fleet_id)
##                                        audio cue at the right moment: key = most specific key
##                                        (weapon.<Type>.<fac>, sfx.*, vo.combat.<detail>.<fac>), keys = the full
##                                        fallback list to try in order.
## Plus: seek(seconds), stop(), is_playing(), camera (BattleCamera), playback_speed, player_empire.

signal round_finished(round_index: int)
signal cue(key: String, keys: PackedStringArray, priority: int, fleet_id: int)

const CELL := 14.0              ## World units per tactical cell.
const FINISH_TAIL := 0.6        ## Seconds after the last job before round_finished.

@export var round_seconds := 6.0
@export var playback_speed := 1.0
@export var player_empire := 0  ## 0 = the encounter's own ships (ships[].mine), barks only for them.
@export var play_audio := true  ## Play cues through BattleAudio (local files or generated tones).
@export var generated_tones := true
@export var formation_spread := 1.0 ## Scales the per-ship offsets that spread ships sharing a cell.

var encounter: Dictionary = {}
var round_index := -1
var timeline: BattleTimeline
var time := 0.0
var camera: BattleCamera
var fx: BattleFx
var audio: BattleAudio
var assets: BattleAssets
var director := BattleTimeline.Director.new()

var _ships := {}                 ## fleet_id -> BattleShip
var _arena := Vector2(16, 12)
var _playing := false
var _finished_sent := true
var _sound_i := 0
var _bark_i := 0
var _env := {}


func _ready() -> void:
	assets = BattleAssets.new()
	_env = BattleEnvironment.build(self, _arena * CELL)
	camera = BattleCamera.new()
	camera.name = "Camera"
	add_child(camera)
	camera.global_position = Vector3(0, 60, 110)
	camera.look_at(Vector3.ZERO)
	fx = BattleFx.new()
	fx.name = "Fx"
	fx.scene = self
	add_child(fx)
	audio = BattleAudio.new()
	audio.name = "Audio"
	audio.assets = assets
	audio.generated_tones = generated_tones
	add_child(audio)


# ------------------------------------------------------------------------------------------------------------------
# Public API
# ------------------------------------------------------------------------------------------------------------------

func play_round(p_encounter: Dictionary, p_round_index: int) -> void:
	encounter = p_encounter
	round_index = p_round_index
	var rounds: Array = encounter.get("rounds", [])
	if round_index < 0 or round_index >= rounds.size():
		push_warning("BattleScene.play_round: no round %d" % round_index)
		return
	_arena = Vector2(float(encounter.get("arenaWidth", 16)), float(encounter.get("arenaHeight", 12)))
	var cfg := BattleTimeline.Config.new()
	cfg.round_seconds = round_seconds
	cfg.player_empire = _player_empire()
	var info := BattleTimeline.round_ship_info(encounter, round_index)
	timeline = BattleTimeline.build(rounds[round_index].get("events", []), info, cfg)
	_sync_ships(info)
	for s in _ships.values():
		s.clear_effects()
	fx.reset(timeline)
	director.reset()
	time = 0.0
	_sound_i = 0
	_bark_i = 0
	_playing = true
	_finished_sent = false
	_apply(0.0, 0.0)


## Jumps to `seconds` into the current round without firing the cues before it.
func seek(seconds: float) -> void:
	if timeline == null:
		return
	time = clampf(seconds, 0.0, timeline.end_seconds + FINISH_TAIL)
	while _sound_i < timeline.sounds.size() and timeline.sounds[_sound_i].time <= time:
		_sound_i += 1
	while _bark_i < timeline.barks.size() and timeline.barks[_bark_i].time <= time:
		_bark_i += 1
	_apply(time, 0.0)


func stop() -> void:
	_playing = false


func is_playing() -> bool:
	return _playing


func ship_node(fleet_id: int) -> BattleShip:
	return _ships.get(fleet_id)


func cell_to_world(x: float, y: float) -> Vector3:
	return Vector3((x - _arena.x * 0.5 + 0.5) * CELL, 0.0, (y - _arena.y * 0.5 + 0.5) * CELL)


# ------------------------------------------------------------------------------------------------------------------
# Playback
# ------------------------------------------------------------------------------------------------------------------

func _process(delta: float) -> void:
	if timeline == null:
		return
	var dt := delta * playback_speed
	if _playing:
		time += dt
		_fire_cues()
		if not _finished_sent and time >= timeline.end_seconds + FINISH_TAIL:
			_finished_sent = true
			_playing = false
			round_finished.emit(round_index)
	_apply(time, delta)


func _player_empire() -> int:
	if player_empire > 0:
		return player_empire
	for s in encounter.get("ships", []):
		if s.get("mine", false):
			return int(s.owner)
	return int(encounter.get("controller", 0))


func _sync_ships(info: Array) -> void:
	var present := {}
	for i in info:
		present[int(i.fleetId)] = true
	var by_id := {}
	for s in encounter.get("ships", []):
		by_id[int(s.id)] = s
	for fid in present:
		if not _ships.has(fid):
			var ship := BattleShip.new()
			add_child(ship)
			ship.setup(by_id.get(fid, {"id": fid, "owner": _owner_from_info(info, fid)}), assets)
			_ships[fid] = ship
	for fid in _ships.keys():
		if not present.has(fid):
			_ships[fid].queue_free()
			_ships.erase(fid)


static func _owner_from_info(info: Array, fid: int) -> int:
	for i in info:
		if int(i.fleetId) == fid:
			return int(i.ownerId)
	return 0


## Fixed per-ship offset inside its cell so ships sharing a cell fly in loose formation (constant across rounds).
func _formation_offset(fid: int, ship: BattleShip) -> Vector3:
	var h := hash(fid)
	var a := float(h % 6283) / 1000.0
	var r := CELL * (0.22 + 0.22 * float((h / 7) % 100) / 100.0)
	var y := (float((h / 11) % 100) / 100.0 - 0.5) * CELL * 0.45
	if ship.is_station:
		return Vector3(0, -CELL * 0.2, 0)
	return Vector3(cos(a) * r, y, sin(a) * r) * formation_spread


func _fire_cues() -> void:
	while _sound_i < timeline.sounds.size() and timeline.sounds[_sound_i].time <= time:
		var s: Dictionary = timeline.sounds[_sound_i]
		_sound_i += 1
		var ev: Dictionary = encounter.rounds[round_index].events[s.event_index] if s.event_index >= 0 else {}
		_emit_cue(s.keys, s.priority, int(ev.get("fleetId", 0)))
	while _bark_i < timeline.barks.size() and timeline.barks[_bark_i].time <= time:
		var b: Dictionary = timeline.barks[_bark_i]
		_bark_i += 1
		var owner := 0
		var track := timeline.track(b.fleet_id)
		if track != null:
			owner = track.owner_id
		var keys := BattleTimeline.bark_keys(b.detail, owner)
		if not keys.is_empty():
			_emit_cue(keys, b.priority, b.fleet_id)


func _emit_cue(keys: PackedStringArray, priority: int, fleet_id: int) -> void:
	cue.emit(keys[0], keys, priority, fleet_id)
	if play_audio and audio != null:
		audio.play(keys, priority)


func _apply(t: float, delta: float) -> void:
	var centroid := Vector3.ZERO
	var count := 0
	var lo := Vector3(INF, INF, INF)
	var hi := -lo
	for track in timeline.ships:
		var ship: BattleShip = _ships.get(track.fleet_id)
		if ship == null:
			continue
		var p := track.pose_at(t)
		var pos := cell_to_world(p.x, p.y) + _formation_offset(track.fleet_id, ship)
		ship.transform = Transform3D(Basis.from_euler(Vector3(deg_to_rad(p.bank), -deg_to_rad(p.facing), 0.0)), pos)
		ship.model.transform = Transform3D.IDENTITY
		var vis := track.visibility_at(t)
		var shimmer := 0.0
		for j in timeline.jobs:
			if j.fleet_id != track.fleet_id or j.start > t or t > j.start + j.duration:
				continue
			var u: float = (t - j.start) / maxf(j.duration, 1e-4)
			match j.kind:
				BattleTimeline.Job.CLOAK, BattleTimeline.Job.DECLOAK:
					shimmer = maxf(shimmer, sin(PI * u))
				BattleTimeline.Job.EXPLOSION:
					# Tumble and crumple inside the fireball until it swallows the ship.
					var k := smoothstep(0.0, 0.35, u)
					var axis := Vector3(0.3, 1.0, 0.6).normalized()
					ship.model.transform = Transform3D(Basis(axis, k * 1.2).scaled(Vector3.ONE * (1.0 - 0.35 * k)), Vector3.ZERO)
				BattleTimeline.Job.WARP:
					var s := 1.0 + 12.0 * u * u * u
					ship.model.transform = Transform3D(Basis.from_scale(Vector3(s, 1.0 - 0.5 * u, 1.0 - 0.5 * u)),
						Vector3(ship.half_extents.x * (s - 1.0) + 40.0 * pow(u, 4.0), 0, 0))
		ship.set_visibility(vis, shimmer)
		ship.update_shield(t)
		if vis > 0.0 and (track.hide_at < 0.0 or t < track.hide_at):
			centroid += pos
			count += 1
			lo = lo.min(pos)
			hi = hi.max(pos)
	fx.update(t)
	if count > 0:
		centroid /= count
	else:
		centroid = Vector3.ZERO
	var spread := (hi - lo).length() if count > 1 else 60.0
	if camera != null and camera.mode == BattleCamera.Mode.ORBIT and camera.orbit_focus == Vector3.ZERO:
		camera.orbit_focus = centroid
	_update_camera(t, delta, centroid, spread)


func _update_camera(t: float, delta: float, centroid: Vector3, spread: float) -> void:
	if camera == null:
		return
	director.update(timeline, t)
	var subject: BattleShip = _ships.get(director.subject)
	var other: BattleShip = _ships.get(director.other)
	if subject == null:
		camera.frame(delta, "wide_%d" % round_index, -1, centroid, 10.0, centroid, centroid, spread, t)
		return
	var other_pos := other.global_position if other != null else centroid
	var key := "%d_%d_%d_%.2f" % [round_index, director.subject, director.other, director.since]
	camera.frame(delta, key, director.kind, subject.global_position, subject.length, other_pos, centroid, spread, t)
