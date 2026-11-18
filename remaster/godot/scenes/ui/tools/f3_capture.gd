extends SceneTree
## F3 "Planets and people" frames: every new screen, for the packaged exe and the editor.
##   BOTFRemastered.exe --audio-driver Dummy --resolution 1920x1080 -s res://scenes/ui/tools/f3_capture.gd -- <out_dir>
##   godot --path remaster/godot --audio-driver Dummy --resolution 1920x1080 -s res://scenes/ui/tools/f3_capture.gd -- <out_dir>
## Writes f3-tutorial-welcome, f3-tutorial-build, f3-captains-log, f3-advisor-hint, f3-system-3d, f3-system-3d-close,
## f3-hail-panel, f3-hail-scene, f3-busts (all powers, Terran and Mirror twins), f3-mirror-hail. Restores the tutorial setting.

var app: App
var out_dir := "user://f3"

func _initialize() -> void:
	var args := Array(OS.get_cmdline_user_args())
	if args.size() > 0:
		out_dir = args[0]
	DirAccess.make_dir_recursive_absolute(out_dir)
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred()

func _wait(frames: int = 8) -> void:
	for n in frames:
		await process_frame

func _wait_ms(ms: int) -> void:
	var t := Time.get_ticks_msec()
	while Time.get_ticks_msec() - t < ms:
		await _wait(4)

func _shot(name_: String) -> void:
	await _wait(6)
	var img := root.get_viewport().get_texture().get_image()
	img.save_png("%s/%s.png" % [out_dir, name_])
	print("SHOT ", name_)

func _press() -> void:
	app.advisor_panel.update()
	await _wait(2)
	var b: Button = app.advisor_panel.find_child("AdvisorAct", true, false)
	if b:
		b.pressed.emit()
	await _wait(6)

func _run() -> void:
	var saved := {}
	AppSaves.load_settings(saved)
	await _wait(6)
	app.settings["tutorial"] = 0
	app.goto("new")
	await _wait(4)
	var nc := app.current_screen()
	nc.faction = 1
	nc.seed_value = 31337
	nc.start()
	await _wait_ms(2500)
	await _shot("f3-tutorial-welcome")
	await _press()          # Begin
	await _press()          # Open system
	await _wait_ms(4000)    # planet textures stream in
	await _shot("f3-tutorial-build")
	await _press()          # Queue a build
	await _wait_ms(1500)
	await _shot("f3-system-3d")
	var sv: SystemView3D = app.current_screen().find_child("SystemView3D", true, false)
	if sv:
		# a close look at the colony world and its structures
		var home: Node3D = sv._bodies[sv.home_index] if sv.home_index >= 0 else null
		if home:
			sv.spin = false
			sv.camera.global_position = home.global_position + Vector3(0.35, 0.45, 1.25)
			sv.camera.look_at(home.global_position, Vector3.UP)
			await _wait_ms(800)
			var img := sv.viewport.get_texture().get_image()
			img.save_png("%s/f3-system-3d-close.png" % out_dir)
			print("SHOT f3-system-3d-close")
	await _press()          # Open research
	await _press()          # Set course
	await _press()          # End turn
	await _wait_ms(1500)
	await _shot("f3-captains-log")
	await _press()          # Close log
	await _press()          # Understood
	await _wait_ms(800)
	await _shot("f3-advisor-hint")
	# diplomacy: the hail panel and the hail scene
	app.goto("diplomacy")
	await _wait(6)
	var ds := app.current_screen()
	var tid := 2
	for e in app.view.get("empires", []):
		if int(e.get("empire", 0)) != app.player():
			tid = int(e["empire"])
			break
	ds.vm.select(false, tid)
	ds.refresh()
	await _wait_ms(1200)
	await _shot("f3-hail-panel")
	ds.open_hail(app.empire_name(tid), tid, false, "At peace")
	await _wait_ms(1000)
	await _shot("f3-hail-scene")
	app.close_all_modals()
	await _busts()
	await _mirror()
	AppSaves.store_settings(saved)
	quit(0)

## Every bust on its viewscreen: the eight powers, the Terran Empire and the Mirror twins.
func _busts() -> void:
	app.goto("galaxy")
	await _wait(4)
	var layer := ColorRect.new()
	layer.color = Lcars.BG
	layer.set_anchors_preset(Control.PRESET_FULL_RECT)
	root.add_child(layer)
	var grid := GridContainer.new()
	grid.columns = 6
	grid.add_theme_constant_override("h_separation", 14)
	grid.add_theme_constant_override("v_separation", 14)
	grid.position = Vector2(40, 40)
	layer.add_child(grid)
	var items := [[1, "fed", "Federation"], [2, "kli", "Klingon"], [3, "rom", "Romulan"], [4, "car", "Cardassian"], [5, "fer", "Ferengi"], [6, "dom", "Dominion"],
		[7, "borg", "Borg"], [8, "breen", "Breen"], [1, "terran", "Terran Empire"], [2, "mirror.kli", "Mirror Klingon"], [3, "mirror.rom", "Mirror Romulan"], [4, "mirror.car", "Mirror Cardassian"],
		[5, "mirror.fer", "Mirror Ferengi"], [6, "mirror.dom", "Mirror Dominion"], [7, "mirror.borg", "Mirror Borg"], [8, "mirror.breen", "Mirror Breen"]]
	for it in items:
		var h := HailViewscreen.make(int(it[0]), Vector2(290, 230), str(it[2]), "Channel open")
		h.key = str(it[1])
		h.force_bust = true
		h.live = false
		grid.add_child(h)
	await _wait_ms(600)
	await _shot("f3-busts")
	layer.queue_free()
	await _wait(2)

## The Mirror Universe: the Terran Empire hails on the viewscreen (scenario start, then diplomacy).
func _mirror() -> void:
	app.settings["tutorial"] = -1
	app.goto("new")
	await _wait(4)
	var nc := app.current_screen()
	if not nc.vm.pick_scenario("mirror-universe"):
		print("SKIP f3-mirror-hail: no Mirror Universe scenario")
		return
	nc.faction = 2
	nc.start()
	await _wait_ms(2500)
	app.goto("diplomacy")
	await _wait(6)
	var ds := app.current_screen()
	ds.open_hail(app.empire_name(1), 1, false, "At war")
	await _wait_ms(1000)
	await _shot("f3-mirror-hail")
	app.close_all_modals()
