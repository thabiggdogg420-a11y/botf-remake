class_name BattleEnvironment
extends RefCounted
## Space backdrop for the battle: procedural star/nebula sky, a sun with its glare, a banded gas giant, a volumetric
## nebula bank, drifting dust, and the WorldEnvironment (filmic tonemapping, HDR glow/bloom, volumetric fog).

const SKY_SHADER := preload("res://scenes/battle/shaders/space_sky.gdshader")
const PLANET_SHADER := preload("res://scenes/battle/shaders/planet.gdshader")
const PLANET_RADIUS := 1100.0 ## ~100x a cruiser hull: at ~4300 units it spans ~30 degrees, so it dwarfs any hull beside it
const FOG_SHADER := preload("res://scenes/battle/shaders/nebula_fog.gdshader")


## `planet_class` picks the backdrop planet's texture set (M, O, L, P, H, K, Y, GasGiant); a class with no set keeps the
## procedural gas giant.
static func build(parent: Node3D, arena_size: Vector2, planet_class: String = "GasGiant") -> Dictionary:
	var out := {}
	var env := Environment.new()
	var sky := Sky.new()
	var sky_mat := ShaderMaterial.new()
	sky_mat.shader = SKY_SHADER
	sky.sky_material = sky_mat
	# M4 flight stage 2 (show footage, Simon 14 Nov): mostly black space with a subtle nebula, one hard key light. The
	# nebula and the galactic band are dimmed; the fill and ambient that flattened the hulls are cut back.
	sky_mat.set_shader_parameter("nebula_strength", 0.35)
	sky_mat.set_shader_parameter("band_strength", 0.35)
	sky.radiance_size = Sky.RADIANCE_SIZE_256
	env.background_mode = Environment.BG_SKY
	env.sky = sky
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.14, 0.145, 0.18) # stage 2: was 0.26 (R3.5, tactical distance); the close lens needs the key light to model the hull
	env.ambient_light_energy = 1.0
	env.reflected_light_source = Environment.REFLECTION_SOURCE_SKY
	env.tonemap_mode = Environment.TONE_MAPPER_ACES
	env.tonemap_exposure = 0.85 # stage 3: hulls read white beside the show (was 1.0); fill light carries the shadow side; 1.15 blew out white saucers
	env.tonemap_white = 8.0
	env.glow_enabled = true
	env.glow_normalized = false
	env.set_glow_level(0, 0.0)
	env.set_glow_level(1, 1.0)
	env.set_glow_level(2, 0.8)
	env.set_glow_level(3, 0.8)
	env.set_glow_level(4, 0.6)
	env.set_glow_level(5, 0.5)
	env.set_glow_level(6, 0.3)
	env.glow_intensity = 0.4 # F2: capped lower (no bloom wash on a close kill); R3.5: clamped so stacked fire never whites out the frame
	env.glow_strength = 1.0
	env.glow_bloom = 0.0
	env.glow_blend_mode = Environment.GLOW_BLEND_MODE_SCREEN
	env.glow_hdr_threshold = 1.8
	env.glow_hdr_scale = 1.2
	env.volumetric_fog_enabled = true
	env.volumetric_fog_density = 0.0
	env.volumetric_fog_albedo = Color(0.6, 0.55, 0.8)
	env.volumetric_fog_length = 480.0
	env.volumetric_fog_detail_spread = 2.0
	env.volumetric_fog_anisotropy = 0.5
	env.volumetric_fog_sky_affect = 0.0
	env.adjustment_enabled = true
	env.adjustment_contrast = 1.06
	env.adjustment_saturation = 1.0
	CompatLook.tune(env) # web (Compatibility): glow for its range (no-op under Forward+)
	var we := WorldEnvironment.new()
	we.name = "WorldEnvironment"
	we.environment = env
	parent.add_child(we)
	out.environment = env

	var sun := DirectionalLight3D.new()
	sun.name = "Sun"
	sun.light_color = Color(1.0, 0.93, 0.82)
	sun.light_energy = CompatLook.sun(4.2) # stage 2: the hard key light (was 3.0)
	sun.shadow_enabled = DisplaySettings.shadows_enabled # Settings > Shadow quality: Off
	sun.directional_shadow_max_distance = 320.0
	# R3.5: the default biases left shadow acne as black speckle over the hero hulls (title close-ups).
	sun.shadow_bias = 0.08
	sun.shadow_normal_bias = 2.5
	sun.shadow_blur = 0.6 # stage 3: hard key-light shadows (was 1.5)
	sun.directional_shadow_mode = DirectionalLight3D.SHADOW_PARALLEL_2_SPLITS
	sun.light_angular_distance = 0.2 # stage 3: a hard-edged key (was 0.6)
	parent.add_child(sun)
	sun.rotation_degrees = Vector3(-24.0, -128.0, 0.0)
	out.sun = sun

	# Fill from the side opposite the key light so the shadow side of a hull (and dark Klingon hulls) still reads.
	var fill := DirectionalLight3D.new()
	fill.name = "Fill"
	fill.light_color = Color(0.75, 0.8, 1.0)
	fill.light_energy = 0.35 # stage 2: was 0.9; the shadow side goes dark as in the show, never black (ambient)
	fill.sky_mode = DirectionalLight3D.SKY_MODE_LIGHT_ONLY
	fill.rotation_degrees = Vector3(-35.0, 52.0, 0.0)
	parent.add_child(fill)
	var rim := DirectionalLight3D.new()
	rim.name = "NebulaRim"
	rim.light_color = Color(0.6, 0.66, 0.9)
	rim.light_energy = 0.45
	rim.sky_mode = DirectionalLight3D.SKY_MODE_LIGHT_ONLY
	parent.add_child(rim)
	rim.rotation_degrees = Vector3(35.0, 40.0, 0.0)

	# Gas giant far below and behind the arena, lit by the sun.
	var planet := MeshInstance3D.new()
	planet.name = "Planet"
	var sphere := SphereMesh.new()
	sphere.radius = 1.0
	sphere.height = 2.0
	sphere.radial_segments = 128
	sphere.rings = 64
	planet.mesh = sphere
	var pm := ShaderMaterial.new()
	pm.shader = PLANET_SHADER
	planet.material_override = pm
	planet.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	# True scale (Simon, 8 Nov 2026: "the ships look huge compared to the nearby planet"): a gas giant is tens of
	# thousands of km across, so it is a backdrop, pushed far beyond the arena where it never sits beside a hull.
	planet.scale = Vector3.ONE * PLANET_RADIUS
	planet.position = Vector3(2600.0, -310.0, -3400.0) # level with the arena, far behind it, off the downward views, and on the side the sun lights (half phase from the arena)
	planet.rotation_degrees = Vector3(12.0, 30.0, 18.0)
	parent.add_child(planet)
	out.planet = planet
	# Art wave 2: the photoreal set (desktop tier, normal map, cloud shell, atmosphere rim) lit by the battle sun, from
	# the same texture packs the system view uses. Falls back to the procedural gas giant above when the class has none.
	out.planet_class = planet_class
	PlanetBody.attach(planet, planet_class, {"hi": true, "variant": 0, "inhabited": false, "sun_energy": 1.5,
		"sun_node": sun, "sun_dir": sun.global_transform.basis.z, "sun_color": Color(1.0, 0.93, 0.82)})

	# Volumetric nebula bank drifting through the far side of the arena.
	# FogVolume shaders are not supported by the web Compatibility renderer; forward_plus only.
	if RenderingServer.get_current_rendering_method() == "forward_plus":
		var fog := FogVolume.new()
		fog.name = "Nebula"
		fog.size = Vector3(arena_size.x * 2.6, 90.0, arena_size.y * 1.4)
		fog.shape = RenderingServer.FOG_VOLUME_SHAPE_ELLIPSOID
		var fmat := ShaderMaterial.new()
		fmat.shader = FOG_SHADER
		fmat.set_shader_parameter("density", 0.008) # stage 2: a subtle nebula, not a purple wash (shader default 0.02)
		fog.material = fmat
		fog.position = Vector3(0.0, -30.0, -arena_size.y * 0.9)
		parent.add_child(fog)
		var fog2 := FogVolume.new()
		fog2.name = "NebulaLow"
		fog2.size = Vector3(arena_size.x * 1.8, 50.0, arena_size.y * 1.6)
		fog2.shape = RenderingServer.FOG_VOLUME_SHAPE_ELLIPSOID
		var fmat2 := ShaderMaterial.new()
		fmat2.shader = FOG_SHADER
		fmat2.set_shader_parameter("albedo", Color(0.25, 0.35, 0.6))
		fmat2.set_shader_parameter("emission", Color(0.03, 0.06, 0.16))
		fmat2.set_shader_parameter("emission_b", Color(0.12, 0.04, 0.12))
		fmat2.set_shader_parameter("density", 0.005)
		fog2.material = fmat2
		fog2.position = Vector3(arena_size.x * 0.3, -45.0, arena_size.y * 0.4)
		parent.add_child(fog2)

	else:
		# Compatibility (the web build) has no volumetric fog: the same two nebula ellipsoids as slices (CompatNebula).
		parent.add_child(CompatNebula.create([
			{"position": Vector3(0.0, -30.0, -arena_size.y * 0.9), "size": Vector3(arena_size.x * 2.6, 90.0, arena_size.y * 1.4),
				"albedo": Color(0.35, 0.25, 0.55), "density": 0.008},
			{"position": Vector3(arena_size.x * 0.3, -45.0, arena_size.y * 0.4), "size": Vector3(arena_size.x * 1.8, 50.0, arena_size.y * 1.6),
				"albedo": Color(0.25, 0.35, 0.6), "density": 0.005},
		], [sun, fill, rim], env.volumetric_fog_length))

	parent.add_child(_dust(arena_size))
	return out


## Faint motes scattered through the arena: they give the camera parallax so motion reads as motion.
static func _dust(arena_size: Vector2) -> Node3D:
	var gp := GPUParticles3D.new()
	gp.name = "Dust"
	gp.amount = 600
	gp.lifetime = 40.0
	gp.preprocess = 40.0
	gp.explosiveness = 0.0
	gp.local_coords = false
	gp.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	gp.visibility_aabb = AABB(Vector3(-400, -200, -400), Vector3(800, 400, 800))
	var pm := ParticleProcessMaterial.new()
	pm.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_BOX
	pm.emission_box_extents = Vector3(arena_size.x * 0.9, 50.0, arena_size.y * 0.9)
	pm.gravity = Vector3.ZERO
	pm.initial_velocity_min = 0.05
	pm.initial_velocity_max = 0.3
	pm.spread = 180.0
	pm.scale_min = 0.03
	pm.scale_max = 0.08
	var ramp := Gradient.new()
	ramp.set_color(0, Color(0.7, 0.75, 0.9, 0.0))
	ramp.add_point(0.1, Color(0.7, 0.75, 0.9, 0.08))
	ramp.add_point(0.9, Color(0.7, 0.75, 0.9, 0.08))
	ramp.set_color(ramp.get_point_count() - 1, Color(0.7, 0.75, 0.9, 0.0))
	var gt := GradientTexture1D.new()
	gt.gradient = ramp
	pm.color_ramp = gt
	gp.process_material = pm
	var mesh := QuadMesh.new()
	var m := StandardMaterial3D.new()
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	m.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	m.vertex_color_use_as_albedo = true
	m.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	var g := Gradient.new()
	g.set_color(0, Color(1, 1, 1, 1))
	g.set_color(1, Color(1, 1, 1, 0))
	var tex := GradientTexture2D.new()
	tex.gradient = g
	tex.fill = GradientTexture2D.FILL_RADIAL
	tex.fill_from = Vector2(0.5, 0.5)
	tex.fill_to = Vector2(1.0, 0.5)
	tex.width = 32
	tex.height = 32
	m.albedo_texture = tex
	mesh.material = m
	gp.draw_pass_1 = mesh
	# Compatibility (web, 21 Nov 2026): a GPUParticles3D compiles a particle process program (two variants, 1.4-1.6 s each in
	# Firefox) on its first draw, which landed in the title's first frames. The same motes as CPUParticles3D need none.
	if RenderingServer.get_current_rendering_method() == "gl_compatibility":
		var cp := CPUParticles3D.new()
		cp.convert_from_particles(gp)
		cp.name = "Dust"
		gp.free()
		# Round 4: the 40 s preprocess cost 0.9 s of CPU in the first frame (Firefox). Instead the motes fill in over the first
		# second at 40x speed (the same 40 s of drift, a step a frame), then run at normal speed.
		cp.preprocess = 0.0
		cp.speed_scale = 40.0
		cp.ready.connect(func():
			cp.create_tween().tween_property(cp, "speed_scale", 1.0, 0.0).set_delay(1.0))
		return cp
	return gp


## Swaps the backdrop planet to `planet_class` after the fact (the battle's system is only known once the encounter
## arrives). `env` is the dictionary build() returned. A class with no texture set keeps the procedural gas giant.
static func set_planet_class(env: Dictionary, planet_class: String) -> void:
	if planet_class == "" or planet_class == env.get("planet_class", "") or not env.has("planet"):
		return
	var planet: MeshInstance3D = env.planet
	for c in planet.get_children():
		if c is PlanetBody:
			planet.remove_child(c)
			c.queue_free()
	var pm := ShaderMaterial.new()
	pm.shader = PLANET_SHADER
	planet.material_override = pm
	var sun: DirectionalLight3D = env.sun
	PlanetBody.attach(planet, planet_class, {"hi": true, "variant": 0, "inhabited": false, "sun_energy": 1.5,
		"sun_node": sun, "sun_dir": sun.global_transform.basis.z, "sun_color": Color(1.0, 0.93, 0.82)})
	env.planet_class = planet_class
