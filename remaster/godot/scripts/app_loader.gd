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

## Mounts the desktop asset bundles, `user://packs/*_desktop.pck` (built by tools/pack_planets.gd). Returns the count.
## The web build mounts nothing here: its packs come from fetch_web_packs and are the `*_web.pck` files only.
static func mount_user_packs() -> int:
	var n := 0
	if OS.has_feature("web"):
		for f in list("user://packs"):
			if f.ends_with("_web.pck") and mount_pack("user://packs/" + f):
				n += 1
		return n
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

## Web bundles served beside index.html in packs/ (built by pack_planets.gd with `web`; budget 40 MB all told).
const WEB_PACKS := ["planets_web.pck"]

## Web only: downloads and mounts one extra bundle by name, e.g. "audio_kli_web.pck" (see fetch_empire_audio). True when mounted.
static func fetch_web_pack(host: Node, name: String) -> bool:
	if not OS.has_feature("web"):
		return false
	DirAccess.make_dir_recursive_absolute("user://packs")
	var dest := "user://packs/" + name
	var url := str(JavaScriptBridge.eval("new URL('packs/%s', location.href).href" % name))
	var req := HTTPRequest.new()
	req.download_file = dest
	host.add_child(req)
	var ok := false
	if req.request(url) == OK:
		var res: Array = await req.request_completed
		ok = int(res[0]) == HTTPRequest.RESULT_SUCCESS and int(res[1]) == 200 and mount_pack(dest)
	req.queue_free()
	return ok

## Web only: the chosen empire's audio (voice, sfx, music, about 12 MB) on top of the `shared` audio that ships in the main pck.
## Call it once when a campaign picks its empire (fed kli rom car fer), then BotfAudioLibrary.load_pack(empire).
static func fetch_empire_audio(host: Node, empire: String) -> bool:
	return await fetch_web_pack(host, "audio_%s_web.pck" % empire)

## Web only: downloads each WEB_PACKS file next to the page into user://packs (IndexedDB) and mounts it. Await it from a Node.
## Returns the count mounted; a missing file is skipped (planets then stay procedural).
static func fetch_web_packs(host: Node) -> int:
	if not OS.has_feature("web"):
		return 0
	var n := 0
	DirAccess.make_dir_recursive_absolute("user://packs")
	for name: String in WEB_PACKS:
		var dest := "user://packs/" + name
		var url := str(JavaScriptBridge.eval("new URL('packs/%s', location.href).href" % name))
		var req := HTTPRequest.new()
		req.download_file = dest
		host.add_child(req)
		if req.request(url) == OK:
			var res: Array = await req.request_completed
			if int(res[0]) == HTTPRequest.RESULT_SUCCESS and int(res[1]) == 200 and mount_pack(dest):
				n += 1
			else:
				push_warning("AppLoader: web pack %s not mounted (result %d, http %d)" % [name, int(res[0]), int(res[1])])
		req.queue_free()
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

## File names in a res:// or user:// directory ([] when it does not exist).
static func list(dir: String) -> PackedStringArray:
	if not _check(dir):
		return PackedStringArray()
	var d := DirAccess.open(dir)
	return d.get_files() if d else PackedStringArray()

static func remove(path: String) -> void:
	if path.begins_with("user://"):
		DirAccess.remove_absolute(path)
