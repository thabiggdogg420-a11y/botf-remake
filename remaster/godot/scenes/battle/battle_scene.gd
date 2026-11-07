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
signal ship_picked(ship_id: int) ## A ship clicked in the view (BattleCamera); the host updates its selection.

## World units per tactical cell. Ships are drawn at true scale against it (BattleShip.display_length): Unreal's
## CellUnits doubled to 170 with the true-scale hulls, so this doubled from 14 with them (a Galaxy spans 1.3 cells).
const CELL := 28.0
## Presentation spread (Simon, 8 Nov 2026: "the battle should be a little more spread out"): grid positions and heights
## are drawn this many times further apart than a cell while hulls keep their true-scale length against CELL, so ships
## sit several lengths apart and the engagement range reads. Rules positions are untouched.
const SPREAD := 1.3
const SPACING_EASE := 2.5       ## 1/s: how fast the presentation spacing offsets relax toward a smaller target.
const SPACING_PUSH_EASE := 5.0  ## 1/s: how fast they grow when hulls close in (12 heaved hulls up and down; R3.5).
## Each round's timeline starts a ship at its home height, and a wing joined at the round start puts it on station;
## the pose it was last shown at eases into the new one over this long, so nothing jumps between rounds.
const ROUND_JOIN_SECONDS := 1.5
const FINISH_TAIL := 0.6        ## Seconds after the last job before round_finished.
const SPACING_SOLVE_INTERVAL := 0.05 ## Seconds between spacing solves (see _update_spacing).
const MAX_FRAME_STEP := 0.05    ## Longest wall-clock step one frame may advance the timeline (a 20 fps floor).

@export var round_seconds := 6.0
@export var playback_speed := 1.0
@export var player_empire := 0  ## 0 = the encounter's own ships (ships[].mine), barks only for them.
@export var play_audio := true  ## Play cues through BattleAudio (local files or generated tones).
@export var generated_tones := true
@export var formation_spread := 1.0 ## Scales the spacing offsets that keep true-scale hulls apart (0 = off).

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
var _prewarmed := false
var _spacing_clock := 0.0
var _spacing_cache: Array[Vector3] = []
var _ship_jobs := {}             ## fleet_id -> the round's cloak/decloak/explosion/warp jobs for that ship
var force_cutaway := false      ## Tools: offer the director's cutaway every round (the close-up strip).
var _shown := {}                 ## fleet_id -> Pose the ship was last shown at (before spacing and station-keeping)
var _carry := {}                 ## fleet_id -> Vector4(x, y, altitude, facing) eased out at the start of the round


func _ready() -> void:
	assets = BattleAssets.new()
	_env = BattleEnvironment.build(self, _arena * CELL)
	camera = BattleCamera.new()
	camera.name = "Camera"
	add_child(camera)
	camera.global_position = Vector3(0, 120, 220)
	camera.look_at(Vector3.ZERO)
	camera.ship_picked.connect(func(id): ship_picked.emit(id))
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
	_sync_ships(info)
	# Flight style and wings follow the length of the class actually shown (the local manifest may know it).
	for i in info:
		var node: BattleShip = _ships.get(int(i.fleetId))
		if node != null:
			i["lengthMeters"] = node.length_meters
	timeline = BattleTimeline.build(rounds[round_index].get("events", []), info, cfg)
	_carry.clear()
	for track in timeline.ships:
		var was: BattleTimeline.Pose = _shown.get(track.fleet_id)
		if was != null:
			var now := timeline.pose_of(track, 0.0)
			_carry[track.fleet_id] = Vector4(was.x - now.x, was.y - now.y, was.altitude - now.altitude,
				BattleTimeline.shortest_turn(now.facing, was.facing))
	_ship_jobs.clear()
	for j in timeline.jobs:
		if j.kind in [BattleTimeline.Job.CLOAK, BattleTimeline.Job.DECLOAK, BattleTimeline.Job.EXPLOSION, BattleTimeline.Job.WARP]:
			if not _ship_jobs.has(j.fleet_id):
				_ship_jobs[j.fleet_id] = []
			_ship_jobs[j.fleet_id].append(j)
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
	if not _prewarmed:
		_prewarmed = true
		fx.prewarm()


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


func cell_to_world(x: float, y: float, altitude := 0.0) -> Vector3:
	return Vector3((x - _arena.x * 0.5 + 0.5) * CELL, altitude * CELL, (y - _arena.y * 0.5 + 0.5) * CELL) * SPREAD


## Ship orientation for a pose: yaw (facing 0 = +x, 90 = +z), then pitch (nose up +) about the ship's starboard axis,
## then roll (bank + = right wing down) about its bow.
static func pose_basis(facing: float, pitch: float, bank: float) -> Basis:
	return Basis(Vector3.UP, -deg_to_rad(facing)) * Basis(Vector3(0, 0, 1), deg_to_rad(pitch)) \
		* Basis(Vector3(1, 0, 0), deg_to_rad(bank))


# ------------------------------------------------------------------------------------------------------------------
# Playback
# ------------------------------------------------------------------------------------------------------------------

func _process(delta: float) -> void:
	if timeline == null:
		return
	# A hitch (first-use shader compile, a model load) must slow playback, not teleport ships across the arena.
	var dt := minf(delta, MAX_FRAME_STEP) * playback_speed
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
			_shown.erase(fid)


static func _owner_from_info(info: Array, fid: int) -> int:
	for i in info:
		if int(i.fleetId) == fid:
			return int(i.ownerId)
	return 0


## The timeline pose (own flight or wing station) plus what is left of the round-start carry.
func _round_pose(track: BattleTimeline.ShipTrack, t: float) -> BattleTimeline.Pose:
	var p := timeline.pose_of(track, t)
	if _carry.has(track.fleet_id):
		var c: Vector4 = _carry[track.fleet_id] * (1.0 - BattleTimeline.smoothstep01(t / ROUND_JOIN_SECONDS))
		p.x += c.x
		p.y += c.y
		p.altitude += c.z
		p.facing = BattleTimeline.wrap_degrees(p.facing + c.w)
	return p


## True-scale hulls can overlap on the grid: shift the visible ones apart (mostly in height) with
## BattleTimeline.spacing_offsets, eased so the formation drifts rather than jumps. A zero-length step snaps only a
## ship that has no spacing yet (a fresh scene). Rules positions are untouched; this is presentation only.
func _update_spacing(t: float, delta: float, poses: Dictionary) -> void:
	var bodies := []
	var tracks := []
	for track in timeline.ships:
		var ship: BattleShip = _ships.get(track.fleet_id)
		var p: BattleTimeline.Pose = poses.get(track.fleet_id)
		var active := ship != null and p != null and track.visibility_at(t) > 0.05
		# Bounding spheres of flat hulls may overlap a little (ABotfCombatScene: 0.75 of the radius).
		bodies.append({"x": p.x if active else 0.0, "y": p.y if active else 0.0, "alt": p.altitude if active else 0.0,
			"radius": 0.75 * ship.half_extents.length() / (CELL * SPREAD) if active else 0.0, "active": active})
		tracks.append(track)
	# The relaxation is the dearest per-frame job at 24 v 24, and the offsets only drift slowly: solve it at 20 Hz and
	# ease every frame (a zero-length step, the first frame of a round or a seek, always solves).
	_spacing_clock += delta
	if delta <= 0.0 or _spacing_clock >= SPACING_SOLVE_INTERVAL or _spacing_cache.size() != bodies.size():
		_spacing_clock = 0.0
		_spacing_cache = BattleTimeline.spacing_offsets(bodies)
	var offsets := _spacing_cache
	var ease := 1.0 if delta <= 0.0 else 1.0 - exp(-SPACING_EASE * delta)
	# Ships closing on each other mid-maneuver need the push at once (a slow ease let an Oberth pass through a Galaxy
	# in the starbase drill); a push that is no longer needed relaxes at the gentle rate.
	var push := 1.0 if delta <= 0.0 else 1.0 - exp(-SPACING_PUSH_EASE * delta)
	for k in tracks.size():
		if not bodies[k].active:
			continue
		var ship: BattleShip = _ships[tracks[k].fleet_id]
		var o: Vector3 = offsets[k] * formation_spread
		var target := Vector3(o.x, o.z, o.y) * CELL * SPREAD # plane x -> world x, altitude -> y, plane y -> z
		var rate := push if target.length() > ship.spacing.length() + 0.01 else ease
		ship.spacing = ship.spacing.lerp(target, 0.0 if delta <= 0.0 and ship.spacing != Vector3.ZERO else rate)


func _fire_cues() -> void:
	while _sound_i < timeline.sounds.size() and timeline.sounds[_sound_i].time <= time:
		var s: Dictionary = timeline.sounds[_sound_i]
		_sound_i += 1
		var ev: Dictionary = encounter.rounds[round_index].events[s.event_index] if s.event_index >= 0 else {}
		_emit_cue(s.keys, s.priority, int(ev.get("fleetId", 0)), str(s.get("cue", "")))
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


## Event-to-cue trace for proof runs ($BOTF_CUE_TRACE=1): one "CUE" line per cue with the round time, the event
## kind, the most specific key, the key and file that resolved, and what played ("" = dropped or silent).
var cue_trace := OS.get_environment("BOTF_CUE_TRACE") != ""

func _emit_cue(keys: PackedStringArray, priority: int, fleet_id: int, event := "bark") -> void:
	cue.emit(keys[0], keys, priority, fleet_id)
	var played := ""
	if play_audio and audio != null:
		played = audio.play(keys, priority)
	if cue_trace:
		var file := ""
		if BotfAudioSystem.instance != null:
			var r: Dictionary = BotfAudioSystem.instance.library.resolve_files(keys, assets)
			if not r.is_empty():
				file = "%s -> %s" % [r.key, str(r.files[0]).get_file() if r.files.size() > 0 else "?"]
		print("CUE t=%.2f round=%d event=%s key=%s file=%s played=%s" % [time, round_index, event, keys[0], file if file != "" else "(none: generated tone)", played])


func _apply(t: float, delta: float) -> void:
	var centroid := Vector3.ZERO
	var count := 0
	var lo := Vector3(INF, INF, INF)
	var hi := -lo
	var me := _player_empire()
	var ours := Vector3.ZERO
	var theirs := Vector3.ZERO
	var n_ours := 0
	var n_theirs := 0
	var ours_lo := Vector3(INF, INF, INF)
	var ours_hi := -ours_lo
	var poses := {}
	for track in timeline.ships:
		if _ships.has(track.fleet_id):
			poses[track.fleet_id] = _round_pose(track, t)
	_update_spacing(t, delta, poses) # wall-clock easing: the drift reads the same at any playback speed
	for track in timeline.ships:
		var ship: BattleShip = _ships.get(track.fleet_id)
		if ship == null:
			continue
		var p: BattleTimeline.Pose = poses[track.fleet_id]
		_shown[track.fleet_id] = p
		var idle := BattleTimeline.station_keeping(track, t, timeline.round_seconds) # holding position still breathes
		var pos := cell_to_world(p.x, p.y, p.altitude + idle.x) + ship.spacing
		ship.transform = Transform3D(pose_basis(p.facing, p.pitch + idle.z, p.bank + idle.y), pos)
		ship.model.transform = Transform3D.IDENTITY
		var vis := track.visibility_at(t)
		var shimmer := 0.0
		for j in _ship_jobs.get(track.fleet_id, []):
			if j.start > t or t > j.start + j.duration:
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
						Vector3(ship.half_extents.x * (s - 1.0) + 3.0 * CELL * pow(u, 4.0), 0, 0))
		ship.set_visibility(vis, shimmer)
		ship.update_shield(t)
		if vis > 0.0 and (track.hide_at < 0.0 or t < track.hide_at):
			centroid += pos
			count += 1
			lo = lo.min(pos)
			hi = hi.max(pos)
			if track.owner_id == me:
				ours += pos
				n_ours += 1
				ours_lo = ours_lo.min(pos)
				ours_hi = ours_hi.max(pos)
			else:
				theirs += pos
				n_theirs += 1
	fx.update(t)
	if count > 0:
		centroid /= count
	else:
		centroid = Vector3.ZERO
	var spread := (hi - lo).length() if count > 1 else 120.0
	if camera != null:
		# Establishing shots frame the player's fleet with the enemy beyond it when both sides are on screen.
		camera.has_sides = n_ours > 0 and n_theirs > 0
		if camera.has_sides:
			camera.side_ours = ours / n_ours
			camera.side_theirs = theirs / n_theirs
			camera.side_ours_spread = (ours_hi - ours_lo).length() if n_ours > 1 else 0.0
	if camera != null and camera.mode == BattleCamera.Mode.ORBIT and camera.orbit_focus == Vector3.ZERO:
		camera.orbit_focus = centroid
	_update_camera(t, delta, centroid, spread)


## The selected ship and target (BattleHost contract): the camera follows and rings them.
func set_selection(ship: int, target: int) -> void:
	if camera != null:
		camera.set_selection(ship, target)


func _update_camera(t: float, delta: float, centroid: Vector3, spread: float) -> void:
	if camera == null:
		return
	director.update(timeline, t)
	var me := _player_empire()
	var by_id := {}
	for s in encounter.get("ships", []):
		by_id[int(s.get("id", 0))] = s
	# Hull and shields as the round shows them: before it, then after it once it has played.
	var state := {}
	var rounds: Array = encounter.get("rounds", [])
	if round_index >= 0 and round_index < rounds.size():
		var done := _finished_sent and not _playing
		for rs in rounds[round_index].get("ships", []):
			state[int(rs.get("id", 0))] = [float(rs.get("hullAfter" if done else "hullBefore", rs.get("hullBefore", 1))),
				float(rs.get("shieldAfter" if done else "shieldBefore", rs.get("shieldBefore", 0)))]
	# What the camera can frame, follow, mark and pick: every ship not cloaked away or destroyed.
	var bodies := []
	for fid in _ships:
		var ship: BattleShip = _ships[fid]
		if not ship.visible or ship.is_queued_for_deletion():
			continue
		var track := timeline.track(fid)
		if track != null and (track.visibility_at(t) < 0.35 or (track.hide_at >= 0.0 and t >= track.hide_at)):
			continue
		var info: Dictionary = by_id.get(fid, {})
		var hs: Array = state.get(fid, [float(info.get("hull", 1)), float(info.get("shields", 0))])
		var bow := ship.global_basis.x
		bodies.append({"id": fid, "center": ship.global_position, "radius": camera.hull_radius(ship), "owner": ship.owner_id,
			"mine": info.get("mine", ship.owner_id == me), "heading": rad_to_deg(atan2(bow.z, bow.x)),
			"name": str(info.get("name", ship.ship_name)).split(" — ")[0],
			"hull": hs[0] / maxf(1.0, float(info.get("maxHull", hs[0]))),
			"shields": hs[1] / maxf(1.0, float(info.get("maxShields", hs[1])))})
	# CHASE: the latest exchange of fire.
	var chase := []
	for j in timeline.jobs:
		if j.start > t:
			break
		if j.fleet_id != 0 and j.target_fleet_id != 0 and j.kind in [BattleTimeline.Job.BEAM, BattleTimeline.Job.PULSE, BattleTimeline.Job.TORPEDO, BattleTimeline.Job.RAM]:
			chase = [j.fleet_id, j.target_fleet_id]
	# CINEMATIC: the director's shot. Between rounds (orders are being chosen) it pulls back to the formations
	# instead of holding the last close-up.
	var shot := {"key": "wide_%d" % round_index, "kind": -1, "centroid": centroid, "spread": spread}
	var subject: BattleShip = _ships.get(director.subject)
	var other: BattleShip = _ships.get(director.other)
	# Cutaways are occasional (coordinator, 8 Nov): about one round in four, or a kill; otherwise the tactical view.
	var cutaway_round := force_cutaway or round_index % 4 == 2 or director.kind == BattleTimeline.Job.EXPLOSION
	if subject != null and cutaway_round and (_playing or not _finished_sent):
		shot = {"key": "%d_%d_%d_%.2f" % [round_index, director.subject, director.other, director.since],
			"kind": director.kind, "subject": subject.global_position, "subject_size": subject.length,
			"other": other.global_position if other != null else centroid, "other_size": other.length if other != null else subject.length, "centroid": centroid, "spread": spread}
	camera.film(delta, t, bodies, chase, shot)
