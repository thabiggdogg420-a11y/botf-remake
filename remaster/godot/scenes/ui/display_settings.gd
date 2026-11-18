class_name DisplaySettings
extends RefCounted
## Display and graphics settings (Simon, 8 Nov 2026: "the settings dont offer any resolution options, FPS caps etc").
## Stored in App.settings (user://botf_settings.json) under the keys in DEFAULTS and applied at startup by App._ready
## and on every change from the Settings overlay. Window and frame options apply to the game window; render scale,
## anti-aliasing and shadow quality apply to the root viewport and to every 3D SubViewport (battle, title, galaxy)
## through apply_viewport, which those views call when they are built and which apply() re-runs on open ones.

const DEFAULTS := {
	"display.resolution": "", # "WxH"; "" = the current screen's size
	"display.windowMode": "windowed", # windowed | borderless | fullscreen
	"display.vsync": true,
	"display.fpsCap": 0, # 0 = unlimited, else 30 / 60 / 120 / 144
	"display.renderScale": 1.0, # 0.5 - 1.0 of the window, upscaled with FSR 1.0 below 1
	"display.antiAlias": "msaa2", # off | fxaa | msaa2 | msaa4 | msaa8
	"display.shadows": "high", # off | low | medium | high
}
const RESOLUTIONS := ["1280x720", "1366x768", "1600x900", "1920x1080", "2560x1440", "3440x1440", "3840x2160"]
const WINDOW_MODES := {"windowed": "Windowed", "borderless": "Borderless window", "fullscreen": "Fullscreen"}
const FPS_CAPS := [30, 60, 120, 144, 0]
const RENDER_SCALES := [0.5, 0.67, 0.75, 0.85, 1.0]
const ANTI_ALIAS := {"off": "Off", "fxaa": "FXAA", "msaa2": "MSAA 2x", "msaa4": "MSAA 4x", "msaa8": "MSAA 8x"}
const SHADOWS := {"off": "Off", "low": "Low", "medium": "Medium", "high": "High"}

## True while running headless or under a test harness: window changes are recorded, not applied.
static var dry_run := false
static var last_applied := {}


## Browser target: settings the web build cannot honour. The browser owns the window (size, mode, vsync, frame
## pacing) and the Compatibility renderer has no FSR upscaling, so these rows are not offered there.
const WEB_UNSUPPORTED := ["display.resolution", "display.windowMode", "display.vsync", "display.fpsCap", "display.renderScale"]
## Test hook: true makes offered() answer as the web build does.
static var web_preview := false

static func is_web() -> bool:
	return web_preview or OS.has_feature("web")

## Whether the Settings overlay offers `key` on this platform.
static func offered(key: String) -> bool:
	return not (is_web() and WEB_UNSUPPORTED.has(key))


static func fill_defaults(s: Dictionary) -> void:
	for k in DEFAULTS:
		if not s.has(k):
			s[k] = DEFAULTS[k]


static func resolution_list() -> Array:
	var out: Array = []
	var screen := DisplayServer.screen_get_size() if DisplayServer.get_name() != "headless" else Vector2i(3840, 2160)
	for r in RESOLUTIONS:
		var v := parse_resolution(r)
		if v.x <= screen.x and v.y <= screen.y:
			out.append(r)
	var native := "%dx%d" % [screen.x, screen.y]
	if not out.has(native) and screen.x > 0:
		out.append(native)
	return out


static func parse_resolution(r: String) -> Vector2i:
	var p := r.split("x")
	if p.size() != 2:
		return Vector2i.ZERO
	return Vector2i(int(p[0]), int(p[1]))


static func fps_label(cap: int) -> String:
	return "Unlimited" if cap <= 0 else "%d fps" % cap


## Applies everything to the window, the engine and every open 3D viewport. Returns what was applied (for tests).
static func apply(s: Dictionary, tree: SceneTree) -> Dictionary:
	fill_defaults(s)
	var out := {}
	var mode := str(s["display.windowMode"])
	var res := parse_resolution(str(s["display.resolution"]))
	out.windowMode = mode
	out.resolution = res
	out.vsync = bool(s["display.vsync"])
	out.fpsCap = int(s["display.fpsCap"])
	Engine.max_fps = out.fpsCap
	var headless := DisplayServer.get_name() == "headless"
	if not dry_run and not headless:
		DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_ENABLED if out.vsync else DisplayServer.VSYNC_DISABLED)
		match mode:
			"fullscreen":
				DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_BORDERLESS, false)
				DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_EXCLUSIVE_FULLSCREEN)
			"borderless":
				DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED)
				DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_BORDERLESS, true)
				var scr := DisplayServer.window_get_current_screen()
				DisplayServer.window_set_position(DisplayServer.screen_get_position(scr))
				DisplayServer.window_set_size(DisplayServer.screen_get_size(scr))
			_:
				DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED)
				DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_BORDERLESS, false)
				if res.x > 0:
					DisplayServer.window_set_size(res)
					var scr := DisplayServer.window_get_current_screen()
					var at := DisplayServer.screen_get_position(scr) + (DisplayServer.screen_get_size(scr) - res) / 2
					DisplayServer.window_set_position(at)
	if tree != null:
		apply_viewport(tree.root, s)
		for v in tree.root.find_children("*", "SubViewport", true, false):
			apply_viewport(v, s)
	out.renderScale = float(s["display.renderScale"])
	out.antiAlias = str(s["display.antiAlias"])
	out.shadows = str(s["display.shadows"])
	_apply_shadows(out.shadows)
	last_applied = out
	return out


## Render scale, anti-aliasing and shadow atlas for one viewport (root or a 3D SubViewport).
static func apply_viewport(v: Viewport, s: Dictionary) -> void:
	if v == null:
		return
	fill_defaults(s)
	var scale := clampf(float(s["display.renderScale"]), 0.5, 1.0)
	v.scaling_3d_scale = scale
	v.scaling_3d_mode = Viewport.SCALING_3D_MODE_FSR if scale < 0.999 else Viewport.SCALING_3D_MODE_BILINEAR
	var aa := str(s["display.antiAlias"])
	v.screen_space_aa = Viewport.SCREEN_SPACE_AA_FXAA if aa == "fxaa" else Viewport.SCREEN_SPACE_AA_DISABLED
	v.msaa_3d = {"msaa2": Viewport.MSAA_2X, "msaa4": Viewport.MSAA_4X, "msaa8": Viewport.MSAA_8X}.get(aa, Viewport.MSAA_DISABLED)
	CompatLook.viewport(v)
	var sh := str(s["display.shadows"])
	v.positional_shadow_atlas_size = {"off": 256, "low": 1024, "medium": 2048, "high": 4096}.get(sh, 4096)


static func _apply_shadows(q: String) -> void:
	var size: int = {"off": 256, "low": 1024, "medium": 2048, "high": 4096}.get(q, 4096)
	RenderingServer.directional_shadow_atlas_set_size(size, q != "low" and q != "off")
	var filt: int = {"off": RenderingServer.SHADOW_QUALITY_HARD, "low": RenderingServer.SHADOW_QUALITY_SOFT_VERY_LOW,
		"medium": RenderingServer.SHADOW_QUALITY_SOFT_LOW, "high": RenderingServer.SHADOW_QUALITY_SOFT_MEDIUM}.get(q, 3)
	RenderingServer.directional_soft_shadow_filter_set_quality(filt)
	RenderingServer.positional_soft_shadow_filter_set_quality(filt)
	# Off: lights keep their flag but the world draws no shadow; the battle sun reads this.
	shadows_enabled = q != "off"


static var shadows_enabled := true
