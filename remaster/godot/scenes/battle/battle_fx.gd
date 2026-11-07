class_name BattleFx
extends Node3D
## Draws BattleTimeline visual jobs. Job visuals (beams, bolts, torpedoes and their trails, flares, shockwaves) are
## a pure function of the timeline time, so seeking works; one-shot extras (particle bursts, flash lights, scorch
## decals, shield hits) start when a job begins and free themselves.

const BEAM_SHADER := preload("res://scenes/battle/shaders/beam.gdshader")
const FLARE_SHADER := preload("res://scenes/battle/shaders/flare.gdshader")
const TRAIL_SHADER := preload("res://scenes/battle/shaders/trail.gdshader")
const SHOCK_SHADER := preload("res://scenes/battle/shaders/shockwave.gdshader")
const Job := BattleTimeline.Job

var scene: Node       ## BattleScene: ship_node(fleet_id), cell size, camera.
var _active := {}     ## job index -> {nodes: Array, ...}
var _tl: BattleTimeline
var _beam_mat: ShaderMaterial
var _flare_mat: ShaderMaterial
var _trail_mat: ShaderMaterial
var _shock_mat: ShaderMaterial
var _cyl: CylinderMesh
var _quad: QuadMesh
var _plane: PlaneMesh
var _soft_tex: Texture2D
var _puff_tex: Texture2D
var _particle_mats := {}


func _ready() -> void:
	_beam_mat = _shader_mat(BEAM_SHADER)
	_flare_mat = _shader_mat(FLARE_SHADER)
	_trail_mat = _shader_mat(TRAIL_SHADER)
	_shock_mat = _shader_mat(SHOCK_SHADER)
	_cyl = CylinderMesh.new()
	_cyl.top_radius = 1.0
	_cyl.bottom_radius = 1.0
	_cyl.height = 1.0
	_cyl.radial_segments = 16
	_cyl.rings = 1
	_cyl.cap_top = false
	_cyl.cap_bottom = false
	_quad = QuadMesh.new()
	_quad.size = Vector2(1, 1)
	_plane = PlaneMesh.new()
	_plane.size = Vector2(2, 2)
	var g := Gradient.new()
	g.set_color(0, Color(1, 1, 1, 1))
	g.set_color(1, Color(1, 1, 1, 0))
	g.add_point(0.35, Color(1, 1, 1, 0.55))
	var gt := GradientTexture2D.new()
	gt.gradient = g
	gt.fill = GradientTexture2D.FILL_RADIAL
	gt.fill_from = Vector2(0.5, 0.5)
	gt.fill_to = Vector2(1.0, 0.5)
	gt.width = 64
	gt.height = 64
	_soft_tex = gt
	_puff_tex = ImageTexture.create_from_image(_puff_image(128))


## Billowy fire/smoke puff: radial falloff broken up by fractal noise.
static func _puff_image(size: int) -> Image:
	var img := Image.create(size, size, false, Image.FORMAT_RGBA8)
	var noise := FastNoiseLite.new()
	noise.seed = 11
	noise.frequency = 0.045
	noise.fractal_octaves = 5
	for y in size:
		for x in size:
			var p := Vector2(x, y) / (size - 1) * 2.0 - Vector2.ONE
			var n := noise.get_noise_2d(x, y) * 0.5 + 0.5
			var a := clampf((1.0 - p.length()) * 1.5, 0.0, 1.0)
			a = clampf(a * (0.35 + 1.1 * n) - 0.1, 0.0, 1.0)
			var v := 0.75 + 0.25 * n
			img.set_pixel(x, y, Color(v, v, v, a))
	return img


static func _shader_mat(shader: Shader) -> ShaderMaterial:
	var m := ShaderMaterial.new()
	m.shader = shader
	return m


## Draws one of every effect material once, tiny and just in front of the camera, then frees them. The first real
## volley otherwise compiles its pipelines mid-round (a 1.35 s freeze on a fresh shader cache on the PC).
func prewarm() -> void:
	var cam := get_viewport().get_camera_3d()
	if cam == null:
		return
	var at := cam.global_position - cam.global_basis.z * (cam.near + 2.0)
	var nodes: Array[Node] = []
	var beam := _beam_node(Color(1, 0.6, 0.2))
	_orient_beam(beam, at, at + cam.global_basis.x * 0.01, 0.002)
	nodes.append(beam)
	var fl := _flare(Color(1, 0.5, 0.2), 0.01)
	fl.global_position = at
	nodes.append(fl)
	var tr := _trail_node()
	_draw_ribbon(tr, PackedVector3Array([at, at + cam.global_basis.x * 0.01]), 0.002, Color(1, 0.5, 0.2))
	nodes.append(tr)
	var ring := MeshInstance3D.new()
	ring.mesh = _plane
	ring.material_override = _shock_mat
	ring.scale = Vector3.ONE * 0.005
	add_child(ring)
	ring.global_position = at
	nodes.append(ring)
	var before := get_child_count()
	for kind in ["sparks", "sparks_small", "fire_small", "fireball", "smoke", "debris"]:
		_burst(at, Color(1, 0.5, 0.2), kind, 1, 0.01)
	for i in range(before, get_child_count()):
		nodes.append(get_child(i))
	for i in 3:
		await get_tree().process_frame
	for n in nodes:
		if is_instance_valid(n):
			n.queue_free()


## Clears job visuals (one-shot particles and decals finish on their own).
func reset(tl: BattleTimeline) -> void:
	for k in _active.keys():
		_despawn(k)
	_active.clear()
	_tl = tl


func update(t: float) -> void:
	if _tl == null:
		return
	for i in _tl.jobs.size():
		var j: Dictionary = _tl.jobs[i]
		var live: bool = t >= j.start and t <= j.start + j.duration
		if live and not _active.has(i):
			_active[i] = {"nodes": []}
			_spawn(i, j)
		elif not live and _active.has(i):
			_despawn(i)
		if _active.has(i):
			_animate(i, j, t)


func _despawn(i: int) -> void:
	for n in _active[i].nodes:
		if is_instance_valid(n):
			n.queue_free()
	_active.erase(i)


# ------------------------------------------------------------------------------------------------------------------
# Geometry helpers
# ------------------------------------------------------------------------------------------------------------------

func _ship(fleet_id: int) -> BattleShip:
	return scene.ship_node(fleet_id)


func _cell() -> float:
	return scene.CELL


func _cam_pos() -> Vector3:
	var cam := get_viewport().get_camera_3d()
	return cam.global_position if cam != null else Vector3(0, 50, 50)


## Where a shot from `src` ends: shield shell or hull on a hit, past the target on a miss.
func _shot_end(j: Dictionary, src: Vector3, target: BattleShip) -> Vector3:
	if target == null:
		return src + Vector3(10, 0, 0)
	if j.outcome == BattleTimeline.Outcome.MISS:
		var d := (target.global_position - src)
		var rng := RandomNumberGenerator.new()
		rng.seed = hash([j.event_index, j.index])
		var side := d.cross(Vector3.UP).normalized() * (target.radius() * rng.randf_range(1.2, 2.2) * (1.0 if rng.randf() < 0.5 else -1.0))
		var up := Vector3.UP * rng.randf_range(-1.0, 1.0) * target.radius()
		return target.global_position + d.normalized() * j.overshoot_cells * _cell() + side + up
	if j.shield_damage > 0 or j.hull_damage <= 0:
		return target.shield_point(src)
	return target.hull_point(src, hash([j.event_index, j.index]))


func _flare(color: Color, size: float, intensity := 1.0, spikes := 0.0) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	mi.mesh = _quad
	mi.material_override = _flare_mat
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	mi.scale = Vector3.ONE * size
	add_child(mi)
	mi.set_instance_shader_parameter("flare_color", color)
	mi.set_instance_shader_parameter("flare_intensity", intensity)
	mi.set_instance_shader_parameter("flare_spikes", spikes)
	return mi


func _beam_node(color: Color) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	mi.mesh = _cyl
	mi.material_override = _beam_mat
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(mi)
	mi.set_instance_shader_parameter("beam_color", color)
	return mi


static func _orient_beam(mi: Node3D, a: Vector3, b: Vector3, radius: float) -> void:
	var d := b - a
	var length := maxf(d.length(), 1e-3)
	var y := d / length
	var x := y.cross(Vector3.UP if absf(y.dot(Vector3.UP)) < 0.95 else Vector3.RIGHT).normalized()
	var z := x.cross(y)
	mi.transform = Transform3D(Basis(x * radius, y * length, z * radius), (a + b) * 0.5)


func _trail_node() -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	mi.mesh = ImmediateMesh.new()
	mi.material_override = _trail_mat
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(mi)
	return mi


## Rebuilds a camera-facing ribbon through `points` (head first) with the given width and colour fade.
func _draw_ribbon(mi: MeshInstance3D, points: PackedVector3Array, width: float, color: Color) -> void:
	var im: ImmediateMesh = mi.mesh
	im.clear_surfaces()
	if points.size() < 2:
		return
	var cam := _cam_pos()
	im.surface_begin(Mesh.PRIMITIVE_TRIANGLE_STRIP)
	var n := points.size()
	for k in n:
		var p := points[k]
		var dir := (points[maxi(k - 1, 0)] - points[mini(k + 1, n - 1)]).normalized()
		var side := dir.cross((cam - p).normalized()).normalized()
		var f := 1.0 - float(k) / (n - 1)
		var w := width * (0.35 + 0.65 * f)
		var c := Color(color.r, color.g, color.b, f * f)
		im.surface_set_color(c)
		im.surface_set_uv(Vector2(f, 0))
		im.surface_add_vertex(p + side * w)
		im.surface_set_color(c)
		im.surface_set_uv(Vector2(f, 1))
		im.surface_add_vertex(p - side * w)
	im.surface_end()


func _torpedo_pos(j: Dictionary, u: float) -> Vector3:
	var shooter := _ship(j.fleet_id)
	var target := _ship(j.target_fleet_id)
	if shooter == null:
		return Vector3.ZERO
	var src := shooter.hardpoint(2, j.mount_index, j.index)
	var dst := target.shield_point(src) if target != null else src + Vector3(20, 0, 0)
	if j.outcome == BattleTimeline.Outcome.HIT and target != null and j.shield_damage <= 0 and j.hull_damage > 0:
		dst = target.hull_point(src, hash([j.event_index, j.index]))
	var d := dst - src
	var length := d.length()
	if length > 1e-6 and j.overshoot_cells > 0.0:
		dst = src + d * ((length + j.overshoot_cells * _cell()) / length)
		d = dst - src
	var n := d.cross(Vector3.UP).normalized()
	var bow := 4.0 * u * (1.0 - u)
	var arc: float = j.arc_cells * _cell()
	return src + d * u + n * arc * bow + Vector3.UP * absf(arc) * bow * 0.8


# ------------------------------------------------------------------------------------------------------------------
# Jobs
# ------------------------------------------------------------------------------------------------------------------

func _spawn(i: int, j: Dictionary) -> void:
	var st: Dictionary = _active[i]
	var col: Color = j.color
	match j.kind:
		Job.BEAM:
			var beam := _beam_node(col)
			beam.set_instance_shader_parameter("beam_phase", float(i))
			st.nodes = [beam, _flare(col, 0.6, 1.0, 0.4), _flare(col, 1.5, 1.0, 0.8)]
		Job.PULSE:
			var bolt := _beam_node(col)
			st.nodes = [bolt, _flare(col, 1.6, 1.2), _flare(col, 1.4, 1.0, 0.8)]
		Job.TORPEDO:
			st.nodes = [_flare(col, 1.5, 1.2, 1.0), _trail_node(), _flare(col, 0.7, 1.0, 0.0)]
			var shooter := _ship(j.fleet_id)
			if shooter != null:
				_flash_light(shooter.hardpoint(2, j.mount_index, j.index), col, 1.2, 6.0, 0.25)
		Job.INTERCEPT:
			var tpos := _torpedo_pos(j, j.end_fraction if j.end_fraction < 1.0 else 0.78)
			st.point = tpos
			st.nodes = [_flare(Color(1.0, 0.8, 0.5), 5.0, 1.5, 1.2), _beam_node(Color(1.0, 0.6, 0.2)), _beam_node(Color(1.0, 0.6, 0.2))]
			_burst(tpos, Color(1.0, 0.7, 0.3), "sparks", 40, 1.0)
			_flash_light(tpos, Color(1.0, 0.7, 0.4), 8.0, 14.0, 0.4)
		Job.SHIELD:
			var target := _ship(j.target_fleet_id)
			var source := _ship(j.source_fleet_id)
			if target != null:
				var from := source.global_position if source != null else target.global_position + Vector3(10, 0, 0)
				target.add_shield_hit(from, j.start, j.duration * 1.8, col.lerp(Color(0.5, 0.8, 1.0), 0.45))
				st.point = target.shield_point(from)
				st.nodes = [_flare(col.lerp(Color.WHITE, 0.3), 3.0, 1.0, 0.8)]
				_burst(st.point, col.lerp(Color(0.6, 0.85, 1.0), 0.5), "sparks_small", 14, 0.6)
		Job.HULL:
			var target := _ship(j.target_fleet_id)
			var source := _ship(j.source_fleet_id)
			if target != null:
				var from := source.global_position if source != null else target.global_position + Vector3(10, 0, 0)
				var p := target.hull_point(from, hash([j.event_index, 7]))
				st.point = p
				st.nodes = [_flare(Color(1.0, 0.6, 0.25), 3.5, 1.4, 1.0)]
				var n: int = clampi(10 + j.hull_damage, 16, 70)
				_burst(p, Color(1.0, 0.55, 0.15), "sparks", n, 1.0, (from - p).normalized())
				_burst(p, Color(1.0, 0.4, 0.1), "fire_small", 10, 0.6)
				_flash_light(p, Color(1.0, 0.55, 0.2), 3.0, 8.0, 0.5)
				target.add_scorch(p, from, clampf(target.length * 0.18, 0.6, 2.2))
		Job.RAM:
			st.nodes = [_flare(Color(1.0, 0.6, 0.2), 4.0, 0.0, 1.0)]
		Job.DECLOAK:
			var s := _ship(j.fleet_id)
			if s != null:
				st.nodes = [_flare(col, s.length * 1.4, 0.0, 0.6)]
		Job.EXPLOSION:
			var s := _ship(j.fleet_id)
			if s != null:
				var p := s.global_position
				var size := s.length
				st.point = p
				st.size = size
				var ring := MeshInstance3D.new()
				ring.mesh = _plane
				ring.material_override = _shock_mat
				ring.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
				add_child(ring)
				ring.global_position = p
				ring.rotation_degrees = Vector3(rng_angle(i, 20.0), rng_angle(i + 3, 180.0), rng_angle(i + 5, 20.0))
				st.nodes = [_flare(Color(1.0, 0.75, 0.4), size * 1.5, 2.0, 1.5), ring]
				_burst(p, Color(1.0, 0.5, 0.15), "fireball", 160, size / 5.0)
				_burst(p, Color(1.0, 0.7, 0.3), "sparks", 90, size / 3.0)
				_burst(p, Color(1.0, 0.45, 0.1), "debris", 30, size / 4.0)
				_flash_light(p, Color(1.0, 0.65, 0.3), 7.0, size * 6.0, 1.4)
		Job.WARP:
			var s := _ship(j.fleet_id)
			if s != null:
				st.nodes = [_flare(Color(0.7, 0.85, 1.0), s.length * 1.5, 0.0, 1.5)]
		_:
			pass


static func rng_angle(seed: int, spread: float) -> float:
	var r := RandomNumberGenerator.new()
	r.seed = seed * 7919 + 13
	return r.randf_range(-spread, spread)


func _animate(i: int, j: Dictionary, t: float) -> void:
	var st: Dictionary = _active[i]
	var u := clampf((t - j.start) / maxf(j.duration, 1e-4), 0.0, 1.0)
	var col: Color = j.color
	match j.kind:
		Job.BEAM:
			var shooter := _ship(j.fleet_id)
			var target := _ship(j.target_fleet_id)
			if shooter == null or st.nodes.size() < 3:
				return
			var src := shooter.hardpoint(0, j.mount_index, j.index)
			var dst := _shot_end(j, src, target)
			var reach := clampf(u / 0.1, 0.0, 1.0)
			var env := smoothstep(0.0, 0.06, u) * (1.0 - smoothstep(0.75, 1.0, u))
			var end := src.lerp(dst, reach)
			var width := 0.09 if j.weapon.begins_with("Phaser") else 0.12
			_orient_beam(st.nodes[0], src, end, width * (0.7 + 0.3 * env) * 1.6)
			st.nodes[0].set_instance_shader_parameter("beam_intensity", env * 1.3)
			st.nodes[1].global_position = src
			st.nodes[1].set_instance_shader_parameter("flare_intensity", env * 0.8)
			st.nodes[2].global_position = end
			var hit: bool = j.outcome == BattleTimeline.Outcome.HIT and reach >= 1.0
			st.nodes[2].set_instance_shader_parameter("flare_intensity", env * (1.3 if hit else 0.0))
			st.nodes[2].set_instance_shader_parameter("flare_rotation", t * 3.0)
		Job.PULSE:
			var shooter := _ship(j.fleet_id)
			var target := _ship(j.target_fleet_id)
			if shooter == null:
				return
			var src := shooter.hardpoint(1, j.mount_index, j.index)
			var dst := _shot_end(j, src, target)
			var head := src.lerp(dst, u)
			var tail := src.lerp(dst, maxf(0.0, u - 0.18))
			_orient_beam(st.nodes[0], tail, head, 0.16)
			st.nodes[0].set_instance_shader_parameter("beam_intensity", 1.6)
			st.nodes[1].global_position = head
			st.nodes[2].global_position = src
			st.nodes[2].set_instance_shader_parameter("flare_intensity", maxf(0.0, 1.0 - u * 5.0))
		Job.TORPEDO:
			var path_u: float = u * j.end_fraction
			var head := _torpedo_pos(j, path_u)
			st.nodes[0].global_position = head
			var pulse := 1.0 + 0.3 * sin(t * 40.0 + i)
			var fade := 1.0 - smoothstep(0.85, 1.0, u) if j.outcome == BattleTimeline.Outcome.MISS else 1.0
			st.nodes[0].set_instance_shader_parameter("flare_intensity", pulse * fade)
			st.nodes[0].set_instance_shader_parameter("flare_rotation", t * 2.0 + i)
			var pts := PackedVector3Array()
			for k in 18:
				pts.append(_torpedo_pos(j, maxf(0.0, path_u - k * 0.018)))
			_draw_ribbon(st.nodes[1], pts, 0.35, col * fade)
			var shooter := _ship(j.fleet_id)
			if shooter != null:
				st.nodes[2].global_position = shooter.hardpoint(2, j.mount_index, j.index)
				st.nodes[2].set_instance_shader_parameter("flare_intensity", maxf(0.0, 1.0 - u * 8.0))
		Job.INTERCEPT:
			var p: Vector3 = st.point
			st.nodes[0].global_position = p
			st.nodes[0].set_instance_shader_parameter("flare_intensity", 2.0 * pow(1.0 - u, 2.0))
			var target := _ship(j.target_fleet_id)
			for k in 2:
				var beam: MeshInstance3D = st.nodes[1 + k]
				if target == null or u > 0.45:
					beam.visible = false
					continue
				var src := target.hardpoint(0, k, 0)
				_orient_beam(beam, src, p, 0.06)
				beam.set_instance_shader_parameter("beam_intensity", 1.0 - u / 0.45)
		Job.SHIELD, Job.HULL:
			if st.nodes.is_empty():
				return
			st.nodes[0].global_position = st.point
			st.nodes[0].set_instance_shader_parameter("flare_intensity", 1.6 * pow(1.0 - u, 2.5))
		Job.RAM:
			var a := _ship(j.fleet_id)
			var b := _ship(j.target_fleet_id)
			if a != null and b != null:
				st.nodes[0].global_position = a.global_position.lerp(b.global_position, 0.5)
				st.nodes[0].set_instance_shader_parameter("flare_intensity", smoothstep(0.6, 0.8, u) * (1.0 - smoothstep(0.8, 1.0, u)) * 3.0)
		Job.DECLOAK:
			var s := _ship(j.fleet_id)
			if s != null and not st.nodes.is_empty():
				st.nodes[0].global_position = s.global_position
				st.nodes[0].set_instance_shader_parameter("flare_intensity", sin(PI * u) * 0.6)
		Job.EXPLOSION:
			if st.nodes.is_empty():
				return
			var size: float = st.size
			st.nodes[0].global_position = st.point
			st.nodes[0].set_instance_shader_parameter("flare_intensity", 2.5 * exp(-u * 7.0) + 0.2 * (1.0 - u))
			st.nodes[0].scale = Vector3.ONE * size * (1.5 + 2.0 * u)
			for k in [1]:
				var ring: MeshInstance3D = st.nodes[k]
				var r := size * (0.3 + 2.6 * (1.0 - pow(1.0 - u, 2.5)))
				ring.scale = Vector3(r, 1.0, r)
				ring.set_instance_shader_parameter("ring_age", u)
		Job.WARP:
			var s := _ship(j.fleet_id)
			if s != null and not st.nodes.is_empty():
				var bow := s.global_transform * Vector3(s.half_extents.x * (1.0 + 10.0 * u * u * u), 0, 0)
				st.nodes[0].global_position = bow
				st.nodes[0].set_instance_shader_parameter("flare_intensity", smoothstep(0.6, 0.9, u) * 3.0 * (1.0 - smoothstep(0.9, 1.0, u) * 0.7))
				st.nodes[0].set_instance_shader_parameter("flare_stretch", 3.0)


# ------------------------------------------------------------------------------------------------------------------
# One-shot extras
# ------------------------------------------------------------------------------------------------------------------

func _flash_light(p: Vector3, color: Color, energy: float, light_range: float, seconds: float) -> void:
	var l := OmniLight3D.new()
	l.light_color = color
	l.light_energy = energy
	l.omni_range = light_range
	l.omni_attenuation = 1.5
	l.shadow_enabled = false
	add_child(l)
	l.global_position = p
	var tw := l.create_tween()
	tw.tween_property(l, "light_energy", 0.0, seconds).set_trans(Tween.TRANS_EXPO).set_ease(Tween.EASE_OUT)
	tw.tween_callback(l.queue_free)


func _particle_material(kind: String, color: Color) -> Array:
	var key := kind + color.to_html()
	if _particle_mats.has(key):
		return _particle_mats[key]
	var pm := ParticleProcessMaterial.new()
	pm.direction = Vector3(0, 0, 1)
	pm.spread = 180.0
	pm.gravity = Vector3.ZERO
	var draw := StandardMaterial3D.new()
	draw.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	draw.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	draw.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	draw.vertex_color_use_as_albedo = true
	draw.albedo_texture = _puff_tex if kind in ["fireball", "smoke", "fire_small"] else _soft_tex
	draw.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	draw.disable_receive_shadows = true
	var ramp := Gradient.new()
	var mesh := QuadMesh.new()
	mesh.size = Vector2(1, 1)
	var align := false
	var hdr := func(c: Color, m: float, a: float) -> Color: return Color(c.r * m, c.g * m, c.b * m, a)
	match kind:
		"sparks", "sparks_small":
			pm.initial_velocity_min = 6.0 if kind == "sparks" else 3.0
			pm.initial_velocity_max = 18.0 if kind == "sparks" else 8.0
			pm.damping_min = 4.0
			pm.damping_max = 8.0
			pm.scale_min = 0.6
			pm.scale_max = 1.0
			ramp.set_color(0, hdr.call(Color(1, 0.9, 0.7), 5.0, 1.0))
			ramp.set_color(1, hdr.call(color, 2.0, 0.0))
			mesh.size = Vector2(0.05, 0.55)
			pm.particle_flag_align_y = true
			draw.billboard_mode = BaseMaterial3D.BILLBOARD_DISABLED
			align = true
		"fire_small":
			pm.initial_velocity_min = 0.5
			pm.initial_velocity_max = 2.0
			pm.scale_min = 0.6
			pm.scale_max = 1.2
			ramp.set_color(0, hdr.call(Color(1, 0.8, 0.5), 5.0, 1.0))
			ramp.set_color(1, hdr.call(color, 1.0, 0.0))
		"fireball":
			pm.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_SPHERE
			pm.emission_sphere_radius = 1.2
			pm.initial_velocity_min = 1.5
			pm.initial_velocity_max = 7.0
			pm.damping_min = 1.0
			pm.damping_max = 2.5
			pm.scale_min = 1.6
			pm.scale_max = 4.0
			var curve := Curve.new()
			curve.add_point(Vector2(0, 0.25))
			curve.add_point(Vector2(0.3, 0.9))
			curve.add_point(Vector2(1, 1.3))
			var ct := CurveTexture.new()
			ct.curve = curve
			pm.scale_curve = ct
			ramp.set_color(0, hdr.call(Color(1, 0.85, 0.6), 3.0, 0.95))
			ramp.set_color(1, Color(0.1, 0.02, 0.01, 0.0))
			ramp.add_point(0.12, hdr.call(Color(1, 0.55, 0.2), 2.2, 0.8))
			ramp.add_point(0.4, hdr.call(color, 1.1, 0.5))
			ramp.add_point(0.7, hdr.call(Color(0.5, 0.12, 0.04), 0.8, 0.18))
			pm.angle_min = -180.0
			pm.angle_max = 180.0
		"smoke":
			pm.initial_velocity_min = 0.5
			pm.initial_velocity_max = 3.0
			pm.damping_min = 0.5
			pm.damping_max = 1.0
			pm.scale_min = 3.0
			pm.scale_max = 6.0
			draw.blend_mode = BaseMaterial3D.BLEND_MODE_MIX
			ramp.set_color(0, Color(color.r, color.g, color.b, 0.0))
			ramp.add_point(0.2, Color(color.r, color.g, color.b, 0.45))
			ramp.set_color(1, Color(color.r * 0.5, color.g * 0.5, color.b * 0.5, 0.0))
			pm.angle_min = -180.0
			pm.angle_max = 180.0
		"debris":
			pm.initial_velocity_min = 2.0
			pm.initial_velocity_max = 9.0
			pm.damping_min = 0.2
			pm.damping_max = 0.6
			pm.scale_min = 0.5
			pm.scale_max = 1.0
			pm.particle_flag_align_y = true
			draw.billboard_mode = BaseMaterial3D.BILLBOARD_DISABLED
			mesh.size = Vector2(0.06, 0.3)
			align = true
			ramp.set_color(0, hdr.call(Color(1, 0.6, 0.2), 3.0, 1.0))
			ramp.add_point(0.5, hdr.call(color, 1.2, 0.8))
			ramp.set_color(1, Color(0.3, 0.08, 0.02, 0.0))
	var gt := GradientTexture1D.new()
	gt.gradient = ramp
	gt.use_hdr = true
	pm.color_ramp = gt
	mesh.material = draw
	var entry := [pm, mesh, align]
	_particle_mats[key] = entry
	return entry


func _burst(p: Vector3, color: Color, kind: String, amount: int, scale_factor := 1.0, dir := Vector3.ZERO) -> void:
	var e := _particle_material(kind, color)
	var gp := GPUParticles3D.new()
	gp.process_material = e[0]
	gp.draw_pass_1 = e[1]
	gp.amount = maxi(1, amount)
	gp.one_shot = true
	gp.explosiveness = 0.95 if kind != "smoke" else 0.7
	gp.lifetime = {"sparks": 0.9, "sparks_small": 0.5, "fire_small": 0.6, "fireball": 1.8, "smoke": 3.5, "debris": 2.5}.get(kind, 1.0)
	gp.speed_scale = 1.0
	gp.randomness = 0.5
	gp.fixed_fps = 0
	gp.local_coords = false
	gp.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	gp.visibility_aabb = AABB(Vector3(-40, -40, -40), Vector3(80, 80, 80))
	if e[2]:
		gp.transform_align = GPUParticles3D.TRANSFORM_ALIGN_Z_BILLBOARD_Y_TO_VELOCITY
	add_child(gp)
	gp.global_position = p
	if dir != Vector3.ZERO or not is_equal_approx(scale_factor, 1.0):
		var pm: ParticleProcessMaterial = (e[0] as ParticleProcessMaterial).duplicate()
		pm.initial_velocity_min *= scale_factor
		pm.initial_velocity_max *= scale_factor
		pm.scale_min *= scale_factor
		pm.scale_max *= scale_factor
		pm.emission_sphere_radius *= scale_factor
		if dir != Vector3.ZERO:
			# Emit in a cone around dir (process direction is local +z).
			gp.global_basis = Basis.looking_at(-dir, Vector3.UP if absf(dir.dot(Vector3.UP)) < 0.95 else Vector3.RIGHT)
			pm.spread = 65.0
		gp.process_material = pm
	gp.emitting = true
	gp.finished.connect(gp.queue_free)
