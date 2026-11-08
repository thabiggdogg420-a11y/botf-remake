class_name BattlePlan
extends Node3D
## Plan-phase overlays in 3D (BATTLE-DESIGN.md proposal 5, WS06 section 2.4), drawn on the selected ship's flight plane:
##  - the move preview: the path the core will fly (Encounter::previewMove; exact under tactical rules 4) and a ghost
##    hull at the end pose;
##  - firing arcs as translucent wedges and range bands, centred on that end pose, because fire follows movement;
##  - the target line, with the predicted-hit readout from the core's forecast (Encounter::forecastFire);
##  - subsystem pips on the target (weapons, engines, shields, sensors, warp core) that the player clicks to aim.
## Range is the core's Chebyshev distance, so a range band is a square on the grid, drawn with softened corners.
## Everything is unshaded and alpha-blended: it renders the same in Forward+ and Compatibility (web).

signal subsystem_picked(target_id: int, subsystem: String)

const FAMILY_COLORS := {"beam": Color(1.0, 0.62, 0.2), "pulse": Color(1.0, 0.45, 0.25), "torpedo": Color(0.95, 0.3, 0.3)}
const BAND_CELLS := [1, 2, 4, 6, 7]     ## Outer edge of point blank, short, medium, long, extreme (rules 4 bands).
const SUBSYSTEMS := ["weapons", "engines", "shields", "sensors", "warpCore"]
const SUBSYSTEM_LABELS := {"weapons": "WPN", "engines": "ENG", "shields": "SHD", "sensors": "SNS", "warpCore": "CORE"}
const PICK_RADIUS_PX := 22.0
const PIP_GAP_Y := 17.0       ## Minimum screen gap between stacked pip labels (px).
const PIP_NEAR_X := 60.0      ## Labels closer than this horizontally count as overlapping.

var scene: BattleScene
var ship_id := 0
var target_id := 0
var subsystem := "none"
var forecast := {}

var _lines: MeshInstance3D
var _fill: MeshInstance3D
var _ghost: Node3D
var _readout: Label3D
var _pips := {}               ## subsystem -> Label3D
var _pip_base := {}           ## subsystem -> world point on the target's hull (the label is nudged off it in screen space)
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
	_readout = _label(17, Color(1.0, 0.8, 0.45))
	_readout.name = "Readout"
	add_child(_readout)
	for k in SUBSYSTEMS:
		var l := _label(13, Color(0.6, 0.8, 1.0))
		l.name = "Pip_" + k
		l.text = SUBSYSTEM_LABELS[k]
		add_child(l)
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


static func _label(px: int, color: Color) -> Label3D:
	var l := Label3D.new()
	l.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	l.no_depth_test = true
	l.fixed_size = true
	l.pixel_size = 0.0011
	l.font_size = px
	l.outline_size = 4
	l.horizontal_alignment = HORIZONTAL_ALIGNMENT_LEFT
	l.modulate = color
	l.outline_modulate = Color(0, 0, 0, 0.85)
	l.render_priority = 3
	l.visible = false
	return l


## Shows the plan for `ship` (a tactical ships[] entry) with `p_forecast` (bridge "tacticalForecast" result).
func show_plan(ship: Dictionary, p_forecast: Dictionary) -> void:
	ship_id = int(ship.get("id", 0))
	forecast = p_forecast
	target_id = int(p_forecast.get("target", 0))
	subsystem = str(p_forecast.get("subsystem", "none"))
	visible = true
	_rebuild(ship)


func _process(_delta: float) -> void:
	if is_showing():
		declutter(get_viewport().get_camera_3d())


## Pushes the subsystem tags apart on screen: the hull points of weapons, shields and the rest project onto nearly the
## same pixels from many angles, so "SHD" and "[WPN]" drew on top of each other. Tags are stacked at least PIP_GAP_Y px
## apart, keeping their depth, and picking follows the moved label.
func declutter(cam: Camera3D) -> void:
	if cam == null or _pip_base.is_empty():
		return
	var items := []
	for k in SUBSYSTEMS:
		var pip: Label3D = _pips[k]
		if pip.visible and _pip_base.has(k) and not cam.is_position_behind(_pip_base[k]):
			items.append([k, cam.unproject_position(_pip_base[k])])
	items.sort_custom(func(a, b): return a[1].y < b[1].y or (a[1].y == b[1].y and a[1].x < b[1].x))
	var placed: Array[Vector2] = []
	var inv := cam.global_transform.affine_inverse()
	for it in items:
		var pos: Vector2 = it[1]
		var moved := true
		var guard := 0
		while moved and guard < 12:
			moved = false
			guard += 1
			for p in placed:
				if absf(pos.x - p.x) < PIP_NEAR_X and absf(pos.y - p.y) < PIP_GAP_Y:
					pos.y = p.y + PIP_GAP_Y
					moved = true
		placed.append(pos)
		var depth := -(inv * Vector3(_pip_base[it[0]])).z
		(_pips[it[0]] as Label3D).global_position = cam.project_position(pos, maxf(depth, cam.near + 0.01))


func clear() -> void:
	visible = false
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
		_readout.global_position = tp + Vector3.UP * (target.radius() * 2.2 + cell * 0.6)
		for k in SUBSYSTEMS:
			var pip: Label3D = _pips[k]
			pip.visible = true
			_pip_base[k] = target.subsystem_point(k)
			pip.global_position = _pip_base[k]
			var chosen: bool = k == subsystem
			pip.modulate = Color(1.0, 0.85, 0.3) if chosen else Color(0.6, 0.8, 1.0, 0.9)
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


## The subsystem pip under a screen point (pixels), or "" (plan-phase subsystem picking).
func subsystem_at(screen: Vector2, camera: Camera3D) -> String:
	if not is_showing() or camera == null:
		return ""
	var best := ""
	var best_d := PICK_RADIUS_PX
	for k in _pips:
		var pip: Label3D = _pips[k]
		if not pip.visible or camera.is_position_behind(pip.global_position):
			continue
		var d := camera.unproject_position(pip.global_position).distance_to(screen)
		if d < best_d:
			best_d = d
			best = k
	return best


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
