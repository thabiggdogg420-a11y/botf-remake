class_name CompatNebula
extends MeshInstance3D
## The battle nebula under the Compatibility renderer (web build). Forward+ draws two FogVolumes with
## nebula_fog.gdshader through volumetric fog, which Compatibility does not have (the docs: volumetric fog is Forward+
## only). This draws the same two ellipsoids with compat_nebula.gdshader: SLICES camera-facing slabs from NEAR to the fog
## length, spaced like volumetric fog's depth slices (closer together near the eye), one mesh and one draw call.
## ART-BIBLE §12.2 "Volumetric fog is not available" asked for billboard clouds; view-aligned slices keep the fog's own
## density field and lighting instead, so the shape matches the desktop's from every camera angle.
##
## What Forward+ actually draws (Godot 4.5 volumetric_fog.glsl and volumetric_fog_process.glsl, read 10 Nov): each froxel
## holds density, albedo x density and emission x density, packed into R11G11B10 integers; light is the sum over
## directional lights of colour (linear) x energy x PI x Henyey-Greenstein(view, light, anisotropy); the froxels integrate
## front to back as (light x albedo x density) (1 - T) / density. nebula_fog.gdshader's emission (about 0.03 linear) times
## a density of 0.02 is below one step of that packing, so on the desktop the nebula has no emission at all: it is lit
## fog only. The slices draw exactly that: radiance light x albedo, opacity 1 - exp(-density x slab thickness).

const SHADER := preload("res://scenes/battle/shaders/compat_nebula.gdshader")
const SLICES := 24
const NEAR := 2.0
## Lattice period of the value noise, in noise cells: the fog's noise runs at 0.012 per world unit, so 32 cells span
## about 2700 units, wider than any arena; the repeat is never in frame.
const LATTICE := 32

static var _noise: ImageTexture3D

var lights: Array[DirectionalLight3D] = []
var material: ShaderMaterial

## volumes: [{position, size (FogVolume box size), albedo, density}] (one or two), the scene's directional lights (up to
## three: sun, fill, rim), and the environment's volumetric_fog_length.
static func create(volumes: Array, p_lights: Array, fog_length: float) -> CompatNebula:
	var n := CompatNebula.new()
	n.name = "NebulaCompat"
	for l in p_lights:
		if l is DirectionalLight3D:
			n.lights.append(l)
	n.mesh = _slices(fog_length)
	n.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	n.extra_cull_margin = 16384.0 # placed by the vertex shader, never culled
	n.custom_aabb = AABB(Vector3(-1e5, -1e5, -1e5), Vector3(2e5, 2e5, 2e5))
	var m := ShaderMaterial.new()
	m.shader = SHADER
	m.set_shader_parameter("noise_tex", noise_texture())
	m.set_shader_parameter("lattice", float(LATTICE))
	m.set_shader_parameter("fog_length", fog_length)
	for i in mini(volumes.size(), 2):
		var v: Dictionary = volumes[i]
		m.set_shader_parameter("c%d" % i, v.position)
		m.set_shader_parameter("h%d" % i, (v.size as Vector3) * 0.5)
		m.set_shader_parameter("albedo%d" % i, _lin(v.albedo))
		m.set_shader_parameter("dens%d" % i, float(v.density))
	if volumes.size() < 2:
		m.set_shader_parameter("c1", Vector3(1e6, 1e6, 1e6))
	n.material = m
	n.material_override = m
	return n

## The fog shader's colour uniforms are `source_color`: an sRGB Color arrives linear in the shader. These uniforms are
## plain vec3, so convert here.
static func _lin(c: Color) -> Vector3:
	var l := c.srgb_to_linear()
	return Vector3(l.r, l.g, l.b)

## Follows the lights (the title stages its sun after the build; directors may move it). Energies are the Forward+ ones,
## times PI as Godot's light storage passes them to the fog: the key light's Compatibility scale (CompatLook.SUN_SCALE) is
## for hull shading, not for the fog.
func _process(_dt: float) -> void:
	for i in 3:
		var c := Color.BLACK
		var d := Vector3.UP
		if i < lights.size() and is_instance_valid(lights[i]) and lights[i].is_visible_in_tree():
			var l := lights[i]
			d = l.global_transform.basis.z.normalized()
			var e := l.light_energy * l.light_volumetric_fog_energy * PI / (CompatLook.SUN_SCALE if i == 0 else 1.0)
			c = l.light_color.srgb_to_linear() * e
		material.set_shader_parameter("l%d_dir" % i, d)
		material.set_shader_parameter("l%d_col" % i, Vector3(c.r, c.g, c.b))

## SLICES quads at distances spaced like volumetric fog's depth distribution, far to near (drawn back to front).
static func _slices(fog_length: float) -> ArrayMesh:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var ds: Array[float] = []
	for i in SLICES + 1:
		var t := float(i) / SLICES
		ds.append(NEAR * pow(fog_length / NEAR, t))
	for i in range(SLICES - 1, -1, -1):
		var d := 0.5 * (ds[i] + ds[i + 1])
		var thick := ds[i + 1] - ds[i]
		var k := 1.08
		var corners := [Vector2(-k, -k), Vector2(k, -k), Vector2(k, k), Vector2(-k, -k), Vector2(k, k), Vector2(-k, k)]
		for c: Vector2 in corners:
			st.set_uv2(Vector2(d, thick))
			st.add_vertex(Vector3(c.x, c.y, 0.0))
	return st.commit()

## The value-noise lattice: one random byte per cell of a LATTICE^3 periodic grid, built once and shared. The shader reads it
## with smoothstep-remapped coordinates, so one trilinear fetch gives nebula_fog.gdshader's n3() (cubic-interpolated value
## noise, uniform 0..1 lattice values) without its per-pixel hashing.
static func noise_texture() -> ImageTexture3D:
	if _noise != null:
		return _noise
	var rng := RandomNumberGenerator.new()
	rng.seed = 7
	var imgs: Array[Image] = []
	for z in LATTICE:
		var data := PackedByteArray()
		data.resize(LATTICE * LATTICE)
		for i in data.size():
			data[i] = rng.randi() & 255
		imgs.append(Image.create_from_data(LATTICE, LATTICE, false, Image.FORMAT_L8, data))
	_noise = ImageTexture3D.new()
	_noise.create(Image.FORMAT_L8, LATTICE, LATTICE, LATTICE, false, imgs)
	return _noise
