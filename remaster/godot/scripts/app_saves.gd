class_name AppSaves
extends RefCounted
## Save slots and the settings file. Saves are the core's `save` blob plus a name, a timestamp and the player's
## name, stored as JSON in user://saves (IndexedDB in the web build). All file access goes through AppLoader.
## See docs/CLIENT-ARCHITECTURE.md.

## The live paths. Capture tools and tests point both at a scratch folder before the App starts (sandbox()), so a run never
## writes the player's saves or settings (F5 round 2: the MOD-9 capture wrote autosaves into the real user://saves; F6 round:
## test_campaign_ui's end-turn autosaves overwrote Simon's Autosave 2 and 3). Any run whose main loop is a script (every
## `-s` test and tool) is sandboxed automatically on first use (ensure()), and AppLoader refuses to write the real paths then.
static var DIR := "user://saves"
static var SETTINGS_PATH := "user://botf_settings.json"
const REAL_DIR := "user://saves"
const REAL_SETTINGS := "user://botf_settings.json"
const AUTO_SANDBOX := "user://capture_sandbox/auto"
static var _paths_set := false

## Points saves, settings and user packs at `root` (a user:// scratch folder). For capture tools and tests only.
static func sandbox(root: String) -> void:
	DIR = root.path_join("saves")
	SETTINGS_PATH = root.path_join("botf_settings.json")
	UserPacks.dir = root.path_join("mods")
	_paths_set = true

## True when this process is a scripted run (godot -s <tool or test>): the game itself runs a plain SceneTree.
static func scripted_run() -> bool:
	var ml := Engine.get_main_loop()
	return ml != null and ml.get_script() != null

## Settles the live paths once: a scripted run with no sandbox of its own gets a fresh one under AUTO_SANDBOX.
static func ensure() -> void:
	if _paths_set:
		return
	_paths_set = true
	if scripted_run():
		sandbox(AUTO_SANDBOX.path_join("%d-%d" % [int(Time.get_unix_time_from_system()), OS.get_process_id()]))

## True when `path` is the player's real saves folder or settings file (AppLoader refuses these in a scripted run).
static func is_real_path(path: String) -> bool:
	return path == REAL_SETTINGS or path == REAL_DIR or path.begins_with(REAL_DIR + "/")

## Writes a slot. Returns "" or the error to show.
static func write(blob: Dictionary, name_: String, player_name: String) -> String:
	ensure()
	blob["name"] = name_
	blob["when"] = Time.get_datetime_string_from_system()
	blob["playerName"] = player_name
	var err := AppLoader.write_json("%s/%s.json" % [DIR, name_.validate_filename()], blob)
	return "Could not write the save file" if err != "" else ""

const AUTOSAVE_SLOTS := 3
const THUMB_SIZE := Vector2i(320, 180)

## The rolling autosave slot for a turn: Autosave 1..AUTOSAVE_SLOTS in turn, so the newest three turns are kept.
static func autosave_name(turn: int) -> String:
	return "Autosave %d" % (posmod(turn, AUTOSAVE_SLOTS) + 1)

## A small JPEG of the screen at the save, base64 for the JSON slot ("" when it cannot be read: the web build, where a
## viewport read-back stalls or crashes some adapters, and headless runs).
static func thumbnail(vp: Viewport) -> String:
	if vp == null or OS.has_feature("web") or DisplayServer.get_name() == "headless":
		return ""
	var img := vp.get_texture().get_image()
	if img == null or img.is_empty():
		return ""
	img.resize(THUMB_SIZE.x, THUMB_SIZE.y, Image.INTERPOLATE_BILINEAR)
	return Marshalls.raw_to_base64(img.save_jpg_to_buffer(0.8))

## The slot's thumbnail as a texture, or null.
static func thumb_texture(s: Dictionary) -> Texture2D:
	var b64 := str(s.get("thumb", ""))
	if b64 == "":
		return null
	var img := Image.new()
	if img.load_jpg_from_buffer(Marshalls.base64_to_raw(b64)) != OK:
		return null
	return ImageTexture.create_from_image(img)

## Every slot, newest first; each blob carries its "file".
static func list() -> Array:
	ensure()
	var out := []
	for fn in AppLoader.list(DIR):
		if fn.ends_with(".json"):
			var data: Variant = AppLoader.json(DIR + "/" + fn)
			if data is Dictionary:
				data["file"] = fn
				out.append(data)
	out.sort_custom(func(a, b): return str(a.get("when", "")) > str(b.get("when", "")))
	return out

static func delete(file: String) -> void:
	ensure()
	AppLoader.remove(DIR + "/" + file)

## Merges the stored settings into `settings`.
static func load_settings(settings: Dictionary) -> void:
	ensure()
	var s: Variant = AppLoader.json(SETTINGS_PATH)
	if s is Dictionary:
		settings.merge(s, true)

static func store_settings(settings: Dictionary) -> void:
	ensure()
	AppLoader.write_json(SETTINGS_PATH, settings)
