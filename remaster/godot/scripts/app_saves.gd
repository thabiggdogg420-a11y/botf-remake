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
