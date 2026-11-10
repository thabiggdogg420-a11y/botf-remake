class_name ShipIcons
extends RefCounted
## Baked ship pictures: LocalAssets/ship_icons/<fac>_<class>_<view>.png (256 px) and ..._<view>_64.png, written by
## godot/tools/bake_ship_icons.gd from the hero models. LocalAssets is third-party-derived and never committed, so the
## icons are found at run time, in the same order as the battle manifests:
##   $BOTF_SHIP_ICONS (a folder; "none" hides every baked icon and shows the vector fallback), then
##   dir of $BOTF_RESOLVED_MANIFEST, <exe dir>/LocalAssets, res://../LocalAssets, res://../game/LocalAssets (each + /ship_icons).
## Views: "iso" (three-quarter top view) and "top" (clean top-down, bow to the right).

const FAC := ["", "fed", "kli", "rom", "car", "fer"]

static var _tex := {}      ## path -> Texture2D or null
static var _dir := ""
static var _dir_done := false

static func slug(s: String) -> String:
	var out := ""
	var sep := false
	for ch in s.to_lower().replace("'", ""):
		var ok := (ch >= "a" and ch <= "z") or (ch >= "0" and ch <= "9")
		if ok:
			if sep and out != "":
				out += "_"
			out += ch
			sep = false
		else:
			sep = true
	return out

static func dirs() -> PackedStringArray:
	var out := PackedStringArray()
	var env := OS.get_environment("BOTF_SHIP_ICONS")
	if env != "":
		out.append(env)
		return out
	var rm := OS.get_environment("BOTF_RESOLVED_MANIFEST")
	if rm != "":
		out.append(rm.get_base_dir().path_join("ship_icons"))
	var hm := OS.get_environment("BOTF_HERO_MANIFEST") # <LocalAssets>/ships/godot-hero-manifest.json
	if hm != "":
		out.append(hm.get_base_dir().get_base_dir().path_join("ship_icons"))
	out.append(OS.get_executable_path().get_base_dir().path_join("LocalAssets/ship_icons"))
	var res := ProjectSettings.globalize_path("res://")
	out.append(res.path_join("../LocalAssets/ship_icons").simplify_path())
	out.append(res.path_join("../game/LocalAssets/ship_icons").simplify_path())
	return out

## The folder baked icons are read from, "" when none exists (or the override is "none").
static func folder() -> String:
	if _dir_done:
		return _dir
	_dir_done = true
	_dir = ""
	if OS.has_feature("web") or OS.get_environment("BOTF_SHIP_ICONS").to_lower() == "none":
		return _dir
	for d in dirs():
		if DirAccess.dir_exists_absolute(d):
			_dir = d
			break
	return _dir

## Test hook: forget the cached folder and textures (after changing BOTF_SHIP_ICONS).
static func reset() -> void:
	_dir_done = false
	_dir = ""
	_tex.clear()

static func file_key(owner: Variant, klass: String, asset_key: String = "") -> String:
	if asset_key.begins_with("ship."):
		return asset_key.substr(5).replace(".", "_")
	var o := int(owner) if (owner is int or owner is float) else 0
	if o < 1 or o >= FAC.size() or klass.strip_edges() == "":
		return ""
	return "%s_%s" % [FAC[o], slug(klass)]

## The baked icon for a class, or null when none was baked / LocalAssets is absent. px picks the 64 or 256 file.
static func texture(owner: Variant, klass: String, view: String = "iso", px: int = 256, asset_key: String = "") -> Texture2D:
	var fk := file_key(owner, klass, asset_key)
	var base := folder()
	if fk == "" or base == "":
		return null
	var path := base.path_join("%s_%s%s.png" % [fk, view, "_64" if px <= 72 else ""])
	if _tex.has(path):
		return _tex[path]
	var t: Texture2D = null
	if FileAccess.file_exists(path):
		var img := Image.load_from_file(path)
		if img != null and not img.is_empty():
			img.generate_mipmaps()
			t = ImageTexture.create_from_image(img)
	_tex[path] = t
	return t

static func has_any() -> bool:
	return folder() != ""
