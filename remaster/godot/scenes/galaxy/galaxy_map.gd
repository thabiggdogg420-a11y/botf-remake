class_name GalaxyMap
extends Control
## The star chart: parallax starfield, nebula features, soft fog of war, territory tint, supply
## range, animated routes with ETAs, fleet/contact/station markers, anomalies and wormholes.
## Pan with drag, zoom with the wheel, left-click selects, right-click opens the order menu.
## Port of remaster/web/src/map/renderer.ts + GalaxyMap.tsx.

signal cell_clicked(cell: int, double: bool)
signal cell_context(cell: int, screen_pos: Vector2)
signal hover_changed(cell: int, screen_pos: Vector2)

const CELL := 84.0
const STAR_COLORS := [Color("#fff4e0"), Color("#ffe3a3"), Color("#ffc27a"), Color("#ff9a6a"), Color("#bcd4ff"), Color("#e8eeff")]

var app: App
var cam := Vector2.ZERO           # map-space point at the centre of the view
var zoom := 1.0
var hover_cell := -1
var layers := {}

var _starfield: ColorRect
var _world: Node2D
var _territory: Sprite2D
var _supply: Sprite2D
var _fog: Sprite2D
var _features: Node2D
var _drawer: MapDrawer
var _labels: MapLabels
var _supply_for := -1
var _dragging := false
var _drag_moved := 0.0
var _glow: Texture2D
var _target_cam := Vector2.ZERO
var _target_zoom := 1.0
var _animating := false

func _ready() -> void:
	clip_contents = true
	mouse_filter = Control.MOUSE_FILTER_STOP
	_glow = _make_glow()
	_starfield = ColorRect.new()
	_starfield.set_anchors_preset(Control.PRESET_FULL_RECT)
	_starfield.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var sm := ShaderMaterial.new()
	sm.shader = load("res://scenes/galaxy/starfield.gdshader")
	_starfield.material = sm
	add_child(_starfield)
	_world = Node2D.new()
	add_child(_world)
	_territory = _field_sprite(0)
	_features = Node2D.new()
	_world.add_child(_features)
	_world.move_child(_features, 0)
	_supply = _field_sprite(0)
	_drawer = MapDrawer.new()
	_drawer.map = self
	_world.add_child(_drawer)
	_fog = _field_sprite(1)
	_labels = MapLabels.new()
	_labels.map = self
	_labels.set_anchors_preset(Control.PRESET_FULL_RECT)
	_labels.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_labels)

func _field_sprite(mode: int) -> Sprite2D:
	var s := Sprite2D.new()
	s.centered = false
	s.texture_filter = CanvasItem.TEXTURE_FILTER_LINEAR
	var m := ShaderMaterial.new()
	m.shader = load("res://scenes/galaxy/field.gdshader")
	m.set_shader_parameter("mode", mode)
	s.material = m
	_world.add_child(s)
	return s

func _make_glow() -> Texture2D:
	var g := Gradient.new()
	g.set_color(0, Color(1, 1, 1, 1))
	g.set_color(1, Color(1, 1, 1, 0))
	g.add_point(0.18, Color(1, 1, 1, 0.55))
	g.add_point(0.45, Color(1, 1, 1, 0.12))
	var t := GradientTexture2D.new()
	t.gradient = g
	t.fill = GradientTexture2D.FILL_RADIAL
	t.fill_from = Vector2(0.5, 0.5)
	t.fill_to = Vector2(1.0, 0.5)
	t.width = 128
	t.height = 128
	return t

func view() -> Dictionary:
	return app.view if app else {}

func grid_size() -> Vector2i:
	var m: Dictionary = view().get("meta", {})
	return Vector2i(int(m.get("width", 1)), int(m.get("height", 1)))

# ---------------------------------------------------------------------------------------------
# Camera
# ---------------------------------------------------------------------------------------------
func cell_center(x: float, y: float) -> Vector2:
	return Vector2((x + 0.5) * CELL, (y + 0.5) * CELL)

func map_to_screen(p: Vector2) -> Vector2:
	return (p - cam) * zoom + size / 2.0

func screen_to_map(p: Vector2) -> Vector2:
	return (p - size / 2.0) / zoom + cam

func cell_at(screen_pos: Vector2) -> int:
	var m := screen_to_map(screen_pos)
	var g := grid_size()
	var x := int(floor(m.x / CELL))
	var y := int(floor(m.y / CELL))
	if x < 0 or y < 0 or x >= g.x or y >= g.y:
		return -1
	return y * g.x + x

func focus_cell(x: int, y: int, z: float = -1.0, animate: bool = true) -> void:
	_target_cam = cell_center(x, y)
	_target_zoom = zoom if z <= 0 else z
	if animate and not app.settings.get("reducedMotion", false):
		_animating = true
	else:
		cam = _target_cam
		zoom = _target_zoom
		_apply_camera()

## Frames every surveyed or sensed sector.
func frame_known() -> void:
	var v := view()
	if v.is_empty():
		return
	var g := grid_size()
	var fog: String = v["fog"]
	var lo := Vector2(INF, INF)
	var hi := Vector2(-INF, -INF)
	for i in fog.length():
		if fog[i] != "0":
			var p := Vector2(i % g.x, i / g.x)
			lo = lo.min(p)
			hi = hi.max(p)
	if lo.x == INF:
		lo = Vector2.ZERO
		hi = Vector2(g.x - 1, g.y - 1)
	var span := (hi - lo + Vector2(2.5, 2.5)) * CELL
	cam = (lo + hi + Vector2.ONE) * 0.5 * CELL
	zoom = clamp(min(size.x / span.x, size.y / span.y), 0.35, 1.6)
	_target_cam = cam
	_target_zoom = zoom
	_apply_camera()

func _apply_camera() -> void:
	_world.position = size / 2.0 - cam * zoom
	_world.scale = Vector2(zoom, zoom)
	var sm := _starfield.material as ShaderMaterial
	sm.set_shader_parameter("offset", cam * zoom)
	sm.set_shader_parameter("resolution", size)
	sm.set_shader_parameter("accent", Lcars.accent())
	_labels.queue_redraw()

func _process(delta: float) -> void:
	if _animating:
		var k: float = clamp(delta * 7.0, 0.0, 1.0)
		cam = cam.lerp(_target_cam, k)
		zoom = lerp(zoom, _target_zoom, k)
		if cam.distance_to(_target_cam) < 0.5 and abs(zoom - _target_zoom) < 0.002:
			cam = _target_cam
			zoom = _target_zoom
			_animating = false
		_apply_camera()
	_drawer.queue_redraw()
	_labels.queue_redraw()

func _notification(what: int) -> void:
	if what == NOTIFICATION_RESIZED and _world:
		_apply_camera()

# ---------------------------------------------------------------------------------------------
# Data -> layers
# ---------------------------------------------------------------------------------------------
## Rebuilds textures and feature nodes from the current view.
func rebuild() -> void:
	var v := view()
	if v.is_empty():
		return
	layers = app.map_layers
	var g := grid_size()
	_build_territory(v, g)
	_build_fog(v, g)
	_supply_for = -1
	update_supply()
	_build_features(v)
	_apply_camera()

func _set_field(sprite: Sprite2D, img: Image, g: Vector2i) -> void:
	sprite.texture = ImageTexture.create_from_image(img)
	sprite.scale = Vector2(CELL, CELL)
	(sprite.material as ShaderMaterial).set_shader_parameter("cells", Vector2(g))

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
	var m := _territory.material as ShaderMaterial
	m.set_shader_parameter("fill", 0.12)
	m.set_shader_parameter("edge", 0.45)
	_territory.visible = layers.get("territory", true)

func _build_fog(v: Dictionary, g: Vector2i) -> void:
	var fog: String = v.get("fog", "")
	var img := Image.create(g.x, g.y, false, Image.FORMAT_RGBAF)
	for i in fog.length():
		var a := 0.0
		if fog[i] == "0":
			a = 0.9
		elif fog[i] == "1":
			a = 0.38
		img.set_pixel(i % g.x, i / g.x, Color(0, 0, 0, a))
	_set_field(_fog, img, g)
	_fog.visible = layers.get("fog", true)

func update_supply() -> void:
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
	var m := _supply.material as ShaderMaterial
	m.set_shader_parameter("fill", 0.05)
	m.set_shader_parameter("edge", 0.5)
	m.set_shader_parameter("noise_amount", 0.02)
	_supply.visible = true

func _build_features(v: Dictionary) -> void:
	for c in _features.get_children():
		c.queue_free()
	var shader: Shader = load("res://scenes/galaxy/nebula.gdshader")
	for f in v.get("features", []):
		var t := str(f["type"])
		var spots := []
		if f.has("cells") and f["cells"] is Array and f["cells"].size() > 0:
			for c in f["cells"]:
				spots.append(Vector2i(int(c) % grid_size().x, int(c) / grid_size().x))
		else:
			spots.append(Vector2i(int(f["x"]), int(f["y"])))
		for p in spots:
			var tint := Color(0.55, 0.35, 0.95)
			var sz := 2.3
			var intensity := 0.85
			var swirl := 0.0
			var crackle := 0.0
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
				"Wormhole":
					tint = Color(0.45, 0.85, 1.0)
					swirl = 1.6
					sz = 1.2
					intensity = 1.0
				"BlackHole":
					tint = Color(1.0, 0.6, 0.3)
					swirl = 2.0
					sz = 1.5
					intensity = 0.9
				_:
					continue
			var r := ColorRect.new()
			var mat := ShaderMaterial.new()
			mat.shader = shader
			mat.set_shader_parameter("tint", tint)
			mat.set_shader_parameter("seed", float(int(f["id"]) * 3 + p.x + p.y * 7))
			mat.set_shader_parameter("intensity", intensity)
			mat.set_shader_parameter("swirl", swirl)
			mat.set_shader_parameter("crackle", crackle)
			r.material = mat
			r.size = Vector2(CELL * sz, CELL * sz)
			r.position = cell_center(p.x, p.y) - r.size / 2.0
			r.mouse_filter = Control.MOUSE_FILTER_IGNORE
			_features.add_child(r)

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
				var before := screen_to_map(mb.position)
				zoom = clamp(zoom * (1.12 if mb.button_index == MOUSE_BUTTON_WHEEL_UP else 1.0 / 1.12), 0.3, 3.0)
				cam += before - screen_to_map(mb.position)
				_animating = false
				_apply_camera()
			accept_event()
		elif mb.button_index == MOUSE_BUTTON_LEFT or mb.button_index == MOUSE_BUTTON_MIDDLE:
			if mb.pressed:
				_dragging = true
				_drag_moved = 0.0
				if mb.double_click and mb.button_index == MOUSE_BUTTON_LEFT:
					var c := cell_at(mb.position)
					if c >= 0:
						cell_clicked.emit(c, true)
					_dragging = false
			else:
				if _dragging and _drag_moved < 6.0 and mb.button_index == MOUSE_BUTTON_LEFT:
					var c2 := cell_at(mb.position)
					if c2 >= 0:
						cell_clicked.emit(c2, false)
				_dragging = false
			accept_event()
		elif mb.button_index == MOUSE_BUTTON_RIGHT and mb.pressed:
			var c3 := cell_at(mb.position)
			if c3 >= 0:
				cell_context.emit(c3, mb.position)
			accept_event()
	elif event is InputEventMouseMotion:
		var mm := event as InputEventMouseMotion
		if _dragging:
			_drag_moved += mm.relative.length()
			cam -= mm.relative / zoom
			_animating = false
			_apply_camera()
		var c4 := cell_at(mm.position)
		if c4 != hover_cell:
			hover_cell = c4
		hover_changed.emit(c4, mm.position)

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
			focus_cell(int(s["x"]), int(s["y"]), 1.4)
		get_viewport().set_input_as_handled()
	elif k.keycode in [KEY_EQUAL, KEY_KP_ADD]:
		zoom = clamp(zoom * 1.2, 0.3, 3.0)
		_apply_camera()
	elif k.keycode in [KEY_MINUS, KEY_KP_SUBTRACT]:
		zoom = clamp(zoom / 1.2, 0.3, 3.0)
		_apply_camera()

# ---------------------------------------------------------------------------------------------
# Drawing (map space)
# ---------------------------------------------------------------------------------------------
class MapDrawer extends Node2D:
	var map: GalaxyMap

	func _draw() -> void:
		var v := map.view()
		if v.is_empty():
			return
		var g := map.grid_size()
		var C := GalaxyMap.CELL
		var t := Time.get_ticks_msec() / 1000.0
		var app := map.app
		var fog: String = v["fog"]
		var accent := Lcars.accent()
		# Grid
		if map.layers.get("grid", true):
			var gc := Color(0.55, 0.67, 0.9, 0.06)
			for x in range(g.x + 1):
				draw_line(Vector2(x * C, 0), Vector2(x * C, g.y * C), gc, 1.0)
			for y in range(g.y + 1):
				draw_line(Vector2(0, y * C), Vector2(g.x * C, y * C), gc, 1.0)
			draw_rect(Rect2(Vector2.ZERO, Vector2(g) * C), Color(accent, 0.25), false, 2.0)
		# Hover / selection cell
		var sel := int(app.selection.get("cell", -1))
		if map.hover_cell >= 0:
			draw_rect(Rect2(Vector2(map.hover_cell % g.x, map.hover_cell / g.x) * C, Vector2(C, C)), Color(1, 1, 1, 0.05))
		if sel >= 0:
			var r := Rect2(Vector2(sel % g.x, sel / g.x) * C, Vector2(C, C)).grow(-3)
			var pulse := 0.55 + 0.25 * sin(t * 3.0)
			draw_rect(r, Color(accent, pulse), false, 2.0)
		# Trade routes
		if map.layers.get("trade", true):
			for tr in v.get("tradeRoutes", []):
				var a := app.find_by_id("systems", int(tr["from"]))
				var b := app.find_by_id("systems", int(tr["to"]))
				if a.is_empty() or b.is_empty():
					continue
				_dashed(map.cell_center(a["x"], a["y"]), map.cell_center(b["x"], b["y"]), Color(Lcars.owner_color(int(tr["owner"])), 0.45), 2.0, 6.0, 8.0, t * 10.0)
		# Wormhole links
		for f in v.get("features", []):
			if f["type"] == "Wormhole" and f.has("exit") and int(f["id"]) < int(f.get("pairId", 0)):
				var a2 := map.cell_center(f["x"], f["y"])
				var b2 := map.cell_center(f["exit"]["x"], f["exit"]["y"])
				_arc_line(a2, b2, Color(0.45, 0.85, 1.0, 0.35), 2.0, t)
		# Systems
		for s in v.get("systems", []):
			var p := map.cell_center(s["x"], s["y"])
			var sc: Color = GalaxyMap.STAR_COLORS[int(s["id"]) % GalaxyMap.STAR_COLORS.size()]
			var tw := 1.0 + 0.06 * sin(t * 2.0 + int(s["id"]))
			draw_texture_rect(map._glow, Rect2(p - Vector2(46, 46) * tw, Vector2(92, 92) * tw), false, Color(sc, 0.55))
			draw_circle(p, 7.5, sc)
			draw_circle(p, 4.0, Color(1, 1, 1, 0.95))
			var owner := int(s.get("owner", 0))
			if owner != 0:
				var oc := Lcars.owner_color(owner)
				draw_arc(p, 17, 0, TAU, 40, oc, 2.5, true)
				if owner == app.player():
					draw_arc(p, 21, -t, -t + PI * 0.6, 16, Color(oc, 0.7), 1.5, true)
			var minor: Dictionary = s.get("minor", {}) if s.has("minor") else {}
			if not minor.is_empty():
				var mc := Lcars.owner_color(int(minor.get("memberOf", 0))) if int(minor.get("memberOf", 0)) != 0 else Color("#d8c3ff")
				var dia := PackedVector2Array([p + Vector2(0, -22), p + Vector2(22, 0), p + Vector2(0, 22), p + Vector2(-22, 0), p + Vector2(0, -22)])
				draw_polyline(dia, Color(mc, 0.85), 1.5, true)
			# planets as orbit dots
			var n: int = s.get("planets", []).size()
			for k in range(n):
				var ang := t * (0.25 + 0.05 * k) + k * 2.1 + int(s["id"])
				var rr := 27.0 + k * 4.0
				draw_circle(p + Vector2(cos(ang), sin(ang) * 0.55) * rr, 1.8, Color(0.75, 0.82, 1.0, 0.6))
		# Anomaly glyphs
		for f in v.get("features", []):
			var fp := map.cell_center(f["x"], f["y"])
			match str(f["type"]):
				"BlackHole":
					draw_circle(fp, 15, Color(0, 0, 0, 1))
					draw_arc(fp, 17, 0, TAU, 40, Color(1.0, 0.7, 0.35, 0.9), 2.5, true)
					draw_arc(fp, 24, t * 0.8, t * 0.8 + PI * 1.2, 30, Color(1.0, 0.55, 0.3, 0.5), 2.0, true)
				"NeutronStar":
					var pl := 0.5 + 0.5 * sin(t * 6.0)
					draw_texture_rect(map._glow, Rect2(fp - Vector2(30, 30), Vector2(60, 60)), false, Color(0.7, 0.9, 1.0, 0.7))
					var beam := Vector2(cos(t * 1.5), sin(t * 1.5)) * (26 + 10 * pl)
					draw_line(fp - beam, fp + beam, Color(0.8, 0.95, 1.0, 0.8), 2.0, true)
					draw_circle(fp, 4, Color.WHITE)
				"Wormhole":
					draw_arc(fp, 12, t * 2.0, t * 2.0 + PI * 1.5, 24, Color(0.6, 0.95, 1.0, 0.9), 2.0, true)
					draw_arc(fp, 7, -t * 3.0, -t * 3.0 + PI * 1.5, 18, Color(0.9, 1.0, 1.0, 0.9), 1.5, true)
			if not f.get("surveyed", true):
				draw_string(Lcars.font("display", 600), fp + Vector2(10, -12), "?", HORIZONTAL_ALIGNMENT_LEFT, -1, 16, Color(1, 1, 1, 0.7))
		# Stations
		for st in v.get("stations", []):
			var sp := map.cell_center(st["x"], st["y"]) + Vector2(-C * 0.3, C * 0.28)
			var sc2 := Lcars.owner_color(int(st["owner"]))
			var rad := 9.0 if st["type"] == "Outpost" else 12.0
			var hex := PackedVector2Array()
			for k in range(7):
				hex.append(sp + Vector2(cos(k * TAU / 6.0 + PI / 6.0), sin(k * TAU / 6.0 + PI / 6.0)) * rad)
			draw_colored_polygon(hex, Color(sc2, 0.25))
			draw_polyline(hex, sc2, 2.0, true)
			if st["type"] == "Starbase":
				draw_circle(sp, 3.5, sc2)
			if st.has("upgrade"):
				draw_arc(sp, rad + 5, -PI / 2, -PI / 2 + TAU * float(st["upgrade"]["work"]) / max(1.0, float(st["upgrade"]["cost"])), 24, Lcars.GOOD, 2.0, true)
		# Routes
		if map.layers.get("routes", true):
			for f in v.get("fleets", []):
				var route: Array = f.get("route", [])
				if route.size() < 2:
					continue
				var pts := PackedVector2Array()
				for c in route:
					pts.append(map.cell_center(int(c) % g.x, int(c) / g.x))
				var selected := int(f["id"]) == int(app.selection.get("fleetId", 0)) or (int(f.get("taskForceId", 0)) != 0 and int(f["taskForceId"]) == int(app.selection.get("taskForceId", 0)))
				var rc := Lcars.WARN if f.get("routeBlocked", false) else (Lcars.accent2() if selected else Color(accent, 0.75))
				for k in range(pts.size() - 1):
					_dashed(pts[k], pts[k + 1], rc, 3.0 if selected else 2.0, 9.0, 7.0, -t * 26.0)
				var end := pts[pts.size() - 1]
				draw_arc(end, 10, 0, TAU, 24, rc, 2.0, true)
				draw_circle(end, 3, rc)
		# Contacts
		var by_cell := {}
		for c in v.get("contacts", []):
			var key := int(c["y"]) * g.x + int(c["x"])
			by_cell[key] = by_cell.get(key, 0) + 1
			var cp := map.cell_center(c["x"], c["y"]) + Vector2(C * 0.28, C * 0.26) + Vector2(-(by_cell[key] - 1) * 8, 0)
			var cc := Lcars.owner_color(int(c["owner"]))
			var d := 8.0
			var dia2 := PackedVector2Array([cp + Vector2(0, -d), cp + Vector2(d, 0), cp + Vector2(0, d), cp + Vector2(-d, 0)])
			draw_colored_polygon(dia2, cc)
			if c.get("atWar", false):
				draw_arc(cp, 13, 0, TAU, 20, Color(Lcars.ALERT, 0.6 + 0.3 * sin(t * 4.0)), 2.0, true)
			if int(c["id"]) == int(app.selection.get("fleetId", 0)):
				draw_arc(cp, 16, 0, TAU, 24, Color.WHITE, 2.0, true)
		# Own fleets
		var own := {}
		for f in v.get("fleets", []):
			var key2 := int(f["y"]) * g.x + int(f["x"])
			if not own.has(key2):
				own[key2] = []
			own[key2].append(f)
		for key3 in own:
			var list: Array = own[key3]
			var f0: Dictionary = list[0]
			var base := map.cell_center(f0["x"], f0["y"]) + Vector2(C * 0.27, -C * 0.27)
			var heading := Vector2(0, -1)
			var route2: Array = f0.get("route", [])
			if route2.size() > 1:
				heading = (map.cell_center(int(route2[1]) % g.x, int(route2[1]) / g.x) - map.cell_center(f0["x"], f0["y"])).normalized()
			var oc2 := Lcars.owner_color(app.player())
			var any_sel := false
			var tf := false
			for f in list:
				if int(f["id"]) == int(app.selection.get("fleetId", 0)):
					any_sel = true
				if int(f.get("taskForceId", 0)) != 0:
					tf = true
			_chevron(base, heading, 11.0, oc2, tf)
			if list.size() > 1:
				draw_circle(base + Vector2(12, 10), 8, Lcars.BG)
				draw_string(Lcars.font("mono"), base + Vector2(8, 14), str(list.size()), HORIZONTAL_ALIGNMENT_LEFT, -1, 12, Color.WHITE)
			if any_sel:
				var pr := 17.0 + 2.0 * sin(t * 5.0)
				draw_arc(base, pr, 0, TAU, 28, Color.WHITE, 2.0, true)
			if f0.get("routeBlocked", false):
				draw_arc(base, 15, 0, TAU, 20, Lcars.WARN, 1.5, true)

	func _chevron(p: Vector2, dir: Vector2, s: float, col: Color, double: bool) -> void:
		var side := Vector2(-dir.y, dir.x)
		var tip := p + dir * s
		var pts := PackedVector2Array([tip, p - dir * s * 0.7 + side * s * 0.8, p - dir * s * 0.25, p - dir * s * 0.7 - side * s * 0.8])
		draw_colored_polygon(pts, col)
		pts.append(tip)
		draw_polyline(pts, Color(1, 1, 1, 0.8), 1.2, true)
		if double:
			var back := p - dir * s * 0.9
			draw_polyline(PackedVector2Array([back + side * s * 0.8, back - dir * s * 0.45, back - side * s * 0.8]), col, 2.0, true)

	func _dashed(a: Vector2, b: Vector2, col: Color, w: float, dash: float, gap: float, phase: float) -> void:
		var len := a.distance_to(b)
		if len < 0.01:
			return
		var dir := (b - a) / len
		var period := dash + gap
		var s := fposmod(phase, period) - period
		while s < len:
			var s0: float = max(s, 0.0)
			var s1: float = min(s + dash, len)
			if s1 > s0:
				draw_line(a + dir * s0, a + dir * s1, col, w, true)
			s += period

	func _arc_line(a: Vector2, b: Vector2, col: Color, w: float, t: float) -> void:
		var mid := (a + b) / 2.0 + Vector2(-(b - a).y, (b - a).x) * 0.15
		var prev := a
		for k in range(1, 33):
			var u := k / 32.0
			var p := a.lerp(mid, u).lerp(mid.lerp(b, u), u)
			if (k + int(t * 8.0)) % 3 != 0:
				draw_line(prev, p, col, w, true)
			prev = p

# ---------------------------------------------------------------------------------------------
# Screen-space labels (system names, ETAs) so text stays crisp at every zoom
# ---------------------------------------------------------------------------------------------
class MapLabels extends Control:
	var map: GalaxyMap

	func _draw() -> void:
		var v := map.view()
		if v.is_empty():
			return
		var app := map.app
		var g := map.grid_size()
		var f := Lcars.font("display", 560)
		var mono := Lcars.font("mono")
		if map.layers.get("labels", true):
			var fs := int(clamp(15.0 * sqrt(map.zoom), 11.0, 22.0))
			for s in v.get("systems", []):
				var p := map.map_to_screen(map.cell_center(s["x"], s["y"]) + Vector2(0, 30))
				if p.x < -100 or p.y < -40 or p.x > size.x + 100 or p.y > size.y + 40:
					continue
				var owner := int(s.get("owner", 0))
				var col := Lcars.owner_color(owner) if owner != 0 else Color(Lcars.TEXT2, 0.85)
				var name_ := str(s["name"]).to_upper()
				var w := f.get_string_size(name_, HORIZONTAL_ALIGNMENT_LEFT, -1, fs).x
				draw_string_outline(f, p + Vector2(-w / 2.0, fs), name_, HORIZONTAL_ALIGNMENT_LEFT, -1, fs, 4, Color(0, 0, 0, 0.75))
				draw_string(f, p + Vector2(-w / 2.0, fs), name_, HORIZONTAL_ALIGNMENT_LEFT, -1, fs, col)
		if map.layers.get("routes", true):
			for fl in v.get("fleets", []):
				var route: Array = fl.get("route", [])
				if route.size() < 2 or int(fl.get("eta", 0)) <= 0:
					continue
				var last := int(route[route.size() - 1])
				var p2 := map.map_to_screen(map.cell_center(last % g.x, last / g.x)) + Vector2(14, -14)
				var txt := "%dT" % int(fl["eta"])
				var tw := mono.get_string_size(txt, HORIZONTAL_ALIGNMENT_LEFT, -1, 13).x
				draw_style_box(Lcars.box(Color(Lcars.BG, 0.85), 6, Lcars.accent2(), 1, Vector4.ZERO), Rect2(p2 + Vector2(-4, -13), Vector2(tw + 8, 17)))
				draw_string(mono, p2, txt, HORIZONTAL_ALIGNMENT_LEFT, -1, 13, Lcars.accent2())
		if map.layers.get("grid", true) and map.zoom > 0.8:
			var cf := Color(Lcars.TEXT3, 0.35)
			for x in range(g.x):
				var p3 := map.map_to_screen(Vector2((x + 0.5) * GalaxyMap.CELL, 0)) + Vector2(-6, -6)
				draw_string(mono, p3, str(x), HORIZONTAL_ALIGNMENT_LEFT, -1, 11, cf)
			for y in range(g.y):
				var p4 := map.map_to_screen(Vector2(0, (y + 0.5) * GalaxyMap.CELL)) + Vector2(-20, 4)
				draw_string(mono, p4, str(y), HORIZONTAL_ALIGNMENT_LEFT, -1, 11, cf)
