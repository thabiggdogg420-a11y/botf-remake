extends SceneTree
## Captures the battle backdrop planet next to a ship, for the planet-vs-ship scale check: BattleEnvironment.build with
## the class's photoreal set, one hull in the foreground, the camera looking at the planet over the hull.
##   godot --path remaster/godot --resolution 1920x1080 --audio-driver Dummy -s res://scenes/galaxy/tools/planet_battle_capture.gd -- \
##       <out_dir> [class,class...]        (add --rendering-driver opengl3 --rendering-method gl_compatibility for Compatibility)
## Writes <out_dir>/battle-<class>-<renderer>.png. Needs a window (GPU).

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var args := OS.get_cmdline_user_args()
	var out_dir: String = args[0] if args.size() > 0 else "user://planet_battle"
	var classes: PackedStringArray = (args[1] if args.size() > 1 else "GasGiant,M,Y").split(",")
	DirAccess.make_dir_recursive_absolute(out_dir)
	AppLoader.mount_user_packs()
	var tag := "compat" if PlanetTextures.compat() else "desktop"
	for cls in classes:
		var scene := Node3D.new()
		root.add_child(scene)
		var env := BattleEnvironment.build(scene, Vector2(16, 12) * BattleScene.CELL, cls)
		var ship := ShipFactory.build(1, false, {})
		ship.scale = Vector3.ONE * 3.0
		ship.position = Vector3(8, 0, 0)
		scene.add_child(ship)
		var planet: Node3D = env.planet
		var cam := Camera3D.new()
		cam.fov = 48.0
		cam.far = 8000.0
		scene.add_child(cam)
		cam.current = true
		cam.position = Vector3(0, 4, 30)
		cam.look_at(planet.position.normalized() * 40.0)
		var t0 := Time.get_ticks_msec()
		while PlanetBody.pending > 0 and Time.get_ticks_msec() - t0 < 30000:
			await process_frame
		for i in 12:
			await process_frame
		await RenderingServer.frame_post_draw
		var path := "%s/battle-%s-%s.png" % [out_dir, cls, tag]
		root.get_viewport().get_texture().get_image().save_png(path)
		print("PLANETCAP ", path, " textured=", planet.get_child_count() > 0, " pending=", PlanetBody.pending)
		scene.queue_free()
		await process_frame
	quit(0)
