class_name SystemView3D
extends SubViewportContainer
## F3 system view: the opened system in 3D. Its star (lit sphere, light source), every planet on its orbit with the L7
## texture set where one exists (PlanetBody, else the procedural globe the galaxy map uses), asteroid belts, and the
## colony's structures: orbital shipyard ring, defence platforms, and the surface works (farms, factories, labs, power)
## as lit sites on the inhabited world. The LCARS overlay (planet labels, the structure legend) is drawn in screen space
## over the viewport. Works in both renderers (Forward+ desktop, Compatibility web: no glow or volumetrics needed).

const STRUCT_COLORS := {"farms": Color(0.45, 1.0, 0.45), "factories": Color(1.0, 0.62, 0.25), "laboratories": Color(0.45, 0.75, 1.0),
	"powerPlants": Color(1.0, 0.92, 0.35), "fusionPlants": Color(1.0, 0.92, 0.35), "refineries": Color(0.85, 0.5, 1.0)}
const STRUCT_LABELS := {"farms": "Farms", "factories": "Factories", "laboratories": "Labs", "powerPlants": "Power", "shipyards": "Shipyard", "defenseBatteries": "Defence"}
## Orbits are drawn as ellipses stretched along the band (it is wide and short), planets stay round.
const EX := 1.75
const RADIUS := {"M": 0.58, "O": 0.58, "L": 0.53, "P": 0.5, "H": 0.53, "K": 0.4, "Y": 0.42, "GasGiant": 0.88}

var planets: Array = []          ## the system detail's planets [{name, class, capacity, maxCapacity}]
var structures: Dictionary = {}  ## the colony's structure counts (farms, factories, ...)
var star_color := Color(1.0, 0.86, 0.58)
var seed_ := 0
var spin := true                 ## orbits move (off under reduced motion and in captures that want a still)

var viewport: SubViewport
var camera: Camera3D
var world: Node3D
var overlay: Control
var home_index := -1
var _pivots: Array[Node3D] = []
var _bodies: Array[MeshInstance3D] = []
var structure_nodes := {}        ## kind -> count of nodes built (tests)

static func make(planets_: Array, structures_: Dictionary, star_color_: Color, seed_v: int, px: Vector2) -> SystemView3D:
	var v := SystemView3D.new()
	v.planets = planets_
	v.structures = structures_
	v.star_color = star_color_
	v.seed_ = seed_v
	v.custom_minimum_size = px
	v.stretch = true
	v.clip_contents = true
	v.name = "SystemView3D"
	return v

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_PASS
	viewport = SubViewport.new()
	viewport.own_world_3d = true
	viewport.msaa_3d = Viewport.MSAA_2X
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	add_child(viewport)
	_build()
	overlay = Overlay.new()
	overlay.view = self
	overlay.set_anchors_preset(Control.PRESET_FULL_RECT)
	overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(overlay)
	set_process(true)

func _build() -> void:
	world = Node3D.new()
	viewport.add_child(world)
	var env := WorldEnvironment.new()
	var e := Environment.new()
	e.background_mode = Environment.BG_COLOR
	e.background_color = Color(0.005, 0.007, 0.015)
	e.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	e.ambient_light_color = Color(0.12, 0.13, 0.18)
	e.ambient_light_energy = 0.5
	e.glow_enabled = true
	e.glow_intensity = 0.6
	env.environment = e
	world.add_child(env)
	camera = Camera3D.new()
	camera.fov = 34.0
	world.add_child(camera)
	# a few background stars
	var rng := RandomNumberGenerator.new()
	rng.seed = seed_ * 31 + 7
	var mm := MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	var q := QuadMesh.new()
	q.size = Vector2(0.03, 0.03)
	var qm := StandardMaterial3D.new()
	qm.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	qm.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
	qm.albedo_color = Color(0.85, 0.88, 1.0)
	q.material = qm
	mm.mesh = q
	mm.instance_count = 160
	for i in 160:
		var d := Vector3(rng.randf_range(-1, 1), rng.randf_range(-0.6, 0.6), rng.randf_range(-1, 1)).normalized() * 30.0
		mm.set_instance_transform(i, Transform3D(Basis.IDENTITY.scaled(Vector3.ONE * rng.randf_range(0.5, 1.6)), d))
	var stars := MultiMeshInstance3D.new()
	stars.multimesh = mm
	world.add_child(stars)
	# the star
	var star := MeshInstance3D.new()
	star.name = "Star"
	var sm := SphereMesh.new()
	sm.radius = 0.85
	sm.height = 1.7
	star.mesh = sm
	var smat := StandardMaterial3D.new()
	smat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	smat.albedo_color = star_color.lerp(Color.WHITE, 0.35)
	smat.emission_enabled = true
	smat.emission = star_color
	smat.emission_energy_multiplier = 3.0
	star.material_override = smat
	world.add_child(star)
	var halo := MeshInstance3D.new()
	var hq := QuadMesh.new()
	hq.size = Vector2(4.6, 4.6)
	halo.mesh = hq
	var hm := ShaderMaterial.new()
	hm.shader = _halo_shader()
	hm.set_shader_parameter("tint", star_color)
	halo.material_override = hm
	world.add_child(halo)
	var light := OmniLight3D.new()
	light.light_color = star_color.lerp(Color.WHITE, 0.55)
	light.light_energy = 2.2
	light.omni_range = 40.0
	light.omni_attenuation = 0.4
	world.add_child(light)
	# planets
	home_index = _home_planet()
	var n := planets.size()
	for i in n:
		var p: Dictionary = planets[i]
		var cls := str(p.get("class", "K"))
		var orbit_r := (1.6 + i * (5.4 / maxf(1.0, n - 1.0))) if n > 1 else 2.4
		var pivot := Node3D.new()
		pivot.name = "Orbit%d" % i
		pivot.set_meta("angle", GalaxyLayout.hash01(i, seed_, 3, 11) * TAU)
		pivot.set_meta("speed", 0.18 / sqrt(orbit_r))
		pivot.set_meta("radius", orbit_r)
		world.add_child(pivot)
		_pivots.append(pivot)
		var oring := _ring(orbit_r)
		oring.scale = Vector3(EX, 1, 1)
		world.add_child(oring)
		if cls == "Asteroids":
			pivot.scale = Vector3(EX, 1, 1)
			pivot.add_child(_belt(orbit_r, seed_ + i))
			_bodies.append(null)
			continue
		var mi := MeshInstance3D.new()
		mi.name = "Planet%d" % i
		var r: float = RADIUS.get(cls, 0.22)
		var ps := SphereMesh.new()
		ps.radius = r
		ps.height = r * 2.0
		ps.radial_segments = 48
		ps.rings = 24
		mi.mesh = ps
		mi.position = _orbit_pos(orbit_r, float(pivot.get_meta("angle")))
		var m := ShaderMaterial.new()
		m.shader = load("res://scenes/galaxy/shaders/planet_globe.gdshader")
		GalaxyMap._planet_look(m, cls)
		m.set_shader_parameter("seed", float(seed_ * 7 + i))
		m.set_shader_parameter("star_pos", Vector3.ZERO)
		m.set_shader_parameter("star_color", star_color.lerp(Color.WHITE, 0.55))
		var inhabited := int(p.get("capacity", 0)) > 0 and i == home_index
		m.set_shader_parameter("colonised", 1.0 if inhabited else 0.0)
		mi.material_override = m
		pivot.add_child(mi)
		_bodies.append(mi)
		PlanetBody.attach(mi, cls, {"star_pos": Vector3.ZERO, "variant": seed_ * 7 + i, "sun_color": star_color.lerp(Color.WHITE, 0.55), "inhabited": inhabited})
		if cls == "GasGiant":
			var ring := _ring(r * 1.7, Color(0.85, 0.75, 0.6, 0.35), 0.06)
			ring.rotation.x = 0.35
			mi.add_child(ring)
		if i == home_index:
			_structures(mi, r)
	_frame_camera()

## The colony's world: the inhabited planet with the most capacity.
func _home_planet() -> int:
	var best := -1
	var cap := 0
	for i in planets.size():
		var c := int(planets[i].get("capacity", 0))
		if c > cap and str(planets[i].get("class", "")) != "Asteroids":
			cap = c
			best = i
	return best

func _structures(host: MeshInstance3D, r: float) -> void:
	structure_nodes.clear()
	var rng := RandomNumberGenerator.new()
	rng.seed = seed_ * 101 + 5
	# surface works: lit sites spread over the globe, one per structure (capped so a big colony still reads)
	for kind in ["farms", "factories", "laboratories", "powerPlants", "fusionPlants", "refineries"]:
		var cnt := mini(int(structures.get(kind, 0)), 14)
		if cnt <= 0:
			continue
		var mm := MultiMesh.new()
		mm.transform_format = MultiMesh.TRANSFORM_3D
		var bm := BoxMesh.new()
		bm.size = Vector3(0.022, 0.022, 0.022)
		var mat := StandardMaterial3D.new()
		mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		mat.albedo_color = STRUCT_COLORS[kind]
		bm.material = mat
		mm.mesh = bm
		mm.instance_count = cnt
		for i in cnt:
			var d := Vector3(rng.randf_range(-1, 1), rng.randf_range(-0.7, 0.7), rng.randf_range(-1, 1)).normalized()
			var b := Basis.looking_at(d, Vector3.UP if absf(d.y) < 0.95 else Vector3.RIGHT)
			mm.set_instance_transform(i, Transform3D(b, d * r * 1.01))
		var mmi := MultiMeshInstance3D.new()
		mmi.name = "Surface_" + kind
		mmi.multimesh = mm
		host.add_child(mmi)
		structure_nodes[kind] = cnt
	# orbital shipyard: a ring dock with trusses
	if int(structures.get("shipyards", 0)) > 0:
		var yard := Node3D.new()
		yard.name = "Shipyard"
		var tor := TorusMesh.new()
		tor.inner_radius = r * 1.55
		tor.outer_radius = r * 1.62
		var ym := StandardMaterial3D.new()
		ym.albedo_color = Color(0.7, 0.72, 0.78)
		ym.metallic = 0.6
		ym.roughness = 0.4
		ym.emission_enabled = true
		ym.emission = Color(0.4, 0.6, 1.0)
		ym.emission_energy_multiplier = 0.4
		tor.material = ym
		var ti := MeshInstance3D.new()
		ti.mesh = tor
		yard.add_child(ti)
		for k in 6:
			var tr := MeshInstance3D.new()
			var bx := BoxMesh.new()
			bx.size = Vector3(0.02, 0.02, r * 0.5)
			bx.material = ym
			tr.mesh = bx
			var a := TAU * k / 6.0
			tr.position = Vector3(cos(a), 0, sin(a)) * r * 1.58
			tr.rotation.y = -a
			yard.add_child(tr)
		yard.rotation = Vector3(0.5, 0, 0.25)
		host.add_child(yard)
		structure_nodes["shipyards"] = 1
	# defence platforms on a polar orbit
	var def := mini(int(structures.get("defenseBatteries", 0)), 8)
	if def > 0:
		var ring := Node3D.new()
		ring.name = "Defence"
		ring.rotation.z = 1.2
		for k in def:
			var pl := MeshInstance3D.new()
			var pm := PrismMesh.new()
			pm.size = Vector3(0.05, 0.05, 0.05)
			var dm := StandardMaterial3D.new()
			dm.albedo_color = Color(0.9, 0.3, 0.25)
			dm.emission_enabled = true
			dm.emission = Color(1.0, 0.25, 0.2)
			dm.emission_energy_multiplier = 1.2
			pm.material = dm
			pl.mesh = pm
			var a := TAU * k / def
			pl.position = Vector3(cos(a), 0, sin(a)) * r * 2.1
			ring.add_child(pl)
		host.add_child(ring)
		structure_nodes["defenseBatteries"] = def

func _ring(radius: float, col: Color = Color(0.55, 0.7, 1.0, 0.22), w: float = 0.012) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	var t := TorusMesh.new()
	t.inner_radius = radius - w
	t.outer_radius = radius + w
	t.rings = 96
	t.ring_segments = 4
	var m := StandardMaterial3D.new()
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	m.albedo_color = col
	t.material = m
	mi.mesh = t
	return mi

func _belt(radius: float, s: int) -> MultiMeshInstance3D:
	var rng := RandomNumberGenerator.new()
	rng.seed = s
	var mm := MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	var sm := SphereMesh.new()
	sm.radius = 0.03
	sm.height = 0.05
	sm.radial_segments = 6
	sm.rings = 3
	var m := StandardMaterial3D.new()
	m.albedo_color = Color(0.5, 0.46, 0.42)
	sm.material = m
	mm.mesh = sm
	mm.instance_count = 220
	for i in 220:
		var a := rng.randf() * TAU
		var rr := radius + rng.randf_range(-0.18, 0.18)
		mm.set_instance_transform(i, Transform3D(Basis.IDENTITY.scaled(Vector3.ONE * rng.randf_range(0.4, 1.4)), Vector3(cos(a) * rr, rng.randf_range(-0.04, 0.04), sin(a) * rr)))
	var mmi := MultiMeshInstance3D.new()
	mmi.multimesh = mm
	return mmi

## Frames the whole system: a low pitch (the band is wide and short) at the distance where the outer orbit fills the width
## and the tilted ellipses fit the height.
func _frame_camera() -> void:
	var outer := _outer_radius() + 0.75
	var pitch := deg_to_rad(13.0)
	var vp := Vector2(viewport.size) if viewport and viewport.size.x > 0 else Vector2(1200, 300)
	var probe: Array[Vector3] = [Vector3(0, 1.0, 0), Vector3(0, -0.9, 0)]
	for k in 24:
		var ang := TAU * k / 24.0
		probe.append(Vector3(cos(ang) * outer * EX, 0, sin(ang) * outer))
	# walk out from close in until the outer orbit and the star all sit inside the frame with a margin (perspective-true)
	var d := outer * 0.8
	for it in 60:
		camera.position = Vector3(0, d * sin(pitch), d * cos(pitch))
		camera.look_at(Vector3(0, 0, 0.6), Vector3.UP)
		var fits := true
		for w in probe:
			if camera.is_position_behind(w):
				fits = false
				break
			var sp := camera.unproject_position(w)
			if sp.x < vp.x * 0.04 or sp.x > vp.x * 0.96 or sp.y < vp.y * 0.06 or sp.y > vp.y * 0.90:
				fits = false
				break
		if fits:
			break
		d *= 1.06

func _orbit_pos(r: float, a: float) -> Vector3:
	return Vector3(cos(a) * r * EX, 0, sin(a) * r)

func _outer_radius() -> float:
	var n := planets.size()
	return 1.6 + 5.4 if n > 1 else 2.4

func _notification(what: int) -> void:
	if what == NOTIFICATION_RESIZED and camera:
		_frame_camera.call_deferred()

func _process(delta: float) -> void:
	if spin and not UiTokens.reduced_motion:
		for i in _pivots.size():
			var p := _pivots[i]
			var a := float(p.get_meta("angle")) - float(p.get_meta("speed")) * delta
			p.set_meta("angle", a)
			if _bodies[i]:
				_bodies[i].position = _orbit_pos(float(p.get_meta("radius")), a)
			else:
				p.rotation.y = -a
		for b in _bodies:
			if b:
				b.rotation.y += delta * 0.25
	if overlay:
		overlay.queue_redraw()

## Screen position of planet i in this control's space (null-safe for belts).
func planet_screen(i: int) -> Vector2:
	if i < 0 or i >= _bodies.size():
		return Vector2(-1, -1)
	var b := _bodies[i]
	var w: Vector3 = b.global_position if b else _orbit_pos(float(_pivots[i].get_meta("radius")), float(_pivots[i].get_meta("angle")))
	if camera.is_position_behind(w):
		return Vector2(-1, -1)
	var vp := Vector2(viewport.size)
	var sp := camera.unproject_position(w)
	return sp * (size / vp) if vp.x > 0 else sp

func textures_pending() -> bool:
	return PlanetBody.pending > 0

static func _halo_shader() -> Shader:
	var s := Shader.new()
	s.code = """shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled;
uniform vec3 tint : source_color = vec3(1.0, 0.86, 0.58);
void vertex() { MODELVIEW_MATRIX = VIEW_MATRIX * mat4(INV_VIEW_MATRIX[0], INV_VIEW_MATRIX[1], INV_VIEW_MATRIX[2], MODEL_MATRIX[3]); }
void fragment() { float d = length(UV - 0.5) * 2.0; float a = pow(max(0.0, 1.0 - d), 2.4); ALBEDO = tint * a; ALPHA = a; }
"""
	return s

# ---------------------------------------------------------------------------------------------
## The LCARS overlay: each planet's numeral and class under it, the capacity, and the structure legend strip.
class Overlay extends Control:
	var view: SystemView3D
	func _draw() -> void:
		if view == null or view.camera == null:
			return
		var f := Lcars.font("display", 600)
		var mono := Lcars.font("mono", 400)
		for i in view.planets.size():
			var p: Dictionary = view.planets[i]
			var sp := view.planet_screen(i)
			if sp.x < 0:
				continue
			var nm := str(p.get("name", ""))
			var spc := nm.rfind(" ")
			var short := nm.substr(spc + 1) if spc > 0 else nm
			var cls := str(p.get("class", ""))
			var tag := "GG" if cls == "GasGiant" else ("BELT" if cls == "Asteroids" else cls)
			var col := Lcars.accent() if i == view.home_index else Lcars.TEXT2
			var at := sp + Vector2(0, 26)
			draw_line(sp + Vector2(0, 12), at - Vector2(0, 12), Color(col, 0.5), 1.0)
			draw_string(f, at - Vector2(40, 0), "%s  %s" % [short, tag], HORIZONTAL_ALIGNMENT_CENTER, 80, 15, col)
			if int(p.get("maxCapacity", 0)) > 0:
				draw_string(mono, at + Vector2(-40, 15), "%d/%d" % [int(p.get("capacity", 0)), int(p.get("maxCapacity", 0))], HORIZONTAL_ALIGNMENT_CENTER, 80, 11, Lcars.TEXT3)
		# legend: the structures shown on the colony world
		var x := 10.0
		var y := size.y - 12.0
		for kind in ["farms", "factories", "laboratories", "powerPlants", "shipyards", "defenseBatteries"]:
			var n := int(view.structures.get(kind, 0)) + (int(view.structures.get("fusionPlants", 0)) if kind == "powerPlants" else 0)
			if n <= 0:
				continue
			var c: Color = STRUCT_COLORS.get(kind, Color(0.7, 0.72, 0.78) if kind == "shipyards" else Color(1.0, 0.3, 0.25))
			draw_rect(Rect2(x, y - 10, 10, 10), c)
			var t := "%s %d" % [STRUCT_LABELS[kind], n]
			draw_string(f, Vector2(x + 14, y), t, HORIZONTAL_ALIGNMENT_LEFT, -1, 13, Lcars.TEXT2)
			x += 24.0 + f.get_string_size(t, HORIZONTAL_ALIGNMENT_LEFT, -1, 13).x
