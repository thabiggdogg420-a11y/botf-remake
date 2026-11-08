extends SceneTree
## PlanetBody / PlanetTextures: the photoreal planet set (art wave 2) on a sphere. Without a texture pack in the build the
## class stays procedural (attach returns null); with one (res:// or a mounted user:// pack) the surface material becomes
## planet_surface, with a cloud shell and an atmosphere shell, the tier follows the renderer, and loading ends.
##   godot --headless --path remaster/godot -s res://scenes/galaxy/tests/test_planet_body.gd
## Exit code 0 on success, 1 on any failure.

var failures := 0

func check(cond: bool, what: String) -> void:
	if cond:
		print("  ok  ", what)
	else:
		failures += 1
		printerr("  FAIL ", what)

func _initialize() -> void:
	_run.call_deferred()

func _sphere() -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	var sm := SphereMesh.new()
	mi.mesh = sm
	mi.material_override = ShaderMaterial.new()
	return mi

func _run() -> void:
	AppLoader.mount_user_packs()
	PlanetTextures.reset()
	PlanetTextures.disabled = true
	var mi := _sphere()
	root.add_child(mi)
	check(PlanetBody.attach(mi, "M") == null, "a class with no texture set stays procedural (attach returns null)")
	check(PlanetBody.attach(mi, "Asteroids") == null, "a belt is never a PlanetBody")
	PlanetTextures.disabled = false
	PlanetTextures.reset()
	check(PlanetTextures.compat() == (RenderingServer.get_current_rendering_method() == "gl_compatibility" or OS.has_feature("web")), "compat() follows the renderer")
	var sets := PlanetTextures.variants("M", true)
	if sets.is_empty():
		print("  skip  no planet pack mounted (user://packs/planets.pck): the textured path is checked on the capture run")
		print("%d failures" % failures)
		quit(1 if failures > 0 else 0)
		return
	check(sets[0].albedo.get_extension() in ["png", "webp"], "set has an albedo (%s)" % sets[0].albedo)
	if PlanetTextures.compat():
		check(not sets[0].has("normal"), "compat gets the web tier: no normal map (relief baked in)")
	else:
		check(sets[0].has("normal") and sets[0].has("rough"), "desktop hi tier carries normal and roughness")
	var body := PlanetBody.attach(mi, "M", {"hi": true, "star_pos": Vector3(10, 0, 0)})
	check(body != null, "a set exists: PlanetBody attaches")
	var t0 := Time.get_ticks_msec()
	while PlanetBody.pending > 0 and Time.get_ticks_msec() - t0 < 30000:
		await process_frame
	await process_frame
	check(PlanetBody.pending == 0, "texture streaming finishes (pending 0)")
	var m := mi.material_override as ShaderMaterial
	check(m != null and m.shader == PlanetBody.SURFACE, "the sphere now uses planet_surface")
	check(m.get_shader_parameter("albedo_tex") is Texture2D, "albedo texture bound")
	check(bool(m.get_shader_parameter("from_point")) and (m.get_shader_parameter("star_pos") as Vector3) == Vector3(10, 0, 0), "lit from its star")
	var shells := mi.get_children().filter(func(c): return c is MeshInstance3D)
	check(shells.size() == 2, "cloud shell and atmosphere shell added (%d)" % shells.size())
	var gas := _sphere()
	root.add_child(gas)
	check(PlanetBody.attach(gas, "GasGiant", {"hi": false, "sun_dir": Vector3(1, 0, 0)}) != null, "gas giant attaches; it has no cloud shell")
	print("%d failures" % failures)
	quit(1 if failures > 0 else 0)
