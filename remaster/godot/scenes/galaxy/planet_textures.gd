class_name PlanetTextures
extends RefCounted
## Planet texture sets from lane L7 for the system close-up, loaded lazily (on a worker thread where threads exist) and cached.
## Layout of a set (L7): planets/<class>/<tier>/v<n>_albedo.(png|webp), v<n>_clouds.png, v<n>_night.png, tier
## "desktop" (4096x2048) or "web" (1024x512). The galaxy map draws a planet at most about 160 px wide, so it reads
## the web tier first in both renderers and caps any image at MAX_WIDTH on the worker (a 4096 upload on the main
## thread was a 0.5 s hitch).
##

## Where the sets come from (M0-LANES "Browser target": no disk paths at runtime): res://assets/planets/ in the exe's or the
## web export's pck, or a pack mounted by AppLoader.mount_user_packs (user://packs/*.pck, built by tools/pack_planets.gd from
## the private texture folder). Every file access goes through AppLoader; nothing here touches a disk path or an
## environment variable. On the web (no threads) images load on the main thread, at the web tier.
## A class with no set returns null and the caller draws the procedural planet (planet_globe.gdshader).

const ROOT := "res://assets/planets"
const CLASS_DIRS := {
	"M": ["m"], "O": ["o"], "P": ["p"], "L": ["l"], "H": ["h"], "K": ["k"], "Y": ["y"],
	"GasGiant": ["gasgiant", "gas_giant", "gas"], "Asteroids": ["asteroids"],
}

static var _sets := {}          # "<class>" -> Array of variant dicts {albedo, clouds, night} (paths), [] when none
static var _tex := {}           # path -> Texture2D
static var _pending := {}       # path -> WorkerThreadPool task id
static var _images := {}        # path -> Image (filled by the worker)
static var _lock := Mutex.new()
## Tests set this to skip lookups entirely.
static var disabled := false
const MAX_WIDTH := 2048
const WEB_MAX_WIDTH := 1024

## Forget cached lookups (after a pack is mounted).
static func reset() -> void:
	_sets.clear()

## The variants available for a planet class (each a dict of file paths), [] when L7 has none.
static func variants(cls: String) -> Array:
	if _sets.has(cls):
		return _sets[cls]
	var out: Array = []
	if disabled:
		return out
	var tiers := ["web"] if OS.has_feature("web") else ["web", "desktop"]
	for root in [ROOT]:
		for d in CLASS_DIRS.get(cls, [cls.to_lower()]):
			for tr in tiers:
				var dir := str(root).path_join(d).path_join(tr)
				if not AppLoader.dir_exists(dir):
					continue
				for n in range(1, 7):
					var alb := ""
					for ext in ["png", "webp", "jpg"]:
						var p := dir.path_join("v%d_albedo.%s" % [n, ext])
						if AppLoader.exists(p):
							alb = p
							break
					if alb == "":
						continue
					var v := {"albedo": alb}
					for layer in ["clouds", "night"]:
						var lp := dir.path_join("v%d_%s.png" % [n, layer])
						if AppLoader.exists(lp):
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

## The texture at `path` if loaded; otherwise starts a background load and returns null (call again next frame). Without
## threads (the web build) the image loads right here and the texture is returned at once.
static func get_texture(path: String) -> Texture2D:
	if _tex.has(path):
		return _tex[path]
	if not AppLoader.threads():
		var img := _read(path)
		_tex[path] = ImageTexture.create_from_image(img)
		return _tex[path]
	_lock.lock()
	var done: Image = _images.get(path)
	_lock.unlock()
	if done != null:
		var tex := ImageTexture.create_from_image(done)
		_tex[path] = tex
		_lock.lock()
		_images.erase(path)
		_lock.unlock()
		_pending.erase(path)
		return tex
	if not _pending.has(path):
		_pending[path] = WorkerThreadPool.add_task(_load_image.bind(path), false, "planet texture")
	return null

static func _read(path: String) -> Image:
	var img := AppLoader.image(path, WEB_MAX_WIDTH if OS.has_feature("web") else MAX_WIDTH)
	if img == null:
		img = Image.create(2, 1, false, Image.FORMAT_RGB8)
	return img

static func _load_image(path: String) -> void:
	var img := _read(path)
	_lock.lock()
	_images[path] = img
	_lock.unlock()
