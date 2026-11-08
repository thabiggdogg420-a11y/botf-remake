class_name BattleShip
extends Node3D
## One ship in the battle scene: a model (imported by asset key, else a procedural faction hull), its shield bubble,
## hardpoints and scorch decals. The local frame is x forward, y up, z starboard; the ship is `length` units long.
##
## True scale (BotfAssetCatalog.h displayLengthForLength): a hull is drawn at its class's canon length, 350 m = 120
## Unreal units with a 60 m floor (runabouts and fighters stay visible) and a 1000-unit cap, on Unreal's 170-unit cell,
## mapped to this scene's BattleScene.CELL. A 642 m Galaxy spans 1.3 cells.

const UE_UNITS_PER_METER := 120.0 / 350.0
const UE_CELL_UNITS := 170.0       ## ABotfCombatScene::CellUnits.
const MIN_DISPLAY_METERS := 60.0
const MAX_DISPLAY_UE_UNITS := 1000.0
const UNKNOWN_DISPLAY_METERS := 470.0 ## Unknown class: Unreal's interim 80-unit bounding radius (about a 470 m cruiser).
const STATION_CELLS := 2.0         ## Stations: Unreal's 170-unit-radius disc footprint.

const HULL_SHADER := preload("res://scenes/battle/shaders/hull.gdshader")
const SHIELD_SHADER := preload("res://scenes/battle/shaders/shield.gdshader")
const CLOAK_SHADER := preload("res://scenes/battle/shaders/cloak.gdshader")
const BLINK_SHADER := preload("res://scenes/battle/shaders/blink.gdshader")

var fleet_id := 0
var owner_id := 0
var ship_name := ""
var length := 4.0
var length_meters := 0.0   ## Canon length of the shown class (0 = unknown): flight style, wings and true scale.
var is_station := false
var spacing := Vector3.ZERO  ## Eased presentation offset that keeps true-scale hulls apart (world units).
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
var _socket_pick := {}   ## (family, mount, emitter) key -> index into sockets (-1 = none): see hardpoint()
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
	is_station = BattleShip.is_station_ship(ship)
	length_meters = BattleShip.length_meters_for(ship, assets)
	if not is_station and assets != null:
		# A hero model's own canon length wins: the hull is drawn at the size its glb was built for.
		var hero_m := assets.hero_length(BattleShip.asset_keys_for(ship))
		if hero_m > 0.0:
			length_meters = hero_m
	if float(ship.get("displayLength", 0.0)) > 0.0:
		length = float(ship.displayLength) # explicit override (the title fleet, tests)
	else:
		length = STATION_CELLS * BattleScene.CELL if is_station else BattleShip.display_length(length_meters)
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


static func is_station_ship(ship: Dictionary) -> bool:
	return int(ship.get("id", 0)) >= 2000000 or int(ship.get("referenceShipId", 0)) < 0 or bool(ship.get("station", false))


## Hull length in world units at true scale (stations: their footprint), from the catalogue only.
## A ship Dictionary may set "displayLength" (world units) to override it (the title fleet does).
static func length_for(ship: Dictionary) -> float:
	if float(ship.get("displayLength", 0.0)) > 0.0:
		return float(ship.displayLength)
	if BattleShip.is_station_ship(ship):
		return STATION_CELLS * BattleScene.CELL
	return BattleShip.display_length(ShipFactory.canon_length_meters(ship))


## Displayed length in Unreal scene units (BotfAssetCatalog.h displayLengthForLength): canon metres at 120 units per
## 350 m, never below a 60 m ship, never above 1000 units.
static func display_length_ue(meters: float) -> float:
	return clampf(meters, MIN_DISPLAY_METERS, MAX_DISPLAY_UE_UNITS / UE_UNITS_PER_METER) * UE_UNITS_PER_METER


## Displayed length in this scene's world units (0 or less = unknown class).
static func display_length(meters: float) -> float:
	var m := meters if meters > 0.0 else UNKNOWN_DISPLAY_METERS
	return BattleShip.display_length_ue(m) / UE_CELL_UNITS * BattleScene.CELL


## Canon length in metres of the class shown for `ship`, as the Unreal client gets lengthMeters (the catalogue's
## targetLengthMeters for the class): the local asset manifest's value for the class key when one is loaded and
## carries it, else the catalogue (ShipFactory.canon_length_meters). 0 = unknown.
static func length_meters_for(ship: Dictionary, assets: BattleAssets = null) -> float:
	if BattleShip.is_station_ship(ship):
		return 0.0
	if assets != null and assets.has_manifest():
		for key in BattleShip.asset_keys_for(ship):
			var rec = assets.assets.get(key)
			if rec is Dictionary and rec.get("targetLengthMeters") != null and float(rec.targetLengthMeters) > 0.0:
				return float(rec.targetLengthMeters)
	return ShipFactory.canon_length_meters(ship)


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


## Asset-key slug (assets/check_asset_keys.py slug): lower-case, apostrophes dropped, other separators one '_'.
static func _slug(s: String) -> String:
	var out := ""
	var sep := false
	for c in s.to_lower():
		if c == "'":
			continue
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
		# The pick depends only on the key (sockets never change after setup): cached, since beams and torpedoes ask for
		# their hardpoint every frame.
		var key := (family * 1000 + maxi(0, mount_index)) * 1000 + maxi(0, emitter_index)
		var pick: int = _socket_pick.get(key, -2)
		if pick == -2:
			var names: Array[String] = []
			for s in sockets:
				names.append(BattleShip.socket_name(str(s.name)))
			pick = BattleShip.pick_socket(names, family, mount_index, emitter_index)
			_socket_pick[key] = pick
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


## Damage feedback (R3.5, coordinator 8 Nov): fires and venting plasma grow on a ship as its hull falls. damage is
## 0 (intact) .. 1 (wrecked); below 15% nothing shows, then up to three fires and a vent appear.
var _damage_fx: Array = []
var _damage_shown := -1.0

func set_damage(damage: float) -> void:
	damage = clampf(damage, 0.0, 1.0)
	if absf(damage - _damage_shown) < 0.02:
		return
	_damage_shown = damage
	if _damage_fx.is_empty():
		if damage < 0.15:
			return
		_build_damage_fx()
	var fires := int(ceil(clampf((damage - 0.15) / 0.28, 0.0, 3.0)))
	for k in _damage_fx.size():
		var on := k < fires if k < 3 else damage > 0.45
		for e in _damage_fx[k]:
			e.emitting = on
			e.visible = on


static var _flame_tex: Texture2D
static var _puff_tex: Texture2D

## A 4x4 flipbook of a licking flame, generated here (no third-party art): a teardrop mask over rising value noise.
static func flame_flipbook() -> Texture2D:
	if _flame_tex != null:
		return _flame_tex
	var fs := 64
	var img := Image.create(fs * 4, fs * 4, false, Image.FORMAT_RGBA8)
	var noise := FastNoiseLite.new()
	noise.seed = 4127
	noise.frequency = 0.09
	noise.fractal_octaves = 3
	for f in 16:
		var ox := (f % 4) * fs
		var oy := (f / 4) * fs
		var rise := float(f) * 6.0
		for y in fs:
			for x in fs:
				var u := (float(x) + 0.5) / fs * 2.0 - 1.0
				var v := 1.0 - (float(y) + 0.5) / fs # 0 bottom .. 1 top
				var n := noise.get_noise_2d(float(x) * 1.2, float(y) * 1.2 + rise) * 0.5 + 0.5
				var width := 0.62 * (1.0 - v) + 0.08
				var shape := clampf(1.0 - absf(u + (n - 0.5) * 0.5 * v) / width, 0.0, 1.0)
				var a := clampf(shape * (n * 1.4 - v * 0.9 + 0.35), 0.0, 1.0)
				var heat := clampf(a * 1.3 - v * 0.4, 0.0, 1.0)
				var col := Color(1.0, 0.25 + 0.7 * heat, 0.05 + 0.5 * heat * heat, a)
				img.set_pixel(ox + x, oy + y, col)
	_flame_tex = ImageTexture.create_from_image(img)
	return _flame_tex


## Soft round puff for smoke and venting gas.
static func puff_texture() -> Texture2D:
	if _puff_tex != null:
		return _puff_tex
	var n := 64
	var img := Image.create(n, n, false, Image.FORMAT_RGBA8)
	for y in n:
		for x in n:
			var d := Vector2(x - n * 0.5 + 0.5, y - n * 0.5 + 0.5).length() / (n * 0.5)
			var a := clampf(1.0 - d, 0.0, 1.0)
			img.set_pixel(x, y, Color(1, 1, 1, a * a))
	_puff_tex = ImageTexture.create_from_image(img)
	return _puff_tex


static func _particle_mat(tex: Texture2D, additive: bool, flipbook: bool) -> StandardMaterial3D:
	var mat := StandardMaterial3D.new()
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.blend_mode = BaseMaterial3D.BLEND_MODE_ADD if additive else BaseMaterial3D.BLEND_MODE_MIX
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	mat.vertex_color_use_as_albedo = true
	mat.albedo_texture = tex
	mat.depth_draw_mode = BaseMaterial3D.DEPTH_DRAW_DISABLED
	if flipbook:
		mat.particles_anim_h_frames = 4
		mat.particles_anim_v_frames = 4
		mat.particles_anim_loop = true
	return mat


func _emitter(mat: StandardMaterial3D, at: Vector3, amount: int, life: float, dir: Vector3, spread: float, vel: Vector2, size: Vector2, ramp: Gradient, flipbook: bool) -> CPUParticles3D:
	var quad := QuadMesh.new()
	quad.material = mat
	var e := CPUParticles3D.new()
	e.mesh = quad
	e.local_coords = false
	e.amount = amount
	e.lifetime = life
	e.position = at
	e.direction = dir
	e.spread = spread
	e.initial_velocity_min = vel.x
	e.initial_velocity_max = vel.y
	e.gravity = Vector3.ZERO
	e.scale_amount_min = size.x
	e.scale_amount_max = size.y
	e.color_ramp = ramp
	if flipbook:
		e.anim_speed_min = 1.0
		e.anim_speed_max = 1.4
		e.anim_offset_max = 1.0
	e.emitting = false
	e.visible = false
	add_child(e)
	return e


func _build_damage_fx() -> void:
	var L := maxf(length, 2.0)
	var fire_mat := BattleShip._particle_mat(BattleShip.flame_flipbook(), true, true)
	var smoke_mat := BattleShip._particle_mat(BattleShip.puff_texture(), false, false)
	var gas_mat := BattleShip._particle_mat(BattleShip.puff_texture(), true, false)
	var fire_ramp := Gradient.new()
	fire_ramp.set_color(0, Color(1.0, 0.95, 0.8, 1.0))
	fire_ramp.set_color(1, Color(1.0, 0.5, 0.2, 0.0))
	var smoke_ramp := Gradient.new()
	smoke_ramp.set_color(0, Color(0.32, 0.3, 0.3, 0.75))
	smoke_ramp.set_color(1, Color(0.18, 0.18, 0.2, 0.0))
	var gas_ramp := Gradient.new()
	gas_ramp.set_color(0, Color(0.85, 0.95, 1.0, 0.9))
	gas_ramp.set_color(1, Color(0.45, 0.65, 1.0, 0.0))
	for k in 3:
		var r := RandomNumberGenerator.new()
		r.seed = hash([name, k])
		var at := Vector3(r.randf_range(-0.4, 0.35) * half_extents.x, half_extents.y * 1.05, r.randf_range(-0.5, 0.5) * half_extents.z)
		var fire := _emitter(fire_mat, at, 18, 0.7, Vector3.UP, 20.0, Vector2(L * 0.04, L * 0.09), Vector2(L * 0.16, L * 0.26), fire_ramp, true)
		var smoke := _emitter(smoke_mat, at, 26, 2.4, Vector3.UP, 30.0, Vector2(L * 0.05, L * 0.1), Vector2(L * 0.14, L * 0.3), smoke_ramp, false)
		smoke.scale_amount_curve = _grow_curve()
		_damage_fx.append([fire, smoke])
	var vat := Vector3(-0.1 * half_extents.x, half_extents.y * 0.6, half_extents.z * 1.02)
	var vent := _emitter(gas_mat, vat, 40, 1.1, Vector3(0.0, 0.25, 1.0).normalized(), 9.0, Vector2(L * 0.35, L * 0.5), Vector2(L * 0.05, L * 0.1), gas_ramp, false)
	vent.scale_amount_curve = _grow_curve()
	_damage_fx.append([vent])


static func _grow_curve() -> Curve:
	var c := Curve.new()
	c.max_value = 2.0
	c.add_point(Vector2(0, 0.4))
	c.add_point(Vector2(1, 1.6))
	return c


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
