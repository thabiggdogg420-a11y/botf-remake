class_name PlanetBody
extends Node
## Makes a sphere mesh a photoreal planet (art wave 2): the L7 texture set from PlanetTextures (res:// or a mounted
## user:// pack, never a disk path) on planet_surface.gdshader, plus a cloud shell and an atmosphere rim shell. Shared by the
## galaxy system view, the system screen's orbs and the battle backdrop so they look like the turntable sheets
## (tools/turntable). Textures stream in on a worker: until the albedo arrives the mesh keeps the material it had (the
## procedural planet), then the textured look replaces it in one frame. Classes with no set stay procedural.
## Renderer: Forward+ gets the desktop tier (when `hi`) with normal map; Compatibility gets the web tier, relief baked in.

const SURFACE := preload("res://scenes/galaxy/shaders/planet_surface.gdshader")
const CLOUD := preload("res://scenes/galaxy/shaders/planet_cloud.gdshader")
const ATMO := preload("res://scenes/galaxy/shaders/planet_atmo.gdshader")

## Planets still streaming textures (captures wait for 0).
static var pending := 0

## Per class look (turntable PLANET_LOOK): atmosphere tint and strength, cloud tint and opacity, night lights or lava.
const LOOK := {
	"M": {"atmo": Color(0.32, 0.58, 1.0), "str": 0.9, "cloud": Color(1, 1, 1), "calpha": 0.8, "lights": true},
	"O": {"atmo": Color(0.3, 0.6, 1.0), "str": 0.9, "cloud": Color(1, 1, 1), "calpha": 0.8, "lights": true},
	"L": {"atmo": Color(0.55, 0.8, 0.5), "str": 0.8, "cloud": Color(0.96, 0.95, 0.84), "calpha": 0.8, "lights": true},
	"P": {"atmo": Color(0.6, 0.78, 0.95), "str": 0.6, "cloud": Color(1, 1, 1), "calpha": 0.7, "lights": true},
	"H": {"atmo": Color(0.9, 0.62, 0.42), "str": 0.45, "cloud": Color(0.95, 0.82, 0.72), "calpha": 0.6, "lights": true},
	"K": {"atmo": Color(0, 0, 0), "str": 0.0, "cloud": Color(1, 1, 1), "calpha": 0.0, "lights": false},
	"Y": {"atmo": Color(1.0, 0.55, 0.2), "str": 1.5, "cloud": Color(0.55, 0.36, 0.2), "calpha": 0.38, "lava": true},
	"GasGiant": {"atmo": Color(0.85, 0.78, 0.62), "str": 0.8, "cloud": Color(1, 1, 1), "calpha": 0.0, "lights": false},
}

var mesh_instance: MeshInstance3D
var cls := "M"
var hi := false
var inhabited := true
var from_point := false
var star_pos := Vector3.ZERO
var sun_dir := Vector3(0.5, 0.3, 0.8)
var sun_color := Color(1.0, 0.95, 0.88)
var sun_energy := 1.35
var sun_node: Node3D ## optional: follow this light's direction (the battle sun), so a staged sun moves the lighting too
var _mats: Array[ShaderMaterial] = []
var _set: Dictionary = {}
var _surface: ShaderMaterial
var _cloud: MeshInstance3D
var _counted := false
var _swapped := false

## Attach to `mi` (a sphere mesh). `opts`: hi (bool), inhabited (bool), variant (int), star_pos (Vector3, point sun) or
## sun_dir (Vector3 toward the sun) or sun_node (a light to follow), sun_color (Color), sun_energy (float). Returns the body, or null when the class has no
## texture set (the mesh then keeps its procedural material).
static func attach(mi: MeshInstance3D, cls_: String, opts: Dictionary = {}) -> PlanetBody:
	var sets := PlanetTextures.variants(cls_, bool(opts.get("hi", false)))
	if sets.is_empty() or not LOOK.has(cls_):
		return null
	var b := PlanetBody.new()
	b.name = "PlanetBody"
	b.mesh_instance = mi
	b.cls = cls_
	b.hi = bool(opts.get("hi", false))
	b.inhabited = bool(opts.get("inhabited", true))
	if opts.has("star_pos"):
		b.from_point = true
		b.star_pos = opts["star_pos"]
	if opts.has("sun_dir"):
		b.sun_dir = (opts["sun_dir"] as Vector3).normalized()
	b.sun_color = opts.get("sun_color", b.sun_color)
	b.sun_energy = float(opts.get("sun_energy", b.sun_energy))
	b.sun_node = opts.get("sun_node")
	b._set = sets[int(opts.get("variant", 0)) % sets.size()]
	mi.add_child(b)
	return b

func _ready() -> void:
	pending += 1
	_counted = true
	set_process(true)

func _exit_tree() -> void:
	_done()

func _done() -> void:
	_finish()
	set_process(false)

## Loading is over (the pending count drops); a body that follows a light keeps processing to track it.
func _finish() -> void:
	if _counted:
		pending -= 1
		_counted = false
	if sun_node == null:
		set_process(false)

func _light(m: ShaderMaterial) -> void:
	_mats.append(m)
	m.set_shader_parameter("from_point", from_point)
	m.set_shader_parameter("star_pos", star_pos)
	m.set_shader_parameter("sun_dir", sun_dir)
	m.set_shader_parameter("sun_color", Vector3(sun_color.r, sun_color.g, sun_color.b))
	m.set_shader_parameter("sun_energy", sun_energy)

func _process(_dt: float) -> void:
	if mesh_instance == null or not is_instance_valid(mesh_instance):
		_done()
		return
	var look: Dictionary = LOOK[cls]
	if sun_node != null and is_instance_valid(sun_node):
		var d := sun_node.global_transform.basis.z.normalized()
		if not d.is_equal_approx(sun_dir):
			sun_dir = d
			for m in _mats:
				m.set_shader_parameter("sun_dir", d)
	if not _swapped:
		var alb := PlanetTextures.get_texture(_set["albedo"])
		if alb == null:
			return
		_surface = ShaderMaterial.new()
		_surface.shader = SURFACE
		_surface.set_shader_parameter("albedo_tex", alb)
		_light(_surface)
		var lava: bool = look.get("lava", false)
		_surface.set_shader_parameter("emit_always", lava)
		_surface.set_shader_parameter("emit_energy", 1.3 if lava else 1.4)
		_surface.set_shader_parameter("emit_amount", 1.0 if inhabited else 0.0)
		mesh_instance.material_override = _surface
		_swapped = true
		if float(look.get("str", 0.0)) > 0.0:
			var am := ShaderMaterial.new()
			am.shader = ATMO
			var ac: Color = look["atmo"]
			am.set_shader_parameter("tint", Vector3(ac.r, ac.g, ac.b))
			am.set_shader_parameter("strength", float(look["str"]))
			_light(am)
			_shell(mesh_instance, 1.035, am)
	# Optional layers arrive after the albedo; the work is done when each wanted one is loaded.
	var left := false
	for layer in ["rough", "normal", "night", "clouds"]:
		if not _set.has(layer) or _surface.get_meta("got_" + layer, false):
			continue
		var wanted := true
		if layer == "night":
			wanted = look.get("lights", false) or look.get("lava", false)
		elif layer == "clouds":
			wanted = float(look.get("calpha", 0.0)) > 0.0
		elif layer == "normal":
			wanted = hi
		if not wanted:
			continue
		var t := PlanetTextures.get_texture(_set[layer])
		if t == null:
			left = true
			continue
		_surface.set_meta("got_" + layer, true)
		match layer:
			"rough": _surface.set_shader_parameter("rough_tex", t)
			"normal":
				_surface.set_shader_parameter("normal_tex", t)
				_surface.set_shader_parameter("has_normal", true)
			"night": _surface.set_shader_parameter("emit_tex", t)
			"clouds":
				var cm := ShaderMaterial.new()
				cm.shader = CLOUD
				cm.set_shader_parameter("clouds_tex", t)
				var tc: Color = look["cloud"]
				cm.set_shader_parameter("tint", Vector3(tc.r, tc.g, tc.b))
				cm.set_shader_parameter("alpha", float(look["calpha"]))
				_light(cm)
				_cloud = _shell(mesh_instance, 1.008, cm)
	if not left:
		_finish()

static func _shell(host: MeshInstance3D, k: float, mat: Material) -> MeshInstance3D:
	var s := MeshInstance3D.new()
	s.name = "Shell"
	s.mesh = host.mesh
	s.scale = Vector3.ONE * k
	s.material_override = mat
	s.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	host.add_child(s)
	return s
