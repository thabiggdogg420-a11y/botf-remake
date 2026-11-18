extends SceneTree
## Mirror Universe hail frames (18 Nov, the three new Mirror portraits): Mirror games as the Mirror Klingons and as the Terrans,
## the hail scene of the Terran Empire, the Mirror Klingons and the Mirror Cardassians, one frame each. Prints "HAIL <name> picture|bust keys [...]".
##   BOTFRemastered.exe --audio-driver Dummy --resolution 1920x1080 -s res://scenes/ui/tools/f3c_mirror_hails.gd -- <out_dir>
##   web: web_driver.py --args "-s res://scenes/ui/tools/f3c_mirror_hails.gd" ; each frame prints "WEBSHOT <name>" and holds 4 s.

var app: App
var out_dir := "user://f3c"
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
	root.get_viewport().get_texture().get_image().save_png("%s/%s.png" % [out_dir, name_])
	print("SHOT ", name_)

func _run() -> void:
	var saved := {}
	AppSaves.load_settings(saved)
	await _wait(6)
	app.settings["tutorial"] = -1
	# The Mirror lineup is Terran (1), Mirror Klingon (2) and Mirror Cardassian (4); a power cannot hail itself, so the Klingons
	# take the Terran and Cardassian hails and a second game as the Terrans takes the Klingon hail.
	for game in [[2, [[1, "terran"], [4, "mirror-cardassian"]]], [1, [[2, "mirror-klingon"]]]]:
		app.goto("new")
		app.close_all_modals()
		await _wait(4)
		var nc := app.current_screen()
		if not nc.vm.pick_scenario("mirror-universe"):
			print("FAIL no Mirror Universe scenario")
			quit(1)
			return
		nc.faction = int(game[0])
		nc.start()
		await _wait_ms(2500)
		app.close_all_modals()
		app.goto("diplomacy")
		await _wait(6)
		var ds := app.current_screen()
		for it in game[1]:
			ds.open_hail(app.empire_name(int(it[0])), int(it[0]), false, "At war")
			await _wait_ms(1200)
			var hv: HailViewscreen = app.find_child("HailViewscreen", true, false) as HailViewscreen
			print("HAIL ", app.empire_name(int(it[0])), " ", ("bust" if hv == null or hv.shows_bust() else "picture"), " keys ", hv.portrait_keys() if hv else [])
			await _shot("mirror-hail-" + str(it[1]))
			app.close_all_modals()
			await _wait(4)
	AppSaves.store_settings(saved)
	quit(0)
