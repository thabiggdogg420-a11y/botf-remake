class_name UserPacks
extends RefCounted
## The player's own packs (lane MOD-10, docs/design/MOD-TOOLS.md). A user pack is two files in the user data folder, laid out
## like the shipped data/overlays and data/presets: `<id>.overlay.json` (data patches) and `<id>.preset.json` (rules, lineup,
## new-game defaults). The overlay's exact bytes are what the core checksums, so files are written and read as plain text and
## never re-serialised. Shipped packs live in the core and are never written here. All file access goes through AppLoader.

static var dir := "user://mods"   ## Tests point this at a scratch folder.

static func overlay_path(id: String) -> String:
	return "%s/%s.overlay.json" % [dir, id]

static func preset_path(id: String) -> String:
	return "%s/%s.preset.json" % [dir, id]

## Pack ids that have an overlay file, sorted.
static func ids() -> Array:
	var out := []
	for fn in AppLoader.list(dir):
		if fn.ends_with(".overlay.json"):
			out.append(fn.trim_suffix(".overlay.json"))
	out.sort()
	return out

## {"overlay": text, "preset": text} for a pack; a missing file reads as "".
static func read(id: String) -> Dictionary:
	return {"overlay": AppLoader.text(overlay_path(id)), "preset": AppLoader.text(preset_path(id))}

## Writes both files. Returns "" or the error to show.
static func write(id: String, overlay_text: String, preset_text: String) -> String:
	var err := AppLoader.write_text(overlay_path(id), overlay_text)
	if err == "":
		err = AppLoader.write_text(preset_path(id), preset_text)
	return err

static func remove(id: String) -> void:
	AppLoader.remove(overlay_path(id))
	AppLoader.remove(preset_path(id))

## True when this core can take user packs (the native or wasm core of this build; the recorded-fixture backend and an older
## wasm do not know the calls). The title menu hides the pack editor when this is false.
static func supported(core: CoreClient) -> bool:
	return core != null and core.is_native() and core.invoke("userPacks").get("ok", false)

## Registers every pack in the folder with the core. A pack may build on another user pack, so the folder is retried until a pass
## adds nothing. Returns {"loaded": [ids], "errors": {id: core message}}.
static func register_all(core: CoreClient) -> Dictionary:
	var loaded := []
	var errors := {}
	if not supported(core):
		return {"loaded": loaded, "errors": errors}
	var pending := ids()
	var progress := true
	while progress and not pending.is_empty():
		progress = false
		for id in pending.duplicate():
			var files := read(id)
			if str(files["overlay"]) == "" or str(files["preset"]) == "":
				errors[id] = "the %s file is missing" % ("overlay" if str(files["overlay"]) == "" else "preset")
				pending.erase(id)
				continue
			var r := core.invoke("installUserPack", {"overlay": files["overlay"], "preset": files["preset"]})
			if r.get("ok", false):
				loaded.append(id)
				errors.erase(id)
				pending.erase(id)
				progress = true
			else:
				errors[id] = str(r.get("error", "refused"))
	return {"loaded": loaded, "errors": errors}

## A save's refusal in plain words: a pack the save needs is missing, or its files changed since the save.
static func friendly_load_error(message: String) -> String:
	var at := message.find("needs overlay ")
	if at >= 0:
		var id := message.substr(at + 14).get_slice(",", 0).strip_edges()
		return "This save was made with the pack \"%s\", which is not installed. Put its files back in the mods folder (%s.overlay.json and %s.preset.json) and load again." % [id, id, id]
	at = message.find("changed since the save")
	if at >= 0:
		var head := message.substr(0, at).strip_edges()
		var id := head.get_slice(" ", head.get_slice_count(" ") - 1)
		return "This save was made with an earlier version of the pack \"%s\"; the pack's files have changed since, so the save will not load. Restore the original files to play it." % id
	return message
