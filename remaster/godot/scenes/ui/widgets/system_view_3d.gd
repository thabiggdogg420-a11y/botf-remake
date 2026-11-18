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
## Orbits are drawn as ellipses stretched along the band (it is wide and short), planets stay round. The view is a chart,
## not to scale: planets are drawn several times what their orbital spacing would allow so a world and its structures read
## in a 300 px band (the lead's F3 frame review: "a thin strip, planets tiny, the orbit plane nearly edge-on").
const EX := 2.35
const PITCH_DEG := 27.0
const RADIUS := {"M": 1.25, "O": 1.25, "L": 1.15, "P": 1.1, "H": 1.15, "K": 0.9, "Y": 0.95, "GasGiant": 1.75}
const STAR_R := 1.5
const ORBIT_IN := 3.3          ## first orbit
const ORBIT_SPAN := 7.2        ## first to last orbit

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
var selected := -1               ## the selected planet (the colony world unless the player clicks another): ringed, label lit
var left_inset := 0.0            ## px of the control's left edge under the title block; the system is framed right of it
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
	# the star: a white-hot core shading to the star's colour at the limb, then an additive corona. Colours stay inside 0..1
	# off Forward+ so the Compatibility renderer (web: no glow, no tonemap) draws the same white-yellow star as the exe
	# (the 0.1.469 web frame drew a flat pink disc: the old HDR emission clipped per channel there).
	var star := MeshInstance3D.new()
	star.name = "Star"
	var sm := SphereMesh.new()
	sm.radius = STAR_R
	sm.height = STAR_R * 2.0
	star.mesh = sm
	var smat := ShaderMaterial.new()
	smat.shader = _star_shader()
	smat.set_shader_parameter("tint", star_color)
	smat.set_shader_parameter("hdr", 1.0 if RenderingServer.get_current_rendering_method() == "forward_plus" else 0.0)
	star.material_override = smat
	world.add_child(star)
	var halo := MeshInstance3D.new()
	halo.name = "Corona"
	var hq := QuadMesh.new()
	hq.size = Vector2.ONE * STAR_R * 5.0
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
		var orbit_r := (ORBIT_IN + i * (ORBIT_SPAN / maxf(1.0, n - 1.0))) if n > 1 else ORBIT_IN + 1.0
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
		var r: float = RADIUS.get(cls, 0.9)
		var ps := SphereMesh.new()
		ps.radius = r
		ps.height = r * 2.0
		ps.radial_segments = 48
		ps.rings = 24
		mi.mesh = ps
		mi.position = _orbit_pos(orbit_r, float(pivot.get_meta("angle")))
		var m := ShaderMaterial.new()
		m.shader = AppLoader.resource("res://scenes/galaxy/shaders/planet_globe.gdshader")
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
	selected = home_index
	_select_ring()
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
	# surface works: lit sites over the camera-facing half of the globe, one per structure (capped so a big colony still
	# reads). They sit on a holder that does not turn with the globe (_process cancels the spin), above the cloud layer, so
	# the colony's works stay in view.
	var works := Node3D.new()
	works.name = "Works"
	host.add_child(works)
	_works = works
	for kind in ["farms", "factories", "laboratories", "powerPlants", "fusionPlants", "refineries"]:
		var cnt := mini(int(structures.get(kind, 0)), 14)
		if cnt <= 0:
			continue
		var mm := MultiMesh.new()
		mm.transform_format = MultiMesh.TRANSFORM_3D
		var bm := BoxMesh.new()
		bm.size = Vector3(0.12, 0.12, 0.07) * r
		var mat := StandardMaterial3D.new()
		mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		mat.albedo_color = STRUCT_COLORS[kind]
		bm.material = mat
		mm.mesh = bm
		mm.instance_count = cnt
		for i in cnt:
			var d := Vector3(rng.randf_range(-0.85, 0.85), rng.randf_range(-0.35, 0.8), rng.randf_range(0.35, 1.0)).normalized()
			var b := Basis.looking_at(d, Vector3.UP if absf(d.y) < 0.95 else Vector3.RIGHT)
			mm.set_instance_transform(i, Transform3D(b, d * r * 1.04))
		var mmi := MultiMeshInstance3D.new()
		mmi.name = "Surface_" + kind
		mmi.multimesh = mm
		works.add_child(mmi)
		structure_nodes[kind] = cnt
	# orbital shipyard: a ring dock with trusses
	if int(structures.get("shipyards", 0)) > 0:
		var yard := Node3D.new()
		yard.name = "Shipyard"
		var tor := TorusMesh.new()
		tor.inner_radius = r * 1.42
		tor.outer_radius = r * 1.5
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
			bx.size = Vector3(0.05, 0.05, r * 0.4) * maxf(1.0, r)
			bx.material = ym
			tr.mesh = bx
			var a := TAU * k / 6.0
			tr.position = Vector3(cos(a), 0, sin(a)) * r * 1.46
			tr.rotation.y = -a
			yard.add_child(tr)
		yard.rotation = Vector3(0.35, 0, 0.2)
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
			pm.size = Vector3.ONE * 0.11 * r
			var dm := StandardMaterial3D.new()
			dm.albedo_color = Color(0.9, 0.3, 0.25)
			dm.emission_enabled = true
			dm.emission = Color(1.0, 0.25, 0.2)
			dm.emission_energy_multiplier = 1.2
			pm.material = dm
			pl.mesh = pm
			var a := TAU * k / def
			pl.position = Vector3(cos(a), 0, sin(a)) * r * 1.8
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

## Frames the whole system: pitched PITCH_DEG down onto the orbit plane, at the distance where the outer orbit (with the
## planets riding it) fills the band right of the title block, then slid sideways (h_offset) so the star sits in the
## middle of that free area. Perspective-true: every probe is projected.
func _frame_camera() -> void:
	var outer := _outer_radius()
	var pitch := deg_to_rad(PITCH_DEG)
	var vp := Vector2(viewport.size) if viewport and viewport.size.x > 0 else Vector2(1600, 300)
	var sx := vp.x / size.x if size.x > 0 else 1.0
	var x0 := left_inset * sx
	var probe: Array[Vector3] = [Vector3(0, STAR_R, 0), Vector3(0, -STAR_R, 0)]
	for k in 24:
		var ang := TAU * k / 24.0
		probe.append(Vector3(cos(ang) * outer * EX, 0, sin(ang) * outer))
	var big := 0.0
	for p in planets:
		big = maxf(big, float(RADIUS.get(str(p.get("class", "")), 0.0)))
	var d := outer * 0.8
	for it in 120:
		camera.position = Vector3(0, d * sin(pitch), d * cos(pitch))
		camera.look_at(Vector3(0, 0, 0), Vector3.UP)
		# centre the system in the free area: px per world unit at the look point
		var upp := 2.0 * d * tan(deg_to_rad(camera.fov) * 0.5) / vp.y
		camera.h_offset = -(x0 * 0.5) * upp
		var pad := big / upp
		var top := pad * 0.5 + 4.0
		var bottom := vp.y - pad * 0.55 - 10.0
		# perspective puts the near half of the orbits lower and larger: slide the camera (v_offset) so the system sits
		# in the middle of the band's height instead of leaving the top empty
		camera.v_offset = 0.0
		var y_lo := 1e9
		var y_hi := -1e9
		for w in probe:
			if not camera.is_position_behind(w):
				var yy := camera.unproject_position(w).y
				y_lo = minf(y_lo, yy)
				y_hi = maxf(y_hi, yy)
		camera.v_offset = -((y_lo + y_hi) * 0.5 - (top + bottom) * 0.5) * upp
		var fits := true
		for w in probe:
			if camera.is_position_behind(w):
				fits = false
				break
			var sp := camera.unproject_position(w)
			if sp.x < x0 + pad * 0.6 or sp.x > vp.x - pad * 0.6 or sp.y < top or sp.y > bottom:
				fits = false
				break
		if fits:
			break
		d *= 1.025

## Px radius of planet i on screen (labels sit under it).
func planet_screen_radius(i: int) -> float:
	if i < 0 or i >= _bodies.size() or _bodies[i] == null or camera == null:
		return 6.0
	var bd := _bodies[i]
	var c := bd.global_position
	var r: float = (bd.mesh as SphereMesh).radius
	var vp := Vector2(viewport.size)
	var a0 := camera.unproject_position(c)
	var e := camera.unproject_position(c + camera.global_transform.basis.x * r)
	return a0.distance_to(e) * (size.x / vp.x if vp.x > 0 else 1.0)

## Selects planet i (a click on it): the ring moves there and its label lights.
func select(i: int) -> void:
	if i >= 0 and i < _bodies.size() and _bodies[i] != null:
		selected = i
		_select_ring()

var _ring_node: MeshInstance3D
var _works: Node3D
func _select_ring() -> void:
	if _ring_node:
		_ring_node.queue_free()
		_ring_node = null
	if selected < 0 or selected >= _bodies.size() or _bodies[selected] == null:
		return
	var r: float = (_bodies[selected].mesh as SphereMesh).radius
	_ring_node = _ring(r * 1.22, Color(Lcars.accent(), 0.85), 0.035)
	_ring_node.name = "SelectRing"
	_ring_node.position = _bodies[selected].position
	_bodies[selected].get_parent().add_child(_ring_node)

func _gui_input(ev: InputEvent) -> void:
	if ev is InputEventMouseButton and ev.pressed and ev.button_index == MOUSE_BUTTON_LEFT:
		var best := -1
		var bd := 1e9
		for i in _bodies.size():
			if _bodies[i] == null:
				continue
			var d := planet_screen(i).distance_to(ev.position)
			if d < planet_screen_radius(i) * 1.2 and d < bd:
				bd = d
				best = i
		if best >= 0:
			select(best)
			accept_event()

func _orbit_pos(r: float, a: float) -> Vector3:
	return Vector3(cos(a) * r * EX, 0, sin(a) * r)

func _outer_radius() -> float:
	var n := planets.size()
	return ORBIT_IN + ORBIT_SPAN if n > 1 else ORBIT_IN + 1.0

func _notification(what: int) -> void:
	if what == NOTIFICATION_RESIZED and camera:
		_frame_camera.call_deferred()

var _framed_for := Vector2.ZERO
func _process(delta: float) -> void:
	# the SubViewport takes its size from the container a frame after the resize note: frame again once it has
	if viewport and Vector2(viewport.size) != _framed_for and viewport.size.x > 0:
		_framed_for = Vector2(viewport.size)
		_frame_camera()
	if spin and not UiTokens.reduced_motion:
		for i in _pivots.size():
			var p := _pivots[i]
			var a := float(p.get_meta("angle")) - float(p.get_meta("speed")) * delta
			p.set_meta("angle", a)
			if _bodies[i]:
				_bodies[i].position = _orbit_pos(float(p.get_meta("radius")), a)
				if i == selected and _ring_node:
					_ring_node.position = _bodies[i].position
			else:
				p.rotation.y = -a
		for b in _bodies:
			if b:
				b.rotation.y += delta * 0.12
		if _works:
			_works.rotation.y = -_works.get_parent().rotation.y
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

static func _star_shader() -> Shader:
	var s := Shader.new()
	s.code = """shader_type spatial;
render_mode unshaded;
uniform vec3 tint : source_color = vec3(1.0, 0.86, 0.58);
uniform float hdr = 1.0;
void fragment() {
	float ndv = clamp(dot(NORMAL, VIEW), 0.0, 1.0);
	vec3 c = mix(tint, vec3(1.0, 0.985, 0.93), smoothstep(0.15, 0.85, ndv));
	ALBEDO = c * (1.0 + hdr * 1.6 * ndv);
}
"""
	return s

static func _halo_shader() -> Shader:
	var s := Shader.new()
	s.code = """shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled;
uniform vec3 tint : source_color = vec3(1.0, 0.86, 0.58);
void vertex() { MODELVIEW_MATRIX = VIEW_MATRIX * mat4(INV_VIEW_MATRIX[0], INV_VIEW_MATRIX[1], INV_VIEW_MATRIX[2], MODEL_MATRIX[3]); }
void fragment() { float d = length(UV - 0.5) * 2.0; float a = pow(max(0.0, 1.0 - d), 2.2) * 0.85; ALBEDO = mix(tint, vec3(1.0, 0.97, 0.88), a); ALPHA = a; }
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
			var col := Lcars.accent() if i == view.selected else (Lcars.TEXT if i == view.home_index else Lcars.TEXT2)
			# the label sits on a dark plate just under the globe (belts: at the orbit point)
			var pr := view.planet_screen_radius(i) if view._bodies[i] != null else 6.0
			var t := "%s  %s" % [short, tag]
			var cap := "%d/%d" % [int(p.get("capacity", 0)), int(p.get("maxCapacity", 0))] if int(p.get("maxCapacity", 0)) > 0 else ""
			var w := maxf(f.get_string_size(t, HORIZONTAL_ALIGNMENT_LEFT, -1, 18).x, mono.get_string_size(cap, HORIZONTAL_ALIGNMENT_LEFT, -1, 14).x) + 14.0
			var hgt := 22.0 + (16.0 if cap != "" else 0.0)
			var top := minf(sp.y + pr * 0.72, size.y - hgt - 4.0)
			var plate := StyleBoxFlat.new()
			plate.bg_color = Color(0.0, 0.0, 0.02, 0.62)
			plate.set_corner_radius_all(6)
			plate.border_color = Color(col, 0.6 if i == view.selected else 0.0)
			plate.set_border_width_all(1)
			draw_style_box(plate, Rect2(sp.x - w * 0.5, top, w, hgt))
			draw_string(f, Vector2(sp.x - w * 0.5, top + 18), t, HORIZONTAL_ALIGNMENT_CENTER, w, 18, col)
			if cap != "":
				draw_string(mono, Vector2(sp.x - w * 0.5, top + 34), cap, HORIZONTAL_ALIGNMENT_CENTER, w, 14, Lcars.TEXT2)
		# legend: the structures shown on the colony world
		var x := view.left_inset + 14.0
		var y := size.y - 12.0
		for kind in ["farms", "factories", "laboratories", "powerPlants", "shipyards", "defenseBatteries"]:
			var n := int(view.structures.get(kind, 0)) + (int(view.structures.get("fusionPlants", 0)) if kind == "powerPlants" else 0)
			if n <= 0:
				continue
			var c: Color = STRUCT_COLORS.get(kind, Color(0.7, 0.72, 0.78) if kind == "shipyards" else Color(1.0, 0.3, 0.25))
			draw_rect(Rect2(x, y - 11, 11, 11), c)
			var lt := "%s %d" % [STRUCT_LABELS[kind], n]
			draw_string(f, Vector2(x + 15, y), lt, HORIZONTAL_ALIGNMENT_LEFT, -1, 15, Lcars.TEXT2)
			x += 26.0 + f.get_string_size(lt, HORIZONTAL_ALIGNMENT_LEFT, -1, 15).x
