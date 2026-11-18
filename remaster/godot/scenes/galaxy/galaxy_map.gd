class_name GalaxyMap
extends Control
## The 3D star chart (MASTER-PLAN decision 2, Simon 8 Nov 2026). The core's 2D sector grid is lifted into a thin
## disc (GalaxyLayout); every star is a typed impostor plus a real photosphere sphere, planets orbit at system zoom,
## the galactic backdrop is a sky shader and a dust disc. Layers: territory and supply as soft regions on the
## galactic plane, fog of war, trade routes and fleet paths with ETAs as lines in space, anomalies as ray-marched
## volumes (Forward+) or billboards (Compatibility). Labels and icons are drawn in screen space (GalaxyOverlay: a static layer redrawn on camera or data change, an animated one every frame).
##
## Camera: an orbit camera around a focus point. `cam` is the focus in map space (pixels of the old 2D chart,
## CELL per sector) and `zoom` is pixels per map unit at the focus, so code written for the 2D chart still reads.
## Zoom runs smoothly from the whole galaxy, to a sector, to a system close-up (zoom_level()).
## Input: left-drag or middle-drag pans, right-drag orbits, wheel zooms to the cursor, left-click selects,
## double-click flies to the target, right-click (no drag) opens the order menu. Keys: WASD / arrows pan, Q/E rotate,
## Page Up/Down tilt, +/- zoom, F frame the whole galaxy, H home system.

signal cell_clicked(cell: int, double: bool)
signal cell_context(cell: int, screen_pos: Vector2)
signal hover_changed(cell: int, screen_pos: Vector2)

const CELL := 84.0
const COMPAT_DISC_STRENGTH := 5.0
const COMPAT_DISC_COOL := Vector3(0.18, 0.16, 0.16) ## gap 6: the disc's cool dust on Compatibility (Forward+ 0.12, 0.15, 0.24)
const COMPAT_FOG_OPACITY := 0.4
const COMPAT_STAR_SATURATION := 1.6
const FOV := 40.0
const ZOOM_MIN := 0.1
const ZOOM_MAX := 40.0
## zoom_level() thresholds: below LEVEL_SECTOR the whole galaxy, from LEVEL_SYSTEM a system close-up.
const LEVEL_SECTOR := 1.2
const LEVEL_SYSTEM := 6.0
## The zoom fly_to() uses for each level.
const FLY_SECTOR := 2.6
const FLY_SYSTEM := 16.0
const PITCH_DEFAULT := 0.95   # radians above the plane (about 54 degrees)
const PITCH_MIN := 0.3
const PITCH_MAX := 1.52
const PICK_RADIUS := 18.0
const PLANET_RADIUS := {"GasGiant": 0.024, "M": 0.0125, "O": 0.013, "L": 0.012, "H": 0.011, "P": 0.011, "K": 0.009, "Y": 0.0105}

var app: App
var cam := Vector2.ZERO           # map-space point at the focus of the view
var zoom := 1.0                   # pixels per map unit at the focus
var yaw := 0.0
var pitch := PITCH_DEFAULT
var hover_cell := -1
var layers := {}
## Screen space covered by the host's panels (left, top, right, bottom). frame_known() fits the chart into the rest.
var view_insets := Vector4.ZERO

var _container: SubViewportContainer
var _viewport: SubViewport
var _root3d: Node3D
var _camera: Camera3D
var _env: Environment
var _disc: MeshInstance3D
var _territory: MeshInstance3D
var _supply: MeshInstance3D
var _fog: MeshInstance3D
var _features: Node3D
var _stars: Node3D
var _decor: Node3D
var _overlay: GalaxyOverlay
var _overlay_fx: GalaxyOverlay
var _overlay_sig := []
var _overlay_dirty := true
var _volumetric := true
var _supply_for := -1
var _focus_h := 0.0
var _target_cam := Vector2.ZERO
var _target_zoom := 1.0
var _target_yaw := 0.0
var _target_pitch := PITCH_DEFAULT
var _animating := false
var _drag_button := 0
var _drag_moved := 0.0
var _last_mouse := Vector2.ZERO
var _seed := 0
## system id -> {"pos": Vector3, "type": int, "cell": int, "node": Node3D, "impostor", "sphere", "planets": Node3D|null, "system": Dictionary}
var systems3d := {}
## cell -> system id
var _cell_system := {}
var _time := 0.0
var _view_proj := Projection.IDENTITY
var _vp_size := Vector2(1920, 1080)

func _ready() -> void:
	clip_contents = true
	mouse_filter = Control.MOUSE_FILTER_STOP
	_volumetric = RenderingServer.get_current_rendering_method() == "forward_plus"
	_container = SubViewportContainer.new()
	_container.name = "View3D"
	_container.stretch = true
	_container.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_container.set_anchors_preset(Control.PRESET_FULL_RECT)
	add_child(_container)
	_viewport = SubViewport.new()
	_viewport.own_world_3d = true
	_viewport.handle_input_locally = false
	_viewport.msaa_3d = get_viewport().msaa_3d
	_viewport.screen_space_aa = get_viewport().screen_space_aa
	_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	_container.add_child(_viewport)
	CompatBloom.attach(_container, _viewport)
	_root3d = Node3D.new()
	_root3d.name = "Galaxy3D"
	_viewport.add_child(_root3d)
	_camera = Camera3D.new()
	_camera.fov = FOV
	_camera.current = true
	_root3d.add_child(_camera)
	_build_environment()
	_disc = MeshInstance3D.new()
	_disc.name = "GalacticDisc"
	_root3d.add_child(_disc)
	_territory = _field_mesh("Territory", 0, -3)
	_supply = _field_mesh("Supply", 0, -2)
	_fog = _field_mesh("Fog", 1, -1)
	_features = Node3D.new()
	_features.name = "Anomalies"
	_root3d.add_child(_features)
	_stars = Node3D.new()
	_stars.name = "Stars"
	_root3d.add_child(_stars)
	_decor = Node3D.new()
	_decor.name = "Decor"
	_root3d.add_child(_decor)
	for k in 2:
		var o := GalaxyOverlay.new()
		o.name = "Overlay" if k == 0 else "OverlayFx"
		o.map = self
		o.anim = k == 1
		o.set_anchors_preset(Control.PRESET_FULL_RECT)
		o.mouse_filter = Control.MOUSE_FILTER_IGNORE
		add_child(o)
		if k == 0:
			_overlay = o
		else:
			_overlay_fx = o
	_apply_camera()

func _build_environment() -> void:
	var we := WorldEnvironment.new()
	_env = Environment.new()
	_env.background_mode = Environment.BG_SKY
	var sky := Sky.new()
	var sm := ShaderMaterial.new()
	sm.shader = load("res://scenes/galaxy/shaders/galaxy_sky.gdshader")
	sky.sky_material = sm
	sky.radiance_size = Sky.RADIANCE_SIZE_32
	_env.sky = sky
	_env.ambient_light_source = Environment.AMBIENT_SOURCE_DISABLED
	_env.reflected_light_source = Environment.REFLECTION_SOURCE_DISABLED
	# Art bible section 4: ACES, exposure 1, white 8; glow 0.4, HDR threshold 1.8 (the battle clamps).
	_env.tonemap_mode = Environment.TONE_MAPPER_ACES
	_env.tonemap_exposure = 1.0
	_env.tonemap_white = 8.0
	_env.glow_enabled = true
	_env.glow_intensity = 0.4
	_env.glow_hdr_threshold = 1.8
	_env.glow_hdr_scale = 1.2
	_env.glow_blend_mode = Environment.GLOW_BLEND_MODE_SCREEN
	# Levels 1-4 only: the wide levels 5-6 wash the whole frame when a star's sphere fills the close-up.
	for lv in range(1, 7):
		_env.set_glow_level(lv, 1.0 if lv <= 4 else 0.0)
	we.environment = _env
	_root3d.add_child(we)

func _field_mesh(name_: String, mode: int, priority: int) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	mi.name = name_
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var m := ShaderMaterial.new()
	m.shader = load("res://scenes/galaxy/shaders/field3d.gdshader")
	m.set_shader_parameter("mode", mode)
	if mode == 1 and PlanetTextures.compat():
		m.set_shader_parameter("opacity", COMPAT_FOG_OPACITY)   # lets the dust show through the unexplored sheet, as on Forward+
	m.render_priority = priority
	mi.material_override = m
	_root3d.add_child(mi)
	return mi

## The ShaderMaterial of an anomaly node (volume or billboard); tests read its parameters.
static func feature_material(node: Node) -> ShaderMaterial:
	return (node as GeometryInstance3D).material_override as ShaderMaterial

func view() -> Dictionary:
	return app.view if app else {}

func grid_size() -> Vector2i:
	var m: Dictionary = view().get("meta", {})
	return Vector2i(int(m.get("width", 1)), int(m.get("height", 1)))

func is_volumetric() -> bool:
	return _volumetric

# ---------------------------------------------------------------------------------------------
# Camera
# ---------------------------------------------------------------------------------------------
func cell_center(x: float, y: float) -> Vector2:
	return Vector2((x + 0.5) * CELL, (y + 0.5) * CELL)

## World position of a map-space point on the focus plane.
func map_to_world(p: Vector2, h: float = NAN) -> Vector3:
	return Vector3(p.x / CELL, _focus_h if is_nan(h) else h, p.y / CELL)

## The 3D anchor of a sector: its star if it has a system, else the sector centre on the galactic plane.
func cell_anchor(cell: int) -> Vector3:
	var sid: int = _cell_system.get(cell, 0)
	if sid != 0 and systems3d.has(sid):
		return systems3d[sid]["pos"]
	var g := grid_size()
	return Vector3(cell % g.x + 0.5, 0.0, cell / g.x + 0.5)

## World -> screen through a projection cached per camera move (Camera3D.unproject_position rebuilds the
## projection on every call, which cost several ms per frame at 80 systems).
func world_to_screen(w: Vector3) -> Vector2:
	if _camera == null:
		return Vector2.ZERO
	var c := _view_proj * Vector4(w.x, w.y, w.z, 1.0)
	if absf(c.w) < 1e-6:
		return Vector2.ZERO
	return Vector2((c.x / c.w * 0.5 + 0.5) * _vp_size.x, (0.5 - c.y / c.w * 0.5) * _vp_size.y)

func is_in_front(w: Vector3) -> bool:
	return _camera != null and not _camera.is_position_behind(w)

func map_to_screen(p: Vector2) -> Vector2:
	return world_to_screen(map_to_world(p))

## Screen position of a sector's anchor (its star, or the sector centre).
func cell_screen_pos(x: int, y: int) -> Vector2:
	return world_to_screen(cell_anchor(y * grid_size().x + x))

func screen_to_map(p: Vector2) -> Vector2:
	if _camera == null or size.x <= 0.0:
		return (p - size / 2.0) / zoom + cam
	var o := _camera.project_ray_origin(p)
	var d := _camera.project_ray_normal(p)
	if absf(d.y) < 1e-4:
		return (p - size / 2.0) / zoom + cam
	var t := (_focus_h - o.y) / d.y
	if t < 0.0:
		return (p - size / 2.0) / zoom + cam
	var hit := o + d * t
	return Vector2(hit.x * CELL, hit.z * CELL)

## Pixels per world unit (sector) at distance `dist` from the camera.
func pixels_per_unit(dist: float) -> float:
	return size.y / (2.0 * tan(deg_to_rad(FOV) * 0.5) * maxf(dist, 1e-4))

func camera_distance() -> float:
	return size.y / (2.0 * tan(deg_to_rad(FOV) * 0.5) * CELL * maxf(zoom, 1e-4)) if size.y > 0.0 else 1080.0 / (2.0 * tan(deg_to_rad(FOV) * 0.5) * CELL * zoom)

func camera_position() -> Vector3:
	return _camera.global_position if _camera else Vector3.ZERO

## "galaxy", "sector" or "system" for a zoom value (default: the current zoom).
func zoom_level(z: float = -1.0) -> String:
	var v := zoom if z <= 0.0 else z
	if v >= LEVEL_SYSTEM:
		return "system"
	if v >= LEVEL_SECTOR:
		return "sector"
	return "galaxy"

## Picks what is under a screen point: the nearest icon or star within PICK_RADIUS px, else the sector the cursor
## ray meets on the focus plane. Returns {"cell", "kind" ("system", "fleet", "contact", "station", "space"), "id"}.
func pick(screen_pos: Vector2) -> Dictionary:
	var v := view()
	var g := grid_size()
	var best := {"cell": -1, "kind": "space", "id": 0}
	var best_d := INF
	if not v.is_empty():
		for c in _overlay.pickables:
			var d: float = screen_pos.distance_to(c["pos"]) - float(c.get("r", 0.0))
			if d < PICK_RADIUS and d < best_d:
				best_d = d
				best = {"cell": int(c["cell"]), "kind": c["kind"], "id": int(c["id"])}
	if best_d < INF:
		return best
	var m := screen_to_map(screen_pos)
	var x := int(floor(m.x / CELL))
	var y := int(floor(m.y / CELL))
	if x < 0 or y < 0 or x >= g.x or y >= g.y:
		return best
	best["cell"] = y * g.x + x
	return best

func cell_at(screen_pos: Vector2) -> int:
	return int(pick(screen_pos)["cell"])

func focus_cell(x: int, y: int, z: float = -1.0, animate: bool = true) -> void:
	_target_cam = cell_center(x, y)
	var a := cell_anchor(y * grid_size().x + x)
	_target_cam = Vector2(a.x, a.z) * CELL
	_target_zoom = zoom if z <= 0 else clampf(z, ZOOM_MIN, ZOOM_MAX)
	_target_yaw = yaw
	_target_pitch = pitch
	if animate and not (app and app.settings.get("reducedMotion", false)):
		_animating = true
	else:
		cam = _target_cam
		zoom = _target_zoom
		_animating = false
		_update_focus_height(true)
		_apply_camera()

## Double-click: flies one level closer to the target (galaxy -> sector -> system close-up).
func fly_to_cell(x: int, y: int) -> void:
	var lvl := zoom_level(maxf(zoom, _target_zoom) if _animating else zoom)
	var has_system := _cell_system.has(y * grid_size().x + x)
	var z := FLY_SECTOR
	if lvl != "galaxy" and has_system:
		z = FLY_SYSTEM
	elif lvl == "system":
		z = zoom
	focus_cell(x, y, maxf(z, zoom) if not has_system else z)

## True when the view is centred on sector (x, y).
func is_focused_on(x: int, y: int) -> bool:
	var a := cell_anchor(y * grid_size().x + x)
	return Vector2(a.x, a.z).distance_to(cam / CELL) < 0.25

## Frames the WHOLE galaxy (every star, charted or not) inside the part of the map the host's panels leave visible, at the
## default tilt, north up: the opening view of a new or loaded game, F, and the benchmark's picture (Simon, 10 Nov 2026).
## The tilted disc is fitted by projection: zoom and centre are refined until the bounds of the galaxy rectangle and every
## star (plus room for its label) sit inside the free rectangle, so Small through Huge never clip behind the nav rail,
## the top bar or the inspector. The kept name frame_known() is what older callers (captures, tests) use.
func frame_known() -> void:
	frame_galaxy()

func frame_galaxy() -> void:
	var v := view()
	if v.is_empty():
		return
	var g := grid_size()
	var vp := size if size.x > 0.0 and size.y > 0.0 else Vector2(1920, 1080)
	var free := Rect2(Vector2(view_insets.x, view_insets.y), vp - Vector2(view_insets.x + view_insets.z, view_insets.y + view_insets.w))
	if free.size.x < 200.0 or free.size.y < 160.0:
		free = Rect2(Vector2.ZERO, vp)
	var pad := Vector2(48.0, 30.0)   # room for the outermost star's ring and label
	var pts: Array[Vector3] = [Vector3(0, 0, 0), Vector3(g.x, 0, 0), Vector3(g.x, 0, g.y), Vector3(0, 0, g.y)]
	for sid in systems3d:
		pts.append(systems3d[sid]["pos"])
	yaw = 0.0
	pitch = PITCH_DEFAULT
	_focus_h = 0.0
	cam = Vector2(g) * 0.5 * CELL
	zoom = clampf(free.size.x / (maxf(float(g.x), 1.0) * CELL), ZOOM_MIN, ZOOM_MAX)
	for i in 10:
		_apply_camera()
		var lo := Vector2(INF, INF)
		var hi := Vector2(-INF, -INF)
		for w in pts:
			var p := world_to_screen(w)
			lo = lo.min(p)
			hi = hi.max(p)
		var box := hi - lo
		var k := minf((free.size.x - pad.x * 2.0) / maxf(box.x, 1.0), (free.size.y - pad.y * 2.0) / maxf(box.y, 1.0))
		zoom = clampf(zoom * clampf(k, 0.5, 2.0), ZOOM_MIN, 1.6)
		_apply_camera()
		# Re-measure after the zoom change, then slide the focus so the box is centred in the free rectangle.
		lo = Vector2(INF, INF)
		hi = Vector2(-INF, -INF)
		for w in pts:
			var p2 := world_to_screen(w)
			lo = lo.min(p2)
			hi = hi.max(p2)
		var want := free.get_center()
		var have := (lo + hi) * 0.5
		cam += screen_to_map(have) - screen_to_map(want)
		if absf(k - 1.0) < 0.004 and have.distance_to(want) < 1.0:
			break
	_target_cam = cam
	_target_zoom = zoom
	_target_yaw = yaw
	_target_pitch = pitch
	_animating = false
	_update_focus_height(true)
	_apply_camera()

func _apply_camera() -> void:
	if _camera == null:
		return
	var f := Vector3(cam.x / CELL, _focus_h, cam.y / CELL)
	var d := camera_distance()
	var off := Vector3(sin(yaw) * cos(pitch), sin(pitch), cos(yaw) * cos(pitch)) * d
	_camera.near = clampf(d * 0.01, 0.001, 0.5)
	_camera.far = d + 400.0
	_camera.look_at_from_position(f + off, f, Vector3.UP)
	_vp_size = size if size.x > 0.0 else Vector2(1920, 1080)
	_view_proj = _camera.get_camera_projection() * Projection(_camera.global_transform.affine_inverse())
	_redraw_overlay()

## Marks the static overlay for redraw (data, selection or layer change).
func mark_overlay_dirty() -> void:
	_overlay_dirty = true

## The static overlay redraws only when the camera moved or the data changed.
func _redraw_overlay() -> void:
	if _overlay == null:
		return
	var sig := [cam, zoom, yaw, pitch, snappedf(_focus_h, 0.0005), size, app.selection.get("cell", -1) if app else -1]
	if _overlay_dirty or sig != _overlay_sig:
		_overlay_sig = sig
		_overlay_dirty = false
		_overlay.queue_redraw()

## The focus rides on the star under it at sector and system zoom, so a close-up of a star above the plane is
## centred on the star; at galaxy zoom it settles back to the plane.
func _update_focus_height(snap: bool, delta: float = 0.0) -> void:
	var target := 0.0
	if zoom >= LEVEL_SECTOR * 0.8 and not _cell_system.is_empty():
		var g := grid_size()
		var cx := int(floor(cam.x / CELL))
		var cy := int(floor(cam.y / CELL))
		if cx >= 0 and cy >= 0 and cx < g.x and cy < g.y:
			var cell := cy * g.x + cx
			if _cell_system.has(cell):
				var a := cell_anchor(cell)
				var w := clampf((zoom - LEVEL_SECTOR * 0.8) / (LEVEL_SECTOR * 1.5), 0.0, 1.0)
				target = a.y * w
	_focus_h = target if snap else lerpf(_focus_h, target, clampf(delta * 5.0, 0.0, 1.0))

func _process(delta: float) -> void:
	_time += delta
	if is_visible_in_tree() and app and not app.has_modal():
		_key_camera(delta)
	if _animating:
		var k: float = clamp(1.0 - exp(-delta * 6.5), 0.0, 0.2)  # capped: a long frame never jumps the flight
		cam = cam.lerp(_target_cam, k)
		zoom = exp(lerpf(log(zoom), log(_target_zoom), k))
		yaw = lerp_angle(yaw, _target_yaw, k)
		pitch = lerpf(pitch, _target_pitch, k)
		if cam.distance_to(_target_cam) < 0.5 and absf(log(zoom / _target_zoom)) < 0.004:
			cam = _target_cam
			zoom = _target_zoom
			_animating = false
	_update_focus_height(false, delta)
	_apply_camera()
	_update_lod()
	_redraw_overlay()
	_overlay_fx.queue_redraw()

func _key_camera(delta: float) -> void:
	var gf := get_viewport().gui_get_focus_owner()
	if gf is LineEdit or gf is TextEdit:
		return
	if Input.is_key_pressed(KEY_CTRL) or Input.is_key_pressed(KEY_ALT):
		return
	var pan := Vector2.ZERO
	if Input.is_physical_key_pressed(KEY_A) or Input.is_physical_key_pressed(KEY_LEFT):
		pan.x -= 1
	if Input.is_physical_key_pressed(KEY_D) or Input.is_physical_key_pressed(KEY_RIGHT):
		pan.x += 1
	if Input.is_physical_key_pressed(KEY_W) or Input.is_physical_key_pressed(KEY_UP):
		pan.y -= 1
	if Input.is_physical_key_pressed(KEY_S) or Input.is_physical_key_pressed(KEY_DOWN):
		pan.y += 1
	var rot := 0.0
	if Input.is_physical_key_pressed(KEY_Q):
		rot -= 1
	if Input.is_physical_key_pressed(KEY_E):
		rot += 1
	var tilt := 0.0
	if Input.is_physical_key_pressed(KEY_PAGEUP):
		tilt += 1
	if Input.is_physical_key_pressed(KEY_PAGEDOWN):
		tilt -= 1
	if pan == Vector2.ZERO and rot == 0.0 and tilt == 0.0:
		return
	_animating = false
	var step := pan.normalized() * 700.0 * delta / zoom
	cam += step.rotated(-yaw)
	yaw += rot * 1.6 * delta
	pitch = clampf(pitch + tilt * 1.0 * delta, PITCH_MIN, PITCH_MAX)

func _notification(what: int) -> void:
	if what == NOTIFICATION_RESIZED and _camera:
		_apply_camera()

# ---------------------------------------------------------------------------------------------
# Data -> scene
# ---------------------------------------------------------------------------------------------
## Rebuilds stars, fields and anomalies from the current view.
func rebuild() -> void:
	var v := view()
	if v.is_empty():
		return
	layers = app.map_layers
	var g := grid_size()
	_seed = int(v["meta"].get("seed", 0))
	_build_stars(v, g)
	_build_disc(g)
	_build_territory(v, g)
	_build_fog(v, g)
	_supply_for = -1
	update_supply()
	_build_features(v)
	_overlay_dirty = true
	_update_focus_height(true)
	_apply_camera()
	_update_lod()

func _build_disc(g: Vector2i) -> void:
	var pm := PlaneMesh.new()
	pm.size = Vector2(g) * 3.0
	_disc.mesh = pm
	_disc.position = Vector3(g.x * 0.5, -GalaxyLayout.DISC_HALF - 0.6, g.y * 0.5)
	var m := ShaderMaterial.new()
	m.shader = load("res://scenes/galaxy/shaders/galaxy_disc.gdshader")
	m.set_shader_parameter("cells", Vector2(g))
	# Compatibility (the web build) blends 8-bit sRGB, so the faint additive dust that Forward+ accumulates in HDR floats all but
	# vanishes: the disc is drawn stronger there (F1 web parity, compared frame for frame with the desktop).
	m.set_shader_parameter("strength", COMPAT_DISC_STRENGTH if PlanetTextures.compat() else 1.0)
	if PlanetTextures.compat():
		m.set_shader_parameter("cool", COMPAT_DISC_COOL)
	m.render_priority = -5
	_disc.material_override = m

## Star colour for the impostor. Compatibility has no HDR to roll the overexposed core off, so every star clips toward white:
## the colour is pushed away from grey there to keep the red, orange, yellow and blue stars readable (F1 web parity).
func _star_tint(c: Color) -> Color:
	if not PlanetTextures.compat():
		return c
	var l := c.r * 0.3 + c.g * 0.59 + c.b * 0.11
	return Color(clampf(l + (c.r - l) * COMPAT_STAR_SATURATION, 0.0, 1.0), clampf(l + (c.g - l) * COMPAT_STAR_SATURATION, 0.0, 1.0), clampf(l + (c.b - l) * COMPAT_STAR_SATURATION, 0.0, 1.0))

func _build_stars(v: Dictionary, g: Vector2i) -> void:
	var keep := {}
	_cell_system.clear()
	for s in v.get("systems", []):
		var sid := int(s["id"])
		var x := int(s["x"])
		var y := int(s["y"])
		keep[sid] = true
		_cell_system[y * g.x + x] = sid
		if systems3d.has(sid) and systems3d[sid]["cell"] == y * g.x + x and (not s.has("starType") or int(s["starType"]) == int(systems3d[sid]["type"])):
			systems3d[sid]["system"] = s
			continue
		if systems3d.has(sid):
			(systems3d[sid]["node"] as Node).queue_free()
		var pos := GalaxyLayout.system_position(x, y, g, _seed)
		var ti := int(s["starType"]) if s.has("starType") else GalaxyLayout.star_type(x, y, _seed)   # schema 24: a race home star is the core's
		var st: Dictionary = GalaxyLayout.STAR_TYPES[ti]
		var node := Node3D.new()
		node.name = "Star%d" % sid
		node.position = pos
		_stars.add_child(node)
		var imp := MeshInstance3D.new()
		var q := QuadMesh.new()
		q.size = Vector2.ONE
		imp.mesh = q
		imp.extra_cull_margin = 16.0
		imp.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		var im := ShaderMaterial.new()
		im.shader = load("res://scenes/galaxy/shaders/star_impostor.gdshader")
		im.set_shader_parameter("star_color", _star_tint(st["color"]))
		im.set_shader_parameter("sphere_radius", st["radius"])
		im.set_shader_parameter("spike", st["spike"])
		im.set_shader_parameter("twinkle_seed", float(sid))
		im.render_priority = 2
		imp.material_override = im
		node.add_child(imp)
		var sph := MeshInstance3D.new()
		var sm := SphereMesh.new()
		sm.radius = st["radius"]
		sm.height = st["radius"] * 2.0
		sm.radial_segments = 48
		sm.rings = 24
		sph.mesh = sm
		sph.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		var ss := ShaderMaterial.new()
		ss.shader = load("res://scenes/galaxy/shaders/star_surface.gdshader")
		ss.set_shader_parameter("star_color", st["color"])
		ss.set_shader_parameter("seed", float(sid))
		sph.material_override = ss
		sph.visible = false
		node.add_child(sph)
		systems3d[sid] = {"pos": pos, "type": ti, "cell": y * g.x + x, "node": node, "impostor": imp, "sphere": sph, "planets": null, "system": s}
	for sid in systems3d.keys():
		if not keep.has(sid):
			(systems3d[sid]["node"] as Node).queue_free()
			systems3d.erase(sid)
	if AppLoader.diag():
		var unch := 0
		for s in v.get("systems", []):
			if not AppSession.is_charted(s):
				unch += 1
		print("DIAG galaxy stars=%d uncharted=%d grid=%dx%d" % [systems3d.size(), unch, g.x, g.y])

## Planets of one system: built on first approach, kept afterwards. Textured from L7 sets when present.
func _build_planets(sid: int) -> void:
	var e: Dictionary = systems3d[sid]
	var s: Dictionary = e["system"]
	var root := Node3D.new()
	root.name = "Planets"
	(e["node"] as Node3D).add_child(root)
	e["planets"] = root
	var planets: Array = s.get("planets", [])
	var x := int(s["x"])
	var y := int(s["y"])
	var st: Dictionary = GalaxyLayout.STAR_TYPES[int(e["type"])]
	var colonised := int(s.get("owner", 0)) != 0
	for i in planets.size():
		var p: Dictionary = planets[i]
		var cls := str(p.get("class", "K"))
		var orb := GalaxyLayout.orbit(i, planets.size(), x, y, _seed)
		var pivot := Node3D.new()
		pivot.name = "Orbit%d" % i
		pivot.rotation.y = orb.y
		pivot.set_meta("phase", orb.y)
		pivot.set_meta("speed", GalaxyLayout.orbit_speed(orb.x))
		pivot.set_meta("radius", orb.x)
		pivot.set_meta("planet", p)
		root.add_child(pivot)
		if cls == "Asteroids":
			pivot.add_child(_asteroid_belt(orb.x, sid * 31 + i))
			continue
		var mi := MeshInstance3D.new()
		mi.name = "Planet"
		var r: float = PLANET_RADIUS.get(cls, 0.011)
		var sm := SphereMesh.new()
		sm.radius = r
		sm.height = r * 2.0
		sm.radial_segments = 64 if _volumetric else 32
		sm.rings = 32 if _volumetric else 16
		mi.mesh = sm
		mi.position = Vector3(orb.x, 0.0, 0.0)
		mi.rotation.z = deg_to_rad(GalaxyLayout.hash01(x, y, _seed, 40 + i) * 30.0 - 15.0)
		var m := ShaderMaterial.new()
		m.shader = load("res://scenes/galaxy/shaders/planet_globe.gdshader")
		_planet_look(m, cls)
		m.set_shader_parameter("seed", float(sid * 7 + i))
		m.set_shader_parameter("star_pos", e["pos"])
		m.set_shader_parameter("star_color", (st["color"] as Color).lerp(Color.WHITE, 0.55))
		m.set_shader_parameter("colonised", 1.0 if colonised and int(p.get("capacity", 0)) > 0 else 0.0)
		mi.material_override = m
		pivot.add_child(mi)
		# Photoreal set (art wave 2) when the class has one, streamed in over the procedural globe above.
		PlanetBody.attach(mi, cls, {"star_pos": e["pos"], "variant": sid * 7 + i,
			"sun_color": (st["color"] as Color).lerp(Color.WHITE, 0.55), "inhabited": colonised and int(p.get("capacity", 0)) > 0})
		if cls == "GasGiant" and GalaxyLayout.hash01(x, y, _seed, 60 + i) < 0.6:
			mi.add_child(_ring(r))

static func _planet_look(m: ShaderMaterial, cls: String) -> void:
	# Anchored on the art bible section 8 class table.
	var look := {
		"M": [0, Color("#1d4f8c"), Color("#4f8a46"), Color("#b39b74"), Color(0.55, 0.75, 1.0), 0.5, 0.82, 0.55],
		"O": [0, Color("#1f5fb0"), Color("#5c8f5a"), Color("#c8b88a"), Color(0.4, 0.65, 1.0), 0.66, 0.86, 0.75],
		"P": [0, Color("#6f8fb0"), Color("#cfe3f0"), Color("#ffffff"), Color(0.85, 0.92, 1.0), 0.42, 0.45, 0.3],
		"L": [0, Color("#2a5a4a"), Color("#3f7a2a"), Color("#7fa35a"), Color(0.7, 0.9, 0.5), 0.4, 0.9, 0.6],
		"H": [3, Color("#8a5a32"), Color("#d0a060"), Color("#e8c890"), Color(1.0, 0.7, 0.45), 0.0, 0.93, 0.1],
		"K": [3, Color("#5a524c"), Color("#9a8a80"), Color("#c0b4aa"), Color(0, 0, 0), 0.0, 0.95, 0.0],
		"Y": [2, Color("#2a1410"), Color("#5a2a1c"), Color("#c0503a"), Color(1.0, 0.45, 0.25), 0.0, 0.99, 0.3],
		"GasGiant": [1, Color("#8a5a32"), Color("#d9a86a"), Color("#f0e0c0"), Color(0.75, 0.85, 1.0), 0.0, 0.99, 0.0],
	}
	var l: Array = look.get(cls, look["K"])
	m.set_shader_parameter("kind", l[0])
	m.set_shader_parameter("c_low", l[1])
	m.set_shader_parameter("c_mid", l[2])
	m.set_shader_parameter("c_high", l[3])
	m.set_shader_parameter("atmosphere", l[4])
	m.set_shader_parameter("atmo_strength", 0.0 if cls == "K" else 1.0)
	m.set_shader_parameter("sea_level", l[5])
	m.set_shader_parameter("ice", l[6])
	m.set_shader_parameter("cloud_amount", l[7])

func _ring(r: float) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	mi.name = "Ring"
	var tm := TorusMesh.new()
	tm.inner_radius = r * 1.5
	tm.outer_radius = r * 2.3
	tm.rings = 48
	tm.ring_segments = 4
	mi.mesh = tm
	mi.scale = Vector3(1.0, 0.04, 1.0)
	var m := StandardMaterial3D.new()
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	m.albedo_color = Color(0.85, 0.75, 0.6, 0.45)
	mi.material_override = m
	return mi

func _asteroid_belt(radius: float, seed_: int) -> MultiMeshInstance3D:
	var mmi := MultiMeshInstance3D.new()
	mmi.name = "Belt"
	var mm := MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	var rock := SphereMesh.new()
	rock.radius = 0.0016
	rock.height = 0.0026
	rock.radial_segments = 6
	rock.rings = 3
	mm.mesh = rock
	var n := 160 if _volumetric else 80
	mm.instance_count = n
	for i in n:
		var a := GalaxyLayout.hash01(i, seed_, 1, 7) * TAU
		var rr := radius * (0.92 + 0.16 * GalaxyLayout.hash01(i, seed_, 2, 7))
		var hh := (GalaxyLayout.hash01(i, seed_, 3, 7) - 0.5) * radius * 0.06
		var sc := 0.5 + 1.2 * GalaxyLayout.hash01(i, seed_, 4, 7)
		var b := Basis().rotated(Vector3.UP, a * 3.0).scaled(Vector3(sc, sc * 0.8, sc))
		mm.set_instance_transform(i, Transform3D(b, Vector3(cos(a) * rr, hh, sin(a) * rr)))
	mmi.multimesh = mm
	var m := StandardMaterial3D.new()
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.albedo_color = Color(0.42, 0.4, 0.38)
	mmi.material_override = m
	return mmi

## Per frame: star sphere and planet visibility by screen size, planet orbits, texture streaming.
func _update_lod() -> void:
	if _camera == null or systems3d.is_empty():
		return
	var cp := _camera.global_position
	var reduced: bool = app != null and bool(app.settings.get("reducedMotion", false))
	var t := 0.0 if reduced else _time
	var focus := Vector3(cam.x / CELL, _focus_h, cam.y / CELL)
	var built := false   # at most one system's planets are built per frame, so zooming in never stalls
	for sid in systems3d:
		var e: Dictionary = systems3d[sid]
		var pos: Vector3 = e["pos"]
		var dist := cp.distance_to(pos)
		var ppu := pixels_per_unit(dist)
		var st: Dictionary = GalaxyLayout.STAR_TYPES[int(e["type"])]
		(e["sphere"] as Node3D).visible = float(st["radius"]) * ppu > 2.5
		var near := zoom >= LEVEL_SECTOR * 2.0 and Vector2(pos.x - focus.x, pos.z - focus.z).length() < 5.0
		if near and e["planets"] == null and not built and AppSession.is_charted(e["system"]):
			_build_planets(sid)
			built = true
		var pr: Node3D = e["planets"]
		if pr:
			var show := near and 0.012 * ppu > 0.8
			pr.visible = show
			if show:
				for pivot in pr.get_children():
					pivot.rotation.y = float(pivot.get_meta("phase")) + t * float(pivot.get_meta("speed"))

## True while planet textures are still streaming in (captures wait for this).
func textures_pending() -> bool:
	return PlanetBody.pending > 0

func _set_field(mi: MeshInstance3D, img: Image, g: Vector2i) -> void:
	var tex := ImageTexture.create_from_image(img)
	var pm := PlaneMesh.new()
	pm.size = Vector2(g) + Vector2(2, 2)
	mi.mesh = pm
	mi.position = Vector3(g.x * 0.5, 0.0, g.y * 0.5)
	var m := mi.material_override as ShaderMaterial
	m.set_shader_parameter("field_tex", tex)
	m.set_shader_parameter("cells", Vector2(g))

func _build_territory(v: Dictionary, g: Vector2i) -> void:
	var img := Image.create(g.x, g.y, false, Image.FORMAT_RGBAF)
	var best := PackedFloat32Array()
	best.resize(g.x * g.y)
	for s in v.get("systems", []):
		var owner := int(s.get("owner", 0))
		var member := int(s.get("minor", {}).get("memberOf", 0)) if s.has("minor") else 0
		if owner == 0 and member == 0:
			continue
		var col := Lcars.owner_color(owner if owner != 0 else member)
		var sx := int(s["x"])
		var sy := int(s["y"])
		for dy in range(-3, 4):
			for dx in range(-3, 4):
				var x := sx + dx
				var y := sy + dy
				if x < 0 or y < 0 or x >= g.x or y >= g.y:
					continue
				var d := sqrt(dx * dx + dy * dy)
				var strength: float = clamp(1.15 - d * 0.28, 0.0, 1.0)
				var i := y * g.x + x
				if strength > best[i]:
					best[i] = strength
					img.set_pixel(x, y, Color(col.r * strength, col.g * strength, col.b * strength, strength))
	_set_field(_territory, img, g)
	var m := _territory.material_override as ShaderMaterial
	# The wash reads as the benchmark's soft coloured fill inside the bright border at whole-galaxy zoom.
	m.set_shader_parameter("fill", 0.2)
	m.set_shader_parameter("edge", 0.6)
	_territory.visible = layers.get("territory", true)

func _build_fog(v: Dictionary, g: Vector2i) -> void:
	var fog: String = v.get("fog", "")
	var img := Image.create(g.x, g.y, false, Image.FORMAT_RGBAF)
	for i in fog.length():
		var a := 0.0
		if fog[i] == "0":
			a = 0.82
		elif fog[i] == "1":
			a = 0.32
		img.set_pixel(i % g.x, i / g.x, Color(0, 0, 0, a))
	_set_field(_fog, img, g)
	_fog.visible = layers.get("fog", true)

func update_supply() -> void:
	if app == null:
		return
	_overlay_dirty = true
	var fid := int(app.selection.get("fleetId", 0))
	var fleet := app.find_by_id("fleets", fid)
	if fleet.is_empty() or not layers.get("supply", true):
		_supply.visible = false
		_supply_for = -1
		return
	if _supply_for == fid:
		_supply.visible = true
		return
	_supply_for = fid
	var s: Variant = app.core.request("supply", {"fleetId": fid})
	if not s is String:
		_supply.visible = false
		return
	var g := grid_size()
	var img := Image.create(g.x, g.y, false, Image.FORMAT_RGBAF)
	var col := Lcars.accent()
	for i in (s as String).length():
		if s[i] == "1":
			img.set_pixel(i % g.x, i / g.x, Color(col.r, col.g, col.b, 1.0))
	_set_field(_supply, img, g)
	var m := _supply.material_override as ShaderMaterial
	m.set_shader_parameter("fill", 0.02)
	m.set_shader_parameter("edge", 0.55)
	m.set_shader_parameter("noise_amount", 0.02)
	_supply.visible = true

## One node per anomaly sector, in feature order: a ray-marched volume on Forward+, a billboard on Compatibility.
## The neutron star is not a cloud: it gets a pulsing white star (decor) and its beam glyph in the overlay.
func _build_features(v: Dictionary) -> void:
	for c in _features.get_children():
		_features.remove_child(c)
		c.queue_free()
	for c in _decor.get_children():
		_decor.remove_child(c)
		c.queue_free()
	var g := grid_size()
	var vol_shader: Shader = load("res://scenes/galaxy/shaders/anomaly_volume.gdshader")
	var bb_shader: Shader = load("res://scenes/galaxy/shaders/anomaly_billboard.gdshader")
	for f in v.get("features", []):
		var t := str(f["type"])
		var spots := []
		if f.has("cells") and f["cells"] is Array and f["cells"].size() > 0:
			for c in f["cells"]:
				spots.append(Vector2i(int(c) % g.x, int(c) / g.x))
		else:
			spots.append(Vector2i(int(f["x"]), int(f["y"])))
		for p in spots:
			var tint := Color(0.55, 0.35, 0.95)
			var sz := 2.3
			var intensity := 0.85
			var swirl := 0.0
			var crackle := 0.0
			var disc := 0.0
			var core := 0.0
			var flat_ := 0.45
			match t:
				"Nebula":
					tint = [Color(0.6, 0.3, 0.9), Color(0.3, 0.45, 0.95), Color(0.85, 0.35, 0.6)][int(f["id"]) % 3]
				"IonStorm":
					tint = Color(0.35, 0.75, 1.0)
					crackle = 1.0
					sz = 1.7
				"SubspaceRift":
					tint = Color(0.9, 0.3, 0.5)
					swirl = 1.2
					sz = 1.6
					flat_ = 0.6
				"Wormhole":
					tint = Color(0.45, 0.85, 1.0)
					swirl = 1.6
					sz = 1.2
					intensity = 1.0
					disc = 0.6
					flat_ = 1.0
				"BlackHole":
					tint = Color(1.0, 0.6, 0.3)
					swirl = 2.0
					sz = 1.5
					intensity = 0.9
					disc = 1.0
					core = 1.0
					flat_ = 1.0
				"NeutronStar":
					_decor.add_child(_neutron_star(Vector3(p.x + 0.5, 0.0, p.y + 0.5), int(f["id"])))
					continue
				_:
					continue
			var mi := MeshInstance3D.new()
			mi.name = "%s%d" % [t, int(f["id"])]
			mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
			var mat := ShaderMaterial.new()
			mat.shader = vol_shader if _volumetric else bb_shader
			mat.set_shader_parameter("tint", tint)
			mat.set_shader_parameter("seed", float(int(f["id"]) * 3 + p.x + p.y * 7))
			mat.set_shader_parameter("intensity", intensity)
			mat.set_shader_parameter("swirl", swirl)
			mat.set_shader_parameter("crackle", crackle)
			mat.set_shader_parameter("disc", disc)
			mat.set_shader_parameter("core", core)
			mat.render_priority = 0
			mi.material_override = mat
			var centre := Vector3(p.x + 0.5, 0.0, p.y + 0.5)
			if _volumetric:
				var sm := SphereMesh.new()
				sm.radius = 1.0
				sm.height = 2.0
				sm.radial_segments = 24
				sm.rings = 12
				mi.mesh = sm
				mi.scale = Vector3(sz * 0.5, sz * 0.5 * flat_, sz * 0.5)
				if disc > 0.0:
					mi.rotation = Vector3(deg_to_rad(18.0), float(int(f["id"])) * 1.3, 0.0)
			else:
				var q := QuadMesh.new()
				q.size = Vector2.ONE
				mi.mesh = q
				mi.scale = Vector3.ONE * sz
				mi.extra_cull_margin = sz
			mi.position = centre
			_features.add_child(mi)

func _neutron_star(pos: Vector3, id: int) -> Node3D:
	var imp := MeshInstance3D.new()
	imp.name = "NeutronStar%d" % id
	var q := QuadMesh.new()
	q.size = Vector2.ONE
	imp.mesh = q
	imp.extra_cull_margin = 16.0
	var im := ShaderMaterial.new()
	im.shader = load("res://scenes/galaxy/shaders/star_impostor.gdshader")
	im.set_shader_parameter("star_color", Color(0.75, 0.9, 1.0))
	im.set_shader_parameter("sphere_radius", 0.01)
	im.set_shader_parameter("spike", 2.0)
	im.set_shader_parameter("brightness", 1.3)
	im.set_shader_parameter("twinkle_seed", float(id))
	im.render_priority = 2
	imp.material_override = im
	imp.position = pos
	return imp

func set_layer(key: String, on: bool) -> void:
	app.map_layers[key] = on
	layers = app.map_layers
	_territory.visible = layers.get("territory", true)
	_fog.visible = layers.get("fog", true)
	_supply_for = -1
	update_supply()

# ---------------------------------------------------------------------------------------------
# Input
# ---------------------------------------------------------------------------------------------
func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		var mb := event as InputEventMouseButton
		if mb.button_index == MOUSE_BUTTON_WHEEL_UP or mb.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			if mb.pressed:
				zoom_at(mb.position, 1.15 if mb.button_index == MOUSE_BUTTON_WHEEL_UP else 1.0 / 1.15)
			accept_event()
		elif mb.button_index in [MOUSE_BUTTON_LEFT, MOUSE_BUTTON_MIDDLE, MOUSE_BUTTON_RIGHT]:
			if mb.pressed:
				_drag_button = mb.button_index
				_drag_moved = 0.0
				_last_mouse = mb.position
				if mb.double_click and mb.button_index == MOUSE_BUTTON_LEFT:
					var c := cell_at(mb.position)
					if c >= 0:
						cell_clicked.emit(c, true)
					_drag_button = 0
			else:
				if _drag_button == mb.button_index and _drag_moved < 6.0:
					var c2 := cell_at(mb.position)
					if c2 >= 0:
						if mb.button_index == MOUSE_BUTTON_LEFT:
							cell_clicked.emit(c2, false)
						elif mb.button_index == MOUSE_BUTTON_RIGHT:
							cell_context.emit(c2, mb.position)
				if _drag_button == mb.button_index:
					_drag_button = 0
			accept_event()
	elif event is InputEventMouseMotion:
		var mm := event as InputEventMouseMotion
		if _drag_button != 0:
			_drag_moved += mm.relative.length()
			if _drag_moved >= 6.0:
				_animating = false
				if _drag_button == MOUSE_BUTTON_RIGHT:
					orbit(-mm.relative.x * 0.006, mm.relative.y * 0.004)
				else:
					cam += screen_to_map(mm.position - mm.relative) - screen_to_map(mm.position)
					_apply_camera()
		var c4 := cell_at(mm.position)
		if c4 != hover_cell:
			hover_cell = c4
		hover_changed.emit(c4, mm.position)

## Zooms by `factor`, keeping the point under `screen_pos` fixed.
func zoom_at(screen_pos: Vector2, factor: float) -> void:
	var before := screen_to_map(screen_pos)
	zoom = clampf(zoom * factor, ZOOM_MIN, ZOOM_MAX)
	_animating = false
	_update_focus_height(true)
	_apply_camera()
	cam += before - screen_to_map(screen_pos)
	_apply_camera()

## Orbits the camera around the focus (radians).
func orbit(d_yaw: float, d_pitch: float) -> void:
	yaw += d_yaw
	pitch = clampf(pitch + d_pitch, PITCH_MIN, PITCH_MAX)
	_animating = false
	_apply_camera()

func _unhandled_key_input(event: InputEvent) -> void:
	var k := event as InputEventKey
	if k == null or not k.pressed or k.echo or not is_visible_in_tree() or app.has_modal():
		return
	if k.keycode == KEY_F and not k.ctrl_pressed:
		frame_known()
		get_viewport().set_input_as_handled()
	elif k.keycode == KEY_H:
		var cols: Array = view().get("colonies", [])
		if cols.size() > 0:
			var s := app.find_by_id("systems", int(cols[0]["systemId"]))
			focus_cell(int(s["x"]), int(s["y"]), FLY_SECTOR)
		get_viewport().set_input_as_handled()
	elif k.keycode in [KEY_EQUAL, KEY_KP_ADD]:
		zoom_at(size / 2.0, 1.25)
	elif k.keycode in [KEY_MINUS, KEY_KP_SUBTRACT]:
		zoom_at(size / 2.0, 1.0 / 1.25)

