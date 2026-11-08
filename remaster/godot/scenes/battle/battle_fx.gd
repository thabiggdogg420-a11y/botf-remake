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
## Effect sizes follow Unreal's TickTimeline (BotfCombatScene.cpp) in its scene units, converted with the cell size:
## Unreal's 170-unit cell is BattleScene.CELL here. R3 doubled the cell and drew hulls at true scale but kept the R1
## effect sizes, so a beam was about 1% of a Galaxy's length and torpedoes were specks (Simon, 8 Nov 2026: "no weapon
## textures to fire"). Everything below is sized in these units, then held to a minimum on-screen size.
const UE := BattleScene.CELL / 170.0
const BEAM_CORE_UE := 2.6       ## Beam core radius (Unreal 0.05 x the 50-unit cylinder radius).
const BEAM_HALO_UE := 8.0       ## Faction-coloured glow around the core.
const PULSE_RADIUS_UE := 4.0    ## Unreal DrawSegment(Head - Dir * 70, Head, 0.08).
const PULSE_LENGTH_UE := 70.0
const TORPEDO_CORE_UE := 22.0   ## Torpedo head (Unreal orb 10-unit radius inside a 22-unit halo).
const MIN_CORE_PX := 3.0        ## A beam core never draws thinner than this many pixels, however far the camera.
const MIN_HALO_PX := 10.0
const MIN_FLARE_PX := 14.0

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
	# The ship-side effects (a shield flash, the cloak overlay, a scorch decal) and a flash light render for the first
	# time mid-battle too, and each carries its own pipeline: draw them once now, tiny and out of the way.
	var shield := MeshInstance3D.new()
	var shield_mesh := SphereMesh.new()
	shield_mesh.radial_segments = 48
	shield_mesh.rings = 24
	shield.mesh = shield_mesh
	var shield_mat := ShaderMaterial.new()
	shield_mat.shader = BattleShip.SHIELD_SHADER
	shield_mat.set_shader_parameter("hits", [Vector4(1, 0, 0, 0.2), Vector4(1, 0, 0, -1), Vector4(1, 0, 0, -1),
		Vector4(1, 0, 0, -1), Vector4(1, 0, 0, -1), Vector4(1, 0, 0, -1)])
	shield_mat.set_shader_parameter("hit_colors", [Vector4(0.5, 0.8, 1, 1), Vector4.ZERO, Vector4.ZERO, Vector4.ZERO,
		Vector4.ZERO, Vector4.ZERO])
	shield.material_override = shield_mat
	shield.scale = Vector3.ONE * 0.004
	add_child(shield)
	shield.global_position = at
	nodes.append(shield)
	var cloak := MeshInstance3D.new()
	cloak.mesh = SphereMesh.new()
	var cloak_mat := ShaderMaterial.new()
	cloak_mat.shader = BattleShip.CLOAK_SHADER
	cloak.material_overlay = cloak_mat
	cloak.scale = Vector3.ONE * 0.004
	add_child(cloak)
	cloak.global_position = at
	cloak.set_instance_shader_parameter("shimmer", 0.5)
	cloak.transparency = 0.5
	nodes.append(cloak)
	var decal := Decal.new()
	decal.texture_albedo = BattleShip.scorch_texture()
	decal.texture_emission = BattleShip.ember_texture()
	decal.emission_energy = 3.0
	decal.size = Vector3.ONE * 0.01
	add_child(decal)
	decal.global_position = at
	nodes.append(decal)
	var light := OmniLight3D.new()
	light.light_energy = 0.001
	light.omni_range = 0.05
	light.shadow_enabled = false
	add_child(light)
	light.global_position = at
	nodes.append(light)
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


var _hits_on := {} ## target fleet id -> beams striking it this frame (stacked impacts share one glare budget)

func update(t: float) -> void:
	if _tl == null:
		return
	_hits_on.clear()
	for j in _tl.jobs:
		if j.start > t:
			break
		if j.kind == Job.BEAM and t <= j.start + j.duration:
			_hits_on[j.target_fleet_id] = _hits_on.get(j.target_fleet_id, 0) + 1
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
	var path := _torpedo_path(j)
	return Vector3.ZERO if path.is_empty() else _path_pos(path, u)


## The torpedo's flight as [source, displacement, side normal, arc height], found once per frame: the trail samples 18
## points along it, and each used to repeat the hardpoint, shield and hull lookups.
func _torpedo_path(j: Dictionary) -> Array:
	var shooter := _ship(j.fleet_id)
	var target := _ship(j.target_fleet_id)
	if shooter == null:
		return []
	var src := shooter.hardpoint(2, j.mount_index, j.index)
	var dst := target.shield_point(src) if target != null else src + Vector3(20, 0, 0)
	if j.outcome == BattleTimeline.Outcome.HIT and target != null and j.shield_damage <= 0 and j.hull_damage > 0:
		dst = target.hull_point(src, hash([j.event_index, j.index]))
	var d := dst - src
	var length := d.length()
	if length > 1e-6 and j.overshoot_cells > 0.0:
		dst = src + d * ((length + j.overshoot_cells * _cell()) / length)
		d = dst - src
	return [src, d, d.cross(Vector3.UP).normalized(), j.arc_cells * _cell()]


static func _path_pos(path: Array, u: float) -> Vector3:
	var bow := 4.0 * u * (1.0 - u)
	var arc: float = path[3]
	return path[0] + path[1] * u + path[2] * arc * bow + Vector3.UP * absf(arc) * bow * 0.3


## World size of `px` pixels at point p for the active camera (0 when there is none).
func _px_world(p: Vector3, px: float) -> float:
	var cam := get_viewport().get_camera_3d()
	if cam == null:
		return 0.0
	var h := float(get_viewport().get_visible_rect().size.y)
	var dist := maxf(0.1, (p - cam.global_position).dot(-cam.global_basis.z))
	return dist * 2.0 * tan(deg_to_rad(cam.fov) * 0.5) / maxf(h, 1.0) * px


## Scales a flare to `size` world units, or to `px` on-screen pixels when that is larger (far-off wide shots).
func _floor_flare(n: Node3D, size: float, px: float) -> void:
	n.scale = Vector3.ONE * maxf(size, _px_world(n.global_position, px))


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
			var halo := _beam_node(col)
			halo.set_instance_shader_parameter("beam_phase", float(i))
			halo.set_instance_shader_parameter("beam_halo", 1.0)
			st.nodes = [beam, _flare(col, 14.0 * UE, 1.0, 0.4), _flare(col, 36.0 * UE, 1.0, 0.8), halo]
		Job.PULSE:
			var bolt := _beam_node(col)
			var bolt_halo := _beam_node(col)
			bolt_halo.set_instance_shader_parameter("beam_halo", 1.0)
			st.nodes = [bolt, _flare(col, 30.0 * UE, 1.2), _flare(col, 30.0 * UE, 1.0, 0.8), bolt_halo]
		Job.TORPEDO:
			st.nodes = [_flare(col, TORPEDO_CORE_UE * 2.2 * UE, 1.2, 1.0), _trail_node(), _flare(col, 30.0 * UE, 1.0, 0.0)]
			var shooter := _ship(j.fleet_id)
			if shooter != null:
				_flash_light(shooter.hardpoint(2, j.mount_index, j.index), col, 2.0, 120.0 * UE, 0.25)
		Job.INTERCEPT:
			var tpos := _torpedo_pos(j, j.end_fraction if j.end_fraction < 1.0 else 0.78)
			st.point = tpos
			st.nodes = [_flare(Color(1.0, 0.8, 0.5), 60.0 * UE, 1.5, 1.2), _beam_node(Color(1.0, 0.6, 0.2)), _beam_node(Color(1.0, 0.6, 0.2))]
			_burst(tpos, Color(1.0, 0.7, 0.3), "sparks", 40, 2.0)
			_flash_light(tpos, Color(1.0, 0.7, 0.4), 8.0, 500.0 * UE, 0.4)
		Job.SHIELD:
			var target := _ship(j.target_fleet_id)
			var source := _ship(j.source_fleet_id)
			if target != null:
				var from := source.global_position if source != null else target.global_position + Vector3(10, 0, 0)
				target.add_shield_hit(from, j.start, j.duration * 1.8, col.lerp(Color(0.5, 0.8, 1.0), 0.45) * 0.55)
				st.point = target.shield_point(from)
				st.size = maxf(40.0 * UE, target.radius() * 0.35)
				st.nodes = [_flare(col.lerp(Color.WHITE, 0.3), st.size, 1.0, 0.8)]
				_burst(st.point, col.lerp(Color(0.6, 0.85, 1.0), 0.5), "sparks_small", 18, maxf(1.5, target.radius() * 0.08))
		Job.HULL:
			var target := _ship(j.target_fleet_id)
			var source := _ship(j.source_fleet_id)
			if target != null:
				var from := source.global_position if source != null else target.global_position + Vector3(10, 0, 0)
				var p := target.hull_point(from, hash([j.event_index, 7]))
				st.point = p
				st.size = maxf(50.0 * UE, target.radius() * 0.4)
				st.nodes = [_flare(Color(1.0, 0.6, 0.25), st.size, 1.4, 1.0)]
				var n: int = clampi(10 + j.hull_damage, 16, 70)
				var k := maxf(1.5, target.radius() * 0.1)
				_burst(p, Color(1.0, 0.55, 0.15), "sparks", n, k, (from - p).normalized())
				_burst(p, Color(1.0, 0.4, 0.1), "fire_small", 12, k)
				_burst(p, Color(0.35, 0.3, 0.28), "smoke", 6, k * 0.6)
				_flash_light(p, Color(1.0, 0.55, 0.2), 6.0, 600.0 * UE, 0.5)
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
				_burst(p, Color(1.0, 0.7, 0.3), "sparks", 90, size / 5.0)
				_burst(p, Color(1.0, 0.45, 0.1), "debris", 70, size / 3.0)
				_burst(p, Color(0.35, 0.3, 0.28), "smoke", 24, size / 3.0)
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
			var width := (1.0 if j.weapon.begins_with("Phaser") else 1.3) * (0.7 + 0.3 * env)
			var mid := src.lerp(end, 0.5)
			_orient_beam(st.nodes[0], src, end, maxf(BEAM_CORE_UE * UE * width, _px_world(mid, MIN_CORE_PX) * 0.5))
			st.nodes[0].set_instance_shader_parameter("beam_intensity", env * 1.0)
			var core_r := maxf(BEAM_CORE_UE * UE * width, _px_world(mid, MIN_CORE_PX) * 0.5)
			# The glow widens with distance (it carries the beam in wide shots) but stays tight close up.
			_orient_beam(st.nodes[3], src, end, clampf(_px_world(mid, MIN_HALO_PX) * 0.5, core_r * 1.6, BEAM_HALO_UE * UE * width))
			st.nodes[3].set_instance_shader_parameter("beam_intensity", env * 0.3)
			st.nodes[1].global_position = src
			_floor_flare(st.nodes[1], 14.0 * UE, MIN_FLARE_PX)
			st.nodes[1].set_instance_shader_parameter("flare_intensity", env * 1.2)
			st.nodes[2].global_position = end
			_floor_flare(st.nodes[2], 36.0 * UE, MIN_FLARE_PX)
			var hit: bool = j.outcome == BattleTimeline.Outcome.HIT and reach >= 1.0
			# Several beams on one hull used to stack their impact glare into a white frame (close strip, 8 Nov).
			var share := 1.0 / float(maxi(1, _hits_on.get(j.target_fleet_id, 1))) # total glare per target is capped
			st.nodes[2].set_instance_shader_parameter("flare_intensity", env * (0.5 * share if hit else 0.0))
			st.nodes[2].set_instance_shader_parameter("flare_rotation", t * 3.0)
		Job.PULSE:
			var shooter := _ship(j.fleet_id)
			var target := _ship(j.target_fleet_id)
			if shooter == null:
				return
			var src := shooter.hardpoint(1, j.mount_index, j.index)
			var dst := _shot_end(j, src, target)
			var head := src.lerp(dst, u)
			var span := maxf((dst - src).length(), 1e-3)
			var plen_w := maxf(PULSE_LENGTH_UE * UE, _px_world(head, 28.0))
			var tail := src.lerp(dst, maxf(0.0, u - minf(0.3, plen_w / span)))
			_orient_beam(st.nodes[0], tail, head, maxf(PULSE_RADIUS_UE * UE * 0.5, _px_world(head, MIN_CORE_PX) * 0.5))
			st.nodes[0].set_instance_shader_parameter("beam_intensity", 1.8)
			_orient_beam(st.nodes[3], tail, head, maxf(PULSE_RADIUS_UE * UE * 1.6, _px_world(head, MIN_HALO_PX) * 0.5))
			st.nodes[3].set_instance_shader_parameter("beam_intensity", 0.4)
			st.nodes[1].global_position = head
			_floor_flare(st.nodes[1], 30.0 * UE, MIN_FLARE_PX)
			st.nodes[2].global_position = src
			st.nodes[2].set_instance_shader_parameter("flare_intensity", maxf(0.0, 1.0 - u * 5.0))
		Job.TORPEDO:
			var path_u: float = u * j.end_fraction
			var path := _torpedo_path(j)
			if path.is_empty():
				return
			var head := _path_pos(path, path_u)
			st.nodes[0].global_position = head
			_floor_flare(st.nodes[0], TORPEDO_CORE_UE * 2.2 * UE, MIN_FLARE_PX * 1.4)
			var pulse := 1.0 + 0.3 * sin(t * 40.0 + i)
			var fade := 1.0 - smoothstep(0.85, 1.0, u) if j.outcome == BattleTimeline.Outcome.MISS else 1.0
			st.nodes[0].set_instance_shader_parameter("flare_intensity", 1.6 * pulse * fade)
			st.nodes[0].set_instance_shader_parameter("flare_rotation", t * 2.0 + i)
			var pts := PackedVector3Array()
			# A short comet tail of about 14 Unreal hull-widths (~13 world units), not a streak over a third of the path.
			var plen := maxf((path[1] as Vector3).length(), 1e-3)
			# Trail at least ~60 px on screen at tactical distance so the flight reads in the wide view.
			var trail_len := maxf(200.0 * UE, _px_world(head, 60.0))
			var step := minf(0.035, trail_len / plen / 17.0)
			for k in 18:
				pts.append(_path_pos(path, maxf(0.0, path_u - k * step)))
			_draw_ribbon(st.nodes[1], pts, maxf(6.0 * UE, _px_world(head, 4.0) * 0.5), col * fade * 1.4)
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
				_orient_beam(beam, src, p, maxf(1.5 * UE, _px_world(p, MIN_CORE_PX) * 0.5))
				beam.set_instance_shader_parameter("beam_intensity", 1.0 - u / 0.45)
		Job.SHIELD, Job.HULL:
			if st.nodes.is_empty():
				return
			st.nodes[0].global_position = st.point
			_floor_flare(st.nodes[0], st.get("size", 6.0), MIN_FLARE_PX * 2.2)
			var tshare := 1.0 / float(maxi(1, _hits_on.get(j.target_fleet_id, 1)))
			st.nodes[0].set_instance_shader_parameter("flare_intensity", 0.8 * tshare * pow(1.0 - u, 2.5))
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
			st.nodes[0].set_instance_shader_parameter("flare_intensity", 1.5 * exp(-u * 7.0) + 0.25 * (1.0 - u)) # R3.5: the 2.5 peak whited out the kill frame
			st.nodes[0].scale = Vector3.ONE * maxf(size * (1.5 + 2.0 * u), _px_world(st.point, 90.0 * (1.0 + u)))
			for k in [1]:
				var ring: MeshInstance3D = st.nodes[k]
				var r := size * (0.3 + 1.5 * (1.0 - pow(1.0 - u, 2.5)))
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
	var reach := 40.0 * maxf(1.0, scale_factor)
	gp.visibility_aabb = AABB(-Vector3.ONE * reach, Vector3.ONE * reach * 2.0)
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
