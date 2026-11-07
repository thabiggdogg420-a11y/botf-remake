extends SceneTree
## Headless test of the title menu's live fleet (TitleFleet): every power builds three ships (procedural hulls when
## no hero model is local), escorts keep their canon size relative to the lead, the fleet sits right of centre in
## front of the camera, a sun, gas giant and sky are present, and nothing plays a battle timeline.
##   godot --headless --path remaster/godot -s res://scenes/battle/tests/test_title_fleet.gd

var failures := 0


func check(cond: bool, what: String) -> void:
	if not cond:
		failures += 1
		printerr("  FAIL  ", what)


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	print("Title fleet tests")
	var lengths := TitleFleet.lengths_for([682.0, 481.0, 170.0])
	check(is_equal_approx(lengths[0], TitleFleet.HERO_LENGTH), "lead is the hero length")
	check(is_equal_approx(lengths[1], TitleFleet.HERO_LENGTH * 481.0 / 682.0), "a Vor'cha beside a Negh'Var at canon ratio")
	check(is_equal_approx(lengths[2], TitleFleet.HERO_LENGTH * 45.0 / 170.0), "a 170 m escort floors at 45/170 of the lead")
	check(is_equal_approx(TitleFleet.lengths_for([350.0, 520.0, 45.0])[1], TitleFleet.HERO_LENGTH * 150.0 / 170.0), "a larger escort caps at 150/170")

	for empire in range(1, 6):
		var vp := SubViewport.new()
		vp.size = Vector2i(1920, 1080)
		vp.own_world_3d = true
		root.add_child(vp)
		var fleet := TitleFleet.new()
		vp.add_child(fleet)
		fleet.setup(empire, null)
		await process_frame
		check(fleet.ships.size() == 3, "empire %d: three ships" % empire)
		for s in fleet.ships:
			check(s.owner_id == empire and not s.imported, "empire %d: procedural hull for %s" % [empire, s.ship_name])
			check(not s.shield.visible, "empire %d: no shield bubble" % empire)
		check(fleet.ships[0].length > fleet.ships[2].length, "empire %d: lead larger than the small escort" % empire)
		check(fleet.env.has("sun") and fleet.env.has("planet"), "empire %d: sun and gas giant" % empire)
		check(fleet.find_child("WorldEnvironment", false, false) != null, "empire %d: sky environment" % empire)
		var cam := fleet.camera
		check(cam != null and cam.current, "empire %d: camera" % empire)
		var lead := cam.unproject_position(fleet.ships[0].global_position)
		check(not cam.is_position_behind(fleet.ships[0].global_position), "empire %d: lead in front of the camera" % empire)
		check(lead.x > 1920 * 0.5 and lead.x < 1920 * 0.85 and lead.y > 1080 * 0.2 and lead.y < 1080 * 0.8,
			"empire %d: lead right of centre, in frame (%s)" % [empire, lead])
		for s in fleet.ships:
			var p := cam.unproject_position(s.global_position)
			check(p.x > 1920 * 0.45 and p.x < 1920 and p.y > 0 and p.y < 1080, "empire %d: %s in frame (%s)" % [empire, s.ship_name, p])
		# The key light faces the hulls: it shines from the camera's side of the fleet.
		var sun: DirectionalLight3D = fleet.env.sun
		var to_cam := (cam.global_position - fleet.ships[0].global_position).normalized()
		check(sun.global_basis.z.dot(to_cam) > 0.3, "empire %d: three-quarter front key" % empire)
		var before := fleet.ships[0].global_position
		for n in 3:
			await process_frame
		check(fleet.ships[0].global_position.distance_to(before) < 1.0, "empire %d: the fleet holds its place" % empire)
		vp.queue_free()
		await process_frame
	print("title fleet tests: %d failures" % failures)
	quit(1 if failures > 0 else 0)
