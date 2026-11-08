class_name PlanetTextures
extends RefCounted
## Planet texture sets from lane L7 for the system close-up, loaded lazily on a worker thread and cached.
## Layout of a set (L7): planets/<class>/<tier>/v<n>_albedo.(png|webp), v<n>_clouds.png, v<n>_night.png, tier
## "desktop" (4096x2048) or "web" (1024x512). The galaxy map draws a planet at most about 160 px wide, so it reads
## the web tier first in both renderers and caps any image at MAX_WIDTH on the worker (a 4096 upload on the main
## thread was a 0.5 s hitch).
##
## Lookup order:
##  1. res://assets/planets/  (a pack built from LocalAssets; the only source on the web build: no disk paths at runtime)
##  2. desktop dev runs only: $BOTF_PLANET_DIR, <exe dir>/LocalAssets/planets, res://../game/LocalAssets/planets
## A class with no set returns null and the caller draws the procedural planet (planet_globe.gdshader).

const CLASS_DIRS := {
	"M": ["m"], "O": ["o"], "P": ["p"], "L": ["l"], "H": ["h"], "K": ["k"], "Y": ["y"],
	"GasGiant": ["gasgiant", "gas_giant", "gas"], "Asteroids": ["asteroids"],
}

static var _roots: Array = []
static var _sets := {}          # "<class>" -> Array of variant dicts {albedo, clouds, night} (paths), [] when none
static var _tex := {}           # path -> Texture2D
static var _pending := {}       # path -> WorkerThreadPool task id
static var _images := {}        # path -> Image (filled by the worker)
static var _lock := Mutex.new()
## Tests set this to skip disk lookups entirely.
static var disabled := false
const MAX_WIDTH := 2048

static func roots() -> Array:
	if not _roots.is_empty() or disabled:
		return _roots
	if DirAccess.dir_exists_absolute("res://assets/planets"):
		_roots.append("res://assets/planets")
	if not OS.has_feature("web"):
		var env := OS.get_environment("BOTF_PLANET_DIR")
		if env != "":
			_roots.append(env)
		_roots.append(OS.get_executable_path().get_base_dir().path_join("LocalAssets/planets"))
		_roots.append(ProjectSettings.globalize_path("res://").path_join("../game/LocalAssets/planets").simplify_path())
	_roots = _roots.filter(func(r): return DirAccess.dir_exists_absolute(r))
	return _roots

## The variants available for a planet class (each a dict of file paths), [] when L7 has none.
static func variants(cls: String) -> Array:
	if _sets.has(cls):
		return _sets[cls]
	var out: Array = []
	for root in roots():
		for d in CLASS_DIRS.get(cls, [cls.to_lower()]):
			for tr in ["web", "desktop"]:
				var dir := str(root).path_join(d).path_join(tr)
				if not DirAccess.dir_exists_absolute(dir):
					continue
				for n in range(1, 7):
					var alb := ""
					for ext in ["png", "webp", "jpg"]:
						var p := dir.path_join("v%d_albedo.%s" % [n, ext])
						if FileAccess.file_exists(p):
							alb = p
							break
					if alb == "":
						continue
					var v := {"albedo": alb}
					for layer in ["clouds", "night"]:
						var lp := dir.path_join("v%d_%s.png" % [n, layer])
						if FileAccess.file_exists(lp):
							v[layer] = lp
					out.append(v)
				if not out.is_empty():
					break
			if not out.is_empty():
				break
		if not out.is_empty():
			break
	_sets[cls] = out
	return out

## The texture at `path` if loaded; otherwise starts a background load and returns null (call again next frame).
static func get_texture(path: String) -> Texture2D:
	if _tex.has(path):
		return _tex[path]
	if path.begins_with("res://") and ResourceLoader.exists(path):
		_tex[path] = load(path)
		return _tex[path]
	_lock.lock()
	var img: Image = _images.get(path)
	_lock.unlock()
	if img != null:
		var tex := ImageTexture.create_from_image(img)
		_tex[path] = tex
		_lock.lock()
		_images.erase(path)
		_lock.unlock()
		_pending.erase(path)
		return tex
	if not _pending.has(path):
		_pending[path] = WorkerThreadPool.add_task(_load_image.bind(path), false, "planet texture")
	return null

static func _load_image(path: String) -> void:
	var img := Image.load_from_file(path)
	if img == null or img.is_empty():
		img = Image.create(2, 1, false, Image.FORMAT_RGB8)
	else:
		if img.get_width() > MAX_WIDTH:
			img.resize(MAX_WIDTH, MAX_WIDTH * img.get_height() / img.get_width(), Image.INTERPOLATE_LANCZOS)
		if not img.has_mipmaps():
			img.generate_mipmaps()
	_lock.lock()
	_images[path] = img
	_lock.unlock()
