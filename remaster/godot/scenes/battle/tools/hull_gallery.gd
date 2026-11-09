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
		var seen := {}
		for ki in range(1, args.size()):
			var f := str(args[ki]).split(".")[1]
			if not seen.has(f):
				seen[f] = true
				await AppLoader.fetch_web_pack(host, "ships_%s_web.pck" % f)
		BattleAssets.shared()
	var code := 0
	for ki in range(1, args.size()):
		var key := str(args[ki])
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
		ship.transform = Transform3D(Basis(Vector3.UP, deg_to_rad(-138.0)), Vector3.ZERO)
		var cam := Camera3D.new()
		cam.fov = 40.0
		cam.near = 0.5
		cam.far = 6000.0
		root.add_child(cam)
		cam.current = true
		var l: float = ship.length
		cam.position = Vector3(0.18 * l, 0.32 * l, 1.15 * l)
		cam.look_at(Vector3.ZERO, Vector3.UP)
		await _wait(90)
		var hero_ok: bool = ship.hero_key != ""
		print("HULL ", "ok" if hero_ok else "FAIL", " ", key, ": ", "hero " + ship.hero_key if hero_ok else "fallback hull")
		if not hero_ok:
			code = 1
		var img := get_root().get_viewport().get_texture().get_image()
		img.save_png("%s/hull-%s.png" % [out_dir, key])
		if OS.has_feature("web"):
			# tools/verify/web_parity.py --script res://scenes/battle/tools/hull_gallery.gd: hand the frame to the page, wait for its ack.
			JavaScriptBridge.eval("window.__shotPng = '%s';" % Marshalls.raw_to_base64(img.save_png_to_buffer()))
			print("SHOT_READY ", key)
			var t0 := Time.get_ticks_msec()
			while str(JavaScriptBridge.eval("window.__shotAck || ''")) != key and Time.get_ticks_msec() - t0 < 120000:
				await _wait(1)
		root.queue_free()
		await _wait(5)
	quit(code)
