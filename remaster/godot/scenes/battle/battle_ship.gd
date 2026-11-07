class_name BattleShip
extends Node3D
## One ship in the battle scene: a model (imported by asset key, else a procedural faction hull), its shield bubble,
## hardpoints and scorch decals. The local frame is x forward, y up, z starboard; the ship is `length` units long.

const HULL_SHADER := preload("res://scenes/battle/shaders/hull.gdshader")
const SHIELD_SHADER := preload("res://scenes/battle/shaders/shield.gdshader")
const CLOAK_SHADER := preload("res://scenes/battle/shaders/cloak.gdshader")
const BLINK_SHADER := preload("res://scenes/battle/shaders/blink.gdshader")

var fleet_id := 0
var owner_id := 0
var ship_name := ""
var length := 4.0
var is_station := false
var model: Node3D          ## Holds the hull (scaled to `length`); tumbles and stretches for explosions / warp-outs.
var shield: MeshInstance3D
var half_extents := Vector3(0.5, 0.15, 0.3) ## In ship units (after scaling to `length`).
var sockets: Array[Node3D] = []            ## WPN_* nodes of an imported model (a hero glb names them SOCKET_WPN_*).
var engine_sockets: Array[Node3D] = []     ## ENG_* nodes of an imported model.
var imported := false
var hero_key := ""                         ## The godot-hero-manifest.json key this ship's model came from.
var canon_length_m := 0.0                  ## The hero model's canon length in metres (0 for other models).
var model_scale := 1.0                     ## World units per model unit (metres for a hero glb).

var _meshes: Array[GeometryInstance3D] = []
var _shield_mat: ShaderMaterial
var _cloak_mat: ShaderMaterial
var _hits: Array = []      ## [{dir: Vector3, start: float, duration: float, color: Color}]
var _decals: Array[Decal] = []
var _center: Node3D

static var _scorch_tex: Texture2D
static var _ember_tex: Texture2D
static var _materials := {}


# ------------------------------------------------------------------------------------------------------------------
# Construction
# ------------------------------------------------------------------------------------------------------------------

## ship: a tactical ship Dictionary (botf_call "tactical" ships[] entry). assets: optional BattleAssets.
func setup(ship: Dictionary, assets: BattleAssets = null) -> void:
	fleet_id = int(ship.get("id", 0))
	owner_id = int(ship.get("owner", 0))
	ship_name = str(ship.get("name", ""))
	is_station = fleet_id >= 2000000 or int(ship.get("referenceShipId", 0)) < 0 or bool(ship.get("station", false))
	length = BattleShip.length_for(ship)
	name = "Ship_%d" % fleet_id
	model = Node3D.new()
	model.name = "Model"
	add_child(model)
	var hull: Node3D = null
	if assets != null:
		hull = assets.instantiate_ship(BattleShip.asset_keys_for(ship))
	if hull != null:
		imported = true
		if hull.has_meta("botf_hero"):
			_fit_hero(hull)
		else:
			_fit_imported(hull)
	else:
		hull = ShipFactory.build(owner_id, is_station, ship)
		hull.scale = Vector3.ONE * length
	_center = Node3D.new()
	_center.name = "Center"
	model.add_child(_center)
	_center.add_child(hull)
	_collect(hull)
	_measure()
	_build_shield()
	_cloak_mat = ShaderMaterial.new()
	_cloak_mat.shader = CLOAK_SHADER


## Hull length in world units: square-root compressed by hull strength (class size), stations largest. A ship
## Dictionary may set "displayLength" (world units) to override it (the title fleet does).
static func length_for(ship: Dictionary) -> float:
	if float(ship.get("displayLength", 0.0)) > 0.0:
		return float(ship.displayLength)
	var fid := int(ship.get("id", 0))
	if fid >= 2000000 or int(ship.get("referenceShipId", 0)) < 0 or bool(ship.get("station", false)):
		return 11.0
	var hull := float(ship.get("maxHull", ship.get("initialHull", 60)))
	var klass := str(ship.get("shipClass", ""))
	var base := 2.0 + sqrt(maxf(hull, 1.0)) * 0.36
	if klass == "Scout":
		base *= 0.75
	return clampf(base, 2.0, 8.5)


## Asset keys to try: the core's assetKey when present, then ship.<fac>.<class slug>, then the faction study.
static func asset_keys_for(ship: Dictionary) -> PackedStringArray:
	var keys := PackedStringArray()
	var fac := BattleTimeline.faction_prefix(int(ship.get("owner", 0)))
	if str(ship.get("assetKey", "")) != "":
		keys.append(str(ship.assetKey))
	var klass := str(ship.get("name", "")).split(" — ")[0].split(" - ")[0].strip_edges()
	if fac != "" and klass != "":
		keys.append("ship.%s.%s" % [fac, _slug(klass)])
	if fac != "":
		keys.append("study." + fac)
	return keys


static func _slug(s: String) -> String:
	var out := ""
	var sep := false
	for c in s.to_lower():
		if (c >= "a" and c <= "z") or (c >= "0" and c <= "9"):
			if sep and out != "":
				out += "_"
			out += c
			sep = false
		else:
			sep = true
	return out if out != "" else "unknown"


func _fit_imported(hull: Node3D) -> void:
	# Normalise: longest horizontal axis forward (+x), centred, scaled to `length`.
	var box := _aabb_of(hull, Transform3D.IDENTITY)
	var rot := Basis.IDENTITY
	if box.size.z > box.size.x:
		rot = Basis(Vector3.UP, PI * 0.5)
	var longest := maxf(box.size.x, box.size.z)
	var s := length / maxf(longest, 1e-3)
	hull.transform = Transform3D(rot.scaled(Vector3.ONE * s), -(rot * box.get_center()) * s)
	_collect_sockets(hull)


## A hero glb (BattleAssets.instantiate_hero): already nose +X, up +Y, in metres, so it is scaled by its canon length
## (lengthMeters -> `length` world units) rather than by its bounds, and its WPN_* sockets scale with it.
func _fit_hero(hull: Node3D) -> void:
	var hero: Dictionary = hull.get_meta("botf_hero")
	hero_key = str(hull.get_meta("botf_hero_key", ""))
	var box := _aabb_of(hull, Transform3D.IDENTITY)
	canon_length_m = float(hero.get("lengthMeters", 0.0))
	if canon_length_m <= 0.0:
		canon_length_m = maxf(box.size.x, 1e-3)
	model_scale = length / canon_length_m
	hull.transform = Transform3D(Basis.from_scale(Vector3.ONE * model_scale), -box.get_center() * model_scale)
	HeroLook.apply(hull, hero, str(hull.get_meta("botf_hero_root", "")))
	HeroLook.apply_lods(hull, hero, length)
	_collect_sockets(hull)


func _collect_sockets(hull: Node3D) -> void:
	for n in hull.find_children("*", "Node3D", true, false):
		var socket := BattleShip.socket_name(str(n.name))
		if socket.begins_with("WPN_"):
			sockets.append(n)
		elif socket.begins_with("ENG_"):
			engine_sockets.append(n)


## A socket node's name without the exporter's prefix: "SOCKET_WPN_Beam_01" (Blender / Unreal convention) and
## "WPN_Beam_01" both give "WPN_BEAM_01".
static func socket_name(node_name: String) -> String:
	var n := node_name.to_upper()
	if n.begins_with("SOCKET_"):
		n = n.substr(7)
	return n


func _collect(root: Node) -> void:
	for n in root.find_children("*", "GeometryInstance3D", true, false):
		_meshes.append(n)
	if root is GeometryInstance3D:
		_meshes.append(root)


static func _aabb_of(root: Node3D, xf: Transform3D) -> AABB:
	var box := AABB()
	var first := true
	var stack := [[root, xf * root.transform]]
	while not stack.is_empty():
		var item = stack.pop_back()
		var node: Node = item[0]
		var t: Transform3D = item[1]
		if node is MeshInstance3D and node.mesh != null:
			var b: AABB = t * node.mesh.get_aabb()
			box = b if first else box.merge(b)
			first = false
		for c in node.get_children():
			if c is Node3D:
				stack.append([c, t * c.transform])
	return box


func _measure() -> void:
	var box := _aabb_of(model, Transform3D.IDENTITY)
	half_extents = Vector3(maxf(box.size.x, 0.5), maxf(box.size.y, 0.2), maxf(box.size.z, 0.3)) * 0.5
	# Keep the model centred on the pose.
	_center.position = -box.get_center()


func _build_shield() -> void:
	shield = MeshInstance3D.new()
	shield.name = "Shield"
	var sphere := SphereMesh.new()
	sphere.radius = 1.0
	sphere.height = 2.0
	sphere.radial_segments = 48
	sphere.rings = 24
	shield.mesh = sphere
	shield.scale = half_extents * Vector3(1.3, 1.9, 1.45) + Vector3.ONE * 0.25
	_shield_mat = ShaderMaterial.new()
	_shield_mat.shader = SHIELD_SHADER
	shield.material_override = _shield_mat
	shield.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	shield.visible = false
	add_child(shield)
	_clear_hits()


func _clear_hits() -> void:
	var off := []
	var cols := []
	for i in 6:
		off.append(Vector4(1, 0, 0, -1))
		cols.append(Vector4(0, 0, 0, 0))
	_shield_mat.set_shader_parameter("hits", off)
	_shield_mat.set_shader_parameter("hit_colors", cols)


# ------------------------------------------------------------------------------------------------------------------
# Queries
# ------------------------------------------------------------------------------------------------------------------

## World position of the emitter for (family, mount, emitter): a WPN_* socket when the model has one, else a
## bounds-based hardpoint (BattleTimeline.fallback_hardpoint).
func hardpoint(family: int, mount_index: int, emitter_index: int) -> Vector3:
	if not sockets.is_empty():
		var names: Array[String] = []
		for s in sockets:
			names.append(BattleShip.socket_name(str(s.name)))
		var pick := BattleShip.pick_socket(names, family, mount_index, emitter_index)
		if pick >= 0:
			return sockets[pick].global_position
	var hp := BattleTimeline.fallback_hardpoint(family, mount_index, emitter_index)
	var local := Vector3(hp.x * half_extents.x, hp.z * half_extents.y * 2.0, hp.y * half_extents.z)
	return global_transform * local


## Index of the WPN_* socket for a mount (BotfCombatTimeline.h pickWeaponSocket), -1 when none fits.
static func pick_socket(names: Array[String], family: int, mount_index: int, emitter_index: int) -> int:
	var preferred := []
	var other := []
	var want := "WPN_PULSE" if family == 1 else "WPN_BEAM"
	for i in names.size():
		var n := names[i].to_upper()
		if not n.begins_with("WPN_"):
			continue
		var torp := n.find("TORP") >= 0
		if family == 2:
			if torp:
				preferred.append(i)
			continue
		if torp:
			continue
		(preferred if n.begins_with(want) else other).append(i)
	var pool: Array = preferred if not preferred.is_empty() else other
	if pool.is_empty():
		return -1
	pool.sort_custom(func(a, b): return names[a].to_upper() < names[b].to_upper())
	return pool[(maxi(0, mount_index) + maxi(0, emitter_index)) % pool.size()]


## Point on the shield shell facing `from` (world), for beam ends and impact effects.
func shield_point(from: Vector3) -> Vector3:
	var local_dir := (global_transform.affine_inverse() * from).normalized()
	return global_transform * (local_dir * shield.scale * 0.98)


## Point on the hull facing `from` (world), jittered by `seed`.
func hull_point(from: Vector3, seed := 0) -> Vector3:
	var local_dir := (global_transform.affine_inverse() * from).normalized()
	var rng := RandomNumberGenerator.new()
	rng.seed = seed
	local_dir += Vector3(rng.randf_range(-0.3, 0.3), rng.randf_range(-0.2, 0.2), rng.randf_range(-0.3, 0.3))
	local_dir = local_dir.normalized()
	return global_transform * (local_dir * half_extents * 0.75)


func radius() -> float:
	return maxf(half_extents.x, half_extents.z) * 1.3


# ------------------------------------------------------------------------------------------------------------------
# Per-frame state
# ------------------------------------------------------------------------------------------------------------------

## visibility 1 = solid, 0 = gone; shimmer 0..1 = cloak ripple strength.
func set_visibility(visibility: float, shimmer: float) -> void:
	visible = visibility > 0.001 or shimmer > 0.01
	var fade := clampf(1.0 - visibility, 0.0, 1.0)
	for m in _meshes:
		m.transparency = fade
		if shimmer > 0.01:
			m.material_overlay = _cloak_mat
			m.set_instance_shader_parameter("shimmer", shimmer)
		elif m.material_overlay == _cloak_mat:
			m.material_overlay = null


## Registers a shield impact from world point `from`, running [start, start + duration] in timeline seconds.
func add_shield_hit(from: Vector3, start: float, duration: float, color: Color) -> void:
	var dir := (global_transform.basis.inverse() * (from - global_position)).normalized()
	_hits.append({"dir": dir, "start": start, "duration": duration, "color": color})


func update_shield(t: float) -> void:
	var dirs := []
	var cols := []
	var active := 0
	var keep := []
	for h in _hits:
		var age: float = (t - h.start) / maxf(h.duration, 1e-3)
		if age > 1.0:
			continue
		keep.append(h)
		if age < 0.0 or active >= 6:
			continue
		dirs.append(Vector4(h.dir.x, h.dir.y, h.dir.z, age))
		cols.append(Vector4(h.color.r, h.color.g, h.color.b, 1.0))
		active += 1
	_hits = keep
	shield.visible = active > 0
	if active == 0:
		return
	while dirs.size() < 6:
		dirs.append(Vector4(1, 0, 0, -1))
		cols.append(Vector4.ZERO)
	_shield_mat.set_shader_parameter("hits", dirs)
	_shield_mat.set_shader_parameter("hit_colors", cols)


func clear_effects() -> void:
	_hits.clear()
	shield.visible = false


## Burn mark where the hull was struck; `heat` glows and cools over the following seconds.
func add_scorch(world_point: Vector3, from: Vector3, size: float) -> Decal:
	if _decals.size() >= 10:
		var old: Decal = _decals.pop_front()
		old.queue_free()
	var d := Decal.new()
	d.texture_albedo = BattleShip.scorch_texture()
	d.texture_emission = BattleShip.ember_texture()
	d.emission_energy = 3.0
	d.albedo_mix = 0.95
	d.size = Vector3(size, size, size)
	d.upper_fade = 0.2
	d.lower_fade = 0.2
	d.cull_mask = 1
	model.add_child(d)
	d.global_position = world_point
	var n := (from - world_point).normalized()
	# Decals project along their local -Y: point +Y out of the hull, toward the shooter.
	var up := n
	var side := up.cross(Vector3.FORWARD if absf(up.dot(Vector3.FORWARD)) < 0.9 else Vector3.RIGHT).normalized()
	d.global_basis = Basis(side, up, side.cross(up)).orthonormalized().scaled(Vector3.ONE)
	d.size = Vector3(size, size * 1.5, size)
	_decals.append(d)
	return d


static func scorch_texture() -> Texture2D:
	if _scorch_tex == null:
		_scorch_tex = ImageTexture.create_from_image(_blotch(96, false))
	return _scorch_tex


static func ember_texture() -> Texture2D:
	if _ember_tex == null:
		_ember_tex = ImageTexture.create_from_image(_blotch(96, true))
	return _ember_tex


static func _blotch(size: int, ember: bool) -> Image:
	var img := Image.create(size, size, false, Image.FORMAT_RGBA8)
	var noise := FastNoiseLite.new()
	noise.seed = 7 if ember else 3
	noise.frequency = 0.09
	noise.fractal_octaves = 4
	for y in size:
		for x in size:
			var p := Vector2(x, y) / (size - 1) * 2.0 - Vector2.ONE
			var r := p.length()
			var n := noise.get_noise_2d(x, y) * 0.5 + 0.5
			var a := clampf((1.0 - r) * 1.6 + (n - 0.5) * 1.2, 0.0, 1.0)
			if ember:
				var e := clampf((0.45 - r) * 3.0 + (n - 0.5) * 1.5, 0.0, 1.0)
				img.set_pixel(x, y, Color(1.0, 0.45 * e + 0.1, 0.05, e * e))
			else:
				var c := 0.03 + 0.06 * n
				img.set_pixel(x, y, Color(c, c * 0.9, c * 0.8, a * a))
	return img
