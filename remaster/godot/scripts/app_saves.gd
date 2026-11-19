class_name AppSaves
extends RefCounted
## Save slots and the settings file. Saves are the core's `save` blob plus a name, a timestamp and the player's
## name, stored as JSON in user://saves (IndexedDB in the web build). All file access goes through AppLoader.
## See docs/CLIENT-ARCHITECTURE.md.

const DIR := "user://saves"
const SETTINGS_PATH := "user://botf_settings.json"

## Writes a slot. Returns "" or the error to show.
static func write(blob: Dictionary, name_: String, player_name: String) -> String:
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
	AppLoader.remove(DIR + "/" + file)

## Merges the stored settings into `settings`.
static func load_settings(settings: Dictionary) -> void:
	var s: Variant = AppLoader.json(SETTINGS_PATH)
	if s is Dictionary:
		settings.merge(s, true)

static func store_settings(settings: Dictionary) -> void:
	AppLoader.write_json(SETTINGS_PATH, settings)
