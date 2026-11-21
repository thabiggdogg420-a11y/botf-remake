class_name BotfAudioLibrary
extends RefCounted
## Resolves a sound key to packed files, then to a generated tone. Everything is optional and private. Every load goes
## through AppLoader (res:// and user:// only; no disk paths, no env-var paths, same code on desktop and web). Sources:
##   1. the resolved manifest record for the key (BattleAssets), kept only when its files are res:// or user://;
##   2. cues.json tables: {"cues": {key: ["voice/english_0001.ogg", ...]}} (paths relative to the table), from
##      - the audio pack: res://scripts/audio/pack/<pack>/cues.json ("shared", "fed", "kli", ...), ogg only, built by
##        remaster/tools/pack_audio_web.py from LocalAssets/audio (BOTF banks) and LocalAssets/audio/trek (Trek library);
##      - user://audio/cues.json and user://audio/trek/cues.json, a mounted or hand-placed bundle;
##   3. res://scenes/battle/audio/<key>.wav|ogg.
## Trek-library clips sit beside the BOTF ones under era keys (weapon.Phaser.ds9); BotfAudioCues.with_era() asks for
## those first. A key with several files rotates through them.

const PACK_ROOT := "res://scripts/audio/pack"
const USER_ROOT := "user://audio"

var assets: BattleAssets
var cue_files := {}             ## key -> PackedStringArray of res:// or user:// paths
var cues_path := ""
var cues_sources: PackedStringArray = []   ## every cues file merged, in load order
var eras_present := {}          ## era code -> true when some key ends with .<era> (so era twins are worth looking up)
var loaded_packs: PackedStringArray = []
var _streams := {}              ## path -> AudioStream (or null when it would not load)
var _tones := {}                ## kind -> AudioStream
var _music_tasks := {}          ## kind -> WorkerThreadPool task id
var _mutex := Mutex.new()


func _init(p_assets: BattleAssets = null) -> void:
	assets = p_assets if p_assets != null else BattleAssets.new()
	for rel in ["cues.json", "trek/cues.json"]:
		var f := USER_ROOT.path_join(rel)
		if AppLoader.exists(f):
			load_cues(f, cues_sources.size() > 0)
	# The packed audio (shared + every empire pack present). On a web export this is all there is.
	for pack in pack_names():
		load_pack(pack)


## Names of the packs listed in pack_manifest.json under PACK_ROOT (empty when no pack was built).
static func pack_names() -> PackedStringArray:
	var out := PackedStringArray()
	var data: Variant = AppLoader.json(PACK_ROOT + "/pack_manifest.json")
	if data is Dictionary and data.get("packs") is Dictionary:
		for k in data.packs:
			out.append(str(k))
	return out


## Mounts one pack's cue table (idempotent). A lazily fetched empire pack calls AppLoader.mount_pack, then this.
func load_pack(pack: String) -> bool:
	if loaded_packs.has(pack):
		return true
	if not load_cues(PACK_ROOT.path_join(pack).path_join("cues.json"), true):
		return false
	loaded_packs.append(pack)
	return true


## Loads a cues.json. Returns false when it is missing or unreadable. merge: keep what is loaded and add this file's
## keys (a key already present gains the new files after its own).
func load_cues(path: String, merge := false) -> bool:
	var data: Variant = AppLoader.json(path)
	if not (data is Dictionary) or not (data.get("cues") is Dictionary):
		return false
	if not merge:
		cue_files.clear()
		eras_present.clear()
		cues_sources.clear()
	cues_path = path
	cues_sources.append(path)
	var base := path.get_base_dir()
	for key in data.cues:
		var list: PackedStringArray = cue_files.get(str(key), PackedStringArray())
		for rel in data.cues[key]:
			var rp := str(rel).replace("\\", "/")
			var full := rp if AppLoader.allowed(rp) else base.path_join(rp)
			if not list.has(full):
				list.append(full)
		cue_files[str(key)] = list
		var tail := str(key).get_extension()
		if BotfAudioCues.ERAS.has(tail):
			eras_present[tail] = true
	return true


## True when some loaded key carries this era's twin, so BotfAudioCues.with_era is worth applying.
func has_era(era: String) -> bool:
	return eras_present.has(era)


## Existing files for one key (not following fallbacks).
func files_for(key: String, extra: BattleAssets = null) -> PackedStringArray:
	var out := PackedStringArray()
	for a in [assets, extra]:
		if a != null and a.has_manifest():
			var rec: Dictionary = a.resolve(key)
			if not rec.is_empty():
				for f in a.local_files(rec, PackedStringArray(["wav", "ogg"])):
					if AppLoader.allowed(f) and not out.has(f) and not AudioDoubtful.blocked(f):   # a disk path never reaches the runtime
						out.append(f)
	if cue_files.has(key):
		for f in cue_files[key]:
			if not out.has(f) and not AudioDoubtful.blocked(f) and AppLoader.exists(f):
				out.append(f)
	for ext in ["wav", "ogg"]:
		var rp := "res://scenes/battle/audio/%s.%s" % [key, ext]
		if AppLoader.exists(rp) and not out.has(rp):
			out.append(rp)
	return out


## The first key of `keys` with local files: {key, files} or {}.
func resolve_files(keys: PackedStringArray, extra: BattleAssets = null) -> Dictionary:
	for k in keys:
		var f := files_for(k, extra)
		if not f.is_empty():
			return {"key": k, "files": f}
	return {}


func has_files(keys: PackedStringArray, extra: BattleAssets = null) -> bool:
	return not resolve_files(keys, extra).is_empty()


## An AudioStream for a res:// or user:// path ("" never). Cached; null when the file would not load. res:// files are
## imported resources (the same in the exe and the web pck); user:// files are read raw (wav or ogg).
func stream_at(path: String) -> AudioStream:
	if _streams.has(path):
		return _streams[path]
	var s: AudioStream = null
	if path.begins_with("res://"):
		if ResourceLoader.exists(path):
			s = AppLoader.resource(path) as AudioStream
		elif path.get_extension().to_lower() == "ogg" and AppLoader.exists(path):
			s = AudioStreamOggVorbis.load_from_file(path)   # a raw ogg from a web empire pack (audio_<emp>_web.pck), not imported
	elif AppLoader.allowed(path) and AppLoader.exists(path):
		match path.get_extension().to_lower():
			"wav": s = AudioStreamWAV.load_from_file(path)
			"ogg": s = AudioStreamOggVorbis.load_from_file(path)
	_streams[path] = s
	return s


## The stream for the n-th file of a resolved record (wraps), or null.
func stream_of(resolved: Dictionary, index := 0) -> AudioStream:
	var files: PackedStringArray = resolved.get("files", PackedStringArray())
	if files.is_empty():
		return null
	for off in files.size():
		var s := stream_at(files[(index + off) % files.size()])
		if s != null:
			return s
	return null


## The generated tone for a key, or null (voice and music keys never get one).
func tone_for(key: String) -> AudioStream:
	var kind := BotfTones.kind_for(key)
	if kind == "":
		return null
	return tone_kind(kind)


func tone_kind(kind: String) -> AudioStream:
	_mutex.lock()
	var have: bool = _tones.has(kind)
	_mutex.unlock()
	if not have:
		var s := BotfTones.synth(kind)
		_mutex.lock()
		_tones[kind] = s
		_mutex.unlock()
	return _tones[kind]


## Starts generating the four music beds on worker threads so the first title frame does not stall.
func prewarm_music() -> void:
	for kind in ["title", "galaxy", "battle", "tension"]:
		if not _music_tasks.has(kind):
			_music_tasks[kind] = WorkerThreadPool.add_task(_gen_music.bind(kind), false, "botf music " + kind)


func _gen_music(kind: String) -> void:
	var s := BotfTones.music(kind)
	_mutex.lock()
	_tones["music." + kind] = s
	_mutex.unlock()


## True when the generated bed for kind is already built (no wait, no generation).
func music_tone_ready(kind: String) -> bool:
	_mutex.lock()
	var have: bool = _tones.has("music." + kind)
	_mutex.unlock()
	return have


func music_tone(kind: String) -> AudioStream:
	if _music_tasks.has(kind):
		WorkerThreadPool.wait_for_task_completion(_music_tasks[kind])
		_music_tasks.erase(kind)
	_mutex.lock()
	var have: bool = _tones.has("music." + kind)
	_mutex.unlock()
	if not have:
		var s := BotfTones.music(kind)
		_mutex.lock()
		_tones["music." + kind] = s
		_mutex.unlock()
	return _tones["music." + kind]


## Blocks until any worker-thread music generation has finished (tests, shutdown).
func finish_tasks() -> void:
	for kind in _music_tasks.keys():
		WorkerThreadPool.wait_for_task_completion(_music_tasks[kind])
	_music_tasks.clear()
