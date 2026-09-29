class_name BattleAssets
extends RefCounted
## Optional local HD assets: LocalAssets/resolved-manifest.json (written by remaster/game/Import-Assets.ps1) plus the
## glTF / FBX models and WAV / OGG audio it points at. Everything here is optional and gitignored; when nothing
## resolves the battle uses procedural hulls and generated tones.
##
## Manifest lookup order: $BOTF_RESOLVED_MANIFEST, <exe dir>/LocalAssets, res://../LocalAssets (remaster/LocalAssets),
## res://../game/LocalAssets (where Import-Assets.ps1 writes it).

var manifest_path := ""
var root_dir := ""
var assets := {}          ## key -> record {status, importFiles, files, fallback, effectiveKey, ...}
var _scenes := {}         ## key -> PackedScene or null
var _streams := {}        ## key -> AudioStream or null


func _init() -> void:
	for p in candidate_paths():
		if FileAccess.file_exists(p):
			load_manifest(p)
			break


static func candidate_paths() -> PackedStringArray:
	var out := PackedStringArray()
	var env := OS.get_environment("BOTF_RESOLVED_MANIFEST")
	if env != "":
		out.append(env)
	out.append(OS.get_executable_path().get_base_dir().path_join("LocalAssets/resolved-manifest.json"))
	var res := ProjectSettings.globalize_path("res://")
	out.append(res.path_join("../LocalAssets/resolved-manifest.json").simplify_path())
	out.append(res.path_join("../game/LocalAssets/resolved-manifest.json").simplify_path())
	return out


func load_manifest(path: String) -> bool:
	var data = JSON.parse_string(FileAccess.get_file_as_string(path))
	if not (data is Dictionary):
		return false
	manifest_path = path
	root_dir = path.get_base_dir()
	assets = data.get("assets", {})
	return true


func has_manifest() -> bool:
	return not assets.is_empty()


## Walks the manifest fallback chain from `key` to the first resolved / in-repo record.
func resolve(key: String) -> Dictionary:
	var seen := {}
	while key != "" and not seen.has(key):
		seen[key] = true
		var rec = assets.get(key)
		if rec == null:
			return {}
		if rec.get("status", "") in ["resolved", "in-repo"]:
			return rec
		key = str(rec.get("fallback", "")) if rec.get("fallback") != null else ""
	return {}


## Local files of a record that exist on this machine (Windows paths rebased under the manifest's folder).
func local_files(rec: Dictionary, exts: PackedStringArray) -> PackedStringArray:
	var out := PackedStringArray()
	for list_key in ["importFiles", "files"]:
		for f in rec.get(list_key, []):
			var p := str(f).replace("\\", "/")
			if not exts.has(p.get_extension().to_lower()):
				continue
			if FileAccess.file_exists(p):
				out.append(p)
				continue
			# Rebase ".../LocalAssets/<rest>" onto this manifest's folder.
			var at := p.findn("LocalAssets/")
			if at >= 0:
				var local := root_dir.path_join(p.substr(at + "LocalAssets/".length()))
				if FileAccess.file_exists(local):
					out.append(local)
	return out


## A new instance of the first model any of `keys` resolves to, or null.
func instantiate_ship(keys: PackedStringArray) -> Node3D:
	if not has_manifest():
		return null
	for key in keys:
		if _scenes.has(key):
			if _scenes[key] != null:
				return _scenes[key].instantiate()
			continue
		_scenes[key] = null
		var rec := resolve(key)
		if rec.is_empty():
			continue
		for path in local_files(rec, PackedStringArray(["glb", "gltf", "fbx"])):
			var scene := _load_model(path)
			if scene != null:
				_scenes[key] = scene
				return scene.instantiate()
	return null


static func _load_model(path: String) -> PackedScene:
	var state: GLTFState
	var doc: GLTFDocument
	if path.get_extension().to_lower() == "fbx":
		doc = FBXDocument.new()
		state = FBXState.new()
	else:
		doc = GLTFDocument.new()
		state = GLTFState.new()
	if doc.append_from_file(path, state) != OK:
		return null
	var node := doc.generate_scene(state)
	if node == null:
		return null
	var packed := PackedScene.new()
	_own(node, node)
	packed.pack(node)
	node.free()
	return packed


static func _own(node: Node, owner: Node) -> void:
	for c in node.get_children():
		c.owner = owner
		_own(c, owner)


## Audio stream for the first of `keys` that resolves to a local file: the manifest first, then
## <manifest dir>/audio/<key>.(wav|ogg) and res://scenes/battle/audio/<key>.(wav|ogg). Null when none.
func stream_for(keys: PackedStringArray) -> AudioStream:
	for key in keys:
		if _streams.has(key):
			if _streams[key] != null:
				return _streams[key]
			continue
		_streams[key] = _find_stream(key)
		if _streams[key] != null:
			return _streams[key]
	return null


func _find_stream(key: String) -> AudioStream:
	var paths := PackedStringArray()
	if has_manifest():
		var rec := resolve(key)
		if not rec.is_empty():
			paths.append_array(local_files(rec, PackedStringArray(["wav", "ogg"])))
		var dir := root_dir.path_join("audio").path_join(key)
		if DirAccess.dir_exists_absolute(dir):
			for f in DirAccess.get_files_at(dir):
				paths.append(dir.path_join(f))
	for ext in ["wav", "ogg"]:
		paths.append("res://scenes/battle/audio/%s.%s" % [key, ext])
	for p in paths:
		var s := _load_stream(p)
		if s != null:
			return s
	return null


static func _load_stream(path: String) -> AudioStream:
	if path.begins_with("res://"):
		return load(path) as AudioStream if ResourceLoader.exists(path) else null
	if not FileAccess.file_exists(path):
		return null
	match path.get_extension().to_lower():
		"wav": return AudioStreamWAV.load_from_file(path)
		"ogg": return AudioStreamOggVorbis.load_from_file(path)
	return null
