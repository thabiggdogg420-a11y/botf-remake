extends SceneTree
## Hull close-ups for the pack proof (M1 shippack): one frame per asset key, the hull alone on the battle backdrop,
## drawn through BattleAssets exactly as a battle does, so a packaged exe with no LocalAssets shows whether the hero
## models arrived. Prints "HULL ok|FAIL <key>: hero|fallback" per key and exits 1 on a fallback hull.
##   <exe or godot --path remaster/godot> --resolution 1920x1080 --audio-driver Dummy -s res://scenes/battle/tools/hull_gallery.gd -- \
##       <out_dir> ship.fed.nebula ship.fed.defiant ship.kli.neghvar ...

func _initialize() -> void:
	_run.call_deferred()


func _wait(n: int) -> void:
	for i in n:
		await process_frame


func _run() -> void:
	var args := OS.get_cmdline_user_args()
	var out_dir := str(args[0]) if args.size() > 0 else "user://hulls"
	DirAccess.make_dir_recursive_absolute(out_dir)
	AppLoader.mount_user_packs() # the packs the game mounts at start (editor: user://packs; exe: beside it)
	if OS.has_feature("web"):
		# The web build fetches a faction's hulls from its pack (the game does this in the background after the title).
		var host := Node.new()
		get_root().add_child(host)
		await AppLoader.fetch_web_pack(host, AppLoader.WEB_SHIPS_BASE)
		for ki in range(1, args.size()):
			var hp := AppLoader.hull_pack_for(str(args[ki]).get_slice(":", 0))
			if hp != "":
				await AppLoader.fetch_web_pack(host, hp)
		BattleAssets.shared()
	var code := 0
	for ki in range(1, args.size()):
		var spec := str(args[ki])
		var key := spec.get_slice(":", 0)
		var view := spec.get_slice(":", 1) if spec.contains(":") else ""
		var parts := key.split(".")
		var empire: int = {"fed": 1, "kli": 2, "rom": 3, "car": 4, "fer": 5}.get(parts[1] if parts.size() > 1 else "fed", 1)
		var root := Node3D.new()
		get_root().add_child(root)
		var env := BattleEnvironment.build(root, Vector2(16, 12) * BattleScene.CELL)
		var sun: DirectionalLight3D = env.sun
		sun.rotation_degrees = Vector3(-28.0, -38.0, 0.0)
		sun.light_energy = CompatLook.sun(2.4)
		var assets := BattleAssets.shared()
		var ship := BattleShip.new()
		root.add_child(ship)
		ship.setup({"id": 1, "owner": empire, "name": key, "assetKey": key, "maxHull": 150}, assets)
		ship.transform = Transform3D(Basis(Vector3.UP, deg_to_rad(-138.0)), Vector3.ZERO) if view == "" else Transform3D.IDENTITY
		var cam := Camera3D.new()
		cam.fov = 40.0
		cam.near = 0.5
		cam.far = 6000.0
		root.add_child(cam)
		cam.current = true
		var l: float = ship.length
		# Framed on the drawn hull's bounds, not its length (F4 round 2: the lightship's sails and the cube's depth
		# spill past a length-based frame): the bounding sphere fills the frame.
		await _wait(2)
		var box := _bounds(ship)
		var centre := box.get_center() if box.size != Vector3.ZERO else Vector3.ZERO
		var radius := box.size.length() * 0.5 if box.size != Vector3.ZERO else l * 0.6
		var dist := radius / sin(deg_to_rad(cam.fov * 0.5)) * 0.62
		cam.position = centre + Vector3(0.18, 0.32, 1.15).normalized() * dist
		cam.far = maxf(6000.0, dist * 3.0)
		if view != "":
			# Hull-relative views (ship local axes: bow is -Z, up is +Y): top bottom port starboard bow stern.
			var d: Vector3 = {"top": Vector3(0, 1, 0.001), "bottom": Vector3(0, -1, 0.001), "port": Vector3(-1, 0.15, 0), "starboard": Vector3(1, 0.15, 0),
				"bow": Vector3(0, 0.15, -1), "stern": Vector3(0, 0.15, 1)}.get(view, Vector3(0.2, 0.3, 1))
			var c := ship.to_global(Vector3.ZERO)
			cam.position = c + ship.global_transform.basis * (d.normalized() * 0.95 * l)
			cam.look_at(c, ship.global_transform.basis.y if absf(d.y) < 0.9 else ship.global_transform.basis.z)
		else:
			cam.look_at(centre, Vector3.UP)
		await _wait(90)
		var hero_ok: bool = ship.hero_key != ""
		print("HULL ", "ok" if hero_ok else "FAIL", " ", spec, ": ", "hero " + ship.hero_key if hero_ok else "fallback hull")
		if not hero_ok:
			code = 1
		var img := get_root().get_viewport().get_texture().get_image()
		img.save_png("%s/hull-%s%s.png" % [out_dir, key, "-" + view if view != "" else ""])
		if OS.has_feature("web"):
			# tools/verify/web_parity.py --script res://scenes/battle/tools/hull_gallery.gd: hand the frame to the page, wait for its ack.
			JavaScriptBridge.eval("window.__shotPng = '%s';" % Marshalls.raw_to_base64(img.save_png_to_buffer()))
			print("SHOT_READY ", spec)
			var t0 := Time.get_ticks_msec()
			while str(JavaScriptBridge.eval("window.__shotAck || ''")) != spec and Time.get_ticks_msec() - t0 < 120000:
				await _wait(1)
		root.queue_free()
		await _wait(5)
	quit(code)


## World-space bounds of the ship's visible meshes (nav lights and shields excluded).
func _bounds(ship: Node3D) -> AABB:
	var out := AABB()
	var first := true
	for n in ship.find_children("*", "MeshInstance3D", true, false):
		var mi := n as MeshInstance3D
		if mi.mesh == null or not mi.is_visible_in_tree() or mi.name.begins_with("NavLight") or "hield" in str(mi.name):
			continue
		var b := mi.global_transform * mi.get_aabb()
		out = b if first else out.merge(b)
		first = false
	return out
