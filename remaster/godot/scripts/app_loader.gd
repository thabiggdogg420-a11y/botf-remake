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
	var t := LoadTrace.start()
	var ok := _check(path) and ProjectSettings.load_resource_pack(path)
	t = LoadTrace.done("pack mount %s" % path.get_file(), t)
	if ok:
		art_events.packs_mounted.emit(path)
		LoadTrace.done("pack mounted signal %s" % path.get_file(), t)
	return ok

## Art widgets (PortraitSlot, PlanetOrb, ChoiceIcon) connect to `AppLoader.art_events.packs_mounted` and re-request their
## picture when a pack lands: the web build mounts art_web.pck after the first screens are drawn (cold first visit).
class ArtEvents extends Object:
	signal packs_mounted(path: String)
static var art_events := ArtEvents.new()

static func exists(path: String) -> bool:
	if not _check(path):
		return false
	if path.begins_with("user://"):
		return FileAccess.file_exists(path)
	return ResourceLoader.exists(path) or FileAccess.file_exists(path)

## F5: loads a res:// resource on a worker thread (desktop warm-up). Returns false when it cannot start. Poll with
## threaded_status (0 loading, 1 loaded, -1 failed) and take the result with threaded_take.
static func threaded_start(path: String) -> bool:
	return _check(path) and path.begins_with("res://") and threads() and ResourceLoader.load_threaded_request(path, "", false) == OK

static func threaded_status(path: String) -> int:
	match ResourceLoader.load_threaded_get_status(path):
		ResourceLoader.THREAD_LOAD_IN_PROGRESS: return 0
		ResourceLoader.THREAD_LOAD_LOADED: return 1
	return -1

static func threaded_take(path: String) -> Resource:
	return ResourceLoader.load_threaded_get(path) if threaded_status(path) == 1 else null

static func cached(path: String) -> bool:
	return ResourceLoader.has_cached(path)

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
	if tex != null:   # a missing picture is never cached: the pack that holds it may mount later
		_art_cache[ck] = tex
	return tex

## F5 performance pass: decodes pack art (raw images in art_desktop.pck) on a worker thread and fills the art cache, so the
## first screen that shows them does not decode on the main thread (the New campaign screen's eight portraits cost about
## 130 ms there). Imported textures and pictures already cached are skipped. Desktop only (needs threads); awaitable.
static func warm_art(tree: SceneTree, keys: Array, category: String) -> int:
	if not threads():
		return 0
	var n := 0
	for key in keys:
		var ck := category + "/" + str(key)
		if _art_cache.has(ck):
			continue
		var path := ""
		for ext in ART_EXTS:
			var p := "res://art/%s/%s.%s" % [category, key, ext]
			if ResourceLoader.exists(p):
				path = ""
				break
			if FileAccess.file_exists(p):
				path = p
				break
		if path == "":
			continue
		var holder := [null]
		var task := WorkerThreadPool.add_task(func():
			var img := Image.load_from_file(path)
			if img != null and not img.is_empty():
				img.generate_mipmaps()
				holder[0] = img, false, "botf art warm")
		while not WorkerThreadPool.is_task_completed(task):
			await tree.process_frame
		WorkerThreadPool.wait_for_task_completion(task)
		if holder[0] != null and not _art_cache.has(ck):
			_art_cache[ck] = ImageTexture.create_from_image(holder[0])
			n += 1
	return n

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

## The logical pack that carries the character portraits (tools/build_ship_packs.py --portraits).
const PORTRAIT_PACK := "art_web.pck"

## True on the web while the portrait pack has not mounted yet this session (a cold first visit): a hail screen that finds no
## portrait then waits for it instead of drawing the bust. Always false off the web (desktop packs mount at start-up).
static func portrait_pack_pending() -> bool:
	return test_pack_pending or (OS.has_feature("web") and not _mounted.has(PORTRAIT_PACK))
## Headless tests only: pretend the portrait pack is still downloading (and fetch nothing).
static var test_pack_pending := false

## Web: fetches pack `name` now, ahead of the background queue (a second call for a pack already in flight just waits for
## it). Run on the tree root so it survives the screen that asked; `on_done` is called once the fetch has finished, loaded or failed.
static func raise_pack(name: String, on_done: Callable = Callable()) -> void:
	var loop := Engine.get_main_loop()
	if test_pack_pending or not (loop is SceneTree):
		return
	_raise_pack(loop.root, name, on_done)

static func _raise_pack(host: Node, name: String, on_done: Callable) -> void:
	await fetch_web_pack(host, name)
	if on_done.is_valid():
		on_done.call()

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
	if _unsynced > 0:
		await _flush_sync(host)

## Idle prefetch (F1 audit, 17 Nov): the packs to fetch quietly while the player sits on the title or the galaxy, in the order they
## will be wanted. Pure over the manifest and the mounted set, so a headless test can drive it. `player_tok` is the empire token
## of the running campaign ("fed", "kli", ...) or "" on the title; `others` are the rival empires' tokens. Packs already mounted
## this session are left out. The shared hull manifests and the big planet tier come first (every campaign needs them), then the
## player's empire audio and every hull of the player's empire, then the rivals' hulls. On the title (`player_tok` empty) only the
## shared packs are queued. Everything else is the preload button's.
static func idle_queue(player_tok: String, others: Array) -> Array:
	var out: Array = []
	var have := {}
	for n in all_packs():
		have[n] = true
	var add_pack := func(n: String) -> void:
		if have.has(n) and not _mounted.has(n) and not out.has(n):
			out.append(n)
	add_pack.call(WEB_SHIPS_BASE)
	add_pack.call(WEB_PLANETS_HI)
	if player_tok == "":
		return out   # the title: nobody has chosen an empire yet, so only what every campaign needs
	add_pack.call("audio_%s_web.pck" % player_tok)
	var toks: Array = [player_tok]
	for t in others:
		if not toks.has(str(t)):
			toks.append(str(t))
	for t in toks:
		var prefix := "hull_ship_%s_" % t
		for n in all_packs():
			if n.begins_with(prefix):
				add_pack.call(n)
	return out

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
	var url := str(JavaScriptBridge.eval("new URL((globalThis.BOTF_PACKS || 'packs/') + 'manifest.json', location.href).href"))
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
		var url := str(JavaScriptBridge.eval("new URL((globalThis.BOTF_PACKS || 'packs/') + '%s', location.href).href" % file))
		ok = await _download_pack(host, url, dest, name, want)
		if ok:
			ok = mount_pack(dest)
			if ok:
				bytes_downloaded += maxi(want, 0)
			else:
				push_warning("AppLoader: web pack %s downloaded but not mounted" % file)
	if ok:
		_mounted[name] = true
		var stem := name.get_basename() + "."
		for f in list("user://packs"):
			if f != file and (f == name or f.begins_with(stem)):
				remove("user://packs/" + f)
	return ok

## Downloads one pack with the browser's own fetch() and writes it into the game's file system (engine.copyToFS), instead of
## Godot's HTTPRequest. fetch() undoes any Content-Encoding a host or CDN adds (gzip, brotli) before the bytes reach us, which
## HTTPRequest.download_file cannot do on the web (it hangs on a compressed body: seen with a host that gzips everything), and
## it reports progress for the loading bar. Returns true when `want` bytes (the manifest's size) are in `dest`.
static func _download_pack(host: Node, url: String, dest: String, name: String, want: int) -> bool:
	var id := "d%d" % Time.get_ticks_usec()
	var path := ProjectSettings.globalize_path(dest)
	var js := "(function(){var s={done:false,ok:false,bytes:0,err:''};(window.__botfDl=window.__botfDl||{})['" + id + "']=s;" \
		+ "fetch(" + JSON.stringify(url) + ").then(function(r){if(!r.ok)throw new Error('http '+r.status);var rd=r.body.getReader();var ch=[];" \
		+ "var pump=function(){return rd.read().then(function(x){if(x.done)return;ch.push(x.value);s.bytes+=x.value.length;return pump();});};" \
		+ "return pump().then(function(){var b=new Uint8Array(s.bytes);var o=0;for(var i=0;i<ch.length;i++){b.set(ch[i],o);o+=ch[i].length;}ch=null;return b;});})" \
		+ ".then(function(b){engine.copyToFS(" + JSON.stringify(path) + ",b.buffer);s.ok=true;s.done=true;})" \
		+ ".catch(function(e){s.err=String(e);s.done=true;});})();"
	progress = {"name": name, "bytes": 0, "total": want}
	LoadTrace.mark("download %s (%d bytes)" % [name, want])
	JavaScriptBridge.eval(js)
	var tree := host.get_tree()
	var st: Dictionary = {}
	while true:
		await tree.create_timer(0.25).timeout
		var parsed: Variant = JSON.parse_string(str(JavaScriptBridge.eval("JSON.stringify(window.__botfDl['" + id + "'])")))
		st = parsed if parsed is Dictionary else {}
		progress = {"name": name, "bytes": int(st.get("bytes", 0)), "total": want}
		if bool(st.get("done", true)):
			break
	JavaScriptBridge.eval("delete window.__botfDl['" + id + "']")
	progress = {}
	if not bool(st.get("ok", false)):
		push_warning("AppLoader: download of %s failed: %s" % [name, str(st.get("err", "no state"))])
		return false
	var f := FileAccess.open(dest, FileAccess.READ)
	var have := f.get_length() if f != null else -1
	f = null
	if want > 0 and have != want:
		push_warning("AppLoader: %s is %d bytes, the manifest says %d" % [name, have, want])
		remove(dest)
		return false
	# Writing through the engine's own file API is what makes it save the new file to IndexedDB (copyToFS alone does not).
	# A preload writes hundreds of packs: it flushes once at the end and every 48 MB (_flush_sync), not after each one.
	_unsynced += maxi(have, 0)
	if not (preload_running or batch_sync) or _unsynced >= 48 * 1048576:
		await _flush_sync(host)
	return true

static var _unsynced := 0
## True while a long background run (the idle prefetch) fetches packs: it flushes to IndexedDB every 48 MB and at its end, like the preload.
static var batch_sync := false

## Saves what has been fetched but not yet flushed (end of a background run).
static func flush_pending(host: Node) -> void:
	if _unsynced > 0:
		await _flush_sync(host)

## Saves the in-memory file system to IndexedDB and waits until it is done (what makes the packs survive the visit). Godot's own
## save-on-close skips a request that arrives while an earlier save is still running, so a long preload lost its last packs; this
## calls the file system's syncfs directly and waits for its callback. Falls back to the open/close nudge when it is not reachable.
static func _flush_sync(host: Node) -> void:
	_unsynced = 0
	LoadTrace.mark("flush to IndexedDB")
	var id := "s%d" % Time.get_ticks_usec()
	var ok := bool(JavaScriptBridge.eval("(function(){try{var fs=engine.rtenv.FS;if(!fs||!fs.syncfs)return false;(window.__botfSync=window.__botfSync||{})['" + id + "']=false;fs.syncfs(false,function(e){window.__botfSync['" + id + "']=e?String(e):true;});return true;}catch(e){return false;}})()"))
	if not ok:
		var touch := FileAccess.open("user://packs/manifest.cache.json", FileAccess.READ_WRITE)
		if touch != null:
			touch.close()
		return
	var tree := host.get_tree()
	for i in 1200:   # up to 5 minutes
		await tree.create_timer(0.25).timeout
		var st := str(JavaScriptBridge.eval("String(window.__botfSync['" + id + "'])"))
		if st != "false":
			if st != "true":
				push_warning("AppLoader: saving packs to the browser store failed: " + st)
			break
	JavaScriptBridge.eval("delete window.__botfSync['" + id + "']")

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
	if AppSaves.is_real_path(path) and AppSaves.scripted_run():
		push_error("AppLoader: a test or tool tried to write the player's %s (sandbox it with AppSaves)" % path)
		return "Refused: the player's saves and settings are off limits to tests and tools"
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
	if AppSaves.is_real_path(path) and AppSaves.scripted_run():
		push_error("AppLoader: a test or tool tried to write the player's %s (sandbox it with AppSaves)" % path)
		return "Refused: the player's saves and settings are off limits to tests and tools"
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
	if AppSaves.is_real_path(path) and AppSaves.scripted_run():
		push_error("AppLoader: a test or tool tried to delete the player's %s" % path)
		return
	if path.begins_with("user://"):
		DirAccess.remove_absolute(path)
