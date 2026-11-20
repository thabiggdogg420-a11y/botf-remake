class_name PlanetTextures
extends RefCounted
## Planet texture sets from lane L7 for the system close-up, loaded lazily (on a worker thread where threads exist) and cached.
## Layout of a set (L7): planets/<class>/<tier>/v<n>_albedo.(png|webp), v<n>_clouds.png, v<n>_night.png, tier
## "desktop" (4096x2048, also _rough and _normal) or "web" (1024x512, relief baked in). The galaxy map draws a planet at most about 160 px wide, so it reads
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

## True when the active renderer is Compatibility (WebGL2 or the desktop gl_compatibility path). The look tweaks for
## 8-bit blending key off this; the WebGPU (Forward+) browser build is not Compatibility and gets the desktop look.
static func compat() -> bool:
	return RenderingServer.get_current_rendering_method() == "gl_compatibility"

## True when only the web texture tier should load: any browser build (pack sizes) or a Compatibility run.
static func web_tier() -> bool:
	return OS.has_feature("web") or compat()

## The variants available for a planet class (each a dict of file paths), [] when L7 has none. `hi` asks for the
## desktop tier (4096 source, capped at MAX_WIDTH on load, with normal map) where the renderer allows it: for a planet that
## fills a good part of the screen (battle backdrop, big system orb). The web build gets the same tier from
## planets_hi_web.pck (tools/build_planet_hi_web.py: 2048 WebP, fetched after the title) once it is mounted, and the
## web tier until then. A desktop gl_compatibility run keeps the web tier (relief baked into the albedo).
static func variants(cls: String, hi: bool = false) -> Array:
	hi = hi and (OS.has_feature("web") or not compat())
	var key := cls + ("#hi" if hi else "")
	if _sets.has(key):
		return _sets[key]
	var out: Array = []
	if disabled:
		return out
	var tiers := ["desktop", "web"] if hi else ["web", "desktop"]
	if web_tier() and not hi:
		tiers = ["web"]
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
					for layer in ["clouds", "night", "rough", "normal"]:
						for ext in ["png", "webp"]:
							var lp := dir.path_join("v%d_%s.%s" % [n, layer, ext])
							if AppLoader.exists(lp):
								v[layer] = lp
								break
					out.append(v)
				if not out.is_empty():
					break
			if not out.is_empty():
				break
		if not out.is_empty():
			break
	_sets[key] = out
	return out

## The texture at `path` if loaded; otherwise starts a background load and returns null (call again next frame). Without
## threads (the web build) the image loads right here and the texture is returned at once.
static func get_texture(path: String) -> Texture2D:
	if _tex.has(path):
		return _tex[path]
	if not AppLoader.threads():
		# One decode per frame on the main thread (a 2048 WebP plus its mips is tens of ms in wasm): callers retry next frame.
		if Engine.get_process_frames() == _sync_frame:
			return null
		_sync_frame = Engine.get_process_frames()
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

static var _sync_frame := -1

static func _read(path: String) -> Image:
	# The web tier is 1024 wide already; the hi tier is capped at MAX_WIDTH like the desktop's.
	var img := AppLoader.image(path, MAX_WIDTH)
	if img == null:
		img = Image.create(2, 1, false, Image.FORMAT_RGB8)
	return img

static func _load_image(path: String) -> void:
	var img := _read(path)
	_lock.lock()
	_images[path] = img
	_lock.unlock()
