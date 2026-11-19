class_name ShaderWarm
extends Node
## Web only (WebGPU round 6, 19 Nov 2026). The browser build compiles a material's shader on the main thread the first
## frame something draws with it, and the title, the galaxy and the battle each drew several new ones in one frame
## (1.2-2.4 s frames with no game code running in them). Behind the "Starting" panel, before the title exists, this
## draws each of the game's own shaders on a small off-screen quad, ONE NEW SHADER PER FRAME, so the compiles are spread
## over frames instead of piling into the title's first frame. The engine keeps every compiled shader for the session,
## so the title, galaxy and battle then find them ready. On WebGPU it also waits (a frame at a time, capped) for the
## GPU process to finish the render pipelines those draws asked for.
## Desktop never runs it. It changes nothing the player sees and no game rule.

## Every shader the game ships, the ones the title draws first. Sky and fog shaders go on their own WorldEnvironment.
const SPATIAL := [
	"res://scenes/battle/shaders/hero_hull.gdshader",
	"res://scenes/battle/shaders/hero_plated.gdshader",
	"res://scenes/battle/shaders/hull.gdshader",
	"res://scenes/battle/shaders/planet.gdshader",
	"res://scenes/battle/shaders/compat_nebula.gdshader",
	"res://scenes/battle/shaders/flare.gdshader",
	"res://scenes/battle/shaders/shield.gdshader",
	"res://scenes/battle/shaders/cloak.gdshader",
	"res://scenes/battle/shaders/blink.gdshader",
	"res://scenes/battle/shaders/hull_piece.gdshader",
	"res://scenes/battle/shaders/beam.gdshader",
	"res://scenes/battle/shaders/weapon_beam.gdshader",
	"res://scenes/battle/shaders/torpedo.gdshader",
	"res://scenes/battle/shaders/trail.gdshader",
	"res://scenes/battle/shaders/shockwave.gdshader",
	"res://scenes/battle/shaders/kill_fire.gdshader",
	"res://scenes/galaxy/shaders/galaxy_disc.gdshader",
	"res://scenes/galaxy/shaders/star_impostor.gdshader",
	"res://scenes/galaxy/shaders/star_surface.gdshader",
	"res://scenes/galaxy/shaders/planet_globe.gdshader",
	"res://scenes/galaxy/shaders/planet_surface.gdshader",
	"res://scenes/galaxy/shaders/planet_atmo.gdshader",
	"res://scenes/galaxy/shaders/planet_cloud.gdshader",
	"res://scenes/galaxy/shaders/field3d.gdshader",
	"res://scenes/galaxy/shaders/anomaly_billboard.gdshader",
	"res://scenes/galaxy/shaders/anomaly_volume.gdshader",
]
const CANVAS := [
	"res://scenes/galaxy/starfield.gdshader",
	"res://scenes/galaxy/nebula.gdshader",
	"res://scenes/galaxy/field.gdshader",
	"res://scenes/ui/widgets/planet_orb.gdshader",
]
## Compatibility (WebGL) only: the title's bloom pass. The WebGPU build never draws them (and they are not baked there).
const COMPAT_CANVAS := ["res://scenes/battle/shaders/compat_bloom.gdshader", "res://scenes/battle/shaders/compat_bloom_down.gdshader"]
const SKY := ["res://scenes/battle/shaders/space_sky.gdshader", "res://scenes/galaxy/shaders/galaxy_sky.gdshader"]
const FOG := ["res://scenes/battle/shaders/nebula_fog.gdshader"]
## The runtime materials (StandardMaterial3D, ParticleProcessMaterial) the title, galaxy and battle build in code, one copy per
## generated shader, captured by tools/capture_runtime_materials.gd. Their .tres files also make the export bake them.
const AUTO_DIR := "res://scenes/warm/bake/auto"

const FRAMES_PER_SHADER := 2   ## frames each new shader stays on screen (the first draw compiles it)
const SETTLE_FRAMES := 10      ## WebGPU: frames in a row with no pipeline compiling before it counts as finished
const SETTLE_CAP_MS := 20000   ## never hold the boot longer than this for the GPU process

static var done := false


## True when this session still has to warm (web only, once).
## WebGPU only. On WebGL (Firefox) each ANGLE compile took 1.3-5 s by itself, so a shader a frame still froze every frame
## and held the boot panel 54 s with no gain in the worst frame (r6 measurement); the Compatibility build skips it.
static func needed() -> bool:
	return not done and OS.has_feature("web") and GpuReadback.webgpu()


## Runs the warm-up under `host` and returns when it is over. `progress` (optional) gets (done, total).
static func run(host: Node, progress: Callable = Callable()) -> void:
	if not needed():
		return
	done = true
	var tree := host.get_tree()
	var t0 := Time.get_ticks_msec()
	var sv := SubViewport.new()
	sv.name = "ShaderWarm"
	sv.size = Vector2i(160, 90)
	sv.own_world_3d = true
	sv.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	var s := {}
	DisplaySettings.fill_defaults(s)
	DisplaySettings.apply_viewport(sv, s)
	var cam := Camera3D.new()
	cam.position = Vector3(0, 0, 3)
	sv.add_child(cam)
	cam.current = true
	var light := DirectionalLight3D.new()
	light.shadow_enabled = true
	light.rotation_degrees = Vector3(-40, 30, 0)
	sv.add_child(light)
	host.add_child(sv)
	var auto: Array = []
	if DirAccess.dir_exists_absolute(AUTO_DIR):
		for f in ResourceLoader.list_directory(AUTO_DIR):
			if f.ends_with(".tres") or f.ends_with(".res"):
				auto.append(AUTO_DIR + "/" + f)
	var canvas: Array = CANVAS.duplicate()
	if not GpuReadback.webgpu():
		canvas.append_array(COMPAT_CANVAS)
	var total := SPATIAL.size() + canvas.size() + SKY.size() + FOG.size() + auto.size() + 1
	var n := 0
	var env_node := WorldEnvironment.new()
	env_node.environment = Environment.new()
	env_node.environment.background_mode = Environment.BG_COLOR
	sv.add_child(env_node)
	var held: Node = null
	for path: String in SKY + FOG + SPATIAL + canvas:
		var t := LoadTrace.start()
		var sh := load(path) as Shader
		if held != null:
			held.queue_free()
			held = null
		if sh != null:
			var m := ShaderMaterial.new()
			m.shader = sh
			if path in SKY:
				var sky := Sky.new()
				sky.sky_material = m
				env_node.environment.background_mode = Environment.BG_SKY
				env_node.environment.sky = sky
			elif path in FOG:
				var fv := FogVolume.new()
				fv.size = Vector3(2, 2, 2)
				fv.material = m
				env_node.environment.volumetric_fog_enabled = true
				sv.add_child(fv)
				held = fv
			elif path in canvas:
				var cr := ColorRect.new()
				cr.size = Vector2(160, 90)
				cr.material = m
				sv.add_child(cr)
				held = cr
			else:
				var mi := MeshInstance3D.new()
				var q := SphereMesh.new()
				q.radial_segments = 8
				q.rings = 4
				mi.mesh = q
				mi.material_override = m
				sv.add_child(mi)
				held = mi
		LoadTrace.done("shader warm: %s" % path.get_file(), t)
		n += 1
		if progress.is_valid():
			progress.call(n, total)
		for _f in FRAMES_PER_SHADER:
			await tree.process_frame
	for path: String in auto:
		var t := LoadTrace.start()
		if held != null:
			held.queue_free()
			held = null
		var mat := load(path) as Material
		if mat is ParticleProcessMaterial:
			var gp := GPUParticles3D.new()
			gp.amount = 8
			gp.process_material = mat
			var dm := SphereMesh.new()
			dm.radial_segments = 4
			dm.rings = 2
			gp.draw_pass_1 = dm
			sv.add_child(gp)
			held = gp
		elif mat is BaseMaterial3D:
			var mi := MeshInstance3D.new()
			var q := SphereMesh.new()
			q.radial_segments = 8
			q.rings = 4
			mi.mesh = q
			mi.material_override = mat
			sv.add_child(mi)
			held = mi
		LoadTrace.done("shader warm: %s" % path.get_file(), t)
		n += 1
		if progress.is_valid():
			progress.call(n, total)
		for _f in FRAMES_PER_SHADER:
			await tree.process_frame
	if held != null:
		held.queue_free()
		held = null
	env_node.environment.volumetric_fog_enabled = false
	# The title's own environment (sky, sun, planet, dust particles) with its real materials, so the title's first frame
	# finds every one of them compiled.
	var t_env := LoadTrace.start()
	var stage := Node3D.new()
	sv.add_child(stage)
	BattleEnvironment.build(stage, Vector2(16, 12) * BattleScene.CELL)
	cam.current = true
	LoadTrace.done("shader warm: title environment", t_env)
	n += 1
	if progress.is_valid():
		progress.call(n, total)
	for _f in 4:
		await tree.process_frame
	# WebGPU: let the GPU process finish the pipelines those draws asked for, a frame at a time.
	var quiet := 0
	var ts := Time.get_ticks_msec()
	while quiet < SETTLE_FRAMES and Time.get_ticks_msec() - ts < SETTLE_CAP_MS:
		quiet = quiet + 1 if WebGpuWarm.pending() == 0 else 0
		await tree.process_frame
	print("shader warm: %d shaders in %d ms (%d pipelines still compiling)" % [n, Time.get_ticks_msec() - t0, WebGpuWarm.pending()])
	sv.queue_free()
