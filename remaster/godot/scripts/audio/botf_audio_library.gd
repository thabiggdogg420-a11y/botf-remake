class_name BotfAudioLibrary
extends RefCounted
## Resolves a sound key to local files, then to a generated tone. Everything is optional and private: files live in
## the gitignored LocalAssets (remaster/game/LocalAssets), found through
##   1. the resolved manifest record for the key (the Unreal importer's output, via BattleAssets), and its fallback chain;
##   2. LocalAssets/audio/cues.json: {"cues": {key: ["voice/english_0001.ogg", ...]}} (paths relative to audio/), written by
##      remaster/tools/build_audio_cues.py from the extracted BOTF banks;
##   3. LocalAssets/audio/<key>.wav|ogg or <key>/*.wav|ogg (the Import-Assets.ps1 layout);
##   4. res://scenes/battle/audio/<key>.wav|ogg.
## $BOTF_AUDIO_DIR overrides the audio folder. A key with several files rotates through them.

var assets: BattleAssets
var audio_dirs: PackedStringArray = []
var cue_files := {}             ## key -> PackedStringArray of absolute paths (cues.json)
var cues_path := ""
var _streams := {}              ## path -> AudioStream (or null when it would not load)
var _tones := {}                ## kind -> AudioStream
var _music_tasks := {}          ## kind -> WorkerThreadPool task id
var _mutex := Mutex.new()


func _init(p_assets: BattleAssets = null) -> void:
	assets = p_assets if p_assets != null else BattleAssets.new()
	var env := OS.get_environment("BOTF_AUDIO_DIR")
	if env != "":
		audio_dirs.append(env)
	if assets.root_dir != "":
		audio_dirs.append(assets.root_dir.path_join("audio"))
	for p in BattleAssets.candidate_paths():
		var d := p.get_base_dir().path_join("audio")
		if not audio_dirs.has(d):
			audio_dirs.append(d)
	for d in audio_dirs:
		var f: String = d.path_join("cues.json")
		if FileAccess.file_exists(f) and load_cues(f):
			break


## Loads a cues.json. Returns false when it is unreadable.
func load_cues(path: String) -> bool:
	var data = JSON.parse_string(FileAccess.get_file_as_string(path))
	if not (data is Dictionary) or not (data.get("cues") is Dictionary):
		return false
	cues_path = path
	cue_files.clear()
	var base := path.get_base_dir()
	for key in data.cues:
		var list := PackedStringArray()
		for rel in data.cues[key]:
			var rp := str(rel).replace("\\", "/")
			list.append(rp if rp.is_absolute_path() else base.path_join(rp))
		cue_files[str(key)] = list
	return true


## Existing local files for one key (not following fallbacks).
func files_for(key: String, extra: BattleAssets = null) -> PackedStringArray:
	var out := PackedStringArray()
	for a in [assets, extra]:
		if a != null and a.has_manifest():
			var rec: Dictionary = a.resolve(key)
			if not rec.is_empty():
				for f in a.local_files(rec, PackedStringArray(["wav", "ogg", "mp3"])):
					if not out.has(f):
						out.append(f)
	if cue_files.has(key):
		for f in cue_files[key]:
			if not out.has(f) and FileAccess.file_exists(f):
				out.append(f)
	for d in audio_dirs:
		for ext in ["wav", "ogg"]:
			var single: String = d.path_join(key + "." + ext)
			if FileAccess.file_exists(single) and not out.has(single):
				out.append(single)
		var dir: String = d.path_join(key)
		if DirAccess.dir_exists_absolute(dir):
			var names := Array(DirAccess.get_files_at(dir))
			names.sort()
			for n in names:
				if str(n).get_extension().to_lower() in ["wav", "ogg", "mp3"]:
					var f: String = dir.path_join(str(n))
					if not out.has(f):
						out.append(f)
	for ext in ["wav", "ogg"]:
		var rp := "res://scenes/battle/audio/%s.%s" % [key, ext]
		if ResourceLoader.exists(rp) and not out.has(rp):
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


## An AudioStream for a path ("" never). Cached; null when the file would not load.
func stream_at(path: String) -> AudioStream:
	if _streams.has(path):
		return _streams[path]
	var s: AudioStream = null
	if path.begins_with("res://"):
		s = load(path) as AudioStream if ResourceLoader.exists(path) else null
	elif FileAccess.file_exists(path):
		match path.get_extension().to_lower():
			"wav": s = AudioStreamWAV.load_from_file(path)
			"ogg": s = AudioStreamOggVorbis.load_from_file(path)
			"mp3":
				var m := AudioStreamMP3.new()
				m.data = FileAccess.get_file_as_bytes(path)
				s = m
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
