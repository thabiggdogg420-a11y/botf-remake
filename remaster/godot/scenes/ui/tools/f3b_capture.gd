extends SceneTree
## F3 follow-up frames (the lead's frame review, 18 Nov): the 3D system view filling its band, the star, the tutorial card
## clear of the control it points at (with the ring on it), and hail scenes with the power's portrait after a real contact.
##   BOTFRemastered.exe --audio-driver Dummy --resolution 1920x1080 -s res://scenes/ui/tools/f3b_capture.gd -- <out_dir>
##   web: web_driver.py --args "-s res://scenes/ui/tools/f3b_capture.gd" ; each frame prints "WEBSHOT <name>" and holds 4 s
##        so the driver can screenshot it (the browser cannot write the PNG to disk itself).
## Contact hook: seed 31337, then end turns pressing only the advisor (it charts the nearest unknown stars) until a major
## power is in contact (at most 60 turns); the hail is opened from that power's dossier Hail button. A Mirror Universe game
## follows (Terran hail). Prints "CONTACT <empire> turn <n>" and "HAIL <empire> picture|bust".

var app: App
var out_dir := "user://f3b"
var web := OS.has_feature("web")

func _initialize() -> void:
	var args := Array(OS.get_cmdline_user_args())
	if args.size() > 0:
		out_dir = args[0]
	if not web:
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
	if web:
		print("WEBSHOT ", name_)
		await _wait_ms(4000)
		return
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

func _contacts() -> Array:
	return app.view.get("empires", []).filter(func(e): return int(e.get("empire", 0)) != app.player() and e.get("contact", false))

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
	await _press()          # Begin
	await _shot("f3b-tutorial-system")   # step 2 points at the Colony rail button
	await _press()          # Open system
	await _wait_ms(5000)    # planet textures stream in
	await _shot("f3b-tutorial-build")    # step 3: the card leaves Workforce and Construction clear
	await _press()          # Queue a build
	await _wait_ms(1200)
	await _shot("f3b-tutorial-research")
	app.advisor_panel.folded = true
	app.advisor_panel.update()
	await _wait_ms(800)
	await _shot("f3b-system-3d")
	var sv: SystemView3D = app.current_screen().find_child("SystemView3D", true, false)
	if sv:
		print("SYSVIEW size %s planets %d selected %d radius_px %.1f" % [str(sv.size), sv.planets.size(), sv.selected, sv.planet_screen_radius(sv.selected)])
		if not web:
			var img := sv.viewport.get_texture().get_image()
			img.save_png("%s/f3b-system-3d-view.png" % out_dir)
	app.advisor_panel.folded = false
	await _press()          # Open research
	await _press()          # Set course
	await _press()          # End turn
	await _wait_ms(1500)
	await _press()          # Close log
	await _press()          # Understood
	# the contact hook: play on with the advisor until a major power answers
	var guard := 0
	while _contacts().is_empty() and int(app.view.get("meta", {}).get("turn", 0)) < 60 and guard < 600:
		guard += 1
		await _press()
		await _wait(2)
	var met := _contacts()
	if met.is_empty():
		print("CONTACT none by turn ", app.view.get("meta", {}).get("turn", 0))
	else:
		app.close_all_modals()
		await _wait(4)
		var tid := int(met[0]["empire"])
		print("CONTACT ", app.empire_name(tid), " turn ", app.view.get("meta", {}).get("turn", 0))
		app.goto("diplomacy")
		await _wait(6)
		var ds := app.current_screen()
		ds.vm.select(false, tid)
		ds.refresh()
		await _wait_ms(1200)
		await _shot("f3b-hail-panel")
		var hb: Button = ds.find_child("Hail", true, false) as Button
		if hb:
			hb.pressed.emit()
		else:
			ds.open_hail(app.empire_name(tid), tid, false, "At peace")
		await _wait_ms(1200)
		var hv: HailViewscreen = app.find_child("HailViewscreen", true, false) as HailViewscreen
		print("HAIL ", app.empire_name(tid), " ", ("bust" if hv == null or hv.shows_bust() else "picture"), " keys ", hv.portrait_keys() if hv else [])
		await _shot("f3b-hail-scene")
		app.close_all_modals()
	await _mirror()
	AppSaves.store_settings(saved)
	quit(0)

## The Mirror Universe: the Terran Empire hails (Mirror character's picture, else the normal one, else the bust).
func _mirror() -> void:
	app.settings["tutorial"] = -1
	app.goto("new")
	await _wait(4)
	var nc := app.current_screen()
	if not nc.vm.pick_scenario("mirror-universe"):
		print("SKIP mirror: no Mirror Universe scenario")
		return
	nc.faction = 2
	nc.start()
	await _wait_ms(2500)
	app.goto("diplomacy")
	await _wait(6)
	var ds := app.current_screen()
	ds.open_hail(app.empire_name(1), 1, false, "At war")
	await _wait_ms(1200)
	var hv: HailViewscreen = app.find_child("HailViewscreen", true, false) as HailViewscreen
	print("HAIL mirror ", app.empire_name(1), " ", ("bust" if hv == null or hv.shows_bust() else "picture"), " keys ", hv.portrait_keys() if hv else [])
	await _shot("f3b-mirror-hail")
	app.close_all_modals()
