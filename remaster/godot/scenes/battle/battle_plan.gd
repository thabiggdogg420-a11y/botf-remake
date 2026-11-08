class_name BattlePlan
extends Node3D
## Plan-phase overlays in 3D (BATTLE-DESIGN.md proposal 5, WS06 section 2.4), drawn on the selected ship's flight plane:
##  - the move preview: the path the core will fly (Encounter::previewMove; exact under tactical rules 4) and a ghost
##    hull at the end pose;
##  - firing arcs as translucent wedges and range bands, centred on that end pose, because fire follows movement;
##  - the target line, with the predicted-hit readout from the core's forecast (Encounter::forecastFire);
##  - a subsystem tag list (weapons, engines, shields, sensors, warp core) beside the target's bracket, joined to it by a
##    leader line, that the player clicks to aim; the forecast readout sits in a panel in the bottom-left corner of the 3D view.
##    Both are 2D (a CanvasLayer inside the battle viewport): fixed 1080p-readable sizes, nothing over the hull.
## Range is the core's Chebyshev distance, so a range band is a square on the grid, drawn with softened corners.
## Everything is unshaded and alpha-blended: it renders the same in Forward+ and Compatibility (web).

signal subsystem_picked(target_id: int, subsystem: String)

const FAMILY_COLORS := {"beam": Color(1.0, 0.62, 0.2), "pulse": Color(1.0, 0.45, 0.25), "torpedo": Color(0.95, 0.3, 0.3)}
const BAND_CELLS := [1, 2, 4, 6, 7]     ## Outer edge of point blank, short, medium, long, extreme (rules 4 bands).
const SUBSYSTEMS := ["weapons", "engines", "shields", "sensors", "warpCore"]
const SUBSYSTEM_LABELS := {"weapons": "WPN", "engines": "ENG", "shields": "SHD", "sensors": "SNS", "warpCore": "CORE"}
const TAG_ROW_H := 30.0       ## Height of one tag row in the list (px).
const TAG_W := 92.0           ## Width of the tag list (px).
const TAG_PAD := 6.0          ## Padding inside the list and the readout panel (px).
const LEAD_LEN := 34.0        ## Gap between the target bracket and the list, crossed by the leader line (px).
const EDGE := 12.0            ## Margin kept clear at the view's edges (px).
const PANEL_BG := Color(0.02, 0.05, 0.09, 0.86)
const LCARS_BLUE := Color(0.55, 0.75, 1.0)
const AIM_GOLD := Color(1.0, 0.85, 0.3)

var scene: BattleScene
var ship_id := 0
var target_id := 0
var subsystem := "none"
var forecast := {}

var _lines: MeshInstance3D
var _fill: MeshInstance3D
var _ghost: Node3D
var _ui: CanvasLayer
var _overlay: PlanOverlay
var _readout: Label           ## Forecast readout, bottom-left of the 3D view.
var _pips := {}               ## subsystem -> Label (a row of the tag list)
var _readout_rect := Rect2()
var _list_rect := Rect2()     ## The tag list panel; empty when there is no target.
var _row_rects := {}          ## subsystem -> its row in screen pixels (picking)
var _lead := PackedVector2Array()   ## Leader polyline from the bracket to the list.
var _line_mat: StandardMaterial3D
var _fill_mat: StandardMaterial3D


func _ready() -> void:
	_line_mat = _material(true)
	_fill_mat = _material(false)
	_lines = MeshInstance3D.new()
	_lines.name = "PlanLines"
	_lines.mesh = ImmediateMesh.new()
	_lines.material_override = _line_mat
	_lines.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(_lines)
	_fill = MeshInstance3D.new()
	_fill.name = "PlanFill"
	_fill.mesh = ImmediateMesh.new()
	_fill.material_override = _fill_mat
	_fill.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(_fill)
	_ui = CanvasLayer.new()
	_ui.name = "PlanUI"
	_ui.layer = 3
	_ui.visible = false
	add_child(_ui)
	_overlay = PlanOverlay.new()
	_overlay.name = "Overlay"
	_overlay.plan = self
	_ui.add_child(_overlay)
	_readout = _label(17, Color(1.0, 0.8, 0.45))
	_readout.name = "Readout"
	_overlay.add_child(_readout)
	for k in SUBSYSTEMS:
		var l := _label(17, LCARS_BLUE)
		l.name = "Pip_" + k
		l.text = SUBSYSTEM_LABELS[k]
		_overlay.add_child(l)
		_pips[k] = l
	visible = false


static func _material(lines: bool) -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	m.vertex_color_use_as_albedo = true
	m.cull_mode = BaseMaterial3D.CULL_DISABLED
	m.no_depth_test = lines # lines read through hulls; fills sit in the scene
	m.render_priority = 2 if lines else 1
	return m


static func _label(px: int, color: Color) -> Label:
	var l := Label.new()
	l.add_theme_font_override("font", ThemeDB.fallback_font)
	l.add_theme_font_size_override("font_size", px)
	l.add_theme_color_override("font_color", color)
	l.add_theme_color_override("font_outline_color", Color(0, 0, 0, 0.9))
	l.add_theme_constant_override("outline_size", 3)
	l.add_theme_constant_override("line_spacing", 3)
	l.mouse_filter = Control.MOUSE_FILTER_IGNORE
	l.visible = false
	return l


## Shows the plan for `ship` (a tactical ships[] entry) with `p_forecast` (bridge "tacticalForecast" result).
func show_plan(ship: Dictionary, p_forecast: Dictionary) -> void:
	ship_id = int(ship.get("id", 0))
	forecast = p_forecast
	target_id = int(p_forecast.get("target", 0))
	subsystem = str(p_forecast.get("subsystem", "none"))
	visible = true
	_ui.visible = true
	_rebuild(ship)


func _process(_delta: float) -> void:
	if is_showing():
		declutter(get_viewport().get_camera_3d())


## Screen layout of the 2D plan overlays, every frame (the camera moves): the forecast readout in a panel in the corner
## of the 3D view (bottom-left), and the subsystem tags as one vertical list beside the target's bracket (on the side with room), joined
## to the bracket by a leader line. The hull points of weapons, shields and the rest project onto nearly the same pixels
## from many angles, so tags anchored on the hull overlapped each other and the ship; a list cannot. Picking uses the rows.
func declutter(cam: Camera3D) -> void:
	if cam == null or _overlay == null:
		return
	var vp := get_viewport().get_visible_rect().size
	_row_rects.clear()
	_list_rect = Rect2()
	_lead = PackedVector2Array()
	# Forecast readout: inside the view, bottom-left (the camera bar owns the top edge), on a backing panel.
	if _readout.visible:
		var rsize := _readout.get_minimum_size()
		_readout.size = rsize
		var panel := rsize + Vector2(2.0 * TAG_PAD + 8.0, 2.0 * TAG_PAD)
		_readout_rect = Rect2(Vector2(EDGE, maxf(EDGE, vp.y - EDGE - panel.y)), panel)
		_readout.position = _readout_rect.position + Vector2(TAG_PAD + 4.0, TAG_PAD)
	else:
		_readout_rect = Rect2()
	var target: BattleShip = scene.ship_node(target_id) if scene != null else null
	if target == null or cam.is_position_behind(target.global_position):
		for k in SUBSYSTEMS:
			(_pips[k] as Label).visible = false
		_overlay.queue_redraw()
		return
	# The target's bracket, as BattleMarkers sizes it.
	var tc := cam.unproject_position(target.global_position)
	var rpx := cam.unproject_position(target.global_position + cam.global_transform.basis.x * target.radius()).distance_to(tc)
	var h := minf(rpx * 0.55, vp.y * 0.2) if rpx > vp.y * 0.16 else maxf(rpx * 1.08, 9.0)
	var ring := maxf(h * 1.08, 12.0) + 8.0
	var n := SUBSYSTEMS.size()
	var size := Vector2(TAG_W + 2.0 * TAG_PAD, n * TAG_ROW_H + 2.0 * TAG_PAD)
	var right := tc.x + ring + LEAD_LEN + size.x + EDGE <= vp.x
	var x := tc.x + ring + LEAD_LEN if right else tc.x - ring - LEAD_LEN - size.x
	x = clampf(x, EDGE, maxf(EDGE, vp.x - EDGE - size.x))
	var y := clampf(tc.y - size.y * 0.5, EDGE, maxf(EDGE, vp.y - EDGE - size.y))
	if _readout_rect.size != Vector2.ZERO and Rect2(Vector2(x, y), size).intersects(_readout_rect.grow(6.0)):
		y = clampf(_readout_rect.position.y - 10.0 - size.y, EDGE, maxf(EDGE, vp.y - EDGE - size.y))
	_list_rect = Rect2(Vector2(x, y), size)
	for i in n:
		var k: String = SUBSYSTEMS[i]
		var row := Rect2(Vector2(x + TAG_PAD, y + TAG_PAD + i * TAG_ROW_H), Vector2(TAG_W, TAG_ROW_H))
		_row_rects[k] = row
		var pip: Label = _pips[k]
		pip.visible = true
		pip.size = pip.get_minimum_size()
		pip.position = row.position + Vector2(10.0, (TAG_ROW_H - pip.size.y) * 0.5)
	# Leader: from the bracket edge, across the gap, into the list at the target's height.
	var edge_x := _list_rect.position.x if right else _list_rect.end.x
	var from := Vector2(tc.x + (ring - 6.0 if right else -(ring - 6.0)), tc.y)
	var ay := clampf(tc.y, _list_rect.position.y + TAG_PAD + TAG_ROW_H * 0.5, _list_rect.end.y - TAG_PAD - TAG_ROW_H * 0.5)
	var elbow_x := lerpf(from.x, edge_x, 0.5)
	_lead = PackedVector2Array([from, Vector2(elbow_x, tc.y), Vector2(elbow_x, ay), Vector2(edge_x, ay)])
	_overlay.queue_redraw()


## Draws the panel backings and the leader (called by the overlay; the labels draw themselves on top of this).
func draw_overlay(c: Control) -> void:
	if _readout.visible and _readout_rect.size != Vector2.ZERO:
		c.draw_rect(_readout_rect, PANEL_BG)
		c.draw_rect(Rect2(_readout_rect.position, Vector2(3.0, _readout_rect.size.y)), Color(1.0, 0.8, 0.45, 0.95))
		c.draw_rect(_readout_rect, Color(LCARS_BLUE, 0.35), false, 1.0)
	if _list_rect.size == Vector2.ZERO:
		return
	if _lead.size() > 1:
		c.draw_polyline(_lead, Color(0.85, 0.95, 1.0, 0.95), 1.6, true)
		c.draw_circle(_lead[0], 3.5, Color(0.85, 0.95, 1.0, 1.0))
	c.draw_rect(_list_rect, PANEL_BG)
	c.draw_rect(_list_rect, Color(LCARS_BLUE, 0.35), false, 1.0)
	for k in SUBSYSTEMS:
		if not _row_rects.has(k):
			continue
		var row: Rect2 = _row_rects[k]
		var chosen: bool = k == subsystem
		c.draw_rect(row, Color(AIM_GOLD, 0.22) if chosen else Color(LCARS_BLUE, 0.07))
		c.draw_rect(Rect2(row.position, Vector2(3.0, row.size.y)), AIM_GOLD if chosen else Color(LCARS_BLUE, 0.7))


func clear() -> void:
	visible = false
	if _ui != null:
		_ui.visible = false
	ship_id = 0
	target_id = 0
	forecast = {}
	if _ghost != null:
		_ghost.queue_free()
		_ghost = null


func is_showing() -> bool:
	return visible and ship_id != 0


## World point of grid cell (x, y) on the selected ship's flight plane.
func _plane(x: float, y: float, h: float) -> Vector3:
	var p := scene.cell_to_world(x, y)
	p.y = h
	return p


func _rebuild(ship: Dictionary) -> void:
	var node: BattleShip = scene.ship_node(ship_id)
	var h := node.global_position.y if node != null else 0.0
	var lines: ImmediateMesh = _lines.mesh
	var fill: ImmediateMesh = _fill.mesh
	lines.clear_surfaces()
	fill.clear_surfaces()
	var path: Array = forecast.get("path", [])
	var end: Dictionary = forecast.get("end", ship.get("position", {"x": 0, "y": 0}))
	var end_facing := float(end.get("facing", ship.get("facing", 0)))
	var ex := float(end.get("x", 0))
	var ey := float(end.get("y", 0))
	var lcars := Color(0.55, 0.75, 1.0)
	var cell := BattleScene.CELL * BattleScene.SPREAD

	# Firing arcs (wedges) and range bands at the end pose.
	fill.surface_begin(Mesh.PRIMITIVE_TRIANGLES)
	var weapons: Array = ship.get("weapons", [])
	for w in weapons:
		var fam := str(w.get("family", "torpedo" if w.get("torpedo", false) else "beam"))
		var col: Color = FAMILY_COLORS.get(fam, lcars)
		var reach := float(w.get("maxRange", 7 if fam == "torpedo" else 4)) * cell
		var half := float(w.get("arcHalfWidth", 180))
		var centres := [float(w.get("arcCentre", 0))]
		if str(w.get("arc", "")) == "Broadside":
			centres = [90.0, -90.0]
		for c in centres:
			_wedge(fill, Vector3(_plane(ex, ey, h)), end_facing + c, half, reach, Color(col, 0.035))
	fill.surface_end()

	lines.surface_begin(Mesh.PRIMITIVE_LINES)
	for i in BAND_CELLS.size():
		var r := float(BAND_CELLS[i]) + 0.5
		var a := 0.55 - 0.08 * i
		_square(lines, _plane(ex, ey, h), r * cell, Color(lcars, a))
	for w in weapons:
		var fam := str(w.get("family", "beam"))
		var col: Color = FAMILY_COLORS.get(fam, lcars)
		var reach := float(w.get("maxRange", 4)) * cell + 0.5 * cell
		var half := float(w.get("arcHalfWidth", 180))
		var centres := [float(w.get("arcCentre", 0))]
		if str(w.get("arc", "")) == "Broadside":
			centres = [90.0, -90.0]
		for c in centres:
			_wedge_edges(lines, _plane(ex, ey, h), end_facing + c, half, reach, Color(col, 0.55))

	# Move preview: the path, chevrons along it and the end pose.
	var prev := Vector3.INF
	for i in path.size():
		var pt: Dictionary = path[i]
		var q := _plane(float(pt.x), float(pt.y), h)
		if prev != Vector3.INF:
			lines.surface_set_color(Color(0.45, 1.0, 0.75, 0.95))
			lines.surface_add_vertex(prev)
			lines.surface_set_color(Color(0.45, 1.0, 0.75, 0.95))
			lines.surface_add_vertex(q)
		_chevron(lines, q, float(pt.get("facing", 0)), cell * 0.28, Color(0.45, 1.0, 0.75, 0.9))
		prev = q

	# Target line and the predicted-hit readout.
	var target: BattleShip = scene.ship_node(target_id)
	_readout.visible = false
	for k in _pips:
		_pips[k].visible = false
	if target != null:
		var tp := target.global_position
		var from := _plane(ex, ey, h)
		_dashed(lines, from, tp, cell * 0.25, Color(1.0, 0.8, 0.45, 0.9))
		_readout.visible = true
		_readout.text = readout_text(forecast)
		for k in SUBSYSTEMS:
			var pip: Label = _pips[k]
			pip.visible = true
			var chosen: bool = k == subsystem
			pip.add_theme_color_override("font_color", AIM_GOLD if chosen else LCARS_BLUE)
			pip.text = ("[%s]" if chosen else "%s") % SUBSYSTEM_LABELS[k]
	lines.surface_end()

	# Ghost hull at the end pose: the ship's model drawn as a translucent LCARS silhouette.
	if _ghost != null:
		_ghost.queue_free()
		_ghost = null
	if node != null and path.size() > 1:
		_ghost = Node3D.new()
		_ghost.name = "Ghost"
		add_child(_ghost)
		var copy := node.model.duplicate(0) as Node3D
		_ghost.add_child(copy)
		var gm := StandardMaterial3D.new()
		gm.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		gm.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		gm.albedo_color = Color(0.45, 1.0, 0.75, 0.28)
		gm.cull_mode = BaseMaterial3D.CULL_BACK
		for g in copy.find_children("*", "GeometryInstance3D", true, false):
			(g as GeometryInstance3D).material_override = gm
			(g as GeometryInstance3D).cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		for p in copy.find_children("*", "GPUParticles3D", true, false) + copy.find_children("*", "CPUParticles3D", true, false) + copy.find_children("*", "Decal", true, false) + copy.find_children("*", "Light3D", true, false):
			p.queue_free()
		_ghost.global_transform = Transform3D(BattleScene.pose_basis(end_facing, 0.0, 0.0), _plane(ex, ey, h))


## One line per firing mount: "PHASER ARRAY 82% x 41 -> FORE 120". Mounts out of arc or range are left out.
static func readout_text(f: Dictionary) -> String:
	var rows := []
	for m in f.get("mounts", []):
		if int(m.get("hitChance", 0)) <= 0:
			continue
		var facing := str(m.get("facing", "none"))
		var tail := "" if facing == "none" else "  ->  %s %d" % [facing.to_upper(), int(m.get("facingShields", 0))]
		rows.append("%s  %d%%  x%d%s" % [str(m.get("weapon", "")).to_upper(), int(m.hitChance), int(m.get("damage", 0)), tail])
	if rows.is_empty():
		rows.append("NO WEAPON BEARS")
	var sub := str(f.get("subsystem", "none"))
	var head := "EXPECTED %d" % int(f.get("expectedDamage", 0))
	if sub != "none":
		head += "  ·  AIM %s" % sub.to_upper()
	return head + "\n" + "\n".join(rows)


## The subsystem tag under a screen point (pixels in the battle viewport), or "" (plan-phase subsystem picking).
func subsystem_at(screen: Vector2, _camera: Camera3D = null) -> String:
	if not is_showing():
		return ""
	for k in _row_rects:
		if (_pips[k] as Label).visible and (_row_rects[k] as Rect2).grow(2.0).has_point(screen):
			return k
	return ""


## Screen rectangle of a subsystem's tag row (px), or an empty Rect2 (tests, capture tools).
func tag_rect(k: String) -> Rect2:
	return _row_rects.get(k, Rect2())


## 2D overlay that paints the panel backings and the leader line under the plan's labels.
class PlanOverlay extends Control:
	var plan: BattlePlan

	func _ready() -> void:
		set_anchors_preset(Control.PRESET_FULL_RECT)
		mouse_filter = Control.MOUSE_FILTER_IGNORE

	func _draw() -> void:
		if plan != null and plan.is_showing():
			plan.draw_overlay(self)


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT and is_showing():
		if pick(event.position, get_viewport().get_camera_3d()):
			get_viewport().set_input_as_handled()


func pick(screen: Vector2, camera: Camera3D) -> bool:
	var k := subsystem_at(screen, camera)
	if k == "":
		return false
	subsystem = "none" if k == subsystem else k
	subsystem_picked.emit(target_id, subsystem)
	return true


func _wedge(m: ImmediateMesh, c: Vector3, centre_deg: float, half_deg: float, reach: float, col: Color) -> void:
	var steps := maxi(4, int(half_deg / 6.0))
	for i in steps:
		var a0 := deg_to_rad(centre_deg - half_deg + 2.0 * half_deg * i / steps)
		var a1 := deg_to_rad(centre_deg - half_deg + 2.0 * half_deg * (i + 1) / steps)
		m.surface_set_color(Color(col, col.a * 1.6))
		m.surface_add_vertex(c)
		m.surface_set_color(Color(col, col.a * 0.4))
		m.surface_add_vertex(c + Vector3(cos(a0), 0, sin(a0)) * reach)
		m.surface_set_color(Color(col, col.a * 0.4))
		m.surface_add_vertex(c + Vector3(cos(a1), 0, sin(a1)) * reach)


func _wedge_edges(m: ImmediateMesh, c: Vector3, centre_deg: float, half_deg: float, reach: float, col: Color) -> void:
	var steps := maxi(4, int(half_deg / 6.0))
	var prev := c + Vector3(cos(deg_to_rad(centre_deg - half_deg)), 0, sin(deg_to_rad(centre_deg - half_deg))) * reach
	if half_deg < 180.0:
		_seg(m, c, prev, col)
	for i in range(1, steps + 1):
		var a := deg_to_rad(centre_deg - half_deg + 2.0 * half_deg * i / steps)
		var q := c + Vector3(cos(a), 0, sin(a)) * reach
		_seg(m, prev, q, Color(col, col.a * 0.6))
		prev = q
	if half_deg < 180.0:
		_seg(m, c, prev, col)


## A Chebyshev range band: a square of half-width r with corners rounded off by a fifth.
func _square(m: ImmediateMesh, c: Vector3, r: float, col: Color) -> void:
	var k := r * 0.2
	var pts := [Vector3(r - k, 0, -r), Vector3(r, 0, -r + k), Vector3(r, 0, r - k), Vector3(r - k, 0, r),
		Vector3(-r + k, 0, r), Vector3(-r, 0, r - k), Vector3(-r, 0, -r + k), Vector3(-r + k, 0, -r)]
	for i in pts.size():
		_seg(m, c + pts[i], c + pts[(i + 1) % pts.size()], col)


func _chevron(m: ImmediateMesh, c: Vector3, facing: float, size: float, col: Color) -> void:
	var f := deg_to_rad(facing)
	var fwd := Vector3(cos(f), 0, sin(f))
	var side := Vector3(-fwd.z, 0, fwd.x)
	_seg(m, c + fwd * size, c - fwd * size * 0.4 + side * size * 0.7, col)
	_seg(m, c + fwd * size, c - fwd * size * 0.4 - side * size * 0.7, col)


func _dashed(m: ImmediateMesh, a: Vector3, b: Vector3, dash: float, col: Color) -> void:
	var n := maxi(1, int(a.distance_to(b) / (dash * 2.0)))
	for i in n:
		_seg(m, a.lerp(b, float(2 * i) / (2 * n)), a.lerp(b, float(2 * i + 1) / (2 * n)), col)


static func _seg(m: ImmediateMesh, a: Vector3, b: Vector3, col: Color) -> void:
	m.surface_set_color(col)
	m.surface_add_vertex(a)
	m.surface_set_color(col)
	m.surface_add_vertex(b)
