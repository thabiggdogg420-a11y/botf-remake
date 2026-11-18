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
## Plus: seek(seconds), skip_round(), stop(), is_playing(), camera (BattleCamera), playback_speed, player_empire.

signal round_finished(round_index: int)
signal cue(key: String, keys: PackedStringArray, priority: int, fleet_id: int)
signal ship_picked(ship_id: int) ## A ship clicked in the view (BattleCamera); the host updates its selection.
signal subsystem_picked(target_id: int, subsystem: String) ## Plan phase: a subsystem pip clicked on the target.

## World units per tactical cell. Ships are drawn at true scale against it (BattleShip.display_length): Unreal's
## CellUnits doubled to 170 with the true-scale hulls, so this doubled from 14 with them (a Galaxy spans 1.3 cells).
const CELL := 28.0
## Presentation spread (Simon, 8 Nov 2026: "the battle should be a little more spread out"): grid positions and heights
## are drawn this many times further apart than a cell while hulls keep their true-scale length against CELL, so ships
## sit several lengths apart and the engagement range reads. Rules positions are untouched.
const SPREAD := 1.8 ## M4 flight stage 3 (Simon, 14 Nov: "the ships all fly a little too close to each other"): was 1.2.
const SPACING_EASE := 2.5       ## 1/s: how fast the presentation spacing offsets relax toward a smaller target.
const SPACING_PUSH_EASE := 5.0  ## 1/s: how fast they grow when hulls close in (12 heaved hulls up and down; R3.5).
## Each round's timeline starts a ship at its home height, and a wing joined at the round start puts it on station;
## the pose it was last shown at eases into the new one over this long, so nothing jumps between rounds.
const ROUND_JOIN_SECONDS := 1.5
const IDLE_ROUND_SPEED := 10.0  ## M4: a round with no weapon fire plays at this multiple of the playback speed.
var last_round_idle := false    ## The round last played had no weapon fire (the combat screen chains straight into the next).
var idle_fast_forward := true
var _fast_forward := 1.0
const FINISH_TAIL := 0.25       ## Seconds after the last job before round_finished.
const SPACING_SOLVE_INTERVAL := 0.05 ## Seconds between spacing solves (see _update_spacing).
const MAX_FRAME_STEP := 0.05    ## Longest wall-clock step one frame may advance the timeline (a 20 fps floor).

## A round plays back as one continuous sequence this long at 1x (M1 battleplay: ships fly the whole round, fire is
## spread across it, torpedoes travel; BattleTimeline "Cinematic playback").
@export var round_seconds := 8.0
@export var cinematic := true
@export var playback_speed := 1.0
@export var player_empire := 0  ## 0 = the encounter's own ships (ships[].mine), barks only for them.
@export var play_audio := true  ## Play cues through BattleAudio (local files or generated tones).
@export var generated_tones := true
@export var film_cuts := true ## Cinematic camera cuts in 1-3 s shots near the action (battlechoreo, proposal 8).
@export var formation_spread := 1.0 ## Scales the spacing offsets that keep true-scale hulls apart (0 = off).
## M4 flight (Simon, 14 Nov 2026): ships are flown by the continuous flight-and-doctrine sim ported from the LCARS battle
## screensaver (BattleFlight), one sim for the whole battle carried from round to round. Off = the M4 combat planner.
@export var flight_sim := true
@export var shot_director := true ## M4 flight stage 2: show-style held moving shots (BattleScene._direct_shot) over the old 1-3 s cuts.
var flight := BattleFlight.new()
var threaded_build := false  ## F2: build each round's timeline on a worker thread (the game's battle host turns it on)
var _build_token := 0
var _building := false
var _build_task := -1
var _flight_key := -1
var _flight_round := -1

var encounter: Dictionary = {}
var round_index := -1
var timeline: BattleTimeline
var time := 0.0
var camera: BattleCamera
var fx: BattleFx
var audio: BattleAudio
var assets: BattleAssets
var director := BattleTimeline.Director.new()
var plan: BattlePlan              ## Plan-phase overlays (arcs, range bands, move preview, forecast, subsystem pips).

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
var prof := [0, 0, 0, 0]       ## Tools: usec of poses, spacing, ships, effects in the last _apply.
var last_apply_usec := 0     ## Tools: script time of the last frame's _apply (poses, spacing, effects, camera).
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
	plan = BattlePlan.new()
	plan.name = "Plan"
	plan.scene = self
	add_child(plan)
	plan.subsystem_picked.connect(func(t, k): subsystem_picked.emit(t, k))
	audio = BattleAudio.new()
	audio.name = "Audio"
	audio.assets = assets
	audio.generated_tones = generated_tones
	add_child(audio)


# ------------------------------------------------------------------------------------------------------------------
# Public API
# ------------------------------------------------------------------------------------------------------------------

func play_round(p_encounter: Dictionary, p_round_index: int) -> void:
	if _build_task >= 0:
		# A build still running for a superseded round shares the flight sim: let it finish before anything here touches
		# the sim (flight.reset/commit below cleared its ship list under the worker: capture-battles crash, 18 Nov).
		WorkerThreadPool.wait_for_task_completion(_build_task)
		_build_task = -1
	encounter = BattleSynth.with_events(p_encounter) # rules 1-2 rounds have no event stream: present them from the round's result
	round_index = p_round_index
	BattleEnvironment.set_planet_class(_env, str(encounter.get("planetClass", ""))) # the battle system's planet
	var rounds: Array = encounter.get("rounds", [])
	if round_index < 0 or round_index >= rounds.size():
		push_warning("BattleScene.play_round: no round %d" % round_index)
		return
	_arena = Vector2(float(encounter.get("arenaWidth", 16)), float(encounter.get("arenaHeight", 12)))
	var cfg := BattleTimeline.Config.new()
	cfg.round_seconds = round_seconds
	cfg.cinematic = cinematic
	cfg.player_empire = _player_empire()
	cfg.encounter = encounter
	if flight_sim and cinematic:
		# One continuous flight per battle: the next round starts where this one left every ship (no stop at the edge).
		var key := BattleFlight.encounter_seed(encounter)
		if key == _flight_key and round_index == _flight_round + 1 and timeline != null:
			flight.commit(timeline.end_seconds + FINISH_TAIL)
		else:
			flight.reset()
			flight.seed = key
			_dir = {"n": -1, "until": -1.0} # a new battle opens on the director's establisher
			_dir_recent.clear()
		_flight_key = key
		_flight_round = round_index
		cfg.flight = flight
	var _pt0 := Time.get_ticks_usec()
	var info := BattleTimeline.round_ship_info(encounter, round_index)
	_sync_ships(info)
	for w0 in rounds[round_index].get("wego", []):
		var node0: BattleShip = _ships.get(int(w0.get("id", 0)))
		if node0 != null:
			node0.facings_after = w0.get("facings", [])
	var _pt1 := Time.get_ticks_usec()
	# Flight style and wings follow the length of the class actually shown (the local manifest may know it).
	for i in info:
		var node: BattleShip = _ships.get(int(i.fleetId))
		if node != null:
			i["lengthMeters"] = node.length_meters
	var evs: Array = rounds[round_index].get("events", [])
	if threaded_build and AppLoader.threads() and DisplayServer.get_name() != "headless":
		# F2: the round's playback (flight sim and schedule) is built on a worker thread over a few frames: on the main
		# thread it cost 20-35 ms at 4v5 and more at 24v24, on the first frame of every round. The view holds the last
		# pose meanwhile (the previous round has finished, so nothing moves).
		var token := _build_token + 1
		_build_token = token
		_playing = false
		var holder := {}
		_building = true
		var task := WorkerThreadPool.add_task(func(): holder.tl = BattleTimeline.build(evs, info, cfg), false, "botf round build")
		_build_task = task
		while token == _build_token and not WorkerThreadPool.is_task_completed(task):
			await get_tree().process_frame
		if token != _build_token:
			return # superseded: the newer round waited for this build and collected it
		if _build_task == task:
			WorkerThreadPool.wait_for_task_completion(task)
			_build_task = -1
		if token != _build_token or not is_inside_tree():
			return # a newer round (or the end of the battle) took over
		_building = false
		timeline = holder.tl
	else:
		_build_token += 1 # any threaded build still waiting is superseded
		_building = false
		timeline = BattleTimeline.build(evs, info, cfg)
	# A round of nothing but closing in (no weapon fires) is run through at speed: the player asked for a battle, and the
	# approach is two rounds of empty space before the first torpedo (Simon, 14 Nov).
	last_round_idle = true
	for jb in timeline.jobs:
		if jb.kind in [BattleTimeline.Job.BEAM, BattleTimeline.Job.PULSE, BattleTimeline.Job.TORPEDO, BattleTimeline.Job.RAM]:
			last_round_idle = false
			break
	_fast_forward = IDLE_ROUND_SPEED if (last_round_idle and idle_fast_forward) else 1.0
	var _pt2 := Time.get_ticks_usec()
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
	if OS.get_environment("ROUND_TIMING") != "":
		print("ROUND_TIMING play sync=%d build=%d rest=%d ms ships=%d" % [(_pt1 - _pt0) / 1000, (_pt2 - _pt1) / 1000, (Time.get_ticks_usec() - _pt2) / 1000, _ships.size()])
	clear_plan()
	director.reset()
	time = 0.0
	_rescue_until = -1.0
	_empty_clock = 0.0
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


## Skips the rest of the round: jumps to its end (no cues for what is skipped) and lets round_finished fire on the next
## frame, so the host moves on as if it had played. The ships stand where the core left them.
func skip_round() -> void:
	if timeline == null or _finished_sent:
		return
	_playing = true
	seek(timeline.end_seconds + FINISH_TAIL)


func is_playing() -> bool:
	return _playing


func ship_node(fleet_id: int) -> BattleShip:
	return _ships.get(fleet_id)


## World transform of a ship at round time t (its flown pose, today's spacing offset), for effects that start from where
## a ship was: a torpedo leaves the tube where it was at launch, and keeps flying when the ship moves on.
func ship_transform_at(fleet_id: int, t: float) -> Transform3D:
	var ship: BattleShip = _ships.get(fleet_id)
	var track := timeline.track(fleet_id) if timeline != null else null
	if ship == null or track == null:
		return ship.global_transform if ship != null else Transform3D.IDENTITY
	var p := _round_pose(track, t)
	var idle := BattleTimeline.station_keeping(track, t, timeline.round_seconds)
	return Transform3D(pose_basis(p.facing, p.pitch + idle.z, p.bank + idle.y), cell_to_world(p.x, p.y, p.altitude + idle.x) + ship.spacing)


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
	if timeline == null or _building:
		return
	# A hitch (first-use shader compile, a model load) must slow playback, not teleport ships across the arena.
	var dt := minf(delta, MAX_FRAME_STEP) * playback_speed * _fast_forward
	if _killcam_dir != "" and _playing and _killcam_step(dt):
		dt = 0.0
	if _playing:
		time += dt
		_fire_cues()
		if not _finished_sent and time >= timeline.end_seconds + FINISH_TAIL:
			_finished_sent = true
			_playing = false
			round_finished.emit(round_index)
	var t_us := Time.get_ticks_usec()
	_apply(time, delta)
	last_apply_usec = Time.get_ticks_usec() - t_us


## Kill-frame capture (diagnostic only, off unless asked): BOTF_KILLCAM=<dir> on the desktop, or ?killcam in the web
## build's URL. At each of the first KILLCAM_MAX kills the playback stops at the breakup and KILLCAM_AT seconds after it,
## holds each moment for KILLCAM_HOLD_MS of real time with the whole engine frozen (time_scale 0, particles too), saves
## <dir>/kill<n>_<k>.png on the desktop and prints "KILLCAM HOLD n k t=..." for a browser driver to screenshot.
const KILLCAM_AT := [0.04, 0.3, 0.75, 1.4]
var KILLCAM_MAX := 99 if (OS.get_environment("BOTF_KILLCAM_ALL") != "" or (OS.has_feature("web") and str(JavaScriptBridge.eval("location.search")).contains("killall"))) else 3 ## every kill of the battle when asked
var _killcam_job := -1
const KILLCAM_HOLD_MS := 1800
var _killcam_dir := _killcam_init()
var _killcam_done := {}   ## explosion job index -> next KILLCAM_AT index
var _killcam_kills := 0
var _killcam_hold := -1   ## ticks_msec the current hold ends (-1 = none)
var _killcam_saved := false
var _killcam_k := 0

static func _killcam_init() -> String:
	var d := OS.get_environment("BOTF_KILLCAM")
	if d == "" and OS.has_feature("web") and str(JavaScriptBridge.eval("location.search")).contains("killcam"):
		d = "web"
	return d

## True while a hold freezes playback.
func _killcam_step(dt: float) -> bool:
	if _killcam_hold >= 0:
		if not _killcam_saved:
			_killcam_saved = true # the frame drawn since the hold began shows the frozen moment
			# Framing measure: the kill's share of the frame (fire lobes) and whether it is on screen, plus the shot.
			var ks := fx.kill_screen_share(_killcam_job)
			print("KILLCAM FRAME %d %d share=%.3f on=%s at=%s shot=%s subj=%d" % [_killcam_kills, _killcam_k, float(ks[0]), str(ks[1]),
				str(ks[2]) if ks.size() > 2 else "-", str(_dir.get("type", "")), int(_dir.get("subject", 0))])
			if _killcam_dir != "web":
				var img := get_viewport().get_texture().get_image()
				img.save_png(_killcam_dir.path_join("kill%d_%d.png" % [_killcam_kills, _killcam_k]))
		if Time.get_ticks_msec() < _killcam_hold:
			return true
		_killcam_hold = -1
		Engine.time_scale = 1.0
	for i in timeline.jobs.size():
		var j: Dictionary = timeline.jobs[i]
		if j.kind != BattleTimeline.Job.EXPLOSION:
			continue
		var boom: float = j.start + float(j.get("breakup", 0.0))
		var k: int = _killcam_done.get(i, 0)
		if k >= KILLCAM_AT.size() or (k == 0 and _killcam_kills >= KILLCAM_MAX):
			continue
		var at: float = boom + KILLCAM_AT[k]
		if time < at and time + dt >= at:
			if k == 0:
				_killcam_kills += 1
			time = at
			_killcam_done[i] = k + 1
			_killcam_k = k
			_killcam_job = i
			_killcam_hold = Time.get_ticks_msec() + KILLCAM_HOLD_MS
			_killcam_saved = false
			Engine.time_scale = 0.0
			print("KILLCAM HOLD %d %d t=%.2f boom=%.2f fleet=%d" % [_killcam_kills, k, at, boom, int(j.fleet_id)])
			return true
	return false


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
			if OS.get_environment("ROUND_TIMING") != "":
				print("ROUND_TIMING sync builds ship %d (round %d)" % [fid, round_index])
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
		# R3.5 (coordinator, 8 Nov: 24v24 hulls overlapped): full bounding spheres plus a 10% gap, no overlap.
		bodies.append({"x": p.x if active else 0.0, "y": p.y if active else 0.0, "alt": p.altitude if active else 0.0,
			"radius": 1.6 * ship.half_extents.length() / (CELL * SPREAD) if active else 0.0, "active": active})
		tracks.append(track)
	# The relaxation is the dearest per-frame job at 24 v 24, and the offsets only drift slowly: solve it at 20 Hz and
	# ease every frame (a zero-length step, the first frame of a round or a seek, always solves).
	_spacing_clock += delta
	if delta <= 0.0 or _spacing_clock >= SPACING_SOLVE_INTERVAL or _spacing_cache.size() != bodies.size():
		_spacing_clock = 0.0
		_spacing_cache = BattleTimeline.spacing_offsets(bodies, 6, 2.5, 6.0) # battlechoreo fps: 10 passes cost 5 ms a solve with ships on the move
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
	var _u0 := Time.get_ticks_usec()
	var poses := {}
	for track in timeline.ships:
		if _ships.has(track.fleet_id):
			poses[track.fleet_id] = _round_pose(track, t)
	var _u1 := Time.get_ticks_usec()
	_update_spacing(t, delta, poses)
	var _u2 := Time.get_ticks_usec() # wall-clock easing: the drift reads the same at any playback speed
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
		ship.model.visible = true
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
					# Battlechoreo: a crippled ship drifts whole (the timeline rolls it) until it breaks up.
					var bu: float = float(j.get("breakup", 0.0))
					if bu > 0.0 and t - j.start < bu:
						continue
					# From the breakup on, BattleFx's torn hull pieces stand in for the hull (SoA 1161 s: the hull stays
					# inside the fireball, then breaks apart); the whole model is hidden so it never doubles them.
					ship.model.visible = false
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
	var _u3 := Time.get_ticks_usec()
	fx.update(t)
	var _u4 := Time.get_ticks_usec()
	prof = [_u1 - _u0, _u2 - _u1, _u3 - _u2, _u4 - _u3]
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


## Plan phase (BattleHost contract, tactical rules 3 and 4): draws `ship`'s firing arcs, range bands and previewed move
## with the forecast readout and subsystem pips on its target. forecast = the bridge's "tacticalForecast" result.
func show_plan(ship: Dictionary, forecast: Dictionary) -> void:
	if plan != null and not ship.is_empty():
		plan.show_plan(ship, forecast)


func clear_plan() -> void:
	if plan != null:
		plan.clear()


## The selected ship and target (BattleHost contract): the camera follows and rings them.
func set_selection(ship: int, target: int) -> void:
	if camera != null:
		camera.set_selection(ship, target)


func _update_camera(t: float, delta: float, centroid: Vector3, spread: float) -> void:
	if camera == null:
		return
	director.outer = _outer_scores()
	director.update(timeline, t)
	var me := _player_empire()
	var by_id := {}
	for s in encounter.get("ships", []):
		by_id[int(s.get("id", 0))] = s
	# Hull and shields as the round shows them: before it, then after it once it has played.
	var state := {}
	var rounds: Array = encounter.get("rounds", [])
	# Rules 4 subsystems and facings: the previous round's state while this one plays (damage persists from round to
	# round), this round's once it has played. Impacts during playback are drawn by the hit jobs themselves.
	var wego_state := {}
	if round_index >= 0 and round_index < rounds.size():
		var done := _finished_sent and not _playing
		var src: Array = rounds[round_index].get("wego", []) if done or round_index == 0 else rounds[round_index - 1].get("wego", [])
		if not done and round_index == 0:
			src = []
			for s0 in encounter.get("ships", []):
				if s0.has("wego"):
					src.append({"id": s0.id, "facings": s0.wego.get("maxFacings", []), "subsystems": {}, "coreBreached": false})
		for w0 in src:
			wego_state[int(w0.get("id", 0))] = w0
		for rs in rounds[round_index].get("ships", []):
			var fid0 := int(rs.get("id", 0))
			if done:
				state[fid0] = [float(rs.get("hullAfter", rs.get("hullBefore", 1))), float(rs.get("shieldAfter", rs.get("shieldBefore", 0)))]
			else:
				# Damage shows as each impact lands (the core's figures, in the core's order per ship).
				var lost := timeline.damage_until(fid0, t) if timeline != null else Vector2i.ZERO
				state[fid0] = [maxf(0.0, float(rs.get("hullBefore", 1)) - lost.x), maxf(0.0, float(rs.get("shieldBefore", 0)) - lost.y)]
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
		ship.set_damage(1.0 - hs[0] / maxf(1.0, float(info.get("maxHull", hs[0]))))
		var ws: Dictionary = wego_state.get(fid, {})
		if not ws.is_empty():
			ship.set_wego_state(ws)
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
	# Playback is group shots; the whole battle is shown once, as the establishing shot of round 1 (first ~2 s).
	var cutaway_round := force_cutaway or not (round_index == 0 and t < 2.0)
	if subject != null and cutaway_round and (_playing or not _finished_sent):
		shot = {"key": "%d_%d_%d_%.2f" % [round_index, director.subject, director.other, director.since],
			"kind": director.kind, "subject": subject.global_position, "subject_size": subject.length,
			"other": other.global_position if other != null else centroid, "other_size": other.length if other != null else subject.length, "centroid": centroid, "spread": spread}
	_gap_clock = 0.0 if (_playing or not _finished_sent) else _gap_clock + delta
	var vista_open := round_index == 0 and t < 2.5 and _env.has("planet")
	if film_cuts and shot_director and _gap_clock < GAP_GRACE and not vista_open:
		var ds := _direct_shot(t, delta, centroid, spread)
		if not ds.is_empty():
			shot = ds
	elif film_cuts and not shot_director and (_playing or not _finished_sent) and not ((round_index == 0 and t < 3.0) or t < 0.25):
		var fs := _film_shot(t, centroid, spread)
		if not fs.is_empty():
			shot = fs
	# The planet vista: the establishing shot and the pull-back at the start of every third round frame the fleet against
	# the planet, so its true size (radius 1100 beside hulls of ~10) reads on screen.
	# M4: the planet vista is the first round's establishing shot only (later ones put a hull over the limb, which read as a
	# ship flying through the gas giant).
	shot["vista"] = int(shot.get("kind", -1)) < 0 and round_index == 0 and t < 2.5 and _env.has("planet")
	if _env.has("planet"):
		camera.planet_center = (_env.planet as Node3D).global_position
		camera.planet_radius = BattleEnvironment.PLANET_RADIUS
	# F2: never hold on empty space. When no hull has read on screen (largest under EMPTY_FRAC of the frame's height)
	# for EMPTY_GRACE seconds, whatever shot is running gives way to a tracking shot on the closest pair of opposing ships.
	var kill_shot := str(shot.get("film", "")) == "direct" and str(_dir.get("type", "")) == "victim"
	if film_cuts and _playing and int(shot.get("kind", -1)) >= -1 and not bool(shot.get("vista", false)) and not kill_shot:
		_empty_clock = _empty_clock + delta if _largest_on_screen(bodies) < EMPTY_FRAC else 0.0
		if _empty_clock > EMPTY_GRACE or _rescue_until > t:
			var pair := _closest_pair(t)
			var a: BattleShip = _ships.get(int(pair[0])) if not pair.is_empty() else null
			var b: BattleShip = _ships.get(int(pair[1])) if not pair.is_empty() else null
			if a != null:
				if _rescue_until <= t:
					_rescue_until = t + 2.5
					_rescue_n += 1
				_empty_clock = 0.0
				shot = {"key": "rescue_%d_%d" % [round_index, _rescue_n], "kind": 0, "film": "behind", "subject": a.global_position,
					"subject_size": a.length, "subject_fwd": a.global_basis.x.normalized(), "other": b.global_position if b != null else centroid,
					"other_size": b.length if b != null else a.length, "centroid": centroid, "spread": spread}
	camera.film(delta, t, bodies, chase, shot)


const EMPTY_FRAC := 0.035   ## F2: a hull under 3.5% of the frame's height does not read as a ship
const EMPTY_GRACE := 0.25   ## F2: seconds of no readable hull before the camera is pulled onto the nearest fight
var _empty_clock := 0.0
var _rescue_until := -1.0
var _rescue_n := 0

## Height on screen (share of the frame) of the largest ship the camera sees.
func _largest_on_screen(bodies: Array) -> float:
	var cam := camera as Camera3D
	if cam == null:
		return 1.0
	var best := 0.0
	for bd in bodies:
		var c: Vector3 = bd.center
		if not cam.is_position_in_frustum(c):
			continue
		var dist := maxf(0.1, (c - cam.global_position).dot(-cam.global_basis.z))
		var frac := float(bd.radius) * 2.0 / (dist * 2.0 * tan(deg_to_rad(cam.fov) * 0.5))
		best = maxf(best, frac)
	return best


## Battlechoreo, proposal 8 (camera language, footage: 1-3 s cuts near the action, brief wide shots): the film cut at
## round time t. Cuts of 1-3 s, seeded per round and cut; each picks a type and a subject from what is about to happen:
## behind a firing ship, chasing a torpedo salvo, a fly-by of an escort on its run; every third cut is a brief wide.
## Returns {} for the tactical shot. The player takes manual control by switching camera mode (BattleCamera input).
var _film := {"n": -1, "until": -1.0, "round": -1}

func _film_shot(t: float, centroid: Vector3, spread: float) -> Dictionary:
	if _film.round != round_index or t < float(_film.get("from", 0.0)) or t >= float(_film.until):
		var n: int = int(_film.n) + 1 if _film.round == round_index and t >= float(_film.get("from", 0.0)) else 0
		var h := BattleTimeline.unit_hash(round_index * 977 + n, 61)
		var length := 2.4 + 2.2 * h # M4: shots of 2.4-4.6 s (the first cut flipped every 1-3 s between unrelated ships)
		_film = {"n": n, "from": t, "until": t + length, "round": round_index, "type": "wide"}
		# M4 flight: every third cut is the wide tactical view in the screensaver's framing, so the flown formations, the
		# waves and the attack runs read as one battle; the others are shots of the fight.
		if n % 3 != 2:
			_film.merge(_pick_film(t, length, n), true)
		if _film.type == "wide" and n % 3 != 2 and _ships.size() <= 14:
			# No weapon about to fire: still never a wide of empty space. Track the closest pair of opposing ships.
			var pair := _closest_pair(t)
			if not pair.is_empty():
				_film.merge({"type": "behind", "subject": pair[0], "other": pair[1]}, true)
		if _film.type == "wide":
			_film.until = t + (3.2 + 1.4 * h if n % 3 == 2 else 1.6 + 0.8 * h)
		elif _film.type == "torpedo":
			_film.until = minf(float(_film.until), t + 2.0) # a chase of a salvo is a beat, not four seconds of specks
	var typ: String = _film.type
	if typ == "wide":
		if int(_film.n) % 3 == 2:
			# The wide tactical view (screensaver framing), held over the director's cutaways.
			return {"key": "wide_%d" % round_index, "kind": -1, "centroid": centroid, "spread": spread}
		return {}
	var a: BattleShip = _ships.get(int(_film.subject))
	var b: BattleShip = _ships.get(int(_film.get("other", 0)))
	var tr := timeline.track(int(_film.subject))
	if a == null or (tr != null and tr.hide_at >= 0.0 and t >= tr.hide_at + 0.6):
		_film.type = "wide"
		return {}
	var out := {"key": "film_%d_%d" % [round_index, int(_film.n)], "kind": 0, "film": typ, "subject": a.global_position,
		"subject_size": a.length, "subject_fwd": a.global_basis.x.normalized(), "other": b.global_position if b != null else centroid,
		"other_size": b.length if b != null else a.length, "centroid": centroid, "spread": spread}
	if _env.has("sun_dir"):
		out["sun_dir"] = _env.sun_dir
	if _env.has("planet"):
		out["planet_pos"] = (_env.planet as Node3D).global_position
	if typ == "torpedo":
		var j: Dictionary = timeline.jobs[int(_film.job)]
		var u := clampf((t - float(j.start)) / maxf(0.1, float(j.duration)), 0.0, 0.92)
		out["torp"] = fx._torpedo_pos(j, u * float(j.end_fraction))
	return out


## M4 flight, stage 2 (Simon, 14 Nov: "compare it yourself to footage of show battles"): the shot director. Show battles
## (DS9 "Sacrifice of Angels") hold one slow, eased camera move for 3-10 s close on one hull (30-60% of the frame) with
## the fleet layered behind it, and cut rarely. Each shot here picks a subject from what the round is about to show (a
## ship about to blow up, a torpedo or beam shooter, an escort on its run), holds a moving lens on it for 3.5-5.5 s of
## real time and cuts only when the move has ended (or its subject has gone). Every fourth shot is a short low
## establisher behind the player's fleet, looking across it at the enemy, so the battle reads as two fleets. Shot
## clocks run in real time and survive the gap between rounds (GAP_GRACE), so a round boundary is not a cut.
## The camera only frames; the core's events are drawn exactly as before.
const GAP_GRACE := 1.2
var _gap_clock := 0.0
var _dir := {"n": -1, "until": -1.0}
var _dir_clock := 0.0
var _dir_recent: Array = []          ## the last subjects filmed, newest last
var _dir_prev := {}                  ## fleet_id -> world position last frame (speed for the approach shot)
var _dir_vel := {}                   ## fleet_id -> smoothed world velocity (units/s)

func _direct_shot(t: float, delta: float, centroid: Vector3, spread: float) -> Dictionary:
	_dir_clock += delta
	for fid in _ships:
		var sh: BattleShip = _ships[fid]
		var p := sh.global_position
		if _dir_prev.has(fid) and delta > 0.0:
			var v: Vector3 = (p - _dir_prev[fid]) / delta
			if v.length() < 200.0:
				_dir_vel[fid] = (_dir_vel.get(fid, v) as Vector3).lerp(v, clampf(delta * 3.0, 0.0, 1.0))
		_dir_prev[fid] = p
		_dir_len[fid] = sh.length
	# Every kill is filmed (lead, 18 Nov: the camera framed 1 of 3 kills on the desktop, 0 of 3 on the web): a moment
	# before a ship breaks up the director cuts to it, whatever shot is running, and holds through the breakup
	# (KILL_LEAD / KILL_HOLD of round time round the breakup). Two kills at once: the bigger ship.
	var kw := _kill_window(t) if _playing else {}
	var on_kill := not kw.is_empty() and str(_dir.get("type", "")) == "victim" and int(_dir.get("subject", 0)) == int(kw.fid)
	if not kw.is_empty() and not on_kill:
		_direct_kill(kw)
		on_kill = true
	elif on_kill and not _dir.has("job"):
		# Already on this ship (a victim shot picked ahead): keep the lens, and follow the kill through its breakup.
		_dir.merge({"boom": float(kw.boom), "L": float(kw.size), "job": int(kw.job)}, true)
	var ended := _dir_clock >= float(_dir.until) and not on_kill
	if str(_dir.get("type", "")) == "victim" and _dir.has("boom"):
		ended = not on_kill and (t >= float(_dir.boom) + KILL_HOLD or not _playing)
	elif not ended and str(_dir.get("type", "")) != "establish":
		var sid := int(_dir.get("subject", 0))
		var a0: BattleShip = _ships.get(sid)
		var tr0 := timeline.track(sid) if timeline != null else null
		# A victim shot stays on the kill while its fireball and torn hull play out (SoA 1161-1163 s holds on it); the hull
		# itself is hidden from the breakup on (BattleFx's pieces stand in for it).
		var victim := str(_dir.get("type", "")) == "victim"
		var hold := 1.9 if victim else 0.6
		if a0 == null or (not a0.visible and not victim) or (tr0 != null and tr0.hide_at >= 0.0 and t >= tr0.hide_at + hold and _playing):
			ended = true
	if ended:
		_direct_pick(t)
	if str(_dir.get("type", "")) == "":
		return {}
	# The lens rides a smoothed heading and target bearing, so an escort jinking or a target crossing the bow swings the
	# shot round over a second or so instead of whipping it.
	var subj: BattleShip = _ships.get(int(_dir.get("subject", 0)))
	if subj != null:
		var fw := subj.global_basis.x
		var fl := Vector3(fw.x, 0.0, fw.z)
		if fl.length() > 1e-3:
			fl = fl.normalized()
			var cur: Vector3 = _dir.get("fwd_s", fl)
			_dir["fwd_s"] = cur.slerp(fl, clampf(delta * 1.2, 0.0, 1.0)).normalized() if cur.dot(fl) > -0.98 else fl
		var tb := fl if fl.length() > 0.5 else Vector3(1, 0, 0)
		var ob: BattleShip = _ships.get(int(_dir.get("other", 0)))
		if ob != null and ob != subj and ob.global_position.distance_to(subj.global_position) > 1e-3:
			tb = (ob.global_position - subj.global_position).normalized()
		var tcur: Vector3 = _dir.get("tob_s", tb)
		_dir["tob_s"] = tcur.lerp(tb, clampf(delta * 1.5, 0.0, 1.0)).normalized() if tcur.lerp(tb, 0.5).length() > 0.05 else tb
	var u := clampf((_dir_clock - float(_dir.from)) / maxf(0.1, float(_dir.until) - float(_dir.from)), 0.0, 1.0)
	var e := u * u * (3.0 - 2.0 * u) # eased: the move starts and settles gently
	var pose := _direct_pose(str(_dir.type), u, e)
	if pose.is_empty():
		return {}
	return {"key": "direct_%d" % int(_dir.n), "kind": 0, "film": "direct", "eye": pose[0], "look": pose[1], "fov": pose[2],
		"subject_id": int(_dir.get("subject", 0)), "subject": pose[1], "centroid": centroid, "spread": spread}


const KILL_DIST := 3.0   ## kill shot lens distance in hull lengths: a 1.5-length fireball fills ~20-25% of the frame
const KILL_LEAD := 0.7   ## round seconds before a breakup the director cuts to the dying ship
const KILL_HOLD := 2.0   ## round seconds after the breakup it holds on the fireball and the torn hull

## The kill to film at t: {fid, other, boom, size} for the biggest ship whose breakup window holds t, or {}.
func _kill_window(t: float) -> Dictionary:
	var out := {}
	if timeline == null:
		return out
	for ji in timeline.jobs.size():
		var j: Dictionary = timeline.jobs[ji]
		if float(j.start) > t + KILL_LEAD + 2.0:
			break
		if int(j.kind) != BattleTimeline.Job.EXPLOSION:
			continue
		var boom := float(j.start) + float(j.get("breakup", 0.0))
		if t < boom - KILL_LEAD or t >= boom + KILL_HOLD:
			continue
		var fid := int(j.target_fleet_id)
		var sh: BattleShip = _ships.get(fid)
		if sh == null and not _dir_prev.has(fid):
			continue
		var size := sh.length if sh != null else float(_dir_len.get(fid, 8.0))
		if out.is_empty() or size > float(out.size) or (size == float(out.size) and boom < float(out.boom)):
			out = {"fid": fid, "other": int(j.get("source_fleet_id", 0)), "boom": boom, "size": size, "job": ji}
	return out

var _dir_len := {}   ## fleet_id -> hull length last seen (a kill shot outlives the ship node)

## Cuts to the kill kw (_kill_window).
func _direct_kill(kw: Dictionary) -> void:
	var n := int(_dir.n) + 1
	_dir = {"n": n, "from": _dir_clock, "until": 0.0, "type": "victim", "subject": int(kw.fid),
		"other": int(kw.other), "sgn": 1.0, "boom": float(kw.boom), "L": float(kw.size), "job": int(kw.job)}
	_dir.until = _dir_clock + KILL_LEAD + KILL_HOLD # the drift's pace; the shot itself ends with the kill window
	_dir_recent.append(int(kw.fid))
	if _dir_recent.size() > 3:
		_dir_recent.pop_front()
	_dir_choose_side(_ships.get(int(kw.fid)), _sun_toward(), n)


## The next shot: {n, from, until, type, subject, other, sgn, ...}.
func _direct_pick(t: float) -> void:
	var n := int(_dir.n) + 1
	var h := BattleTimeline.unit_hash(n * 131 + 7, 71)
	_dir.erase("fwd_s")
	_dir.erase("tob_s")
	var prev_type := str(_dir.get("type", ""))
	_dir = {"n": n, "from": _dir_clock, "until": _dir_clock + 3.5 + 2.0 * h, "type": "", "subject": 0, "other": 0, "sgn": 1.0}
	var me := _player_empire()
	if n == 0 or (n % 4 == 0 and prev_type != "establish"):
		_dir.type = "establish"
		_dir.until = _dir_clock + 2.6 + 1.0 * h
		_dir.sgn = 1.0 if BattleTimeline.unit_hash(n, 72) < 0.5 else -1.0
		return
	var best := {}
	var best_score := -INF
	if timeline != null:
		for k in timeline.jobs.size():
			var j: Dictionary = timeline.jobs[k]
			if float(j.start) > t + 3.2:
				break
			if float(j.start) + float(j.duration) < t + 0.2:
				continue
			var typ := ""
			var subj := int(j.fleet_id)
			var oth := int(j.target_fleet_id)
			var score := 0.0
			var lead := float(j.start) - t
			match int(j.kind):
				BattleTimeline.Job.EXPLOSION:
					subj = int(j.target_fleet_id)
					oth = int(j.source_fleet_id)
					if lead < 0.5 or lead > 2.8:
						continue
					typ = "victim"
					score = 5.0 - absf(lead - 1.4)
				BattleTimeline.Job.TORPEDO, BattleTimeline.Job.BEAM, BattleTimeline.Job.PULSE:
					if lead < -0.4:
						continue
					var sh: BattleShip = _ships.get(subj)
					if sh == null:
						continue
					var trs := timeline.track(subj)
					var capital := sh.length >= 22.0 or (trs != null and trs.style == 0)
					typ = "push" if capital else "track"
					if capital and BattleTimeline.unit_hash(n * 17 + subj, 73) < 0.35:
						typ = "approach"
					score = (3.0 if int(j.kind) == BattleTimeline.Job.TORPEDO else 2.5) - 0.4 * absf(lead - 0.6)
				_:
					continue
			if not _ships.has(subj):
				continue
			var tr := timeline.track(subj)
			if tr != null and ((tr.hide_at >= 0.0 and tr.hide_at < t + 0.3 and typ != "victim") or tr.visibility_at(t) < 0.35):
				continue
			if _dir_recent.has(subj):
				score -= 2.5
			if subj >= 2000000 and typ != "victim":
				score -= 3.0 # a starbase is scenery to fly past, not a subject to hold on (stage 2 gate review)
			if typ == prev_type:
				score -= 1.0
			var sh2: BattleShip = _ships[subj]
			if sh2.owner_id == me:
				score += 0.5 # the show films its heroes in the foreground
			score += BattleTimeline.unit_hash(n * 53 + k, 74)
			if score > best_score:
				best_score = score
				best = {"type": typ, "subject": subj, "other": oth}
	if best.is_empty():
		# Nothing about to fire (between rounds, a quiet stretch): a capital passing the lens, else any ship, never empty space.
		var cand := -1
		var big := -1.0
		for fid in _ships:
			var sh: BattleShip = _ships[fid]
			var tr := timeline.track(fid) if timeline != null else null
			if not sh.visible or (tr != null and (tr.visibility_at(t) < 0.35 or (tr.hide_at >= 0.0 and t >= tr.hide_at))):
				continue
			var sc := minf(sh.length, 40.0) * (0.3 if _dir_recent.has(fid) else 1.0) * (1.0 + BattleTimeline.unit_hash(n * 7 + fid, 75))
			if fid >= 2000000:
				sc *= 0.05
			if sc > big:
				big = sc
				cand = fid
		if cand < 0:
			_dir.type = "establish"
			return
		var pair := _closest_pair(t)
		var other := 0
		if pair.size() == 2:
			other = int(pair[1]) if int(pair[0]) == cand else (int(pair[0]) if int(pair[1]) == cand else 0)
		best = {"type": "approach" if (_ships[cand] as BattleShip).length >= 22.0 and cand < 2000000 else "track", "subject": cand, "other": other}
	_dir.merge(best, true)
	if best.type == "victim":
		_dir.until = maxf(float(_dir.until), _dir_clock + 3.6)
	_dir_recent.append(int(best.subject))
	if _dir_recent.size() > 3:
		_dir_recent.pop_front()
	# Pick the side of the hull that keeps the lens out of the sun and the subject unblocked.
	var a: BattleShip = _ships.get(int(_dir.subject))
	var sd := _sun_toward()
	_dir_choose_side(a, sd, n)
	# Still looking into the sun from either side: take the same subject from the other end (the lens ahead of it,
	# looking back), so the hull is lit from the front instead of a silhouette in the glare.
	var pose0 := _direct_pose(str(_dir.type), 0.0, 0.0)
	if sd != Vector3.ZERO and not pose0.is_empty() and str(_dir.type) in ["track", "approach", "push"] 			and ((pose0[1] as Vector3) - (pose0[0] as Vector3)).normalized().dot(sd) > 0.5:
		_dir.type = "lead" if str(_dir.type) == "track" else "push_rev"
		_dir_choose_side(a, sd, n)
	_dir["eye0"] = Vector3.ZERO # the approach lens is planted once, from where the ship is at the cut


## Unit vector toward the sun (a DirectionalLight shines down its -Z), or zero.
func _sun_toward() -> Vector3:
	var sun: DirectionalLight3D = _env.get("sun")
	return sun.global_basis.z.normalized() if sun != null else Vector3.ZERO


## Picks the side of the hull (sgn) that keeps the lens out of the sun and the subject unblocked.
func _dir_choose_side(a: BattleShip, sd: Vector3, n: int) -> void:
	var best_s := 1.0
	var best_bad := INF
	var signs := [1.0, -1.0]
	if str(_dir.type) in ["push", "push_rev"] and a != null:
		var oth: BattleShip = _ships.get(int(_dir.get("other", 0)))
		if oth != null and oth != a:
			var fw0 := a.global_basis.x
			var sd0 := Vector3(fw0.x, 0.0, fw0.z).normalized().cross(Vector3.UP)
			signs = [-1.0 if sd0.dot(oth.global_position - a.global_position) > 0.0 else 1.0]
	for sg in signs:
		_dir.sgn = sg
		_dir["eye0"] = Vector3.ZERO
		var pose := _direct_pose(str(_dir.type), 0.0, 0.0)
		if pose.is_empty() or a == null:
			continue
		var eye: Vector3 = pose[0]
		var look: Vector3 = pose[1]
		var bad := 0.0
		if sd != Vector3.ZERO:
			bad += maxf(0.0, (look - eye).normalized().dot(sd.normalized()) - 0.3) * 4.0
		for fid in _ships:
			var o: BattleShip = _ships[fid]
			if o == a or not o.visible:
				continue
			var c := o.global_position
			var seg := a.global_position - eye
			var w := clampf((c - eye).dot(seg) / maxf(1e-3, seg.length_squared()), 0.0, 1.0)
			if (eye + seg * w).distance_to(c) < o.length * 0.45 and w < 0.95:
				bad += 1.0
		if _env.has("planet"):
			# Never film through (or from inside) the planet: the third kill of the drill sat behind the gas giant.
			var pc := (_env.planet as Node3D).global_position
			var pr := BattleEnvironment.PLANET_RADIUS * 1.02
			var seg2 := look - eye
			var w2 := clampf((pc - eye).dot(seg2) / maxf(1e-3, seg2.length_squared()), 0.0, 1.0)
			if (eye + seg2 * w2).distance_to(pc) < pr:
				bad += 20.0
		bad += BattleTimeline.unit_hash(n * 3 + int(sg > 0.0), 76) * 0.1
		if bad < best_bad:
			best_bad = bad
			best_s = sg
	_dir.sgn = best_s


## [eye, look, fov] for the current shot at progress u (eased e), or [] when the subject is gone.
func _direct_pose(typ: String, u: float, e: float) -> Array:
	var sg := float(_dir.get("sgn", 1.0))
	if typ == "establish":
		var me := _player_empire()
		var pc := Vector3.ZERO
		var ec := Vector3.ZERO
		var np := 0
		var ne := 0
		for fid in _ships:
			var sh: BattleShip = _ships[fid]
			if not sh.visible:
				continue
			if sh.owner_id == me:
				pc += sh.global_position
				np += 1
			else:
				ec += sh.global_position
				ne += 1
		if np == 0 or ne == 0:
			return []
		pc /= float(np)
		ec /= float(ne)
		var rp := 0.0
		for fid in _ships:
			var sh: BattleShip = _ships[fid]
			if sh.visible and sh.owner_id == me:
				rp = maxf(rp, sh.global_position.distance_to(pc) + sh.length * 0.5)
		var d := Vector3(ec.x - pc.x, 0.0, ec.z - pc.z)
		d = d.normalized() if d.length() > 1e-3 else Vector3(1, 0, 0)
		var side := d.cross(Vector3.UP).normalized()
		var back := rp * 0.9 + 30.0
		var eye := pc - d * back * (1.0 - 0.25 * e) + side * sg * rp * (0.45 - 0.2 * e) + Vector3.UP * (rp * 0.16 + 8.0)
		return [eye, pc.lerp(ec, 0.6), 46.0]
	var a: BattleShip = _ships.get(int(_dir.get("subject", 0)))
	if typ == "victim" and _dir.has("job"):
		# Once the fireball has lit, the lens holds on it (the hull node is hidden and no longer where the kill is).
		var st: Dictionary = fx._active.get(int(_dir.job), {})
		if st.has("boom_at"):
			_dir["kp"] = st.point
			a = null
	if a == null and typ == "victim" and (_dir.has("kp") or _dir_prev.has(int(_dir.get("subject", 0)))):
		# The fireball has lit, or the ship's node is gone mid-breakup: hold on where it died.
		var p0: Vector3 = _dir.get("kp", _dir_prev.get(int(_dir.subject), Vector3.ZERO))
		var L0 := maxf(float(_dir.get("L", 8.0)), 8.0)
		var fl0: Vector3 = _dir.get("fwd_s", Vector3(1, 0, 0))
		var off0 := (fl0.cross(Vector3.UP).normalized() * sg - fl0 * 0.35 + Vector3.UP * 0.32).normalized().rotated(Vector3.UP, deg_to_rad(10.0) * (u - 0.5) * 4.0 * sg) * L0 * KILL_DIST
		return [p0 + off0, p0, 40.0]
	if a == null:
		return []
	var p := a.global_position
	var L := maxf(a.length, 8.0)
	var fwd := a.global_basis.x.normalized()
	var flat := Vector3(fwd.x, 0.0, fwd.z)
	flat = flat.normalized() if flat.length() > 1e-3 else Vector3(1, 0, 0)
	if _dir.has("fwd_s"):
		flat = _dir.fwd_s
	var side := flat.cross(Vector3.UP).normalized() * sg
	var b: BattleShip = _ships.get(int(_dir.get("other", 0)))
	var to_b := flat
	if _dir.has("tob_s"):
		to_b = _dir.tob_s
	elif b != null and b != a and b.global_position.distance_to(p) > 1e-3:
		to_b = (b.global_position - p).normalized()
	match typ:
		"track":
			# Behind and beside the attacker, gaining on it slightly, looking past it toward its target.
			var eye := p - flat * L * lerpf(1.7, 1.25, e) + side * L * lerpf(0.95, 0.6, e) + Vector3.UP * L * 0.32
			var look := p + flat.lerp(to_b, 0.45).normalized() * L * 0.9
			return [eye, look, 40.0]
		"push":
			# Alongside a capital's broadside, sliding from its quarter toward its bow, the enemy beyond the hull.
			# The side away from the target (sgn is chosen so at the cut, and kept: a target crossing the bow never flips it).
			var eye := p + side * L * 1.05 + Vector3.UP * L * 0.42 + flat * L * lerpf(-0.95, 0.55, e)
			var look := p + to_b * L * 0.55 + flat * L * lerpf(-0.15, 0.25, e)
			return [eye, look, 40.0]
		"lead":
			# Ahead of the ship, falling back along it as it comes on (the lens the show uses for a ship charging in).
			var eye := p + flat * L * lerpf(1.5, 1.1, e) + side * L * lerpf(0.9, 0.65, e) + Vector3.UP * L * 0.28
			return [eye, p - flat * L * 0.25, 40.0]
		"push_rev":
			# The broadside slide the other way: from the bow quarter back toward the stern.
			var eye := p + side * L * 1.05 + Vector3.UP * L * 0.42 + flat * L * lerpf(1.0, -0.4, e)
			var look := p + to_b * L * 0.55 + flat * L * lerpf(0.2, -0.15, e)
			return [eye, look, 40.0]
		"approach":
			# A lens planted ahead of the capital's path, low: it comes on and slides past, the camera panning with it.
			if (_dir.get("eye0", Vector3.ZERO) as Vector3) == Vector3.ZERO:
				var v: Vector3 = _dir_vel.get(int(_dir.subject), Vector3.ZERO)
				var span := float(_dir.until) - float(_dir.from)
				var ahead := maxf(v.length() * span * 0.55, L * 0.6) + L * 1.4
				_dir["eye0"] = p + flat * ahead + side * L * 1.1 - Vector3.UP * L * 0.18
			return [_dir.eye0, p + flat * L * 0.2, 42.0]
		"victim":
			# A medium shot of the ship about to die, drifting round it, so the blast and the debris fill the frame.
			var ang := deg_to_rad(10.0) * (u - 0.5) * 4.0 * sg
			var off := (side * 1.0 - flat * 0.35 + Vector3.UP * 0.32).normalized().rotated(Vector3.UP, ang) * L * KILL_DIST
			return [p + off, p, 40.0]
	return []


## The closest pair of opposing ships that are on screen at t: [fleet id, fleet id], or [].
func _closest_pair(t: float) -> Array:
	var best := INF
	var out := []
	for ida in _ships:
		var a: BattleShip = _ships[ida]
		var ta := timeline.track(ida)
		if not a.visible or (ta != null and (ta.visibility_at(t) < 0.35 or (ta.hide_at >= 0.0 and t >= ta.hide_at))):
			continue
		for idb in _ships:
			var b: BattleShip = _ships[idb]
			if b.owner_id == a.owner_id or not b.visible:
				continue
			var tb := timeline.track(idb)
			if tb != null and (tb.visibility_at(t) < 0.35 or (tb.hide_at >= 0.0 and t >= tb.hide_at)):
				continue
			var d := a.global_position.distance_to(b.global_position)
			if d < best:
				best = d
				out = [ida, idb]
	return out


## The subject of a film cut starting at t: the most watchable shot starting soon.
func _pick_film(t: float, length: float, n: int) -> Dictionary:
	var want: String = ["behind", "torpedo", "flyby"][n % 3] if (n % 3 != 2 or _ships.size() <= 14) else "wide"
	if BattleTimeline.unit_hash(round_index * 31 + n, 62) < 0.5:
		want = "torpedo" if want == "behind" else ("flyby" if want == "torpedo" else want)
	var best := {}
	var best_score := -INF
	for k in timeline.jobs.size():
		var j: Dictionary = timeline.jobs[k]
		if j.start > t + 0.6 * length:
			break
		if j.start + j.duration < t + 0.3:
			continue
		var tr := timeline.track(int(j.fleet_id))
		if tr == null or not _ships.has(int(j.fleet_id)) or (tr.hide_at >= 0.0 and tr.hide_at < t + length):
			continue
		var score := -absf(float(j.start) - (t + 0.2))
		var typ := ""
		match int(j.kind):
			BattleTimeline.Job.TORPEDO:
				typ = "torpedo"
				if j.start > t - 0.2:
					score -= 0.5 # join the salvo in flight, not before it leaves
			BattleTimeline.Job.BEAM, BattleTimeline.Job.PULSE:
				typ = "flyby" if tr.style == 2 else "behind"
			BattleTimeline.Job.EXPLOSION:
				typ = "behind"
				score += 3.0
			_:
				continue
		if typ == "flyby" and _ships.size() <= 14:
			typ = "behind" # likewise the fixed fly-by lens: an escort across a wide frame is a speck
		if typ == "torpedo" and _ships.size() <= 14:
			typ = "behind" # small battle: the pair-fitted tracking shot shows salvo, shooter and target; a chase cam showed empty space
		if typ == want or (want != "wide" and typ == "behind" and _ships.size() <= 14):
			score += 2.0
		if score > best_score:
			best_score = score
			var other := int(j.target_fleet_id)
			best = {"type": typ, "subject": int(j.fleet_id) if j.kind != BattleTimeline.Job.EXPLOSION else int(j.source_fleet_id), "other": other, "job": k}
			if int(j.kind) == BattleTimeline.Job.EXPLOSION:
				best.subject = other
				best.other = 0
	return best


## fleet_id -> 0..3: distance from the ship's own side's centre, as a share of that side's widest member.
func _outer_scores() -> Dictionary:
	var sums := {}
	for fid in _ships:
		var sh: BattleShip = _ships[fid]
		var k: int = sh.owner_id
		if not sums.has(k):
			sums[k] = [Vector3.ZERO, 0]
		sums[k][0] += sh.global_position
		sums[k][1] += 1
	var far := {}
	var dist := {}
	for fid in _ships:
		var sh: BattleShip = _ships[fid]
		var c: Vector3 = sums[sh.owner_id][0] / float(sums[sh.owner_id][1])
		dist[fid] = sh.global_position.distance_to(c)
		far[sh.owner_id] = maxf(far.get(sh.owner_id, 0.0), dist[fid])
	var out := {}
	for fid in _ships:
		var sh: BattleShip = _ships[fid]
		out[fid] = int(round(3.0 * dist[fid] / maxf(1e-3, far[sh.owner_id])))
	return out
