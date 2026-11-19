class_name BotfAudioSystem
extends Node
## The BotfAudio autoload. One place for all game sound:
##  - five buses (Master, Effects, Voice, Interface, Music; default_bus_layout.tres) with persisted sliders;
##  - cues by name (BotfAudioCues.REGISTRY) or by key list (BattleAudio), run through BotfCueScheduler;
##  - voice lines one at a time (queue by priority), with Effects and Music ducked under them;
##  - music with a crossfade between title, galaxy, battle and battle tension (local playlists, else generated beds);
##  - app-level hooks: every button click, screen changes, the end-turn / briefing events, contacts, victory.
## Silent when there is nothing to play (voice) and generated tones when there are no files (everything else).
## Design reference: UBotfAudioSubsystem in the frozen Unreal client; nothing is ported from it.

signal cue_played(cue: String, key: String, bus: String)
signal music_changed(context: String)

const POOL := 24
const FADE_SECONDS := 1.6
const SETTINGS_PATH := "user://botf_settings.json"

static var instance: BotfAudioSystem

var library: BotfAudioLibrary
var scheduler := BotfCueScheduler.new()
var settings: Dictionary = {}          ## the vol_* keys (shared with the client settings dict when attached)
var duck_gain := 1.0
var enabled := true                    ## false in tests that want silence without touching buses
var recent: Array = []                 ## last played [{cue, key, bus, kind, t}], newest last (debug and tests)
var music_context := ""
var music_key := ""                    ## key or generated kind currently playing
var music_mood := "calm"               ## F5 adaptive music: calm / build / peak on the strategic map (BotfMusicDirector)
var adaptive_music := true             ## off: segments play straight through in game order, as before F5
var clock_override := -1.0             ## tests drive time with this
var chatter := BotfBridgeChatter.new()
var chatter_enabled := true            ## the bridge chatter layer (battle only); settings can switch it off
var era := ""                          ## "" = settings["audio_era"], else the default era (BotfAudioCues.DEFAULT_ERA)

var _sfx: Array[AudioStreamPlayer] = []
var _sfx_handle: Dictionary = {}       ## player -> scheduler handle
var _voice: AudioStreamPlayer
var _music: Array[AudioStreamPlayer] = []   ## two players for the crossfade
var _music_front := 0
var _fade := 1.0                       ## 0..1 progress of the running crossfade
var _fading := false
var _playlist := PackedStringArray()
var _playlist_pos := 0
var _app: Node
var _last_ui_frame := -1
var _poll := 0.0
var _seen_contacts := {}
var _tension := false
var _bound_buttons := true
var _prev_screen := ""
var _music_fac := ""
var _seed := -1
var _last_colonies := -1               ## the player's colony count at the last view (adaptive music: a loss raises to peak)


func _init() -> void:
	instance = self


## R3.5: an 8-voice torpedo salvo summed to +15 dBFS (4 voices already +9). A hard limiter on the master bus keeps
## every mix under -1 dBFS; the salvo cap can then be 8 without clipping.
static func _ensure_limiter() -> void:
	var idx := AudioServer.get_bus_index("Master")
	for k in AudioServer.get_bus_effect_count(idx):
		if AudioServer.get_bus_effect(idx, k) is AudioEffectHardLimiter:
			return
	var lim := AudioEffectHardLimiter.new()
	lim.ceiling_db = -1.0
	lim.pre_gain_db = 0.0
	AudioServer.add_bus_effect(idx, lim)


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	ensure_buses()
	_ensure_limiter()
	library = BotfAudioLibrary.new()
	library.prewarm_music()
	for i in POOL:
		var p := AudioStreamPlayer.new()
		p.finished.connect(_on_sfx_finished.bind(p))
		add_child(p)
		_sfx.append(p)
	_voice = AudioStreamPlayer.new()
	_voice.bus = BotfAudioCues.BUS_VOICE
	add_child(_voice)
	for i in 2:
		var m := AudioStreamPlayer.new()
		m.bus = BotfAudioCues.BUS_MUSIC
		m.volume_db = -80.0
		m.finished.connect(_on_music_finished.bind(m))
		add_child(m)
		_music.append(m)
	load_settings_file()
	apply_all()
	get_tree().node_added.connect(_on_node_added)


func _exit_tree() -> void:
	if library != null:
		library.finish_tasks()
	if instance == self:
		instance = null


# ------------------------------------------------------------------------------------------------------------
# Buses and volume
# ------------------------------------------------------------------------------------------------------------

## Makes sure the five buses exist (default_bus_layout.tres normally does this) and routes them to Master.
static func ensure_buses() -> void:
	for b in BotfAudioCues.BUSES:
		if AudioServer.get_bus_index(b) < 0:
			AudioServer.add_bus()
			var idx := AudioServer.bus_count - 1
			AudioServer.set_bus_name(idx, b)
			AudioServer.set_bus_send(idx, "Master")


static func bus_names() -> PackedStringArray:
	var out := PackedStringArray()
	for i in AudioServer.bus_count:
		out.append(AudioServer.get_bus_name(i))
	return out


## Reads the vol_* settings from the client settings file (before the App has loaded it).
func load_settings_file() -> void:
	var s := {}
	AppSaves.ensure()
	var data: Variant = AppLoader.json(AppSaves.SETTINGS_PATH)   # user:// (IndexedDB on web); a capture sandbox moves it
	if data is Dictionary:
		s = data
	attach_settings(s)


## Uses `s` (the client settings dictionary) as the source of the five sliders; fills defaults in place.
func attach_settings(s: Dictionary) -> void:
	BotfAudioMix.fill_defaults(s)
	settings = s
	apply_all()


func set_volume(bus: String, v: float) -> void:
	var k := BotfAudioMix.key_for(bus)
	if k == "":
		return
	settings[k] = clampf(v, 0.0, 1.0)
	apply_bus(bus)


## A short sample on `bus` after a slider is released, so the new level can be judged.
func preview(bus: String) -> void:
	match bus:
		"Effects": play_keys(PackedStringArray(["weapon.generic.beam"]), 3)
		"Voice": cue("research_complete")
		"Music": pass
		_: cue("ui_click")


func get_volume(bus: String) -> float:
	return BotfAudioMix.volume_of(settings, bus)


func apply_all() -> void:
	for b in BotfAudioCues.BUSES:
		apply_bus(b)


func apply_bus(bus: String) -> void:
	var idx := AudioServer.get_bus_index(bus)
	if idx < 0:
		return
	var g := BotfAudioMix.bus_gain(settings, bus, duck_gain)
	AudioServer.set_bus_volume_db(idx, BotfAudioMix.to_db(g))
	AudioServer.set_bus_mute(idx, g <= 0.0005)


## The bus volume as set on the AudioServer, as a slider-style linear value (tests).
static func bus_linear(bus: String) -> float:
	var idx := AudioServer.get_bus_index(bus)
	if idx < 0 or AudioServer.is_bus_mute(idx):
		return 0.0
	return db_to_linear(AudioServer.get_bus_volume_db(idx))


# ------------------------------------------------------------------------------------------------------------
# Time and the frame loop
# ------------------------------------------------------------------------------------------------------------

func now() -> float:
	return clock_override if clock_override >= 0.0 else Time.get_ticks_msec() / 1000.0


func _process(delta: float) -> void:
	_tick(delta)
	_poll += delta
	if _poll >= 0.25:
		_poll = 0.0
		_poll_app()


## One frame: start the next voice line, move the duck envelope, advance the music crossfade.
func _tick(delta: float) -> void:
	var line := scheduler.next_voice(now(), _voice != null and _voice.playing)
	if not line.is_empty():
		_start_voice(line)
	elif chatter_enabled and music_context in ["battle", "tension"]:
		chatter_tick()
	var target := BotfAudioMix.duck_target(scheduler.voice_active())
	if not is_equal_approx(duck_gain, target):
		duck_gain = BotfAudioMix.duck_step(duck_gain, target, delta)
		for b in BotfAudioMix.DUCKED:
			apply_bus(b)
	if _fading:
		_fade = minf(_fade + delta / FADE_SECONDS, 1.0)
		var g := BotfAudioMix.crossfade(_fade)
		_music[_music_front].volume_db = BotfAudioMix.to_db(g.y)
		_music[1 - _music_front].volume_db = BotfAudioMix.to_db(g.x)
		if _fade >= 1.0:
			_fading = false
			_music[1 - _music_front].stop()


# ------------------------------------------------------------------------------------------------------------
# Era
# ------------------------------------------------------------------------------------------------------------

## The era whose show sounds are preferred: set_era(), else settings["audio_era"], else the Dominion War default.
## "auto" and unknown names mean the default.
func current_era() -> String:
	var e := BotfAudioCues.era_code(era if era != "" else str(settings.get("audio_era", "")))
	return e if e != "" else BotfAudioCues.DEFAULT_ERA


## Picks the era by code ("tng") or roster era name ("NextGeneration"); persists in the client settings.
func set_era(name: String) -> void:
	era = BotfAudioCues.era_code(name)
	settings["audio_era"] = era if era != "" else "auto"


## `keys` with their era twins in front, when the library holds any clip for the current era.
func era_keys(keys: PackedStringArray) -> PackedStringArray:
	var e := current_era()
	if library != null and library.has_era(e):
		return BotfAudioCues.with_era(keys, e)
	return keys


# ------------------------------------------------------------------------------------------------------------
# Playing cues
# ------------------------------------------------------------------------------------------------------------

## Plays a named cue (BotfAudioCues.REGISTRY). Returns what was started: {sfx: key, voice: key} (empty = silent).
func cue(cue_name: String, fac := "", other := "", detail := "") -> Dictionary:
	var out := {}
	var c := BotfAudioCues.cue_for(cue_name, fac if fac != "" else player_faction(), other, detail)
	if c.is_empty() or not enabled:
		return out
	var voice_ok := false
	if not c.voice.is_empty():
		var r := library.resolve_files(era_keys(c.voice))
		if not r.is_empty():
			voice_ok = request_voice(r.key, c.priority, cue_name, r)
			if voice_ok:
				out["voice"] = r.key
	if not c.sfx.is_empty():
		var key := _play_effect(era_keys(c.sfx), c.bus, c.priority, cue_name, null, true)
		if key != "":
			out["sfx"] = key
	elif c.beep and not voice_ok:
		var k2 := _play_effect(PackedStringArray(["sfx.ui.beep"]), BotfAudioCues.BUS_INTERFACE, c.priority, cue_name, null, true)
		if k2 != "":
			out["sfx"] = k2
	if c.bus == BotfAudioCues.BUS_INTERFACE and out.has("sfx"):
		_last_ui_frame = Engine.get_process_frames()
	return out


## Plays the first resolvable of `keys` (BattleAudio and anything else with raw keys). vo.* keys go to the voice
## queue and only play from files. Returns the key that played ("" when nothing).
func play_keys(keys: PackedStringArray, priority := 1, extra_assets: BattleAssets = null, tones := true) -> String:
	if keys.is_empty() or not enabled:
		return ""
	var bus := BotfAudioCues.bus_for_key(keys[0])
	keys = era_keys(keys)
	if bus == BotfAudioCues.BUS_VOICE:
		var r := library.resolve_files(keys, extra_assets)
		if r.is_empty():
			return ""
		var ok := request_voice(r.key, priority, "", r)
		if ok:
			chatter.note_voice(now(), _combat_detail(r.key))
		return r.key if ok else ""
	if bus == BotfAudioCues.BUS_EFFECTS:
		_combat_activity(keys[0])
	return _play_effect(keys, bus, priority, "", extra_assets, tones)


func _play_effect(keys: PackedStringArray, bus: String, priority: int, cue_name: String, extra: BattleAssets, tones: bool) -> String:
	var key := keys[0]
	var stream: AudioStream = null
	var r := library.resolve_files(keys, extra)
	if not r.is_empty():
		key = r.key
		stream = library.stream_of(r, scheduler.pick_variant(r.key, r.files.size()))
	if stream == null:
		if not tones:
			return ""
		for k in keys:
			stream = library.tone_for(k)
			if stream != null:
				key = k
				break
		if stream == null:
			return ""
	var d := scheduler.request_sfx(key, bus, priority, now())
	if not d.play:
		return ""
	if d.cull != 0:
		for p in _sfx:
			if _sfx_handle.get(p, 0) == d.cull:
				p.stop()
				_sfx_handle.erase(p)
	var player := _free_sfx()
	if player == null:
		scheduler.sfx_finished(d.handle)
		return ""
	player.bus = bus
	player.stream = stream
	player.pitch_scale = 1.0 if bus == BotfAudioCues.BUS_INTERFACE else randf_range(0.97, 1.03)
	player.volume_db = -3.0
	_sfx_handle[player] = d.handle
	player.play()
	_note(cue_name, key, bus, "sfx")
	return key


func _free_sfx() -> AudioStreamPlayer:
	for p in _sfx:
		if not p.playing:
			return p
	return null


func _on_sfx_finished(p: AudioStreamPlayer) -> void:
	if _sfx_handle.has(p):
		scheduler.sfx_finished(_sfx_handle[p])
		_sfx_handle.erase(p)


## Queues a voice line whose files are already resolved; it starts when the channel is free.
func request_voice(key: String, priority: int, cue_name := "", resolved: Dictionary = {}) -> bool:
	if not scheduler.request_voice(key, priority, now()):
		return false
	_pending_voice[key] = {"cue": cue_name, "resolved": resolved if not resolved.is_empty() else library.resolve_files(PackedStringArray([key]))}
	return true


var _pending_voice := {}
var _has_cache := {}                   ## chatter detail -> has a clip (files do not change while the game runs)


func _start_voice(line: Dictionary) -> void:
	var info: Dictionary = _pending_voice.get(line.key, {})
	_pending_voice.erase(line.key)
	var r: Dictionary = info.get("resolved", {})
	var s := library.stream_of(r, scheduler.pick_variant(line.key, r.get("files", PackedStringArray()).size())) if not r.is_empty() else null
	if s == null:
		return
	_voice.stream = s
	_voice.volume_db = 0.0
	_voice.play()
	_note(str(info.get("cue", "")), line.key, BotfAudioCues.BUS_VOICE, "voice")


func _note(cue_name: String, key: String, bus: String, kind: String) -> void:
	recent.append({"cue": cue_name, "key": key, "bus": bus, "kind": kind, "t": now()})
	if recent.size() > 64:
		recent.pop_front()
	cue_played.emit(cue_name, key, bus)


# ------------------------------------------------------------------------------------------------------------
# Music
# ------------------------------------------------------------------------------------------------------------

## Switches music to a context ("title", "galaxy", "battle", "tension") with a crossfade. Same context: no change.
func set_music(context: String) -> void:
	var fac := player_faction()
	if context == "" or not BotfAudioCues.MUSIC.has(context) or (context == music_context and fac == _music_fac):
		return
	music_context = context
	_music_fac = fac
	_tension = context == "tension"
	var spec: Dictionary = BotfAudioCues.MUSIC[context]
	var r := library.resolve_files(BotfAudioCues.music_keys(context, fac))
	var stream: AudioStream = null
	_playlist = PackedStringArray()
	if not r.is_empty():
		_playlist = r.files
		_playlist_pos = 0   # a track's segments are in game order: intro first, then on through
		if _adaptive_here():
			_playlist_pos = BotfMusicDirector.first_index(_playlist, 0, music_mood)
		music_key = r.key
		stream = library.stream_at(_playlist[_playlist_pos])
	if stream == null:
		stream = library.music_tone(str(spec.tone))
		music_key = "tone:" + str(spec.tone)
		_playlist = PackedStringArray()
	_crossfade_to(stream)
	music_changed.emit(context)


## Adaptive music applies to the strategic map's tracks only (battle keeps approach / tension).
func _adaptive_here() -> bool:
	return adaptive_music and music_context == "galaxy" and not _playlist.is_empty()


## F5: sets the strategic map's mood. A rise to peak crossfades at once to a peak segment; any other change waits for the
## playing segment to end, then the next segment the mood allows plays.
func set_music_mood(mood: String) -> void:
	if not (mood in BotfMusicDirector.MOODS) or mood == music_mood:
		return
	music_mood = mood
	if not _adaptive_here():
		return
	if mood == "peak" and not BotfMusicDirector.allows(mood, _playlist[_playlist_pos]):
		_playlist_pos = BotfMusicDirector.next_index(_playlist, _playlist_pos, mood)
		var s := library.stream_at(_playlist[_playlist_pos])
		if s != null:
			_crossfade_to(s)


func _crossfade_to(stream: AudioStream) -> void:
	var back := _music[1 - _music_front]
	var front := _music[_music_front]
	# The player that was in front fades out; the idle one fades in.
	_music_front = 1 - _music_front
	back.stream = stream
	back.volume_db = -80.0
	if enabled:
		back.play()
	_fade = 0.0
	_fading = true
	if not front.playing:
		_fade = 1.0


func _on_music_finished(p: AudioStreamPlayer) -> void:
	if p != _music[_music_front] or _playlist.is_empty():
		if p == _music[_music_front] and p.stream is AudioStreamWAV:
			p.play()
		return
	_playlist_pos = BotfMusicDirector.next_index(_playlist, _playlist_pos, music_mood) if _adaptive_here() else (_playlist_pos + 1) % _playlist.size()
	var s := library.stream_at(_playlist[_playlist_pos])
	if s != null:
		p.stream = s
		p.play()


func _combat_activity(key: String) -> void:
	if music_context == "battle" and (key.begins_with("weapon.") or key.begins_with("sfx.shield") or key.begins_with("sfx.explosion")):
		set_music("tension")
	var kind := chatter_kind(key)
	if kind != "":
		chatter.note(kind, now())


## What a played effect key tells the chatter layer ("" = nothing).
static func chatter_kind(key: String) -> String:
	if key.begins_with("weapon."):
		return "torpedo" if key.to_lower().contains("torpedo") else "beam"
	if key.begins_with("sfx.shield"):
		return "shield"
	if key.begins_with("sfx.explosion.small"):
		return "hull"
	if key.begins_with("sfx.explosion"):
		return "destroyed"
	return ""


## "shields.50" from "vo.combat.shields.50.fed(.tng)"; "" for anything else. Used to keep chatter off a detail a bark just spoke.
static func _combat_detail(key: String) -> String:
	if not key.begins_with("vo.combat."):
		return ""
	var parts := key.trim_prefix("vo.combat.").split(".")
	if parts.size() >= 3 and parts[0] == "shields" and parts[1].is_valid_int():
		return "shields." + parts[1]
	return parts[0] if parts.size() > 0 else ""


## The bridge chatter layer: in a fight, a quiet crew line drawn from what just happened. Returns the key played ("" none).
func chatter_tick() -> String:
	if not chatter_enabled or not enabled or library == null:
		return ""
	if music_context != "battle" and music_context != "tension":
		return ""
	if scheduler.voice_active() or scheduler.queued_voices() > 0:
		return ""
	var fac := player_faction()
	if fac == "":
		return ""
	var t := now()
	var detail := chatter.next_line(t, func(d: String) -> bool:
		var ck := fac + "/" + d + "/" + current_era()
		if not _has_cache.has(ck):
			_has_cache[ck] = library.has_files(era_keys(BotfAudioCues.combat_voice_keys(d, fac)))
		return _has_cache[ck])
	if detail == "":
		return ""
	var keys := era_keys(BotfAudioCues.combat_voice_keys(detail, fac))
	var r := library.resolve_files(keys)
	var played := not r.is_empty() and request_voice(r.key, 2, "chatter", r)
	chatter.commit(detail, t, played)
	return r.key if played else ""


## An order button was pressed (Maneuver_<key> in the combat screen): the beep, then the crew's acknowledgement
## (vo.combat.<detail>.<fac>, era twin first, "ack" last). Returns what played, like cue().
func order_ack(maneuver: String) -> Dictionary:
	var out := {}
	if not enabled:
		return out
	var fac := player_faction()
	var keys := era_keys(BotfAudioCues.order_voice_keys(maneuver, fac))
	var r := library.resolve_files(keys) if not keys.is_empty() else {}
	if not r.is_empty() and request_voice(r.key, 6, "order_accepted", r):
		out["voice"] = r.key
		chatter.note_voice(now())
	var c := BotfAudioCues.cue_for("order_accepted", fac)
	var k := _play_effect(c.sfx, c.bus, c.priority, "order_accepted", null, true)
	if k != "":
		out["sfx"] = k
	_last_ui_frame = Engine.get_process_frames()
	return out


# ------------------------------------------------------------------------------------------------------------
# App hooks (the App calls the statics; everything is null-safe when no autoload exists)
# ------------------------------------------------------------------------------------------------------------

## Called once from App._ready: shares the settings dictionary and listens for screen changes.
static func attach(app: Node) -> void:
	var sys := instance
	if sys == null:
		return
	sys._app = app
	if "settings" in app:
		sys.attach_settings(app.settings)
	if app.has_signal("screen_changed") and not app.screen_changed.is_connected(sys._on_screen):
		app.screen_changed.connect(sys._on_screen)


## Web: after an empire's audio pck is mounted, reads its cue table (the library only loaded the packs present at boot).
## Web: the shared music pack has just been mounted. A generated bed that is playing in its place switches to the real
## tracks at once (set_music finds the files now; before the mount they did not exist, so the key resolved to a tone).
static func music_pack_ready() -> void:
	if instance == null or not instance.music_key.begins_with("tone:"):
		return
	var ctx: String = instance.music_context
	instance.music_context = ""
	instance.set_music(ctx)


static func load_empire_pack(empire: String) -> bool:
	return instance != null and instance.library != null and instance.library.load_pack(empire)


## F3: campaign orders the crew acknowledges by voice (command type -> the battle maneuver whose voice group fits).
## Unit and fleet orders only; queue, policy and diplomacy clicks keep their interface beep.
const CAMPAIGN_ACKS := {"MoveFleet": "Move", "SetFleetOrder": "Move", "SetTaskForceOrder": "Move", "Colonize": "Move",
	"TerraformPlanet": "Move", "UpgradeStation": "Move", "RetreatFleet": "Retreat", "Invade": "Engage", "SetTacticalStance": "Move",
	"CreateTaskForce": "Move", "EstablishTradeRoute": "Move"}
const ACK_GAP := 0.8
var _last_ack := -100.0

## Called by AppSession.issue after the core accepts a command. Returns what played ({} when the order has no voice).
func campaign_ack(command: Dictionary) -> Dictionary:
	var m: String = CAMPAIGN_ACKS.get(str(command.get("type", "")), "")
	if m == "" or not enabled or now() - _last_ack < ACK_GAP:
		return {}
	_last_ack = now()
	return order_ack(m)

static func ack_command(command: Dictionary) -> void:
	if instance != null:
		instance.campaign_ack(command)


static func cue_now(cue_name: String, fac := "", other := "", detail := "") -> void:
	if instance != null:
		instance.cue(cue_name, fac, other, detail)


static func on_battle_joined() -> void:
	if instance != null:
		instance.chatter.reset()
		instance.cue("encounter_opens")


## The turn's briefing: turn start, then one cue per kind from its events (research, construction, diplomacy...).
## skip_outcome: the end screen is about to open and plays the leader's speech itself (F4 end-game speeches).
static func on_turn_briefing(briefing: Variant, view: Dictionary, skip_outcome := false) -> void:
	var sys := instance
	if sys == null or not (briefing is Dictionary):
		return
	var meta: Dictionary = view.get("meta", {})
	var player_id := int(meta.get("player", 0))
	var empires: Array = []
	for e in view.get("empires", []):
		empires.append({"id": int(e.get("empire", e.get("id", 0))), "name": str(e.get("name", ""))})
	if empires.is_empty() and sys._app != null and "meta" in sys._app:
		empires = sys._app.meta.get("empires", [])
	for c in BotfAudioCues.turn_cues(briefing, str(meta.get("playerName", "")), empires, player_id):
		if skip_outcome and c.cue in ["victory", "defeat"]:
			continue
		sys.cue(c.cue, "", c.other, c.detail)


func _on_screen(id: String) -> void:
	var ctx := BotfAudioCues.music_for_screen(id)
	if ctx != "":
		set_music(ctx)
	if _prev_screen != "" and id != "title":
		cue("ui_open")
	_prev_screen = id


func _on_node_added(n: Node) -> void:
	if _bound_buttons and n is BaseButton and not n.has_meta("botf_silent"):
		var nm := str(n.name)
		if nm.begins_with("Maneuver_"):
			var ack := order_ack.bind(nm.trim_prefix("Maneuver_"))
			if not n.pressed.is_connected(ack):
				n.pressed.connect(ack)   # before the click sound: it then skips the frame
		if not n.pressed.is_connected(_on_button):   # a node re-added to the tree must not be bound twice
			n.pressed.connect(_on_button)


func _on_button() -> void:
	if Engine.get_process_frames() == _last_ui_frame:
		return   # a specific cue already played this frame (end turn, etc.)
	cue("ui_click")


func player_faction() -> String:
	if _app != null and _app.has_method("player"):
		return BotfAudioCues.faction_prefix(int(_app.player()))
	return ""


## 4 Hz: battle tension once rounds have been fought, red alert when a new hostile contact appears.
func _poll_app() -> void:
	if _app == null or not enabled:
		return
	var screen: String = str(_app.get("screen"))
	if screen == "combat":
		var enc: Variant = _app.get("encounter")
		if enc is Dictionary and music_context == "battle" and enc.get("rounds", []).size() > 0:
			set_music("tension")
	elif music_context == "tension":
		set_music(BotfAudioCues.music_for_screen(screen))
	var view: Dictionary = _app.get("view")
	if view.is_empty():
		_seen_contacts.clear()
		_last_colonies = -1
		return
	if _app.has_method("player"):
		var pid := int(_app.player())
		var cols := BotfMusicDirector.player_colonies(view, pid)
		# A loss in the same campaign plays one peak segment (another campaign or save resets the count).
		var same := int(view.get("meta", {}).get("seed", 0)) == _seed
		var lost := same and _last_colonies >= 0 and cols < _last_colonies
		_last_colonies = cols
		set_music_mood(BotfMusicDirector.mood_for(view, pid, lost))
	var fresh := false
	var seed_now := int(view.get("meta", {}).get("seed", 0))
	if seed_now != _seed:
		_seed = seed_now
		_seen_contacts.clear()
	var first := not _seen_contacts.has("init")
	for c in view.get("contacts", []):
		var id := int(c.get("id", 0))
		if not _seen_contacts.has(id):
			_seen_contacts[id] = true
			fresh = true
	_seen_contacts["init"] = true
	if fresh and not first:
		cue("red_alert")
