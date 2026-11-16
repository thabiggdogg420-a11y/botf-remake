class_name AppLoader
extends RefCounted
## The client's single loader service. Every asset, data and save access in the UI layers (scenes/ui, scripts/app*,
## scripts/viewmodels) goes through here; nothing else in those layers touches a file path or FileAccess/DirAccess.
## Paths are `res://` (the exe's pck or the web export's pck) or `user://` (AppData on Windows, IndexedDB on web).
## No absolute disk paths, no environment-variable paths and no threads: every call is synchronous and works the same
## in the web build. Pack paths: a mounted resource pack (ProjectSettings.load_resource_pack) is just more res://
## files, so `mount_pack` is the one place that adds one. See docs/CLIENT-ARCHITECTURE.md, "Browser target".

const ALLOWED_ROOTS := ["res://", "user://"]

## True for a path the loader will serve.
static func allowed(path: String) -> bool:
	for r in ALLOWED_ROOTS:
		if path.begins_with(r):
			return true
	return false

static func _check(path: String) -> bool:
	if allowed(path):
		return true
	push_error("AppLoader: refused non-pack path %s" % path)
	return false

## Mounts an extra resource pack (a web-fetchable asset bundle already in user:// or res://).
static func mount_pack(path: String) -> bool:
	return _check(path) and ProjectSettings.load_resource_pack(path)

static func exists(path: String) -> bool:
	if not _check(path):
		return false
	if path.begins_with("user://"):
		return FileAccess.file_exists(path)
	return ResourceLoader.exists(path) or FileAccess.file_exists(path)

## A resource (scene, script, font, shader, texture), or null.
static func resource(path: String) -> Resource:
	if not _check(path):
		return null
	return load(path)

const ART_EXTS := ["webp", "png", "jpg"]
static var _art_cache := {}

## The texture for an asset-manifest key (docs/UI-DESIGN-SYSTEM.md §7): res://art/<category>/<key>.<webp|png|jpg>
## from the base pack or a mounted asset pack, or null when the art is not there yet (callers fall back).
static func art(key: String, category: String) -> Texture2D:
	if key == "":
		return null
	var ck := category + "/" + key
	if _art_cache.has(ck):
		return _art_cache[ck]
	var tex: Texture2D = null
	for ext in ART_EXTS:
		var p := "res://art/%s/%s.%s" % [category, key, ext]
		if ResourceLoader.exists(p):
			tex = load(p) as Texture2D
			if tex:
				break
		elif FileAccess.file_exists(p):
			# A raw image in an asset pack (art_desktop.pck / art_web.pck, tools/build_ship_packs.py --portraits): no import step.
			var img := Image.load_from_file(p)
			if img != null and not img.is_empty():
				img.generate_mipmaps()
				tex = ImageTexture.create_from_image(img)
				break
	_art_cache[ck] = tex
	return tex

## A res:// or user:// text file ("" when missing). Content packs are read this way and handed to the core (loadContent).
static func text(path: String) -> String:
	if not _check(path) or not FileAccess.file_exists(path):
		return ""
	return FileAccess.get_file_as_string(path)

## True when `dir` exists as a directory in the pack (res://) or user://.
static func dir_exists(dir: String) -> bool:
	if not _check(dir):
		return false
	return DirAccess.dir_exists_absolute(dir)

## True when this build may start worker threads (desktop yes; the web export is not threaded).
static func threads() -> bool:
	return OS.has_feature("threads") and not OS.has_feature("web")

## An Image from a res:// or user:// file (a pack or a loose file), resized to at most `max_width` wide with mipmaps;
## null when missing. Synchronous and thread-safe, so the planet textures can call it on a worker or on the main thread.
static func image(path: String, max_width: int = 2048) -> Image:
	if not _check(path):
		return null
	var img: Image = null
	if path.begins_with("res://") and ResourceLoader.exists(path):
		var t := load(path) as Texture2D
		img = t.get_image() if t else null
		if img != null and img.is_compressed():
			img.decompress()
	elif FileAccess.file_exists(path):
		img = Image.load_from_file(path)
	if img == null or img.is_empty():
		return null
	if img.get_width() > max_width:
		img.resize(max_width, max_width * img.get_height() / img.get_width(), Image.INTERPOLATE_LANCZOS)
	if not img.has_mipmaps():
		img.generate_mipmaps()
	return img

## Mounts the desktop asset bundles, `user://packs/*_desktop.pck` (planets_desktop.pck from tools/pack_planets.gd,
## ships_desktop.pck from tools/build_ship_packs.py). Returns the count. The web build mounts nothing here: its packs are
## fetched (or found in the IndexedDB cache) by fetch_web_pack after the title is up.
static func mount_user_packs() -> int:
	var n := 0
	if OS.has_feature("web"):
		return 0
	for f in list("user://packs"):
		if f.ends_with("_desktop.pck") and mount_pack("user://packs/" + f):
			n += 1
	# Packs shipped beside the exe (build\packs\*_desktop.pck from the verify/package step). The loader is the one place a
	# disk path is allowed, and only for this folder; an editor or test run finds no such folder and mounts nothing.
	var beside := OS.get_executable_path().get_base_dir().path_join("packs")
	if not OS.has_feature("editor") and DirAccess.dir_exists_absolute(beside):
		for f in DirAccess.get_files_at(beside):
			if f.ends_with("_desktop.pck") and ProjectSettings.load_resource_pack(beside.path_join(f)):
				n += 1
	return n

## Web bundles served beside index.html in packs/. Logical names; tools/verify/hash_web_packs.py renames each file to
## <stem>.<content hash>.pck and lists the mapping in packs/manifest.json, so a file name never changes meaning: the
## server lets the browser keep hashed files forever, and a pack already in user://packs (IndexedDB) is mounted with no
## download at all on a return visit.
const WEB_PACKS := ["planets_web.pck", "art_web.pck"]
## The heavy web bundles, fetched one by one in the background after the title shows (App._fetch_background):
## each faction's hero hulls (tools/build_ship_packs.py) and the full-resolution planet tier (tools/build_planet_hi_web.py).
const _OLD_WEB_SHIP_FACTIONS := ["fed", "kli", "rom", "car", "fer", "dom", "borg", "breen"] # M4: the Dominion, Borg and Breen packs follow the five (fetched last, in the background)
const WEB_PLANETS_HI := "planets_hi_web.pck"
## The shared music beds (about 9 MB of ogg). Not in the main pck: the first playable load is the engine and the game alone, and
## the title plays a generated bed until this arrives (BotfAudioSystem.music_pack_ready swaps to the real tracks).
const WEB_MUSIC := "audio_music_web.pck"
## The baked menu ship pictures (about 12 MB of PNG), fetched after the title; ShipIcons finds them once mounted.
const WEB_SHIP_ICONS := "ship_icons_web.pck"

static var _web_manifest: Variant = null
static var _mounted := {}   ## logical name -> true once mounted this session
## Bytes fetched over the network and served from the IndexedDB cache this session (web only; for the verify report).
static var bytes_downloaded := 0
static var bytes_cached := 0

## Diagnostics switch: BOTF_DIAG in the environment, or ?diag in the page address (web). Prints "DIAG ..." lines, nothing else changes.
static var _diag := -1
static func diag() -> bool:
	if _diag < 0:
		var on := OS.get_environment("BOTF_DIAG") != ""
		if not on and OS.has_feature("web"):
			on = str(JavaScriptBridge.eval("location.search")).contains("diag")
		_diag = 1 if on else 0
	return _diag == 1

# ---- Preload the whole game (F1, Simon 16 Nov: a button to preload the whole game) ----------------------------------
## Every pack in packs/manifest.json, in the order a player needs them: manifests, portraits, planets, music, ship pictures,
## voices, the full planet tier, then every hull. Each pack lands in user:// (IndexedDB), so it survives the tab and the visit.
static var preload_running := false
static var preload_cancel := false
static var preload_error := ""
static var preload_current := ""

static func _pack_rank(n: String) -> int:
	if n == WEB_SHIPS_BASE:
		return 0
	if n.begins_with("art_"):
		return 1
	if n.begins_with("planets_web"):
		return 2
	if n == WEB_MUSIC:
		return 3
	if n == WEB_SHIP_ICONS:
		return 4
	if n.begins_with("audio_"):
		return 5
	if n.begins_with("planets_hi"):
		return 6
	return 7

## Logical names of every pack in the build, in preload order ([] until the manifest is loaded).
static func all_packs() -> Array:
	var names: Array = []
	var man: Variant = _web_manifest
	if man is Dictionary:
		for k in man.get("files", {}):
			names.append(str(k))
	names.sort_custom(func(a, b): return _pack_rank(a) < _pack_rank(b) or (_pack_rank(a) == _pack_rank(b) and a < b))
	return names

static func total_bytes() -> int:
	var t := 0
	for n in all_packs():
		t += pack_bytes(n)
	return t

## True when this exact pack content is already on the device (mounted this session or complete in user://packs).
static func is_cached(name: String) -> bool:
	if _mounted.has(name):
		return true
	var man: Variant = _web_manifest
	if not (man is Dictionary):
		return false
	var file := str(man.get("files", {}).get(name, ""))
	if file == "" or not FileAccess.file_exists("user://packs/" + file):
		return false
	var f := FileAccess.open("user://packs/" + file, FileAccess.READ)
	return f != null and f.get_length() == int(man.get("bytes", {}).get(file, -1))

static func cached_bytes() -> int:
	var t := 0
	for n in all_packs():
		if is_cached(n):
			t += pack_bytes(n)
	return t

static func fully_cached() -> bool:
	var names := all_packs()
	if names.is_empty():
		return false
	for n in names:
		if not is_cached(n):
			return false
	return true

## Downloads (or finds cached) and mounts every pack. Resumes where it stopped: finished packs are not fetched again.
## preload_cancel = true stops after the pack in flight. A failed pack sets preload_error and stops.
static func preload_all(host: Node) -> void:
	if preload_running or not OS.has_feature("web"):
		return
	preload_running = true
	preload_cancel = false
	preload_error = ""
	await _manifest(host)
	JavaScriptBridge.eval("(navigator.storage && navigator.storage.persist) ? navigator.storage.persist() : false")   # ask the browser not to evict it
	for n in all_packs():
		if preload_cancel:
			break
		preload_current = n
		if not await fetch_web_pack(host, n):
			preload_error = n
			break
	preload_current = ""
	preload_running = false

## Progress of the pack being fetched right now (web): {"name", "bytes", "total"}; empty when idle. The loading overlay reads it.
static var progress := {}

## The manifests every hull needs (godot-hero-manifest.json, resolved-manifest.json). Mount it before any hull pack.
const WEB_SHIPS_BASE := "ships_base_web.pck"

## The logical pack holding the hero model of asset key `key` ("ship.fed.galaxy"): hull_ship_fed_galaxy_web.pck, one pack per
## hull so a battle fetches only the classes on screen. "" when the build has none (a procedural hull is drawn).
static func hull_pack_for(key: String) -> String:
	var name := "hull_%s_web.pck" % key.replace(".", "_")
	var man: Variant = _web_manifest
	if man is Dictionary and man.get("files", {}).has(name):
		return name
	return ""

## Size in bytes of a logical pack from packs/manifest.json (0 when unknown).
static func pack_bytes(name: String) -> int:
	var man: Variant = _web_manifest
	if not (man is Dictionary):
		return 0
	var f: String = str(man.get("files", {}).get(name, ""))
	return int(man.get("bytes", {}).get(f, 0))

## Web: loads packs/manifest.json now (no pack fetched). Idempotent.
static func load_manifest(host: Node) -> Dictionary:
	return await _manifest(host)

## True once the logical pack `name` is mounted in this session.
static func web_pack_mounted(name: String) -> bool:
	return _mounted.has(name)

## packs/manifest.json: {"files": {logical name: hashed name}, "bytes": {hashed name: size}}. Fetched once per session,
## never cached (the server marks it no-cache). {} when the build has none (unhashed packs, older exports).
static func _manifest(host: Node) -> Dictionary:
	while _manifest_loading:   # another fetch is already reading it: wait, never answer with the empty placeholder
		await host.get_tree().process_frame
	if _web_manifest != null:
		return _web_manifest
	_manifest_loading = true
	_web_manifest = {}
	var req := HTTPRequest.new()
	req.accept_gzip = false   # the browser already decodes a compressed body; Godot must not try to inflate it again
	host.add_child(req)
	var url := str(JavaScriptBridge.eval("new URL('packs/manifest.json', location.href).href"))
	if req.request(url, PackedStringArray(["Cache-Control: no-cache"])) == OK:
		var res: Array = await req.request_completed
		if int(res[0]) == HTTPRequest.RESULT_SUCCESS and int(res[1]) == 200:
			var d = JSON.parse_string((res[3] as PackedByteArray).get_string_from_utf8())
			if d is Dictionary:
				_web_manifest = d
				write_json("user://packs/manifest.cache.json", d)   # a preloaded game starts with no network at all
	if _web_manifest.is_empty():
		var cached: Variant = json("user://packs/manifest.cache.json")
		if cached is Dictionary:
			_web_manifest = cached
	req.queue_free()
	_manifest_loading = false
	return _web_manifest

static var _manifest_loading := false
static var _inflight := {}   ## logical name -> true while one coroutine is fetching it

## Web only: mounts the bundle `name` (a logical name such as "audio_kli_web.pck"), from user://packs when this exact
## content is already there, else downloaded first. Older copies of the same bundle are removed from user://packs.
## Safe to call from several places at once: a second call for the same pack waits for the first.
static func fetch_web_pack(host: Node, name: String) -> bool:
	if not OS.has_feature("web"):
		return false
	while _inflight.has(name):
		await host.get_tree().process_frame
	if _mounted.has(name):
		return true
	_inflight[name] = true
	var ok: bool = await _fetch_web_pack(host, name)
	_inflight.erase(name)
	return ok

static func _fetch_web_pack(host: Node, name: String) -> bool:
	DirAccess.make_dir_recursive_absolute("user://packs")
	var man: Dictionary = await _manifest(host)
	var file: String = str(man.get("files", {}).get(name, name))
	var want := int(man.get("bytes", {}).get(file, -1))
	var dest := "user://packs/" + file
	var ok := false
	# A cached copy counts when its size matches the manifest (a hashed name already pins the content; the size check
	# catches a download cut short). Unhashed builds (no manifest) always download.
	var cached := -1
	if file != name and FileAccess.file_exists(dest):
		var have := FileAccess.open(dest, FileAccess.READ)
		if have != null:
			cached = have.get_length()
	if cached > 0 and (want < 0 or cached == want):
		bytes_cached += cached
		ok = mount_pack(dest)
	else:
		var url := str(JavaScriptBridge.eval("new URL('packs/%s', location.href).href" % file))
		var req := HTTPRequest.new()
		req.download_file = dest
		req.accept_gzip = false   # a host that compresses .pck must not break the download
		host.add_child(req)
		var ticker := Timer.new()   # polls the download so a loading overlay can show bytes and a percentage
		ticker.wait_time = 0.25
		ticker.timeout.connect(func(): progress = {"name": name, "bytes": req.get_downloaded_bytes(), "total": maxi(want, req.get_body_size())})
		host.add_child(ticker)
		ticker.start()
		progress = {"name": name, "bytes": 0, "total": want}
		if req.request(url) == OK:
			var res: Array = await req.request_completed
			ticker.queue_free()
			progress = {}
			ok = int(res[0]) == HTTPRequest.RESULT_SUCCESS and int(res[1]) == 200 and mount_pack(dest)
			if ok:
				bytes_downloaded += req.get_downloaded_bytes()
			else:
				push_warning("AppLoader: web pack %s not mounted (result %d, http %d)" % [file, int(res[0]), int(res[1])])
		else:
			ticker.queue_free()
			progress = {}
		req.queue_free()
	if ok:
		_mounted[name] = true
		var stem := name.get_basename() + "."
		for f in list("user://packs"):
			if f != file and (f == name or f.begins_with(stem)):
				remove("user://packs/" + f)
	return ok

## Web only: the chosen empire's audio (voice, sfx, music, about 12 MB) on top of the `shared` audio that ships in the main pck.
## Call it once when a campaign picks its empire (fed kli rom car fer), then BotfAudioLibrary.load_pack(empire).
static func fetch_empire_audio(host: Node, empire: String) -> bool:
	return await fetch_web_pack(host, "audio_%s_web.pck" % empire)

## Web only: fetches and mounts each WEB_PACKS bundle (the small ones the galaxy and system screens want first). Await it
## from a Node. Returns the count mounted; a missing file is skipped (planets then stay procedural).
static func fetch_web_packs(host: Node) -> int:
	if not OS.has_feature("web"):
		return 0
	var n := 0
	for name: String in WEB_PACKS:
		if await fetch_web_pack(host, name):
			n += 1
	return n

## Parsed JSON from a res:// or user:// file, or null when missing or invalid.
static func json(path: String) -> Variant:
	if not _check(path) or not FileAccess.file_exists(path):
		return null
	return JSON.parse_string(FileAccess.get_file_as_string(path))

## Writes JSON to user://. Returns "" or an error.
static func write_json(path: String, data: Variant) -> String:
	if not path.begins_with("user://"):
		return "Only user:// is writable"
	DirAccess.make_dir_recursive_absolute(path.get_base_dir())
	var f := FileAccess.open(path, FileAccess.WRITE)
	if f == null:
		return "Could not write %s" % path.get_file()
	f.store_string(JSON.stringify(data))
	f.close()
	return ""

## Writes exact text bytes to user:// (no re-serialising: a user pack's checksum is over these bytes). Returns "" or an error.
static func write_text(path: String, text: String) -> String:
	if not path.begins_with("user://"):
		return "Only user:// is writable"
	DirAccess.make_dir_recursive_absolute(path.get_base_dir())
	var f := FileAccess.open(path, FileAccess.WRITE)
	if f == null:
		return "Could not write %s" % path.get_file()
	f.store_string(text)
	f.close()
	return ""

## File names in a res:// or user:// directory ([] when it does not exist).
static func list(dir: String) -> PackedStringArray:
	if not _check(dir):
		return PackedStringArray()
	var d := DirAccess.open(dir)
	return d.get_files() if d else PackedStringArray()

static func remove(path: String) -> void:
	if path.begins_with("user://"):
		DirAccess.remove_absolute(path)
