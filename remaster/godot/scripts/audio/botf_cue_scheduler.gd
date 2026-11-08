class_name BotfCueScheduler
extends RefCounted
## Decides what actually plays: per-key cooldowns, a cap on simultaneous effects (lowest priority, then oldest,
## is culled; a new sound never culls a higher priority one), and one voice line at a time with a gap between
## lines, a priority queue and a staleness timeout. Time is passed in (seconds), so it is deterministic and
## unit-tested. Port of the CueScheduler in the Unreal client's BotfAudioCues.h. Audio never feeds the simulation.

var max_sfx := 24
var max_voice_queue := 4
var voice_gap := 0.6
var voice_timeout := 6.0
var history_seconds := 10.0
var salvo_voices := 4

var _played: Array = []     ## [{key, at}] recent plays, for cooldowns
var _sfx: Array = []        ## [{handle, priority, at}]
var _queue: Array = []      ## [{key, priority, at, seq}]
var _variants := {}         ## key -> next variant index
var _handle := 0
var _seq := 0
var _voice_ended_at := -1.0
var _voice_playing := false


## Effect request. {play: bool, handle: int, cull: int}. cull is an instance the caller must stop first (0 = none).
func request_sfx(key: String, bus: String, priority: int, now: float) -> Dictionary:
	var d := {"play": false, "handle": 0, "cull": 0}
	if not _off_cooldown(key, bus, now):
		return d
	_prune(now)
	if _sfx.size() >= maxi(1, max_sfx):
		var victim := 0
		for i in _sfx.size():
			var a: Dictionary = _sfx[i]
			var v: Dictionary = _sfx[victim]
			if a.priority < v.priority or (a.priority == v.priority and a.at < v.at):
				victim = i
		if _sfx[victim].priority > priority:
			return d
		d.cull = _sfx[victim].handle
		_sfx.remove_at(victim)
	_played.append({"key": key, "at": now})
	_handle += 1
	d.play = true
	d.handle = _handle
	_sfx.append({"handle": _handle, "priority": priority, "at": now})
	return d


func sfx_finished(handle: int) -> void:
	for i in _sfx.size():
		if _sfx[i].handle == handle:
			_sfx.remove_at(i)
			return


func active_sfx() -> int:
	return _sfx.size()


## Voice request: queued by priority (higher first, then request order). A repeat of the key inside its cooldown,
## or of a key already queued, is dropped. A full queue gives way only to a higher priority line.
func request_voice(key: String, priority: int, now: float) -> bool:
	if key == "" or not _off_cooldown(key, BotfAudioCues.BUS_VOICE, now):
		return false
	for line in _queue:
		if line.key == key:
			return false
	if _queue.size() >= maxi(1, max_voice_queue):
		var lowest := 0
		for i in _queue.size():
			var a: Dictionary = _queue[i]
			var l: Dictionary = _queue[lowest]
			if a.priority < l.priority or (a.priority == l.priority and a.seq > l.seq):
				lowest = i
		if _queue[lowest].priority >= priority:
			return false
		_queue.remove_at(lowest)
	_seq += 1
	_queue.append({"key": key, "priority": priority, "at": now, "seq": _seq})
	return true


## Call every frame with whether the voice channel is still playing. Returns the line to start now ({} = none).
func next_voice(now: float, channel_busy: bool) -> Dictionary:
	if channel_busy:
		_voice_ended_at = -1.0
		_voice_playing = true
		return {}
	if _voice_playing:
		_voice_playing = false
		_voice_ended_at = now
	_queue = _queue.filter(func(l): return now - l.at <= voice_timeout)
	if _queue.is_empty():
		return {}
	if _voice_ended_at >= 0.0 and now - _voice_ended_at < voice_gap:
		return {}
	var best := 0
	for i in _queue.size():
		var a: Dictionary = _queue[i]
		var b: Dictionary = _queue[best]
		if a.priority > b.priority or (a.priority == b.priority and a.seq < b.seq):
			best = i
	var line: Dictionary = _queue[best]
	_queue.remove_at(best)
	_played.append({"key": line.key, "at": now})
	_voice_playing = true
	return line


func voice_active() -> bool:
	return _voice_playing


func queued_voices() -> int:
	return _queue.size()


## Round-robin choice among a key's files, stable per key.
func pick_variant(key: String, count: int) -> int:
	if count <= 1:
		return 0
	var i: int = int(_variants.get(key, 0))
	_variants[key] = i + 1
	return i % count


func _prune(now: float, max_seconds := 12.0) -> void:
	_sfx = _sfx.filter(func(s): return now - s.at <= max_seconds)


func _off_cooldown(key: String, bus: String, now: float) -> bool:
	var cd := BotfAudioCues.cooldown_for(key, bus)
	_played = _played.filter(func(p): return now - p.at <= history_seconds)
	# Salvos (R3.5, Simon: "no sounds for weapons"): a weapon or impact key may overlap itself up to salvo_voices
	# times inside its cooldown, so 8 torpedoes from a broadside sound as a ripple, not one blip. Other keys still
	# play once per cooldown.
	# Cloak and decloak play for every ship (coordinator, 8 Nov: a wing cloaking together played one sound in four).
	var allowed := salvo_voices if (key.begins_with("weapon.") or key.begins_with("sfx.shield") or key.begins_with("sfx.explosion") or key.begins_with("sfx.cloak") or key.begins_with("sfx.decloak")) else 1
	var recent := 0
	for p in _played:
		if p.key == key and now - p.at < cd:
			recent += 1
	return recent < allowed
