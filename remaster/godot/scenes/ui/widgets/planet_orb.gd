class_name PlanetOrb
extends Control
## A lit planet sphere by class (M, L, O, P, H, K, Y, GasGiant, Asteroids). Uses lane L7's albedo
## (res://art/planet/planet.<class>.*, through AppLoader.art) when present; otherwise the procedural surface of
## planet_orb.gdshader in the class look of ART-BIBLE §8. Asteroids are drawn as a belt. Compatibility-safe.

var cls := "M"
var seed_id := 0
var animate := true

const COLORS := {"M": Color("#4fae6a"), "L": Color("#7fa35a"), "O": Color("#3f7fd0"), "P": Color("#cfe3f0"), "H": Color("#d0a060"), "K": Color("#9a8a80"), "J": Color("#d9a86a"), "GasGiant": Color("#d9a86a"), "Y": Color("#c0503a"), "N": Color("#b08040"), "D": Color("#80808a"), "Asteroids": Color("#707078")}
## Class looks: [style, deep, land, high, rim, clouds, atmosphere]. Styles match the shader's `style` uniform.
const LOOKS := {
	"M": [0, "#1d4f8f", "#4f9a55", "#f2f4f0", "#8fc4ff", 0.55, 0.7],
	"O": [1, "#1a4d9a", "#3f8f6a", "#eef4f8", "#6fb0ff", 0.7, 0.8],
	"P": [4, "#7f9fb8", "#9aa7ae", "#f4fbff", "#e8f4ff", 0.3, 0.45],
	"L": [7, "#2f5f3a", "#6f9a3a", "#c8d890", "#d6e88a", 0.6, 0.65],
	"H": [6, "#8a5a2a", "#d6a868", "#f0d8a8", "#ffb070", 0.1, 0.4],
	"N": [6, "#7a5a30", "#b08040", "#d8c090", "#ffb070", 0.1, 0.35],
	"K": [3, "#5a524c", "#9a8a80", "#c8bcb2", "#000000", 0.0, 0.0],
	"D": [3, "#4a4a52", "#80808a", "#b0b0b8", "#000000", 0.0, 0.0],
	"Y": [5, "#2a0e0a", "#6a2418", "#ff7a2a", "#ff6a3a", 0.0, 0.55],
	"GasGiant": [2, "#a87848", "#e8c890", "#fff0d0", "#cfe0ff", 0.0, 0.5],
	"J": [2, "#a87848", "#e8c890", "#fff0d0", "#cfe0ff", 0.0, 0.5],
}
const ART_KEYS := {"M": "planet.m", "O": "planet.o", "P": "planet.p", "L": "planet.l", "H": "planet.h", "K": "planet.k", "Y": "planet.y", "GasGiant": "planet.gasgiant", "J": "planet.gasgiant", "Asteroids": "planet.asteroids"}

static var _shader: Shader
var _spin := 0.0

static func make(cls_: String, px: float, seed_: int = 0) -> PlanetOrb:
	var o := PlanetOrb.new()
	o.cls = cls_
	o.seed_id = seed_
	o.custom_minimum_size = Vector2(px, px)
	o.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return o

var _body_mesh: MeshInstance3D
var _spin3d := 0.0

func _ready() -> void:
	if cls == "Asteroids":
		return
	if _build_3d():
		return
	if _shader == null:
		_shader = AppLoader.resource("res://scenes/ui/widgets/planet_orb.gdshader") as Shader
	if _shader == null:
		return
	var m := ShaderMaterial.new()
	m.shader = _shader
	var look: Array = LOOKS.get(cls, LOOKS["M"])
	m.set_shader_parameter("style", int(look[0]))
	m.set_shader_parameter("c_deep", Color(look[1]))
	m.set_shader_parameter("c_land", Color(look[2]))
	m.set_shader_parameter("c_high", Color(look[3]))
	m.set_shader_parameter("c_rim", Color(look[4]))
	m.set_shader_parameter("clouds", float(look[5]))
	m.set_shader_parameter("atmosphere", float(look[6]))
	m.set_shader_parameter("seed", 1.0 + float(seed_id % 97) * 0.173)
	var tex := AppLoader.art(ART_KEYS.get(cls, ""), "planet")
	if tex is Texture2D:
		m.set_shader_parameter("albedo", tex)
		m.set_shader_parameter("use_tex", true)
	_spin = float(seed_id % 11) / 11.0
	m.set_shader_parameter("spin", _spin)
	material = m
	set_process(animate and not UiTokens.reduced_motion)

## The photoreal planet (art wave 2): the L7 texture set on a real sphere in a small transparent SubViewport, lit from the
## upper left like the battle sun, with cloud shell and atmosphere rim (PlanetBody). Needs a class with a set; otherwise
## false and the procedural 2D orb below is used. Works on both renderers (a plain 3D viewport, no screen textures).
func _build_3d() -> bool:
	if not PlanetTextures.variants(cls).size() > 0 or not PlanetBody.LOOK.has(cls):
		return false
	var side: float = maxf(custom_minimum_size.x, custom_minimum_size.y)
	var svc := SubViewportContainer.new()
	svc.name = "Globe"
	svc.stretch = true
	svc.mouse_filter = Control.MOUSE_FILTER_IGNORE
	svc.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var vp := SubViewport.new()
	vp.transparent_bg = true
	vp.own_world_3d = true
	vp.msaa_3d = Viewport.MSAA_4X
	vp.size = Vector2i(maxi(int(side), 32), maxi(int(side), 32))
	vp.render_target_update_mode = SubViewport.UPDATE_ALWAYS if animate and not UiTokens.reduced_motion else SubViewport.UPDATE_WHEN_VISIBLE
	svc.add_child(vp)
	var cam := Camera3D.new()
	cam.projection = Camera3D.PROJECTION_ORTHOGONAL
	cam.size = 2.0 / 0.42 # the disc fills 84% of the square, the halo of the 2D orb sits outside it
	cam.position = Vector3(0, 0, 4)
	vp.add_child(cam)
	_body_mesh = MeshInstance3D.new()
	var sm := SphereMesh.new()
	sm.radius = 1.0
	sm.height = 2.0
	sm.radial_segments = 64
	sm.rings = 32
	_body_mesh.mesh = sm
	var tilt := Node3D.new()
	tilt.rotation_degrees = Vector3(0, 0, -12.0)
	vp.add_child(tilt)
	tilt.add_child(_body_mesh)
	var hi := side >= 150.0
	var sets := PlanetTextures.variants(cls, hi)
	PlanetBody.attach(_body_mesh, cls, {"hi": hi, "variant": seed_id, "inhabited": true,
		"sun_dir": Vector3(-0.62, 0.38, 0.69), "sun_color": Color(1.0, 0.95, 0.88), "sun_energy": 1.3 if sets.size() > 0 else 1.0})
	add_child(svc)
	_spin3d = float(seed_id % 11) / 11.0 * TAU
	_body_mesh.rotation.y = _spin3d
	set_process(animate and not UiTokens.reduced_motion)
	return true

func _process(delta: float) -> void:
	if _body_mesh != null:
		_spin3d = fmod(_spin3d + delta * TAU / 90.0, TAU)
		_body_mesh.rotation.y = _spin3d
		return
	_spin = fmod(_spin + delta / 40.0, 1.0)
	if material is ShaderMaterial:
		(material as ShaderMaterial).set_shader_parameter("spin", _spin)

func _draw() -> void:
	var c: Color = COLORS.get(cls, Color("#8899aa"))
	var ctr := Vector2(size.x / 2.0, size.y / 2.0)
	var r: float = min(size.x, size.y) * 0.42
	if cls == "Asteroids":
		var rng := RandomNumberGenerator.new()
		rng.seed = 7 + seed_id
		draw_arc(ctr, r * 0.9, 0, TAU, 48, Color(c, 0.15), r * 0.35, true)
		for k in range(26):
			var a := rng.randf() * TAU
			var rr := r * rng.randf_range(0.7, 1.1)
			var p := ctr + Vector2(cos(a) * rr, sin(a) * rr * 0.42)
			var s := rng.randf_range(1.2, 3.6) * r / 30.0
			draw_circle(p, s, c.lightened(rng.randf_range(-0.2, 0.25)))
		return
	if _body_mesh != null:
		return
	if material != null:
		# The shader draws the sphere into a square that is a little larger than the disc, for the halo.
		var side: float = min(size.x, size.y)
		draw_rect(Rect2(ctr - Vector2(side, side) / 2.0, Vector2(side, side)), Color.WHITE)
		return
	draw_circle(ctr, r + 3, Color(c, 0.18))
	draw_circle(ctr, r, c.darkened(0.35))
	draw_circle(ctr - Vector2(r * 0.18, r * 0.18), r * 0.78, c)
	draw_circle(ctr - Vector2(r * 0.35, r * 0.35), r * 0.3, Color(1, 1, 1, 0.25))
