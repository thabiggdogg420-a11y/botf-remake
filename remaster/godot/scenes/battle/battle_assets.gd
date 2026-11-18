class_name BattleAssets
extends RefCounted
## Optional local HD assets: LocalAssets/resolved-manifest.json (written by remaster/game/Import-Assets.ps1) plus the
## glTF / FBX models and WAV / OGG audio it points at. Everything here is optional and gitignored; when nothing
## resolves the battle uses procedural hulls and generated tones.
##
## Manifest lookup order: $BOTF_RESOLVED_MANIFEST (dev override), the mounted ship pack (res://assets/ships/, from
## ships_desktop.pck or the web ships_<fac>_web.pck, built by tools/build_ship_packs.py), then the dev-only disk folders:
## <exe dir>/LocalAssets, res://../LocalAssets (remaster/LocalAssets), res://../game/LocalAssets (where Import-Assets.ps1
## writes it). A packaged build needs no LocalAssets folder and no environment variable: the packs carry everything.
##
## Hero ships (remaster/game/Tools/blender_hero_ships.py --gltf) come from ships/godot-hero-manifest.json beside any of
## those (or $BOTF_HERO_MANIFEST): a hero glb wins over the resolved manifest's record for its key. Its instance
## carries meta "botf_hero" (the manifest entry: canon length, sockets, LODs, material hints) and "botf_hero_root".

const HERO_MANIFEST := "godot-hero-manifest.json"
## Where the ship packs put the manifests and glbs (res://assets/ships/<key>/SM_<slug>.glb).
const PACK_DIR := "res://assets/ships"

var manifest_path := ""
var root_dir := ""
var assets := {}          ## key -> record {status, importFiles, files, fallback, effectiveKey, ...}
var _scenes := {}         ## key -> PackedScene or null
var _streams := {}        ## key -> AudioStream or null
var hero_manifest_path := ""
var hero_root := ""
var heroes := {}          ## key -> godot-hero-manifest.json entry {glb, lengthMeters, sockets, lods, slots, ...}
var _hero_scenes := {}    ## key -> PackedScene or null


## Every file probe in the battle scenes goes through these three. res:// and user:// go to AppLoader (the browser
## target's one loader). Other paths are the developer's LocalAssets (hero glbs, resolved manifest, local audio):
## a desktop-only dev step, refused outright in the web build, where those assets must arrive as a mounted pack.
static func local_disk_allowed(path: String) -> bool:
	return not AppLoader.allowed(path) and not OS.has_feature("web")


static func file_exists(path: String) -> bool:
	if AppLoader.allowed(path):
		return AppLoader.exists(path)
	return local_disk_allowed(path) and FileAccess.file_exists(path)


static func read_json(path: String) -> Variant:
	if AppLoader.allowed(path):
		return AppLoader.json(path)
	if not BattleAssets.file_exists(path):
		return null
	return JSON.parse_string(FileAccess.get_file_as_string(path))


static func list_dir(dir: String) -> PackedStringArray:
	if AppLoader.allowed(dir):
		return AppLoader.list(dir)
	if not local_disk_allowed(dir) or not DirAccess.dir_exists_absolute(dir):
		return PackedStringArray()
	return DirAccess.get_files_at(dir)


func _init() -> void:
	for p in candidate_paths():
		if BattleAssets.file_exists(p):
			load_manifest(p)
			break
	for p in hero_candidate_paths():
		if BattleAssets.file_exists(p) and load_hero_manifest(p):
			break


## Where godot-hero-manifest.json may be: $BOTF_HERO_MANIFEST, then ships/ beside each resolved-manifest location.
static func hero_candidate_paths() -> PackedStringArray:
	var out := PackedStringArray()
	var env := OS.get_environment("BOTF_HERO_MANIFEST")
	if env != "":
		out.append(env)
	out.append(PACK_DIR.path_join(HERO_MANIFEST))
	for p in candidate_paths():
		if not p.begins_with(PACK_DIR):
			out.append(p.get_base_dir().path_join("ships").path_join(HERO_MANIFEST))
	return out


func load_hero_manifest(path: String) -> bool:
	var data = BattleAssets.read_json(path)
	if not (data is Dictionary) or not (data.get("heroes") is Dictionary):
		return false
	hero_manifest_path = path
	hero_root = path.get_base_dir()
	heroes = data.heroes
	_hero_scenes.clear()
	return true


## Packaged-build check (pc_smoke, screenshot.gd SHOT_HERO_CHECK): for the ships of a battle `view` (a BattleScene), how many
## have a hero model here and the names of those drawn with a fallback hull anyway. {heroes: int, fallback: PackedStringArray}.
## A build that lost its ship pack has an empty `heroes` dictionary; callers fail on that too.
func hero_report(view: Node, ships: Array) -> Dictionary:
	var n := 0
	var bad := PackedStringArray()
	for sd in ships:
		if not (sd is Dictionary):
			continue
		var want := ""
		for k in BattleShip.asset_keys_for(sd):
			if not hero(k).is_empty():
				want = k
				break
		var node: BattleShip = view.ship_node(int(sd.get("id", 0))) if want != "" else null
		if node == null:
			continue
		n += 1
		if node.hero_key == "":
			bad.append("%s (%s)" % [str(sd.get("name", "?")), want])
	return {"heroes": n, "fallback": bad}


## The hero entry for `key` whose glb exists on this machine, or {}.
func hero(key: String) -> Dictionary:
	var e = heroes.get(key)
	if not (e is Dictionary) or hero_glb_path(e) == "":
		return {}
	return e


func hero_glb_path(entry: Dictionary) -> String:
	var rel := str(entry.get("glb", "")).replace("\\", "/")
	if rel == "":
		return ""
	var p := rel if rel.is_absolute_path() else hero_root.path_join(rel)
	return p if BattleAssets.file_exists(p) else ""


## Canon length in metres of the hero model for the first of `keys` that has one, else 0.
func hero_length(keys: PackedStringArray) -> float:
	for key in keys:
		var e := hero(key)
		if not e.is_empty():
			return float(e.get("lengthMeters", 0.0))
	return 0.0


static func candidate_paths() -> PackedStringArray:
	var out := PackedStringArray()
	var env := OS.get_environment("BOTF_RESOLVED_MANIFEST")
	if env != "":
		out.append(env)
	out.append(PACK_DIR.path_join("resolved-manifest.json"))
	out.append(OS.get_executable_path().get_base_dir().path_join("LocalAssets/resolved-manifest.json"))
	var res := ProjectSettings.globalize_path("res://")
	out.append(res.path_join("../LocalAssets/resolved-manifest.json").simplify_path())
	out.append(res.path_join("../game/LocalAssets/resolved-manifest.json").simplify_path())
	return out


func load_manifest(path: String) -> bool:
	var data = BattleAssets.read_json(path)
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
			if BattleAssets.file_exists(p):
				out.append(p)
				continue
			# Rebase ".../LocalAssets/<rest>" onto this manifest's folder.
			var at := p.findn("LocalAssets/")
			if at >= 0:
				var local := root_dir.path_join(p.substr(at + "LocalAssets/".length()))
				if BattleAssets.file_exists(local):
					out.append(local)
	return out


## A new instance of the first model any of `keys` resolves to (a hero glb first), or null.
func instantiate_ship(keys: PackedStringArray) -> Node3D:
	for key in keys:
		var h := instantiate_hero(key)
		if h != null:
			return h
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


## A new instance of the hero glb for `key` (meta botf_hero / botf_hero_key / botf_hero_root), or null.
## The parsed model is shared by every BattleAssets (a PackedScene per glb path) and loaded once, on a worker thread when
## warm() asked for it earlier, so a battle opening with 24 ships a side does not parse each hull one after another.
func instantiate_hero(key: String) -> Node3D:
	if not _hero_scenes.has(key):
		var e := hero(key)
		# The hero entry goes along so a synchronous load (the web build: no worker threads, warm() starts nothing) also
		# mip-chains the glb's images from the decoded bytes. Without it HeroLook.mipmapped fell back to a GPU readback,
		# which WebGL cannot give, and the hulls drew unmipped: white, speckled (WEB-PARITY.md gap 5, 19 Nov).
		_hero_scenes[key] = _shared_model(hero_glb_path(e), e, hero_root) if not e.is_empty() else null
		if e.is_empty() or _hero_scenes[key] == null:
			if not e.is_empty():
				push_warning("BattleAssets: hero %s glb did not load (%s)" % [key, hero_glb_path(e)])
	var packed: PackedScene = _hero_scenes[key]
	if packed == null:
		return null
	var node: Node3D = packed.instantiate()
	node.set_meta("botf_hero", heroes[key])
	node.set_meta("botf_hero_key", key)
	node.set_meta("botf_hero_root", hero_root)
	return node


## Starts loading the hero models for these asset keys on worker threads (one task per distinct glb; already loaded or
## loading ones are skipped). instantiate_hero waits for a task still running, so calling this early only helps.
## Returns the number of loads started.
func warm(keys: PackedStringArray) -> int:
	var started := 0
	if not AppLoader.threads():
		return 0 # the web build has no worker threads: a glb parses when its battle opens
	for key in keys:
		var e := hero(key)
		if e.is_empty():
			continue
		var path := hero_glb_path(e)
		if _shared.has(path) or _tasks.has(path):
			continue
		_tasks[path] = WorkerThreadPool.add_task(_load_job.bind(path, e, hero_root), false, "botf hero " + key.get_file())
		started += 1
	return started


static var _instance: BattleAssets

## One BattleAssets for callers that only want to warm models early (the galaxy screen warms the player's own classes
## so the first battle opens without parsing them).
static func shared() -> BattleAssets:
	if _instance == null or _instance.heroes.is_empty():
		_instance = BattleAssets.new() # again once a ship pack has been mounted (the web build fetches them after the title)
	return _instance


## Asset keys of every ship in a tactical encounter (the first key of each that has a hero glb is the one drawn).
static func encounter_keys(encounter: Dictionary) -> PackedStringArray:
	var out := PackedStringArray()
	return ships_keys(encounter.get("ships", []))


## Asset keys of ships or fleets (anything with owner and name: a tactical ship, a campaign-view fleet).
static func ships_keys(ships: Array) -> PackedStringArray:
	var out := PackedStringArray()
	for s in ships:
		if s is Dictionary:
			out.append_array(BattleShip.asset_keys_for(s))
	return out


## Blocks until every model load started by warm() has finished. Quitting while a glb is still converting on a worker
## thread hangs the process at shutdown (verify: suite-galaxy-map never exited), so App calls this as the tree closes.
static func wait_for_loads() -> void:
	for path in _tasks.keys():
		_shared_model(path) # waits, then collects the result and forgets the task id (a task id is single use)


## True while any worker-thread model load started by warm() is still running.
static func loading() -> bool:
	for path in _tasks:
		if not WorkerThreadPool.is_task_completed(_tasks[path]):
			return true
	return false


static var _shared := {}   ## glb path -> PackedScene or null (a failed load)
static var _tasks := {}    ## glb path -> WorkerThreadPool task id still to be collected
static var _results := {}  ## glb path -> PackedScene, written by the worker, collected on the main thread
static var _mutex := Mutex.new()


static func _load_job(path: String, hero: Dictionary = {}, root: String = "") -> void:
	var packed := _load_model(path, hero, root)
	_mutex.lock()
	_results[path] = packed
	_mutex.unlock()


static func _shared_model(path: String, hero: Dictionary = {}, root: String = "") -> PackedScene:
	if path == "":
		return null
	if _tasks.has(path):
		WorkerThreadPool.wait_for_task_completion(_tasks[path])
		_tasks.erase(path)
		_mutex.lock()
		_shared[path] = _results.get(path)
		_results.erase(path)
		_mutex.unlock()
	if not _shared.has(path):
		_shared[path] = _load_model(path, hero, root)
	return _shared[path]


static func _load_model(path: String, hero: Dictionary = {}, root: String = "") -> PackedScene:
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
	if not hero.is_empty():
		# Mip chains and plating images are built here, on the worker, not on the first battle frame: HeroLook.apply on
		# the main thread cost 0.2-1.8 s per class (M1-BETAFIX profile) generating them from a GPU readback.
		BattleAssets.mip_scene_images(node, state)
		HeroLook.prime(hero, root)
	var packed := PackedScene.new()
	_own(node, node)
	packed.pack(node)
	node.free()
	return packed


## Decodes the glb's embedded images on this (worker) thread, builds their mip chains, and points every material
## texture of `node` at the mipped copy (tagged "botf_mipped" so HeroLook.mipmapped leaves it alone). Any image it cannot
## decode here keeps the plain texture, which HeroLook.mipmapped still handles on the main thread.
static func mip_scene_images(node: Node, state: GLTFState) -> void:
	var imgs: Array = state.get_images()
	var jimgs: Array = state.json.get("images", []) if state.json is Dictionary else []
	var views: Array = state.get_buffer_views()
	var buffers: Array = state.get_buffers()
	var swap := {} # texture instance id -> mipped texture
	for i in mini(imgs.size(), jimgs.size()):
		var j: Dictionary = jimgs[i]
		var bv := int(j.get("bufferView", -1))
		if imgs[i] == null or bv < 0 or bv >= views.size():
			continue
		var view: GLTFBufferView = views[bv]
		if view.buffer < 0 or view.buffer >= buffers.size():
			continue
		var bytes: PackedByteArray = (buffers[view.buffer] as PackedByteArray).slice(view.byte_offset, view.byte_offset + view.byte_length)
		var img := Image.new()
		var mime := str(j.get("mimeType", "image/png"))
		# The web packs carry WebP (EXT_texture_webp, 17 Nov): decoded as PNG it failed and the hull kept its unmipped
		# texture, which sparkled into speckle at battle range (WEB-PARITY.md, 19 Nov).
		var err: Error
		match mime:
			"image/jpeg":
				err = img.load_jpg_from_buffer(bytes)
			"image/webp":
				err = img.load_webp_from_buffer(bytes)
			_:
				err = img.load_png_from_buffer(bytes)
		if err != OK or img.is_empty() or img.is_compressed():
			continue
		img.generate_mipmaps()
		var tex := ImageTexture.create_from_image(img)
		tex.set_meta("botf_mipped", true)
		swap[(imgs[i] as Object).get_instance_id()] = tex
	if swap.is_empty():
		return
	var meshes: Array = node.find_children("*", "MeshInstance3D", true, false)
	if node is MeshInstance3D:
		meshes.append(node)
	for mi: MeshInstance3D in meshes:
		if mi.mesh == null:
			continue
		for s in mi.mesh.get_surface_count():
			var m := mi.mesh.surface_get_material(s) as BaseMaterial3D
			if m == null:
				continue
			for prop in ["albedo_texture", "normal_texture", "roughness_texture", "metallic_texture", "ao_texture", "emission_texture"]:
				var t: Texture2D = m.get(prop)
				if t != null and swap.has(t.get_instance_id()):
					m.set(prop, swap[t.get_instance_id()])


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
		for f in BattleAssets.list_dir(dir):
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
	if not BattleAssets.file_exists(path):
		return null
	match path.get_extension().to_lower():
		"wav": return AudioStreamWAV.load_from_file(path)
		"ogg": return AudioStreamOggVorbis.load_from_file(path)
	return null
