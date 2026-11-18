class_name BattleFx
extends Node3D
## Draws BattleTimeline visual jobs. Job visuals (beams, bolts, torpedoes and their trails, flares, shockwaves) are
## a pure function of the timeline time, so seeking works; one-shot extras (particle bursts, flash lights, scorch
## decals, shield hits) start when a job begins and free themselves.

const FLARE_SHADER := preload("res://scenes/battle/shaders/flare.gdshader")
const TRAIL_SHADER := preload("res://scenes/battle/shaders/trail.gdshader")
const SHOCK_SHADER := preload("res://scenes/battle/shaders/shockwave.gdshader")
const TORPEDO_SHADER := preload("res://scenes/battle/shaders/torpedo.gdshader")
const RIBBON_SHADER := preload("res://scenes/battle/shaders/weapon_beam.gdshader")
const KILL_FIRE_SHADER := preload("res://scenes/battle/shaders/kill_fire.gdshader")
const HULL_PIECE_SHADER := preload("res://scenes/battle/shaders/hull_piece.gdshader")
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
const MIN_FLARE_PX := 14.0
const TORPEDO_TAIL_HEADS := 0.3 ## Trail length in head sizes. Footage (WotW 4891 s, SoA 343 s): a photon has no tail at
                                ## all, only its glare; 0.3 S is a faint smear inside the glow (Simon, 10 Nov: "way too long tails").
## M1 weaponfx (docs/WEAPON-FX.md; Simon, 9 Nov 2026: "torpedoes still havent been done properly, phaser either").
## World sizes in world units (1 wu = 17.7 m: a Galaxy is 36 wu); pixel floors at 1080 lines, scaled with the viewport.
const CHARGE_SECONDS := 0.3     ## A phaser array charges (glow runs along the strip to the emitter) before it fires.
const BEAM_WIDTH := 1.3         ## Phaser, disruptor beam, spiral and polaron ribbons: full width (23 m) ...
const BEAM_MIN_PX := 8.0        ## Floor of the full width on screen. Footage: a phaser is 1/15-1/25 of the target hull wide and only a few px at range (BATTLE-REFERENCE.md); 18 px read as a bar.
const BOLT_LENGTH := 6.0       ## Disruptor / pulse bolt: capsule length ...
const BOLT_LENGTH_PX := 50.0 ## footage SoA 1249: a teardrop about 3:1, not a rod
const BOLT_WIDTH := 1.4         ## ... and width.
const BOLT_WIDTH_PX := 22.0
const TORPEDO_SIZE := 6.0       ## Torpedo star height S (the sprite is TORPEDO_ASPECT x S wide for the lens streak).
const MIN_TORPEDO_PX := 30.0 ## battlechoreo compare 02: 48 read bigger than the show's star;    ## S never draws under this: the white core then spans about 6 px inside a 20 px glow.
const TORPEDO_ASPECT := 1.0 ## round: the footage shows no horizontal lens streak on torpedoes
const SHIELD_BLUE := Color(0.5, 0.8, 1.0)

var scene: Node       ## BattleScene: ship_node(fleet_id), cell size, camera.
var _active := {}     ## job index -> {nodes: Array, ...}
var _tl: BattleTimeline
var _flare_mat: ShaderMaterial
var _trail_mat: ShaderMaterial
var _shock_mat: ShaderMaterial
var _torp_mat: ShaderMaterial
var _ribbon_mat: ShaderMaterial
var _ribbon: ArrayMesh
var _web := false
var _cyl: CylinderMesh
var _quad: QuadMesh
var _plane: PlaneMesh
var _soft_tex: Texture2D
var _puff_tex: Texture2D
var _particle_mats := {}


func _ready() -> void:
	_flare_mat = _shader_mat(FLARE_SHADER)
	_trail_mat = _shader_mat(TRAIL_SHADER)
	_shock_mat = _shader_mat(SHOCK_SHADER)
	_torp_mat = _shader_mat(TORPEDO_SHADER)
	_ribbon_mat = _shader_mat(RIBBON_SHADER)
	# Draw order (docs/WEAPON-FX.md): hulls, shield, smoke, then beams and bolts, torpedo trails, torpedo heads, and
	# flashes and rings last. All additive and depth-tested against the hulls.
	_ribbon_mat.render_priority = 1
	_trail_mat.render_priority = 2
	_torp_mat.render_priority = 3
	_flare_mat.render_priority = 4
	_shock_mat.render_priority = 4
	_ribbon = _ribbon_mesh(12)
	_web = OS.has_feature("web")
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


## The weapon ribbon (weapon_beam.gdshader): rows of two vertices (x = -1, +1) from y = -0.5 to +0.5, plus a rounded
## end-cap row at each end (UV2.x = -1 / +1, pushed out by the half width in the shader). The shader faces it to the
## camera; the custom AABB keeps it from being culled edge-on.
static func _ribbon_mesh(segments: int) -> ArrayMesh:
	var verts := PackedVector3Array()
	var uvs := PackedVector2Array()
	var uv2s := PackedVector2Array()
	var idx := PackedInt32Array()
	var rows: Array = [[-0.5, -1.0]]
	for k in segments + 1:
		rows.append([-0.5 + float(k) / segments, 0.0])
	rows.append([0.5, 1.0])
	for r in rows:
		for sx in [-1.0, 1.0]:
			verts.append(Vector3(sx, r[0], 0.0))
			uvs.append(Vector2(sx, r[0] + 0.5))
			uv2s.append(Vector2(r[1], 0.0))
	for i in rows.size() - 1:
		var a := i * 2
		idx.append_array(PackedInt32Array([a, a + 1, a + 2, a + 1, a + 3, a + 2]))
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = verts
	arrays[Mesh.ARRAY_TEX_UV] = uvs
	arrays[Mesh.ARRAY_TEX_UV2] = uv2s
	arrays[Mesh.ARRAY_INDEX] = idx
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	mesh.custom_aabb = AABB(Vector3(-1, -0.6, -1), Vector3(2, 1.2, 2))
	return mesh


static func _shader_mat(shader: Shader) -> ShaderMaterial:
	var m := ShaderMaterial.new()
	m.shader = shader
	return m


## Sets one effect parameter. Every effect node (beam, flare, torpedo head, shock ring) carries a material of its own
## with plain uniforms, not instance uniforms: in the web build (WebGL2, Compatibility) instance-uniform effects spawned
## mid-battle drew nothing (torpedo heads, impact flares) while the plain-uniform trails beside them drew (M1 battleplay).
static func _fxp(n: Node, param: StringName, value: Variant) -> void:
	((n as GeometryInstance3D).material_override as ShaderMaterial).set_shader_parameter(param, value)


## Draws one of every effect material once, tiny and just in front of the camera, then frees them. The first real
## volley otherwise compiles its pipelines mid-round (a 1.35 s freeze on a fresh shader cache on the PC).
func prewarm() -> void:
	var cam := get_viewport().get_camera_3d()
	if cam == null:
		return
	var at := cam.global_position - cam.global_basis.z * (cam.near + 2.0)
	var nodes: Array[Node] = []
	var beam := _ribbon_node(weapon_style("PhaserArray", 1, Job.BEAM, Color(1, 0.6, 0.2)))
	_orient_beam(beam, at, at + cam.global_basis.x * 0.01, 0.002)
	_fxp(beam, "min_px", 0.01)
	_fxp(beam, "width_world", 0.002)
	nodes.append(beam)
	var bring := MeshInstance3D.new()
	bring.mesh = _quad
	bring.material_override = _shock_mat.duplicate()
	_fxp(bring, "ring_billboard", 1.0)
	bring.scale = Vector3.ONE * 0.005
	add_child(bring)
	bring.global_position = at
	nodes.append(bring)
	var spk := _spark_stream(Color(1, 0.6, 0.2), 0.01)
	spk.global_position = at
	spk.emitting = true
	nodes.append(spk)
	var fl := _flare(Color(1, 0.5, 0.2), 0.01)
	fl.global_position = at
	nodes.append(fl)
	var th := _torpedo_head(Color(1, 0.3, 0.1), 0.0)
	th.scale = Vector3.ONE * 0.01
	th.global_position = at
	nodes.append(th)
	var tr := _trail_node()
	_draw_ribbon(tr, PackedVector3Array([at, at + cam.global_basis.x * 0.01]), 0.002, Color(1, 0.5, 0.2))
	nodes.append(tr)
	var ring := MeshInstance3D.new()
	ring.mesh = _plane
	ring.material_override = _shock_mat.duplicate()
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
	# The kill's fire lobe and torn-hull materials (_kill_fire, _hull_pieces).
	var lobe := MeshInstance3D.new()
	lobe.mesh = _quad
	lobe.material_override = _shader_mat(KILL_FIRE_SHADER)
	lobe.scale = Vector3.ONE * 0.005
	add_child(lobe)
	lobe.global_position = at
	nodes.append(lobe)
	var piece := MeshInstance3D.new()
	piece.mesh = BoxMesh.new()
	piece.material_override = _shader_mat(HULL_PIECE_SHADER)
	piece.scale = Vector3.ONE * 0.004
	add_child(piece)
	piece.global_position = at
	nodes.append(piece)
	var before := get_child_count()
	for kind in ["sparks", "sparks_small", "streamer", "fire_small", "fireball", "smoke", "debris", "vent"]:
		_burst(at, Color(1, 0.5, 0.2), kind, 1, 0.01)
	for i in range(before, get_child_count()):
		nodes.append(get_child(i))
	for i in 3:
		await get_tree().process_frame
	for n in nodes:
		if is_instance_valid(n):
			n.queue_free()


## Clears job visuals (one-shot particles and decals finish on their own).
var _launch_cache := {} ## job -> launcher transform at launch (BattleFx._torpedo_path)

func reset(tl: BattleTimeline) -> void:
	_launch_cache.clear()
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
		# A phaser array charges for CHARGE_SECONDS before its beam lights (drawn only; the beam, its impact and the
		# damage keep the playback's times).
		var live: bool = t >= j.start - _lead(j) and t <= j.start + j.duration
		if live and not _active.has(i):
			_active[i] = {"nodes": []}
			_spawn(i, j)
		elif not live and _active.has(i):
			_despawn(i)
		if _active.has(i):
			_animate(i, j, t)
	_cap_flares()


## Seconds a job draws before its start: the phaser charge.
static func _lead(j: Dictionary) -> float:
	if j.kind != Job.BEAM:
		return 0.0
	var w := str(j.weapon)
	return CHARGE_SECONDS if w.begins_with("Phaser") or w == "PhaseCannon" or w == "PulsePhaser" else 0.0


func _despawn(i: int) -> void:
	for n in _active[i].nodes:
		if is_instance_valid(n):
			n.queue_free()
	# Continuous sparks stop emitting and finish their last particles before they go.
	var sp = _active[i].get("sparks")
	if sp is GPUParticles3D and is_instance_valid(sp):
		sp.emitting = false
		get_tree().create_timer(sp.lifetime + 0.1).timeout.connect(sp.queue_free)
	_active.erase(i)


# ------------------------------------------------------------------------------------------------------------------
# Geometry helpers
# ------------------------------------------------------------------------------------------------------------------

func _ship(fleet_id: int) -> BattleShip:
	return scene.ship_node(fleet_id)


## Owner faction of a ship node (0 when it is gone): picks the per-empire weapon look.
func _owner(fleet_id: int) -> int:
	var s := _ship(fleet_id)
	return s.owner_id if s != null else 0


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
	mi.material_override = _flare_mat.duplicate()
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	mi.scale = Vector3.ONE * size
	add_child(mi)
	_fxp(mi, "flare_color", color)
	_fxp(mi, "flare_intensity", intensity)
	_fxp(mi, "flare_spikes", spikes)
	_flares.append(mi)
	return mi


## F2 glare cap: no flare (hit flash, fireball, launch flash) draws wider than this share of the frame's height, so a
## close kill or stacked hits never wash the screen white (footage: a capital's fireball fills a close shot, never the
## whole frame with glare).
const FLARE_MAX_SCREEN := 0.32
var _flares: Array = []

func _cap_flares() -> void:
	var cam := get_viewport().get_camera_3d()
	if cam == null:
		return
	var h := float(get_viewport().get_visible_rect().size.y)
	var keep: Array = []
	for n in _flares:
		if not is_instance_valid(n) or not n.is_inside_tree():
			continue
		keep.append(n)
		var cap := _px_world(n.global_position, FLARE_MAX_SCREEN * h)
		if cap > 0.0 and n.scale.x > cap:
			n.scale = Vector3.ONE * cap
	_flares = keep


## The look of one shot (docs/WEAPON-FX.md, DS9 Dominion War era). kind is the job kind (BEAM, PULSE, TORPEDO);
## color the playback's weapon colour, used for weapons the table does not name. Keys: ribbon (weapon_beam style:
## 0 phaser, 1 disruptor beam, 2 spiral, 3 polaron, 5 bolt), core / body / glow colours, width and min_px (full
## width), length and length_px (bolts), heavy (Romulan wobble), charge (phaser arrays charge before they fire),
## torp (0 photon, 1 quantum, 2 plasma), torp_color, torp_core, size (torpedo S multiplier).
static func weapon_style(weapon: String, owner: int, kind: int, color: Color) -> Dictionary:
	var st := {"ribbon": 1, "core": color.lerp(Color.WHITE, 0.85), "body": color, "glow": color * 0.65,
		"width": BEAM_WIDTH, "min_px": BEAM_MIN_PX, "length": BOLT_LENGTH, "length_px": BOLT_LENGTH_PX, "heavy": 0.0,
		"charge": false, "torp": 0, "torp_color": color, "torp_core": Color.html("FFE2B8"), "size": 1.0}
	var phaser := weapon.begins_with("Phaser") or weapon == "PhaseCannon" or weapon == "PulsePhaser"
	var romulan := owner == 3 or weapon == "PlasmaCannon"
	if kind == Job.TORPEDO:
		if weapon == "QuantumTorpedo":
			st.merge({"torp": 1, "torp_color": Color.html("8CC0FF"), "torp_core": Color.html("F2F8FF"), "size": 1.15}, true)
		elif weapon == "PlasmaTorpedo":
			st.merge({"torp": 2, "torp_color": Color.html("40FF80"), "torp_core": Color.html("E8FFF0"), "size": 1.3}, true)
		return st
	if phaser:
		# Footage: SoA 1160 s / 782 s (Excelsior, Galaxy): pale gold with a white core; WotW 4916 s (station): amber.
		# Battlechoreo compare 01: ours read wider and brighter than the show's thin gold line.
		st.merge({"ribbon": 0, "core": Color.html("FFF4D8"), "body": Color.html("E8B850"), "glow": Color.html("A8640F"),
			# F2 side-by-side (SoA 784, 1162): the show's phaser is a thin line, 1-3 px across a fleet shot; ours read as a bar.
			"charge": kind == Job.BEAM, "width": BEAM_WIDTH * 0.5, "min_px": BEAM_MIN_PX * 0.4}, true)
		if owner == 5:
			st.merge({"body": Color.html("FFC21E"), "glow": Color.html("FF9A0A")}, true)
	elif weapon == "PolaronBeam":
		# Footage SoA 1186 s: pale lavender-white, wider than a phaser, with a broad soft halo.
		st.merge({"ribbon": 3, "core": Color.html("F8F6FF"), "body": Color.html("BFB2FF"), "glow": Color.html("6E52FF"),
			"width": BEAM_WIDTH * 1.5, "min_px": BEAM_MIN_PX * 1.4}, true)
	elif weapon.begins_with("Disruptor") or weapon == "PlasmaCannon":
		if owner == 4:
			# Footage SoA 343 s / 1186 s: a thin, steady yellow beam (no spiral reads at battle range).
			st.merge({"ribbon": 2, "core": Color.html("FFF8D8"), "body": Color.html("F2D24A"), "glow": Color.html("B88A0C"),
				"width": BEAM_WIDTH * 0.9, "min_px": BEAM_MIN_PX * 1.1}, true)
		elif romulan:
			st.merge({"core": Color.html("E0FFF0"), "body": Color.html("33FF99"), "glow": Color.html("0FB060")}, true)
		elif kind == Job.BEAM:
			# Footage WotW 4805 s: Klingon (Vor'cha, K'Vort) disruptor beams are red with a pink-white core.
			st.merge({"core": Color.html("FFD6CC"), "body": Color.html("FF2A1C"), "glow": Color.html("B00808")}, true)
		else:
			# Footage WotW 4916 s, SoA 1249 s: green bolts with a white-green head and a wide green halo.
			st.merge({"core": Color.html("F0FFE8"), "body": Color.html("3CFF50"), "glow": Color.html("12C02C")}, true)
	if kind == Job.PULSE:
		st.ribbon = 5
		st.width = BOLT_WIDTH
		st.min_px = BOLT_WIDTH_PX
		if not romulan and not phaser and weapon.begins_with("Disruptor") and owner != 4:
			# Battlechoreo compare 04 (SoA d_1249): a fat glowing green blob with a wide halo, not a thin dart.
			st.merge({"core": Color.html("F4FFEE"), "body": Color.html("5CFF6A"), "glow": Color.html("22E040"),
				"width": BOLT_WIDTH * 2.4, "min_px": BOLT_WIDTH_PX * 1.3, "length": BOLT_LENGTH * 0.8,
				"length_px": BOLT_LENGTH_PX * 0.85}, true)
		if romulan:
			st.merge({"heavy": 1.0, "width": BOLT_WIDTH * 1.5, "min_px": BOLT_WIDTH_PX * 1.5, "length": BOLT_LENGTH * 1.2,
				"length_px": BOLT_LENGTH_PX * 1.2}, true)
		if phaser:
			# Footage Tears 2053 s (Defiant pulse phasers): short gold-orange blobs about 2:1 with a soft halo, in pairs.
			st.merge({"core": Color.html("FFF6DC"), "body": Color.html("FFB040"), "glow": Color.html("FF6A10"),
				"width": BOLT_WIDTH * 1.4, "min_px": BOLT_WIDTH_PX * 1.2, "length": BOLT_LENGTH * 0.5,
				"length_px": BOLT_LENGTH_PX * 0.55}, true)
		elif weapon == "PolaronBeam":
			# Footage SoA 343 s / 1160 s: Jem'Hadar fighters fire short white-gold blobs in pairs, about 2:1 long.
			st.merge({"core": Color.html("FFFBEA"), "body": Color.html("FFD98A"), "glow": Color.html("FF9A3A"),
				"width": BOLT_WIDTH * 1.3, "min_px": BOLT_WIDTH_PX * 1.2, "length": BOLT_LENGTH * 0.45,
				"length_px": BOLT_LENGTH_PX * 0.5}, true)
	return st


## A weapon ribbon node in the given style (weapon_style). Its ends are set per frame with _orient_beam.
func _ribbon_node(sty: Dictionary, ribbon := -1) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	mi.mesh = _ribbon
	mi.material_override = _ribbon_mat.duplicate()
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	mi.extra_cull_margin = 4.0
	add_child(mi)
	_fxp(mi, "style", sty.ribbon if ribbon < 0 else ribbon)
	_fxp(mi, "col_core", sty.core)
	_fxp(mi, "col_body", sty.body)
	_fxp(mi, "col_glow", sty.glow)
	_fxp(mi, "width_world", sty.width)
	_fxp(mi, "min_px", sty.min_px)
	_fxp(mi, "heavy", sty.heavy)
	return mi


## World size of `px` pixels at 1080 lines (scaled with the viewport, as the ribbon shader's floor) at point p.
func _px_world_1080(p: Vector3, px: float) -> float:
	var h := float(get_viewport().get_visible_rect().size.y)
	# M4: a battle shown in a panel (the combat screen's middle) is a short viewport; its beams, bolts and flares keep at least
	# three quarters of their 1080-line pixel width so they stay readable when the video is scaled down.
	return _px_world(p, px * clampf(h / 1080.0, 0.75, 2.0))


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


## A torpedo's flight as quadratic Bezier points [tube, handle, end], found once per frame (the trail samples it 12
## times). It leaves the tube where the launcher was at launch (round time j.launch, else j.start), along the tube's line
## (bow or stern), and curves onto the target's shield or hull as the target is now: it homes. A miss runs on past the
## target, an intercepted one stops short (end_fraction). Each tube's arc_cells swings its handle to its own side, so a
## spread fans out and closes in.
func _torpedo_path(j: Dictionary) -> Array:
	var shooter := _ship(j.fleet_id)
	var target := _ship(j.target_fleet_id)
	if shooter == null:
		return []
	var now_src := shooter.hardpoint(2, j.mount_index, j.index)
	# The launch pose is fixed in time: computed once per job (battlechoreo fps: 60 torpedoes each re-ran the flown
	# pose of their launcher every frame).
	var ck := int(j.event_index) * 64 + int(j.index) + (100000000 if j.kind == Job.INTERCEPT else 0)
	var launch_xf: Transform3D = _launch_cache.get(ck, Transform3D())
	if not _launch_cache.has(ck):
		launch_xf = shooter.global_transform
		if scene != null and scene.has_method("ship_transform_at"):
			launch_xf = scene.ship_transform_at(j.fleet_id, float(j.get("launch", j.start)))
		_launch_cache[ck] = launch_xf
	var src: Vector3 = launch_xf * (shooter.global_transform.affine_inverse() * now_src)
	var bow := launch_xf.basis.x.normalized()
	var dst := target.shield_point(src) if target != null else src + bow * 20.0
	if j.outcome == BattleTimeline.Outcome.HIT and target != null and j.shield_damage <= 0 and j.hull_damage > 0:
		dst = target.hull_point(src, hash([j.event_index, j.index]))
	var d := dst - src
	var length := d.length()
	if length > 1e-6 and j.overshoot_cells > 0.0:
		dst = src + d * ((length + j.overshoot_cells * _cell()) / length)
		d = dst - src
	var out := bow if bow.dot(d) >= 0.0 else -bow
	var side := d.cross(Vector3.UP).normalized() if length > 1e-6 else Vector3.ZERO
	var swing: float = j.arc_cells * _cell() * 3.0
	var handle := src + out * clampf(length * 0.35, 0.0, length) + side * swing + Vector3.UP * absf(swing) * 0.35
	return [src, handle, dst]


static func _path_pos(path: Array, u: float) -> Vector3:
	var a: Vector3 = path[0]
	var b: Vector3 = path[1]
	var c: Vector3 = path[2]
	var v := 1.0 - u
	return a * (v * v) + b * (2.0 * u * v) + c * (u * u)


func _torpedo_head(color: Color, phase: float) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	mi.mesh = _quad
	mi.material_override = _torp_mat.duplicate()
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(mi)
	_fxp(mi, "torp_color", color)
	_fxp(mi, "torp_phase", phase)
	return mi


## Where a beam strikes at u (0..1 of its life). Phasers rake: the aim point walks about 40 degrees round the target's
## shield or hull while the beam holds (direction seeded per shot); other beams hold still.
func _beam_end(j: Dictionary, src: Vector3, target: BattleShip, u: float) -> Vector3:
	if target == null or j.outcome == BattleTimeline.Outcome.MISS or not (str(j.weapon).begins_with("Phaser") or j.weapon == "PolaronBeam"):
		return _shot_end(j, src, target)
	var r := RandomNumberGenerator.new()
	r.seed = hash([j.event_index, j.index, 3])
	var turn := (1.0 if r.randf() < 0.5 else -1.0) * deg_to_rad(40.0) * (smoothstep(0.0, 1.0, u) - 0.5)
	var c := target.global_position
	var from := c + (src - c).rotated(Vector3.UP, turn)
	if j.shield_damage > 0 or j.hull_damage <= 0:
		return target.shield_point(from)
	return target.hull_point(from, hash([j.event_index, j.index]))


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
			# M1 weaponfx: one camera-facing ribbon per beam (core, body and glow in one profile, screen-space floor),
			# an emitter glow, a contact splash and, for phaser arrays, the charge strip (docs/WEAPON-FX.md).
			var sty := weapon_style(str(j.weapon), _owner(j.fleet_id), Job.BEAM, col)
			st.style = sty
			var beam := _ribbon_node(sty)
			_fxp(beam, "phase", float(i % 31) * 1.37)
			beam.visible = false
			st.nodes = [beam, _flare(sty.body, 14.0 * UE, 0.0, 0.6), _flare(sty.core.lerp(sty.body, 0.35), 36.0 * UE, 0.0, 0.8)]
			if sty.charge:
				var strip := _ribbon_node(sty, 4)
				_fxp(strip, "width_world", 0.55)
				_fxp(strip, "min_px", 5.0)
				st.nodes.append(strip)
				st.strip = strip
			st.next_ripple = -INF
			var tgt := _ship(j.target_fleet_id)
			if tgt != null and j.outcome == BattleTimeline.Outcome.HIT and j.shield_damage <= 0 and j.hull_damage > 0:
				st.sparks = _spark_stream(Color(1.0, 0.6, 0.2), maxf(1.2, tgt.radius() * 0.08))
		Job.PULSE:
			# A capsule bolt with its own glow, and a muzzle flash. No round flare on the head: a bolt must never read as
			# a torpedo star (Simon, 9 Nov 2026: disruptors looked the same as torpedoes).
			var sty := weapon_style(str(j.weapon), _owner(j.fleet_id), Job.PULSE, col)
			st.style = sty
			var bolt := _ribbon_node(sty)
			_fxp(bolt, "phase", float(i % 31) * 1.37)
			st.nodes = [bolt, _flare(sty.body, 22.0 * UE, 1.2, 0.6)]
		Job.TORPEDO:
			# A pulsing star with spikes and a lens streak, a short motion smear, and a spiked launch flash at the tube.
			var sty := weapon_style(str(j.weapon), _owner(j.fleet_id), Job.TORPEDO, col)
			st.style = sty
			var head := _torpedo_head(sty.torp_color, float(i % 17))
			_fxp(head, "torp_core", sty.torp_core)
			_fxp(head, "torp_style", sty.torp)
			_fxp(head, "aspect", TORPEDO_ASPECT if sty.torp != 2 else 1.0)
			st.nodes = [head, _trail_node(), _flare(sty.torp_color.lerp(Color.WHITE, 0.3), 30.0 * UE, 0.0, 1.0)]
			var shooter := _ship(j.fleet_id)
			if shooter != null and int(j.index) == 0: # one launch light per spread (battlechoreo fps)
				_flash_light(shooter.hardpoint(2, j.mount_index, j.index), sty.torp_color, 3.0, 160.0 * UE, 0.25)
		Job.INTERCEPT:
			var tpos := _torpedo_pos(j, j.end_fraction if j.end_fraction < 1.0 else 0.78)
			st.point = tpos
			var pd := weapon_style("PhaserArray", 0, Job.INTERCEPT, Color(1.0, 0.6, 0.2))
			pd.width = BEAM_WIDTH * 0.5
			pd.min_px = BEAM_MIN_PX * 0.6
			st.nodes = [_flare(Color(1.0, 0.8, 0.5), 60.0 * UE, 1.5, 1.2), _ribbon_node(pd), _ribbon_node(pd)]
			_burst(tpos, Color(1.0, 0.7, 0.3), "sparks", 40, 2.0)
			_flash_light(tpos, Color(1.0, 0.7, 0.4), 8.0, 500.0 * UE, 0.4)
		Job.SHIELD:
			var target := _ship(j.target_fleet_id)
			var source := _ship(j.source_fleet_id)
			if target != null:
				var from := source.global_position if source != null else target.global_position + Vector3(10, 0, 0)
				var facing: String = j.get("facing", "")
				target.add_shield_hit(from, j.start, j.duration, col.lerp(Color(0.5, 0.8, 1.0), 0.45) * 0.55,
					facing, target.facing_strength(facing))
				st.point = target.shield_point(from)
				st.size = maxf(40.0 * UE, target.radius() * 0.35)
				st.nodes = [_flare(col.lerp(Color.WHITE, 0.3), st.size, 1.0, 0.8)]
				_burst(st.point, col.lerp(Color(0.6, 0.85, 1.0), 0.5), "sparks_small", 18, maxf(1.5, target.radius() * 0.08))
				if int(j.get("family", 0)) == 2:
					_torpedo_impact(st, j, target, st.point)
		Job.HULL:
			var target := _ship(j.target_fleet_id)
			var source := _ship(j.source_fleet_id)
			if target != null:
				var from := source.global_position if source != null else target.global_position + Vector3(10, 0, 0)
				var p := target.hull_point(from, hash([j.event_index, 7]))
				st.point = p
				st.size = maxf(50.0 * UE, target.radius() * 0.4)
				# The killing blow (kill redesign, 18 Nov): no spiked flash and no pale vent puffs on a ship about to die;
				# the kill reads as orange fire from the first frame (SoA 1161 s).
				var killing := _killing_hit(j)
				st.nodes = [_flare(Color(1.0, 0.6, 0.25), st.size, 1.0 if killing else 1.4, 0.0 if killing else 1.0)]
				var n: int = clampi(6 + j.hull_damage / 2, 8, 30) # F2: fewer streaks (SoA hull hits are fire, not a spray)
				var k := maxf(1.5, target.radius() * 0.1)
				_burst(p, Color(1.0, 0.55, 0.15), "sparks", n, k, (from - p).normalized())
				_burst(p, Color(1.0, 0.4, 0.1), "fire_small", 12, k)
				_burst(p, Color(0.1, 0.08, 0.07), "smoke", 6, k * 0.6) # dark smoke (SoA): the 0.35 grey read as white puffs by a kill
				# Hull breach: a spray of debris and plasma venting out of the wound for a couple of seconds.
				var out := (p - target.global_position).normalized()
				_burst(p, Color(1.0, 0.5, 0.2), "debris", clampi(6 + j.hull_damage / 2, 8, 30), k * 0.8, out)
				_burst(p, Color(1.0, 0.55, 0.25) if killing else Color(0.7, 0.85, 1.0), "vent", 16 if killing else 36, k * 0.7, out)
				_flash_light(p, Color(1.0, 0.55, 0.2), 9.0, 800.0 * UE, 0.6)
				target.add_scorch(p, from, clampf(target.length * 0.18, 0.6, 2.2))
				# Tactical rules 4: a hit that damages a subsystem sparks where that system sits (engines, weapons...).
				if str(j.get("subsystem", "none")) != "none" and int(j.get("subsystem_damage", 0)) > 0 and not j.get("subsystem_destroyed", false):
					var sq := target.subsystem_point(str(j.get("subsystem", "none")))
					var hot0 := Color(0.6, 0.8, 1.0) if str(j.get("subsystem", "")) in ["warpCore", "engines"] else Color(1.0, 0.75, 0.35)
					_burst(sq, hot0, "sparks_small", 24, k)
					_flash_light(sq, hot0, 4.0, 300.0 * UE, 0.35)
				# Tactical rules 4: a subsystem knocked out (or a warp core breach) gets its own secondary blast.
				if j.get("subsystem_destroyed", false) or int(j.get("breach", 0)) > 0:
					var big := int(j.get("breach", 0)) > 0
					var q := target.subsystem_point(str(j.get("subsystem", "none")))
					var hot := Color(0.6, 0.8, 1.0) if str(j.get("subsystem", "")) == "warpCore" else Color(1.0, 0.7, 0.3)
					_burst(q, hot, "sparks", 60 if big else 36, k * (2.2 if big else 1.4))
					_burst(q, Color(1.0, 0.45, 0.1), "fire_small", 24 if big else 12, k * (2.0 if big else 1.2))
					_flash_light(q, hot, 18.0 if big else 11.0, (1300.0 if big else 800.0) * UE, 1.0)
					var blast := _flare(hot.lerp(Color.WHITE, 0.4), st.size * (2.4 if big else 1.6), 1.6, 1.0)
					blast.global_position = q # was left at the world origin
					st.nodes.append(blast)
				# One torpedo ring per hit: here when nothing struck the shield, else on the SHIELD job.
				if int(j.get("family", 0)) == 2 and int(j.shield_damage) <= 0:
					_torpedo_impact(st, j, target, p)
		Job.RAM:
			st.nodes = [_flare(Color(1.0, 0.6, 0.2), 4.0, 0.0, 1.0)]
		Job.DECLOAK:
			var s := _ship(j.fleet_id)
			if s != null:
				st.nodes = [_flare(col, s.length * 1.4, 0.0, 0.6)]
		Job.EXPLOSION:
			var s := _ship(j.fleet_id)
			if s != null and float(j.get("breakup", 0.0)) > 0.0:
				# Battlechoreo, proposal 5: the killing blow first: a hull flash, fire and venting plasma; the ship drifts
				# on, venting, and breaks up into the fireball j.breakup seconds later (_animate).
				var p0 := s.global_position
				var k0 := maxf(1.5, s.length * 0.08)
				st.point = p0
				st.size = s.length
				st.boomed = false
				st.next_vent = j.start + 0.3
				# F2 side-by-side (SoA 1160 1.4-1.9 s): the killing blow is an orange fire burst, not a white flash with a
				# radial spray of white streaks.
				st.nodes = [_flare(Color(1.0, 0.5, 0.18), s.length * 0.3, 0.5, 0.0)]
				_burst(p0, Color(1.0, 0.45, 0.12), "fire_small", 24, k0 * 1.8)
				_burst(p0, Color(1.0, 0.55, 0.2), "sparks", 14, k0 * 1.2)
				_burst(p0, Color(1.0, 0.55, 0.25), "vent", 16, k0)
				_flash_light(p0, Color(1.0, 0.6, 0.3), 11.0, s.length * 8.0, 0.9)
			elif s != null:
				_fireball(i, st, s.global_position, s.length, s, j.start)
		Job.WARP:
			var s := _ship(j.fleet_id)
			if s != null:
				st.nodes = [_flare(Color(0.7, 0.85, 1.0), s.length * 1.5, 0.0, 1.5)]
		_:
			pass


## True when hull hit j lands on a ship in its last moments (its kill or hide comes within 3 s of the hit).
func _killing_hit(j: Dictionary) -> bool:
	if _tl == null:
		return false
	var tr = _tl.track(int(j.target_fleet_id))
	if tr == null:
		return false
	# Every hit in the last 3 s before the killing blow counts: their pale vent puffs otherwise hang round the fireball.
	var dead: float = tr.kill_at if tr.kill_at >= 0.0 else tr.hide_at
	return dead >= 0.0 and dead - float(j.start) < 3.0 and dead - float(j.start) > -0.6


## The kill (redesign 18 Nov against DS9 "Sacrifice of Angels" 1161-1163 s): a lumpy, volumetric orange-yellow fireball
## of layered noise lobes with a dark smoke rim (_kill_fire), the torn hull still inside it that breaks into 2-4 pieces
## which drift, tumble, char and burn away (_hull_pieces), orange streamers and a little debris. White is only a small
## core flare with no spikes; no starburst, no shock rings. nodes[0] is the core flare.
func _fireball(i: int, st: Dictionary, p: Vector3, size: float, s: BattleShip = null, t0 := 0.0) -> void:
	st.point = p
	st.size = size
	st.boom_at = t0
	var core := _flare(Color(1.0, 0.55, 0.2), size * 0.45, 1.0, 0.0)
	core.global_position = p
	st.nodes = [core]
	_kill_fire(i, st, p, size)
	if s != null:
		_hull_pieces(i, st, s)
	var k := size / 3.0
	# No particle fire or grey smoke puffs: the lobes carry the volume and the dark rim (their hot particle cores read as
	# white dots in the first strip). A few soft streamers and fine glinting debris.
	_burst(p, Color(1.0, 0.5, 0.12), "streamer", 6 if _web else 8, k * 0.9)
	_burst(p, Color(1.0, 0.45, 0.1), "debris", 40 if _web else 60, size / 2.4)
	_flash_light(p, Color(1.0, 0.6, 0.25), 9.0, size * 12.0, 2.0)


## The fire lobes: camera-facing noise billboards (kill_fire.gdshader) scattered over the hull, lit one after another
## over the first 0.3 s, swelling and cooling to smoke. st.lobes: [{node, delay, off, s0, life}].
func _kill_fire(i: int, st: Dictionary, p: Vector3, size: float) -> void:
	var lobes: Array = []
	var n := 7 if _web else 9
	for k in n:
		var h1 := BattleTimeline.unit_hash(i * 31 + k, 11)
		var h2 := BattleTimeline.unit_hash(i * 31 + k, 23)
		var h3 := BattleTimeline.unit_hash(i * 31 + k, 37)
		var inner := k < 3
		var off := Vector3(h1 - 0.5, (h2 - 0.5) * 0.6, h3 - 0.5) * size * (0.25 if inner else 0.6)
		var mi := MeshInstance3D.new()
		mi.mesh = _quad
		var m := ShaderMaterial.new()
		m.shader = KILL_FIRE_SHADER
		m.render_priority = 3 if inner else 2
		mi.material_override = m
		mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		add_child(mi)
		mi.global_position = p + off
		mi.scale = Vector3.ONE * 0.001
		_fxp(mi, "seed", float(k) * 1.37 + h1 * 10.0)
		_fxp(mi, "heat", 1.0 if inner else 0.5 + 0.35 * h2)
		_fxp(mi, "age", 0.0)
		_fxp(mi, "alpha_gain", 0.0)
		_flares.append(mi) # the glare cap holds every lobe under FLARE_MAX_SCREEN of the frame too
		lobes.append({"node": mi, "delay": (0.0 if inner else 0.06 + 0.25 * h3), "off": off,
			"s0": size * ((0.95 if inner else 0.75) + 0.35 * h2), "life": 1.6 + 0.8 * h1})
		st.nodes.append(mi)
	st.lobes = lobes


## The torn hull: the ship's model copied once per piece, each copy clipped to its slab between two jagged cut planes
## across the hull's length (hull_piece.gdshader). They start exactly where the hull was, so the ship reads whole inside
## the fire for a beat, then part, drift and tumble, char and burn away. BattleScene hides the real hull from the
## breakup on. st.pieces: [{node, xf0, vel, axis, spin, mats}].
func _hull_pieces(i: int, st: Dictionary, s: BattleShip) -> void:
	if s.model == null or not s.model.is_inside_tree():
		return
	var he := s.half_extents
	var n := 2 + int(BattleTimeline.unit_hash(i, 5) * 2.99) # 2-4 pieces
	var cn := Vector3(1.0, (BattleTimeline.unit_hash(i, 7) - 0.5) * 0.8, (BattleTimeline.unit_hash(i, 9) - 0.5) * 0.8).normalized()
	var span := he.x * 1.1
	var cuts: Array = [-1e6]
	for k in range(1, n):
		cuts.append(-span + 2.0 * span * (float(k) + (BattleTimeline.unit_hash(i + k, 13) - 0.5) * 0.5) / float(n))
	cuts.append(1e6)
	var model_len := maxf(he.x * 2.0, 0.01)
	var pieces: Array = []
	for k in n:
		var root := Node3D.new()
		add_child(root)
		root.global_transform = s.model.global_transform
		var dup: Node3D = s.model.duplicate()
		dup.transform = Transform3D.IDENTITY
		dup.visible = true # BattleScene hides the real model from the breakup frame on
		root.add_child(dup)
		var mats: Array = []
		for g in dup.find_children("*", "MeshInstance3D", true, false):
			var mi := g as MeshInstance3D
			if mi.name.begins_with("NavLight"):
				mi.visible = false
				continue
			if mi.mesh == null:
				continue
			mi.material_overlay = null
			mi.material_override = null
			mi.transparency = 0.0
			mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
			var m2r := root.global_transform.affine_inverse() * mi.global_transform
			for si in mi.mesh.get_surface_count():
				var pm := _piece_material(mi, si)
				pm.set_shader_parameter("mesh_to_root", Projection(m2r))
				pm.set_shader_parameter("cut_n", cn)
				pm.set_shader_parameter("cut_lo", cuts[k])
				pm.set_shader_parameter("cut_hi", cuts[k + 1])
				pm.set_shader_parameter("jag", model_len * 0.06)
				pm.set_shader_parameter("noise_scale", 1.0 / model_len)
				mi.set_surface_override_material(si, pm)
				mats.append(pm)
		for g in dup.find_children("*", "GeometryInstance3D", true, false):
			if not (g is MeshInstance3D):
				(g as GeometryInstance3D).visible = false # damage fire and smoke emitters stay with the real ship
		# Drift away from the hull's middle along the cut axis, with a little sideways scatter and a slow tumble.
		var mid := -span if k == 0 else (span if k == n - 1 else (float(cuts[k]) + float(cuts[k + 1])) * 0.5)
		var side := Vector3(0.0, BattleTimeline.unit_hash(i + k, 17) - 0.5, BattleTimeline.unit_hash(i + k, 19) - 0.5)
		var local_dir := (cn * (1.0 if mid >= 0.0 else -1.0) + side).normalized()
		var wdir := (s.model.global_basis * local_dir).normalized()
		var axis := Vector3(BattleTimeline.unit_hash(i + k, 21) - 0.5, BattleTimeline.unit_hash(i + k, 25) - 0.5,
			BattleTimeline.unit_hash(i + k, 29) - 0.5)
		if axis.length() < 0.01:
			axis = Vector3.UP
		pieces.append({"node": root, "xf0": root.global_transform,
			"vel": wdir * s.length * (0.10 + 0.08 * BattleTimeline.unit_hash(i + k, 31)),
			"axis": axis.normalized(), "spin": (0.35 + 0.5 * BattleTimeline.unit_hash(i + k, 33)) * (1.0 if k % 2 == 0 else -1.0),
			"mats": mats})
		st.nodes.append(root)
	st.pieces = pieces


## A hull_piece material carrying the look of one surface of the ship (its colour map and tint, or vertex colours).
func _piece_material(mi: MeshInstance3D, si: int) -> ShaderMaterial:
	var src: Material = mi.get_surface_override_material(si)
	if src == null:
		src = mi.mesh.surface_get_material(si)
	var m := ShaderMaterial.new()
	m.shader = HULL_PIECE_SHADER
	if src is BaseMaterial3D:
		var sm := src as BaseMaterial3D
		m.set_shader_parameter("tint", Vector3(sm.albedo_color.r, sm.albedo_color.g, sm.albedo_color.b))
		if sm.albedo_texture != null:
			m.set_shader_parameter("bc_tex", sm.albedo_texture)
			m.set_shader_parameter("has_tex", true)
		m.set_shader_parameter("use_vc", sm.vertex_color_use_as_albedo)
	elif src is ShaderMaterial:
		var shm := src as ShaderMaterial
		var tex = shm.get_shader_parameter("bc_tex")
		if tex is Texture2D:
			m.set_shader_parameter("bc_tex", tex)
			m.set_shader_parameter("has_tex", true)
		var tint = shm.get_shader_parameter("tint")
		if tint == null:
			tint = shm.get_shader_parameter("base_color")
		if tint is Color:
			tint = Vector3(tint.r, tint.g, tint.b)
		m.set_shader_parameter("tint", tint if tint is Vector3 else Vector3(0.6, 0.6, 0.62))
		m.set_shader_parameter("use_vc", shm.get_shader_parameter("use_vertex_color") == true)
	return m


## The kill after its fireball lit (_fireball): core flare, fire lobes and hull pieces as a function of time.
func _animate_kill(st: Dictionary, t: float, t_end: float) -> void:
	if not st.has("boom_at"):
		return
	var el := maxf(0.0, t - float(st.boom_at))
	var life := maxf(0.5, t_end - float(st.boom_at))
	var size: float = st.size
	var p: Vector3 = st.point
	var c = st.nodes[0] if not st.nodes.is_empty() else null
	if c is MeshInstance3D and is_instance_valid(c):
		# The white core: small, short, no spikes.
		c.global_position = p
		_fxp(c, "flare_intensity", 1.1 * exp(-el * 6.0) + 0.15 * exp(-el * 1.5))
		c.scale = Vector3.ONE * maxf(size * (0.45 + 0.25 * minf(el, 1.0)), _px_world(p, 24.0))
	for lb in st.get("lobes", []):
		var mi: MeshInstance3D = lb.node
		if not is_instance_valid(mi):
			continue
		var a := clampf((el - float(lb.delay)) / float(lb.life), 0.0, 1.0)
		_fxp(mi, "age", a)
		_fxp(mi, "alpha_gain", 1.0 if el >= float(lb.delay) else 0.0)
		var grow := 0.35 + 0.95 * (1.0 - exp(-a * 5.0))
		mi.scale = Vector3.ONE * maxf(float(lb.s0) * grow, 0.001)
		mi.global_position = p + (lb.off as Vector3) * (1.0 + 0.8 * a)
	for pc in st.get("pieces", []):
		var root: Node3D = pc.node
		if not is_instance_valid(root):
			continue
		# Whole for a beat (0.2 s), then the pieces part and tumble; charred by 1.2 s; burnt away over the last 40%.
		var m := maxf(0.0, el - 0.2)
		var drift := m * m / (m + 0.4)
		var xf0: Transform3D = pc.xf0
		root.global_transform = Transform3D(Basis(pc.axis, float(pc.spin) * drift) * xf0.basis, xf0.origin + (pc.vel as Vector3) * drift)
		var burn := smoothstep(0.0, 1.2, el)
		var dis := smoothstep(life * 0.6, life * 0.98, el)
		for mat in pc.mats:
			(mat as ShaderMaterial).set_shader_parameter("burn", burn)
			(mat as ShaderMaterial).set_shader_parameter("dissolve", dis)


## Torpedo impact (docs/WEAPON-FX.md): a big spiked flash and a camera-facing shockwave ring that grows to twice the
## target's radius over the impact job, plus a flash light. Animated by the SHIELD / HULL branch of _animate.
func _torpedo_impact(st: Dictionary, j: Dictionary, target: BattleShip, p: Vector3) -> void:
	var sty := weapon_style(str(j.weapon), 0, Job.TORPEDO, j.color)
	var tc: Color = sty.torp_color
	st.t_flash = _flare(tc.lerp(Color.WHITE, 0.3), maxf(0.9 * target.radius(), 30.0 * UE), 0.0, 1.2)
	st.t_flash.global_position = p
	var ring := MeshInstance3D.new()
	ring.mesh = _quad
	ring.material_override = _shock_mat.duplicate()
	ring.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(ring)
	ring.global_position = p
	_fxp(ring, "ring_billboard", 1.0)
	_fxp(ring, "ring_color", tc.lerp(Color(1.0, 0.92, 0.8), 0.6))
	_fxp(ring, "ring_intensity", 1.2)
	st.t_ring = ring
	st.t_radius = target.radius()
	st.nodes.append_array([st.t_flash, ring])
	_flash_light(p, tc.lerp(Color.WHITE, 0.3), 9.0, target.radius() * 8.0, 0.5)


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
			var sty: Dictionary = st.style
			var src := shooter.hardpoint(0, j.mount_index, j.index)
			var dst := _beam_end(j, src, target, u)
			var lit: bool = t >= j.start
			var charge := clampf((t - (j.start - CHARGE_SECONDS)) / CHARGE_SECONDS, 0.0, 1.0)
			# The charge: a strip on the hull across the line of fire; the glow runs along it to the emitter.
			if st.has("strip"):
				var strip: MeshInstance3D = st.strip
				var after: float = t - j.start
				strip.visible = after < 0.3
				if strip.visible:
					var up := shooter.global_basis.y.normalized()
					var axis := (dst - src).normalized().cross(up)
					if axis.length() < 0.1:
						axis = shooter.global_basis.z
					axis = axis.normalized()
					var half := clampf(shooter.length * 0.11, 1.0, 4.5)
					var lift := (src - shooter.global_position).normalized() * 0.12
					_orient_beam(strip, src + lift - axis * half, src + lift + axis * half, maxf(0.3, _px_world_1080(src, 5.0)))
					_fxp(strip, "charge", charge)
					_fxp(strip, "intensity", 1.0 - smoothstep(0.0, 0.25, after) if lit else 1.0)
			var beam: MeshInstance3D = st.nodes[0]
			beam.visible = lit
			var reach := clampf(u / 0.1, 0.0, 1.0)
			var env := smoothstep(0.0, 0.06, u) * (1.0 - smoothstep(0.75, 1.0, u))
			var end := src.lerp(dst, reach)
			var mid := src.lerp(end, 0.5)
			_orient_beam(beam, src, end, maxf(float(sty.width) * 0.5, _px_world_1080(mid, sty.min_px) * 0.5))
			_fxp(beam, "intensity", env)
			_fxp(beam, "end_dim", sqrt(1.0 / float(maxi(1, _hits_on.get(j.target_fleet_id, 1)))))
			var em: MeshInstance3D = st.nodes[1]
			em.global_position = src
			# The emitter: a small hot point where the beam leaves the array, not a glare over the hull.
			_floor_flare(em, 9.0 * UE, MIN_FLARE_PX)
			_fxp(em, "flare_intensity", env * 0.55 if lit else 0.45 * charge * charge)
			var hit: bool = lit and j.outcome == BattleTimeline.Outcome.HIT and reach >= 1.0
			var on_shield: bool = j.shield_damage > 0 or j.hull_damage <= 0
			# Several beams on one hull used to stack their impact glare into a white frame (close strip, 8 Nov).
			var share := 1.0 / float(maxi(1, _hits_on.get(j.target_fleet_id, 1))) # total glare per target is capped
			var sp: MeshInstance3D = st.nodes[2]
			sp.global_position = end
			var tr := target.radius() if target != null else 4.0
			var flick := 0.8 + 0.2 * sin(t * 41.0 + float(i))
			if on_shield:
				# Contact splash on the shield, and a ripple across the bubble from the moving contact point.
				sp.scale = Vector3.ONE * maxf(0.35 * tr * 2.0, _px_world_1080(end, 44.0))
				_fxp(sp, "flare_color", Color(1.0, 0.91, 0.66))
				_fxp(sp, "flare_intensity", env * 0.9 * share * flick if hit else 0.0)
				if hit and target != null and t >= float(st.next_ripple):
					target.add_shield_hit(target.global_position + (end - target.global_position) * 2.0, t, 0.3,
						col.lerp(SHIELD_BLUE, 0.45) * 0.55)
					st.next_ripple = t + 0.3
			else:
				# A burn on the hull with sparks spraying back off it.
				sp.scale = Vector3.ONE * maxf(0.18 * tr * 2.0, _px_world_1080(end, 32.0))
				_fxp(sp, "flare_color", Color(1.0, 0.75, 0.45))
				_fxp(sp, "flare_intensity", env * 1.1 * share * flick if hit else 0.0)
			_fxp(sp, "flare_rotation", t * 3.0)
			if st.has("sparks") and is_instance_valid(st.sparks):
				var spk: GPUParticles3D = st.sparks
				spk.global_position = end
				var back := (src - end).normalized()
				if back.length() > 0.5:
					spk.global_basis = Basis.looking_at(-back, Vector3.UP if absf(back.dot(Vector3.UP)) < 0.95 else Vector3.RIGHT)
				var on := hit and u < 0.8
				if spk.emitting != on:
					spk.emitting = on
		Job.PULSE:
			var shooter := _ship(j.fleet_id)
			var target := _ship(j.target_fleet_id)
			if shooter == null:
				return
			var sty: Dictionary = st.style
			var src := shooter.hardpoint(1, j.mount_index, j.index)
			var dst := _shot_end(j, src, target)
			var head := src.lerp(dst, u)
			var span := maxf((dst - src).length(), 1e-3)
			var blen := maxf(float(sty.length), _px_world_1080(head, sty.length_px))
			var tail := src.lerp(dst, maxf(0.0, u - minf(0.35, blen / span)))
			_orient_beam(st.nodes[0], tail, head, maxf(float(sty.width) * 0.5, _px_world_1080(head, sty.min_px) * 0.5))
			var fade := 1.0 - smoothstep(0.85, 1.0, u) if j.outcome == BattleTimeline.Outcome.MISS else 1.0
			_fxp(st.nodes[0], "intensity", fade)
			st.nodes[1].global_position = src
			_floor_flare(st.nodes[1], 22.0 * UE, MIN_FLARE_PX)
			_fxp(st.nodes[1], "flare_intensity", maxf(0.0, 1.0 - u * 5.0) * 1.2)
		Job.TORPEDO:
			var path_u: float = u * j.end_fraction
			var path := _torpedo_path(j)
			if path.is_empty():
				return
			var sty: Dictionary = st.style
			var head := _path_pos(path, path_u)
			st.nodes[0].global_position = head
			# The star: S tall (true scale up close, never under MIN_TORPEDO_PX in a wide shot), TORPEDO_ASPECT x S wide
			# so the horizontal lens streak fits. Plasma is a round ball with no streak.
			var head_size := maxf(TORPEDO_SIZE * float(sty.size), _px_world_1080(head, MIN_TORPEDO_PX * float(sty.size)))
			var aspect := TORPEDO_ASPECT if int(sty.torp) != 2 else 1.0
			st.nodes[0].scale = Vector3(head_size * aspect, head_size, head_size)
			var fade := 1.0 - smoothstep(0.85, 1.0, u) if j.outcome == BattleTimeline.Outcome.MISS else 1.0
			var strike := 1.0 + 0.8 * smoothstep(0.92, 1.0, u) if j.outcome == BattleTimeline.Outcome.HIT else 1.0
			_fxp(st.nodes[0], "torp_intensity", 1.7 * fade * strike) # stage 2: a hot core that reads against the darker sky
			# The motion smear: TORPEDO_TAIL_HEADS head sizes long, about as wide as the core, fading fast.
			var plen := maxf(((path[2] as Vector3) - (path[0] as Vector3)).length(), 1e-3)
			var step := minf(0.03, head_size * TORPEDO_TAIL_HEADS / plen / 11.0)
			var pts := PackedVector3Array()
			# Photons: no tail at all (footage); skip the per-frame ribbon rebuild.
			for k in (12 if int(sty.torp) != 0 else 0):
				pts.append(_path_pos(path, maxf(0.0, path_u - k * step)))
			if int(sty.torp) != 0:
				_draw_ribbon(st.nodes[1], pts, head_size * 0.1, (sty.torp_color as Color) * fade * 0.6)
			# Battlechoreo: a spread's earlier tubes strike too (drawn only; the damage and the impact job land with the last).
			if j.outcome == BattleTimeline.Outcome.HIT and int(j.index) < int(j.get("count", 1)) - 1 and u >= 0.96 and not st.get("struck", false):
				st.struck = true
				var tg := _ship(j.target_fleet_id)
				var r0 := tg.radius() if tg != null else 4.0
				_burst(head, (sty.torp_color as Color).lerp(Color.WHITE, 0.3), "sparks_small", 14, maxf(1.2, r0 * 0.07))
			# Launch flash at the tube: a spiked flare that fades in 0.18 s.
			st.nodes[2].global_position = path[0]
			st.nodes[2].scale = Vector3.ONE * maxf(1.6 * TORPEDO_SIZE, _px_world_1080(path[0], 1.4 * MIN_TORPEDO_PX))
			_fxp(st.nodes[2], "flare_intensity", maxf(0.0, 1.0 - (t - j.start) / 0.18) * 1.6)
		Job.INTERCEPT:
			var p: Vector3 = st.point
			st.nodes[0].global_position = p
			_fxp(st.nodes[0], "flare_intensity", 2.0 * pow(1.0 - u, 2.0))
			var target := _ship(j.target_fleet_id)
			for k in 2:
				var beam: MeshInstance3D = st.nodes[1 + k]
				if target == null or u > 0.45:
					beam.visible = false
					continue
				var src := target.hardpoint(0, k, 0)
				_orient_beam(beam, src, p, maxf(BEAM_WIDTH * 0.25, _px_world_1080(p, BEAM_MIN_PX * 0.6) * 0.5))
				_fxp(beam, "intensity", 1.0 - u / 0.45)
		Job.SHIELD, Job.HULL:
			if st.nodes.is_empty():
				return
			st.nodes[0].global_position = st.point
			_floor_flare(st.nodes[0], st.get("size", 6.0), MIN_FLARE_PX * 2.2)
			var tshare := 1.0 / float(maxi(1, _hits_on.get(j.target_fleet_id, 1)))
			_fxp(st.nodes[0], "flare_intensity", 0.8 * tshare * pow(1.0 - u, 2.5))
			if st.has("t_flash"):
				# Torpedo impact: the big flash and the shockwave ring.
				var r0: float = st.t_radius
				var fl: MeshInstance3D = st.t_flash
				fl.scale = Vector3.ONE * maxf(0.8 * r0, _px_world_1080(fl.global_position, 90.0))
				_fxp(fl, "flare_intensity", 1.2 * exp(-9.0 * u) * maxf(tshare, 0.5)) # footage WotW 4916 s: a 4-point orange flare, gone in 0.25 s
				var ring: MeshInstance3D = st.t_ring
				ring.visible = false # footage: no shockwave ring on a torpedo hit (WotW, SoA, Tears); rings only on kills
				var grow := 0.3 + 1.3 * (1.0 - pow(1.0 - u, 2.0))
				var rr := maxf(r0, _px_world_1080(ring.global_position, 40.0)) * grow
				ring.scale = Vector3.ONE * rr / 0.4 # the ring sits at 0.4 of the quad (0.8 of its half size)
				_fxp(ring, "ring_age", u)
		Job.RAM:
			var a := _ship(j.fleet_id)
			var b := _ship(j.target_fleet_id)
			if a != null and b != null:
				st.nodes[0].global_position = a.global_position.lerp(b.global_position, 0.5)
				_fxp(st.nodes[0], "flare_intensity", smoothstep(0.6, 0.8, u) * (1.0 - smoothstep(0.8, 1.0, u)) * 3.0)
		Job.DECLOAK:
			var s := _ship(j.fleet_id)
			if s != null and not st.nodes.is_empty():
				st.nodes[0].global_position = s.global_position
				_fxp(st.nodes[0], "flare_intensity", sin(PI * u) * 0.6)
		Job.EXPLOSION:
			if st.nodes.is_empty():
				return
			if st.has("boomed") and not st.boomed:
				var s := _ship(j.fleet_id)
				var el: float = t - j.start
				var p0: Vector3 = s.global_position if s != null else st.point
				st.point = p0
				st.nodes[0].global_position = p0
				_fxp(st.nodes[0], "flare_intensity", 0.8 * exp(-el * 5.0) + 0.2) # 18 Nov: the white core stays small
				st.nodes[0].scale = Vector3.ONE * maxf(st.size * (0.3 + 0.1 * sin(el * 23.0)), _px_world(p0, 24.0))
				if t >= float(st.next_vent):
					st.next_vent = t + 0.3
					var q: Vector3 = s.hull_point(p0 + Vector3(sin(el * 7.0), 0.4, cos(el * 5.0)) * 50.0, int(el * 10.0)) if s != null else p0
					var k0 := maxf(1.5, float(st.size) * 0.08)
					_burst(q, Color(1.0, 0.45, 0.1), "fire_small", 14, k0)
					_burst(q, Color(1.0, 0.55, 0.25), "vent", 8, k0 * 0.8, (q - p0).normalized())
					_flash_light(q, Color(1.0, 0.55, 0.2), 3.0, float(st.size) * 2.5, 0.3)
				if el < float(j.breakup):
					return
				# Breakup: the fireball where the hulk has drifted to.
				st.boomed = true
				for n in st.nodes:
					if is_instance_valid(n):
						n.queue_free()
				_fireball(i, st, p0, float(st.size), s, t)
			_animate_kill(st, t, j.start + j.duration)
		Job.WARP:
			var s := _ship(j.fleet_id)
			if s != null and not st.nodes.is_empty():
				var bow := s.global_transform * Vector3(s.half_extents.x * (1.0 + 10.0 * u * u * u), 0, 0)
				st.nodes[0].global_position = bow
				_fxp(st.nodes[0], "flare_intensity", smoothstep(0.6, 0.9, u) * 3.0 * (1.0 - smoothstep(0.9, 1.0, u) * 0.7))
				_fxp(st.nodes[0], "flare_stretch", 3.0)


# ------------------------------------------------------------------------------------------------------------------
# One-shot extras
# ------------------------------------------------------------------------------------------------------------------

func _flash_light(p: Vector3, color: Color, energy: float, light_range: float, seconds: float) -> void:
	var l := OmniLight3D.new()
	l.light_color = color
	l.light_energy = minf(energy, 9.0) # F2: a kill or breach light no longer blows the hulls round it out to white
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
	# fire_small: soft round glows; the noisy puff read as dotted discs at that size (M1 weaponfx hull-hit strips).
	draw.albedo_texture = _puff_tex if kind in ["fireball", "smoke", "vent"] else _soft_tex
	draw.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	draw.disable_receive_shadows = true
	var ramp := Gradient.new()
	var mesh := QuadMesh.new()
	mesh.size = Vector2(1, 1)
	var align := false
	var hdr := func(c: Color, m: float, a: float) -> Color: return Color(c.r * m, c.g * m, c.b * m, a)
	if _web and kind in ["fireball", "fire_small", "debris", "streamer"]:
		# F2 follow-up (18 Nov): the web build (Compatibility, no HDR) clamps every channel above 1, so the 3-5x hot core
		# of a fire ramp turned (1, 1, 1) and 280 added particles read as a white puff. On the web the core keeps its hue
		# (no multiplier above 1, pulled toward the burst's orange) and each particle adds less, so kills and hull hits
		# read as orange fire as they do on the desktop build.
		hdr = func(c: Color, m: float, a: float) -> Color:
			var h := c.lerp(color, 0.55) * minf(m, 1.0)
			return Color(h.r, h.g, h.b, a * 0.55)
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
		"streamer":
			# Kill streamers (SoA 1161 s): long orange tongues of fire thrown out of the fireball, cooling to ember red.
			# Soft and fat, not needles: the show's streamers are smoky fire, never white spike lines.
			pm.initial_velocity_min = 2.5
			pm.initial_velocity_max = 7.0
			pm.damping_min = 1.5
			pm.damping_max = 3.0
			pm.scale_min = 0.8
			pm.scale_max = 1.4
			ramp.set_color(0, Color(color.r, color.g, color.b, 0.0))
			ramp.add_point(0.12, hdr.call(Color(1, 0.55, 0.15), 1.3, 0.75))
			ramp.add_point(0.55, hdr.call(Color(0.7, 0.22, 0.05), 1.0, 0.45))
			ramp.set_color(1, Color(0.15, 0.05, 0.02, 0.0))
			mesh.size = Vector2(0.55, 1.1)
			pm.particle_flag_align_y = true
			draw.albedo_texture = _puff_tex
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
		"vent":
			# Plasma / atmosphere venting from a breach: a pale jet that thins and spreads.
			pm.initial_velocity_min = 2.5
			pm.initial_velocity_max = 5.0
			pm.damping_min = 0.6
			pm.damping_max = 1.2
			pm.scale_min = 0.8
			pm.scale_max = 1.6
			var vcurve := Curve.new()
			vcurve.add_point(Vector2(0, 0.3))
			vcurve.add_point(Vector2(1, 1.6))
			var vct := CurveTexture.new()
			vct.curve = vcurve
			pm.scale_curve = vct
			# A dying ship's vents (warm colour) burn orange; a breach elsewhere vents pale plasma.
			ramp.set_color(0, hdr.call(Color(0.9, 0.95, 1.0) if color.b >= color.r else color, 1.6 if color.b >= color.r else 1.0, 0.7))
			ramp.add_point(0.35, hdr.call(color, 0.9, 0.35))
			ramp.set_color(1, Color(color.r * 0.4, color.g * 0.4, color.b * 0.4, 0.0))
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
			ramp.set_color(0, hdr.call(Color(1, 0.6, 0.2), 1.6, 1.0))
			ramp.add_point(0.5, hdr.call(color, 1.0, 0.8))
			ramp.set_color(1, Color(0.3, 0.08, 0.02, 0.0))
	var gt := GradientTexture1D.new()
	gt.gradient = ramp
	gt.use_hdr = true
	pm.color_ramp = gt
	mesh.material = draw
	var entry := [pm, mesh, align]
	_particle_mats[key] = entry
	return entry


## A continuous spark spray (a phaser burning on the hull): emits while `emitting` is on; BattleFx moves and aims it
## each frame and frees it after the beam. Half the sparks on the web build (ART-BIBLE 12.1 web VFX budget).
func _spark_stream(color: Color, scale_factor: float) -> GPUParticles3D:
	var e := _particle_material("sparks_small", color)
	var gp := GPUParticles3D.new()
	var pm: ParticleProcessMaterial = (e[0] as ParticleProcessMaterial).duplicate()
	pm.initial_velocity_min *= scale_factor
	pm.initial_velocity_max *= scale_factor
	pm.scale_min *= scale_factor
	pm.scale_max *= scale_factor
	pm.spread = 60.0
	gp.process_material = pm
	gp.draw_pass_1 = e[1]
	gp.amount = 8 if _web else 16
	gp.lifetime = 0.5
	gp.one_shot = false
	gp.explosiveness = 0.0
	gp.randomness = 0.5
	gp.local_coords = false
	gp.emitting = false
	gp.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var reach := 40.0 * maxf(1.0, scale_factor)
	gp.visibility_aabb = AABB(-Vector3.ONE * reach, Vector3.ONE * reach * 2.0)
	gp.transform_align = GPUParticles3D.TRANSFORM_ALIGN_Z_BILLBOARD_Y_TO_VELOCITY
	add_child(gp)
	return gp


func _burst(p: Vector3, color: Color, kind: String, amount: int, scale_factor := 1.0, dir := Vector3.ZERO) -> void:
	var e := _particle_material(kind, color)
	var gp := GPUParticles3D.new()
	gp.process_material = e[0]
	gp.draw_pass_1 = e[1]
	gp.amount = maxi(1, amount)
	gp.one_shot = true
	gp.explosiveness = {"smoke": 0.7, "vent": 0.0}.get(kind, 0.95) # a vent streams out over its life
	gp.lifetime = {"sparks": 0.9, "streamer": 1.4, "sparks_small": 0.5, "fire_small": 0.6, "fireball": 1.8, "smoke": 3.5, "debris": 2.5, "vent": 1.8}.get(kind, 1.0)
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
