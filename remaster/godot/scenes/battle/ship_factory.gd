class_name ShipFactory
extends RefCounted
## Procedural placeholder hulls per faction, built from primitives and extruded outlines, one unit long, facing +x
## (y up, z starboard). PBR hull shader with panels and windows, emissive nacelles / engines / deflectors and blinking
## navigation lights (red to port, green to starboard, white strobes).

const HULL_SHADER := preload("res://scenes/battle/shaders/hull.gdshader")
const BLINK_SHADER := preload("res://scenes/battle/shaders/blink.gdshader")

## Owner faction look: hull colours, window light, engine / nacelle / accent glow.
const LOOK := {
	1: {"base": Color(0.70, 0.72, 0.76), "accent": Color(0.56, 0.59, 0.66), "window": Color(1.0, 0.9, 0.7),
		"engine": Color(0.25, 0.55, 1.0), "accent_glow": Color(1.0, 0.25, 0.1), "metal": 0.3, "rough": 0.42},
	2: {"base": Color(0.33, 0.36, 0.30), "accent": Color(0.42, 0.30, 0.24), "window": Color(1.0, 0.55, 0.25),
		"engine": Color(1.0, 0.25, 0.08), "accent_glow": Color(0.3, 1.0, 0.25), "metal": 0.6, "rough": 0.5},
	3: {"base": Color(0.33, 0.48, 0.40), "accent": Color(0.25, 0.36, 0.31), "window": Color(0.55, 1.0, 0.65),
		"engine": Color(0.25, 1.0, 0.45), "accent_glow": Color(0.3, 1.0, 0.5), "metal": 0.5, "rough": 0.45},
	4: {"base": Color(0.58, 0.49, 0.36), "accent": Color(0.44, 0.36, 0.28), "window": Color(1.0, 0.72, 0.25),
		"engine": Color(1.0, 0.6, 0.15), "accent_glow": Color(1.0, 0.75, 0.2), "metal": 0.5, "rough": 0.5},
	5: {"base": Color(0.62, 0.40, 0.24), "accent": Color(0.50, 0.32, 0.20), "window": Color(0.5, 1.0, 0.55),
		"engine": Color(0.3, 1.0, 0.5), "accent_glow": Color(1.0, 0.7, 0.2), "metal": 0.65, "rough": 0.35},
}

static var _cache := {}


static func look(owner: int) -> Dictionary:
	return LOOK.get(owner, LOOK[1])


static func hull_material(owner: int, panel := 0.035) -> ShaderMaterial:
	var key := "hull_%d_%f" % [owner, panel]
	if _cache.has(key):
		return _cache[key]
	var l := look(owner)
	var m := ShaderMaterial.new()
	m.shader = HULL_SHADER
	m.set_shader_parameter("base_color", l.base)
	m.set_shader_parameter("accent_color", l.accent)
	m.set_shader_parameter("metallic", l.metal)
	m.set_shader_parameter("roughness", l.rough)
	m.set_shader_parameter("window_color", l.window)
	m.set_shader_parameter("panel_size", 0.35)
	_cache[key] = m
	return m


static func glow_material(color: Color, energy := 5.0) -> StandardMaterial3D:
	var key := "glow_%s_%f" % [color.to_html(), energy]
	if _cache.has(key):
		return _cache[key]
	var m := StandardMaterial3D.new()
	m.albedo_color = color * 0.3
	m.emission_enabled = true
	m.emission = color
	m.emission_energy_multiplier = energy
	m.roughness = 0.3
	_cache[key] = m
	return m


static func dark_material() -> StandardMaterial3D:
	if _cache.has("dark"):
		return _cache.dark
	var m := StandardMaterial3D.new()
	m.albedo_color = Color(0.08, 0.08, 0.09)
	m.metallic = 0.7
	m.roughness = 0.5
	_cache.dark = m
	return m


static func build(owner: int, station: bool, ship := {}) -> Node3D:
	var root := Node3D.new()
	root.name = "Hull"
	if station:
		_station(root, owner)
	else:
		match owner:
			2: _klingon(root, ship)
			3: _romulan(root, ship)
			4: _cardassian(root, ship)
			5: _ferengi(root, ship)
			_: _federation(root, ship)
	return root


# ------------------------------------------------------------------------------------------------------------------
# Building blocks
# ------------------------------------------------------------------------------------------------------------------

static func part(parent: Node3D, mesh: Mesh, mat: Material, pos: Vector3, rot_deg := Vector3.ZERO,
		scl := Vector3.ONE) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	mi.mesh = mesh
	mi.material_override = mat
	mi.position = pos
	mi.rotation_degrees = rot_deg
	mi.scale = scl
	parent.add_child(mi)
	return mi


static func sphere(r := 0.5, segs := 32) -> SphereMesh:
	var m := SphereMesh.new()
	m.radius = r
	m.height = r * 2.0
	m.radial_segments = segs
	m.rings = segs / 2
	return m


static func cylinder(top: float, bottom: float, h: float, segs := 32) -> CylinderMesh:
	var m := CylinderMesh.new()
	m.top_radius = top
	m.bottom_radius = bottom
	m.height = h
	m.radial_segments = segs
	return m


static func capsule(r: float, h: float) -> CapsuleMesh:
	var m := CapsuleMesh.new()
	m.radius = r
	m.height = maxf(h, r * 2.0)
	m.radial_segments = 24
	m.rings = 8
	return m


static func box(size: Vector3) -> BoxMesh:
	var m := BoxMesh.new()
	m.size = size
	return m


## Capsule / box from a to b.
static func strut(parent: Node3D, a: Vector3, b: Vector3, width: float, thick: float, mat: Material) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	mi.mesh = box(Vector3(width, (b - a).length(), thick))
	mi.material_override = mat
	parent.add_child(mi)
	var up := (b - a).normalized()
	var side := up.cross(Vector3.RIGHT if absf(up.dot(Vector3.RIGHT)) < 0.95 else Vector3.FORWARD).normalized()
	mi.transform = Transform3D(Basis(up.cross(side), up, side).orthonormalized(), (a + b) * 0.5)
	return mi


static func nacelle_along(parent: Node3D, a: Vector3, b: Vector3, r: float, mat: Material) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	mi.mesh = capsule(r, (b - a).length() + 2.0 * r)
	mi.material_override = mat
	parent.add_child(mi)
	var up := (b - a).normalized()
	var side := up.cross(Vector3.UP if absf(up.dot(Vector3.UP)) < 0.95 else Vector3.RIGHT).normalized()
	mi.transform = Transform3D(Basis(side, up, side.cross(up) * -1.0).orthonormalized(), (a + b) * 0.5)
	return mi


## Extrudes an outline given in the (x, z) plane into a slab of `thickness` along y, with bevelled top and bottom
## faces (the top is `taper` times the outline, so edges slope like a hull).
static func extrude(outline: PackedVector2Array, thickness: float, taper := 0.85) -> ArrayMesh:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var centroid := Vector2.ZERO
	for p in outline:
		centroid += p
	centroid /= outline.size()
	var tris := Geometry2D.triangulate_polygon(outline)
	var h := thickness * 0.5
	var inset := func(p: Vector2) -> Vector2: return centroid + (p - centroid) * taper
	# Top and bottom caps (inset), then the bevelled rim: inset top -> full middle -> inset bottom.
	for i in range(0, tris.size(), 3):
		var a: Vector2 = inset.call(outline[tris[i]])
		var b: Vector2 = inset.call(outline[tris[i + 1]])
		var c: Vector2 = inset.call(outline[tris[i + 2]])
		for v in [a, c, b]:
			st.set_uv(v)
			st.add_vertex(Vector3(v.x, h, v.y))
		for v in [a, b, c]:
			st.set_uv(v)
			st.add_vertex(Vector3(v.x, -h, v.y))
	var n := outline.size()
	# Make the outline counter-clockwise-agnostic: orient rim faces outward using the polygon's winding.
	var ccw := Geometry2D.is_polygon_clockwise(outline)
	for i in n:
		var p0 := outline[i]
		var p1 := outline[(i + 1) % n]
		var q0: Vector2 = inset.call(p0)
		var q1: Vector2 = inset.call(p1)
		var top0 := Vector3(q0.x, h, q0.y)
		var top1 := Vector3(q1.x, h, q1.y)
		var mid0 := Vector3(p0.x, 0.0, p0.y)
		var mid1 := Vector3(p1.x, 0.0, p1.y)
		var bot0 := Vector3(q0.x, -h, q0.y)
		var bot1 := Vector3(q1.x, -h, q1.y)
		var quads := [[top0, top1, mid1, mid0], [mid0, mid1, bot1, bot0]]
		for q in quads:
			var order := [q[0], q[1], q[2], q[0], q[2], q[3]] if ccw else [q[0], q[2], q[1], q[0], q[3], q[2]]
			for v in order:
				st.set_uv(Vector2(v.x, v.y))
				st.add_vertex(v)
	st.generate_normals()
	return st.commit()


static func ring_sector(r_in: float, r_out: float, a0: float, a1: float, segs := 24,
		center := Vector2.ZERO) -> PackedVector2Array:
	var pts := PackedVector2Array()
	for i in segs + 1:
		var a := deg_to_rad(lerpf(a0, a1, float(i) / segs))
		pts.append(center + Vector2(cos(a), sin(a)) * r_out)
	for i in range(segs, -1, -1):
		var a := deg_to_rad(lerpf(a0, a1, float(i) / segs))
		pts.append(center + Vector2(cos(a), sin(a)) * r_in)
	return pts


static func nav_light(parent: Node3D, pos: Vector3, color: Color, phase := 0.0, rate := 0.8, size := 0.012) -> void:
	var mi := MeshInstance3D.new()
	mi.mesh = sphere(size, 8)
	var m := ShaderMaterial.new()
	m.shader = BLINK_SHADER
	mi.material_override = m
	mi.position = pos
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	parent.add_child(mi)
	mi.set_instance_shader_parameter("light_color", color)
	mi.set_instance_shader_parameter("blink_phase", phase)
	mi.set_instance_shader_parameter("blink_rate", rate)


# ------------------------------------------------------------------------------------------------------------------
# Faction hulls
# ------------------------------------------------------------------------------------------------------------------

## Federation: saucer, secondary hull, deflector, pylons and two warp nacelles with red bussard collectors.
static func _federation(root: Node3D, _ship: Dictionary) -> void:
	var l := look(1)
	var hull := hull_material(1)
	# Saucer: bevelled disc with upper and lower domes.
	part(root, cylinder(0.28, 0.31, 0.035, 64), hull, Vector3(0.2, 0.07, 0))
	part(root, cylinder(0.31, 0.25, 0.03, 64), hull, Vector3(0.2, 0.038, 0))
	part(root, sphere(0.5, 32), hull, Vector3(0.2, 0.09, 0), Vector3.ZERO, Vector3(0.2, 0.06, 0.2))
	part(root, sphere(0.5, 24), glow_material(Color(0.6, 0.8, 1.0), 2.0), Vector3(0.2, 0.118, 0), Vector3.ZERO, Vector3(0.04, 0.02, 0.04))
	part(root, sphere(0.5, 24), hull, Vector3(0.2, 0.02, 0), Vector3.ZERO, Vector3(0.14, 0.05, 0.14))
	# Rim window band.
	part(root, cylinder(0.312, 0.312, 0.006, 64), glow_material(l.window, 1.2), Vector3(0.2, 0.054, 0))
	# Neck and secondary hull.
	part(root, box(Vector3(0.14, 0.12, 0.05)), hull, Vector3(0.02, -0.0, 0), Vector3(0, 0, 28))
	var eng := nacelle_along(root, Vector3(0.1, -0.06, 0), Vector3(-0.3, -0.03, 0), 0.07, hull)
	eng.scale = Vector3(1.0, 1.0, 0.85)
	# Deflector dish.
	part(root, cylinder(0.045, 0.03, 0.02, 24), glow_material(Color(0.35, 0.65, 1.0), 6.0), Vector3(0.155, -0.065, 0), Vector3(0, 0, -75))
	# Pylons and nacelles.
	for side in [-1.0, 1.0]:
		var z: float = side * 0.19
		strut(root, Vector3(-0.2, -0.04, side * 0.04), Vector3(-0.26, 0.07, z * 0.95), 0.07, 0.018, hull)
		nacelle_along(root, Vector3(0.0, 0.085, z), Vector3(-0.48, 0.085, z), 0.027, hull)
		# Blue warp grille on the inboard flank and a red bussard collector.
		part(root, box(Vector3(0.4, 0.022, 0.01)), glow_material(l.engine, 3.5), Vector3(-0.24, 0.085, z - side * 0.025))
		part(root, box(Vector3(0.4, 0.01, 0.02)), glow_material(l.engine, 2.0), Vector3(-0.24, 0.111, z))
		part(root, sphere(0.029, 24), glow_material(Color(1.0, 0.22, 0.08), 6.0), Vector3(0.02, 0.085, z), Vector3.ZERO, Vector3(1.2, 0.95, 0.95))
		nav_light(root, Vector3(-0.5, 0.09, z), Color(1, 1, 1), 0.3 if side > 0 else 0.8, 0.7)
	# Impulse engines.
	part(root, box(Vector3(0.02, 0.018, 0.12)), glow_material(Color(1.0, 0.25, 0.1), 6.0), Vector3(-0.07, 0.07, 0))
	nav_light(root, Vector3(0.2, 0.075, -0.312), Color(1, 0.08, 0.05), 0.0)
	nav_light(root, Vector3(0.2, 0.075, 0.312), Color(0.1, 1, 0.2), 0.5)


## Klingon bird-of-prey: head on a long neck, a raised body and down-swept wings with disruptor cannons.
static func _klingon(root: Node3D, ship: Dictionary) -> void:
	var l := look(2)
	var hull := hull_material(2)
	var big := int(ship.get("maxHull", 60)) > 90
	part(root, sphere(0.5, 32), hull, Vector3(0.38, -0.01, 0), Vector3.ZERO, Vector3(0.2, 0.08, 0.14))
	part(root, sphere(0.5, 24), glow_material(Color(1.0, 0.25, 0.08), 3.0), Vector3(0.47, -0.02, 0), Vector3.ZERO, Vector3(0.03, 0.02, 0.05))
	nacelle_along(root, Vector3(0.3, 0.0, 0), Vector3(0.0, 0.03, 0), 0.035, hull)
	var body := extrude(PackedVector2Array([Vector2(0.08, 0.0), Vector2(0.0, 0.12), Vector2(-0.3, 0.16),
		Vector2(-0.36, 0.0), Vector2(-0.3, -0.16), Vector2(0.0, -0.12)]), 0.12, 0.7)
	part(root, body, hull, Vector3(0, 0.03, 0))
	part(root, sphere(0.5, 24), hull, Vector3(-0.12, 0.08, 0), Vector3.ZERO, Vector3(0.28, 0.08, 0.16))
	# Wings: swept, angled down (attack posture), a cannon at each tip.
	var span := 0.55 if big else 0.48
	for side in [-1.0, 1.0]:
		var wing := extrude(PackedVector2Array([Vector2(0.0, 0.0), Vector2(-0.3, 0.0), Vector2(-0.34, span * side),
			Vector2(-0.18, span * side), Vector2(0.02, 0.1 * side)]), 0.03, 0.9)
		part(root, wing, hull, Vector3(-0.02, 0.03, side * 0.08), Vector3(side * 18.0, 0, 0))
		var tip := Vector3(-0.22, 0.03 - sin(deg_to_rad(18.0)) * span, side * (0.08 + cos(deg_to_rad(18.0)) * span))
		nacelle_along(root, tip + Vector3(0.2, 0, 0), tip + Vector3(-0.1, 0, 0), 0.018, dark_material())
		part(root, sphere(0.02, 12), glow_material(l.accent_glow, 7.0), tip + Vector3(0.21, 0, 0))
		part(root, box(Vector3(0.02, 0.012, 0.2)), glow_material(l.engine, 2.5), Vector3(-0.31, 0.045, side * 0.19), Vector3(side * 18.0, 0, 0))
		nav_light(root, tip + Vector3(-0.1, 0.01, 0), Color(1, 0.08, 0.05) if side < 0 else Color(0.1, 1, 0.2), 0.2 * side + 0.3)
	part(root, box(Vector3(0.03, 0.05, 0.14)), glow_material(l.engine, 3.0), Vector3(-0.36, 0.05, 0))


## Romulan warbird: two crescent hulls (dorsal and ventral) joined at the stern, a long neck and a beaked head.
static func _romulan(root: Node3D, _ship: Dictionary) -> void:
	var l := look(3)
	var hull := hull_material(3)
	var cres := ring_sector(0.2, 0.36, 70.0, 290.0, 32, Vector2(-0.08, 0.0))
	var upper := part(root, extrude(cres, 0.035, 0.9), hull, Vector3(0, 0.1, 0), Vector3(0, 0, -4))
	var lower := part(root, extrude(cres, 0.035, 0.9), hull, Vector3(0, -0.1, 0), Vector3(0, 0, 4))
	# Stern connector and the warp nacelles along the outer rims.
	part(root, box(Vector3(0.1, 0.2, 0.14)), hull, Vector3(-0.4, 0.0, 0))
	for side in [-1.0, 1.0]:
		part(root, box(Vector3(0.05, 0.2, 0.02)), hull, Vector3(-0.38, 0.0, side * 0.16), Vector3(0, 0, 0))
		part(root, box(Vector3(0.3, 0.012, 0.02)), glow_material(l.engine, 6.0), Vector3(-0.12, 0.12, side * 0.33), Vector3(0, side * 30.0, 0))
		part(root, box(Vector3(0.3, 0.012, 0.02)), glow_material(l.engine, 6.0), Vector3(-0.12, -0.12, side * 0.33), Vector3(0, side * 30.0, 0))
		nav_light(root, Vector3(-0.08, 0.12, side * 0.36), Color(1, 0.08, 0.05) if side < 0 else Color(0.1, 1, 0.2), 0.4 * side)
	nacelle_along(root, Vector3(-0.35, 0.0, 0), Vector3(0.35, 0.0, 0), 0.04, hull)
	part(root, extrude(PackedVector2Array([Vector2(0.58, 0.0), Vector2(0.44, 0.1), Vector2(0.3, 0.07),
		Vector2(0.3, -0.07), Vector2(0.44, -0.1)]), 0.07, 0.75), hull, Vector3(0, 0, 0))
	part(root, sphere(0.5, 16), glow_material(l.accent_glow, 6.0), Vector3(0.56, 0.0, 0), Vector3.ZERO, Vector3(0.03, 0.02, 0.03))
	# Singularity-core glow amidships.
	part(root, sphere(0.5, 24), glow_material(Color(0.3, 1.0, 0.55), 3.0), Vector3(-0.3, 0, 0), Vector3.ZERO, Vector3(0.08, 0.12, 0.08))


## Cardassian Galor: a spade-shaped head on a long tapering spine, ventral keel, amber emitters.
static func _cardassian(root: Node3D, _ship: Dictionary) -> void:
	var l := look(4)
	var hull := hull_material(4)
	var spade := PackedVector2Array([Vector2(0.5, 0.0), Vector2(0.38, 0.13), Vector2(0.18, 0.2), Vector2(0.06, 0.16),
		Vector2(0.02, 0.06), Vector2(-0.5, 0.045), Vector2(-0.5, -0.045), Vector2(0.02, -0.06), Vector2(0.06, -0.16),
		Vector2(0.18, -0.2), Vector2(0.38, -0.13)])
	part(root, extrude(spade, 0.07, 0.8), hull, Vector3.ZERO)
	part(root, extrude(PackedVector2Array([Vector2(0.3, 0.0), Vector2(0.1, 0.05), Vector2(-0.45, 0.03),
		Vector2(-0.45, -0.03), Vector2(0.1, -0.05)]), 0.06, 0.6), hull, Vector3(0, 0.05, 0))
	part(root, extrude(PackedVector2Array([Vector2(0.2, 0.0), Vector2(0.0, 0.04), Vector2(-0.4, 0.03),
		Vector2(-0.4, -0.03), Vector2(0.0, -0.04)]), 0.08, 0.6), hull, Vector3(0, -0.06, 0))
	part(root, sphere(0.5, 16), glow_material(l.accent_glow, 7.0), Vector3(0.4, -0.01, 0), Vector3.ZERO, Vector3(0.06, 0.03, 0.12))
	for side in [-1.0, 1.0]:
		part(root, box(Vector3(0.24, 0.012, 0.012)), glow_material(l.engine, 5.0), Vector3(-0.22, 0.02, side * 0.045))
		nav_light(root, Vector3(0.18, 0.02, side * 0.2), Color(1, 0.08, 0.05) if side < 0 else Color(0.1, 1, 0.2), 0.3 * side)
	part(root, box(Vector3(0.02, 0.05, 0.08)), glow_material(l.engine, 6.0), Vector3(-0.5, 0.0, 0))


## Ferengi Marauder: a broad horseshoe hull opening astern, a central command pod and green emitters.
static func _ferengi(root: Node3D, _ship: Dictionary) -> void:
	var l := look(5)
	var hull := hull_material(5)
	var shoe := ring_sector(0.14, 0.4, -125.0, 125.0, 36, Vector2(-0.1, 0.0))
	part(root, extrude(shoe, 0.07, 0.85), hull, Vector3.ZERO)
	part(root, sphere(0.5, 32), hull, Vector3(0.08, 0.04, 0), Vector3.ZERO, Vector3(0.34, 0.1, 0.24))
	part(root, sphere(0.5, 16), glow_material(l.window, 3.0), Vector3(0.24, 0.07, 0), Vector3.ZERO, Vector3(0.05, 0.02, 0.09))
	part(root, box(Vector3(0.05, 0.03, 0.12)), glow_material(l.accent_glow, 6.0), Vector3(0.3, 0.0, 0))
	for side in [-1.0, 1.0]:
		var tip := Vector3(-0.1 + cos(deg_to_rad(125.0)) * 0.3, 0.0, side * sin(deg_to_rad(125.0)) * 0.3)
		part(root, box(Vector3(0.05, 0.03, 0.1)), glow_material(l.engine, 6.0), tip + Vector3(-0.02, 0, 0), Vector3(0, side * 35.0, 0))
		nav_light(root, Vector3(-0.1, 0.02, side * 0.4), Color(1, 0.08, 0.05) if side < 0 else Color(0.1, 1, 0.2), 0.25 * side)


## Starbase: a spindle with habitat rings, docking arms and a great deal of light.
static func _station(root: Node3D, owner: int) -> void:
	var l := look(owner)
	var hull := hull_material(owner)
	part(root, cylinder(0.05, 0.08, 0.7, 32), hull, Vector3.ZERO)
	part(root, sphere(0.5, 32), hull, Vector3(0, 0.33, 0), Vector3.ZERO, Vector3(0.22, 0.14, 0.22))
	part(root, sphere(0.5, 32), hull, Vector3(0, -0.36, 0), Vector3.ZERO, Vector3(0.16, 0.2, 0.16))
	var ring := TorusMesh.new()
	ring.inner_radius = 0.36
	ring.outer_radius = 0.46
	ring.rings = 96
	ring.ring_segments = 16
	part(root, ring, hull, Vector3(0, 0.05, 0), Vector3.ZERO, Vector3(1, 0.6, 1))
	var ring2 := TorusMesh.new()
	ring2.inner_radius = 0.2
	ring2.outer_radius = 0.25
	ring2.rings = 64
	ring2.ring_segments = 12
	part(root, ring2, hull, Vector3(0, -0.12, 0))
	part(root, cylinder(0.3, 0.3, 0.02, 64), glow_material(l.window, 1.5), Vector3(0, 0.05, 0))
	for i in 6:
		var a := TAU * i / 6.0
		var d := Vector3(cos(a), 0, sin(a))
		strut(root, Vector3(0, 0.05, 0) + d * 0.06, Vector3(0, 0.05, 0) + d * 0.37, 0.03, 0.02, hull)
		strut(root, Vector3(0, 0.3, 0) + d * 0.1, Vector3(0, 0.42, 0) + d * 0.3, 0.02, 0.02, hull)
		nav_light(root, Vector3(0, 0.05, 0) + d * 0.465, Color(1, 1, 1) if i % 2 == 0 else Color(1, 0.1, 0.05), i * 0.17, 0.5, 0.01)
	part(root, sphere(0.5, 16), glow_material(l.engine, 5.0), Vector3(0, 0.41, 0), Vector3.ZERO, Vector3(0.05, 0.03, 0.05))
	part(root, cylinder(0.02, 0.02, 0.15, 12), glow_material(l.engine, 5.0), Vector3(0, -0.5, 0))
