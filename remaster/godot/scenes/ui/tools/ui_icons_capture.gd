extends SceneTree
## Trek icon pass proof frames (18 Nov): the top status bar and the LCARS rail (galaxy screen), and the system screen with three
## items in the build queue (workforce rows, the growth/morale/energy cards, queue up/down/remove) on the Installed and the
## Structures tab (construction cost and time). Federation, tutorial off, fixed seed. Writes <tag>-<shot>.png.
##   BOTFRemastered.exe --audio-driver Dummy --resolution 1920x1080 -s res://scenes/ui/tools/ui_icons_capture.gd -- <out_dir> <tag>
##   web: web_driver.py --args "-s res://scenes/ui/tools/ui_icons_capture.gd" ; each frame prints "WEBSHOT <tag>-<shot>" and holds 4 s.

var app: App
var out_dir := "user://ui-icons"
var tag := "after"
var web := OS.has_feature("web")

func _initialize() -> void:
	var a := Array(OS.get_cmdline_user_args())
	if a.size() > 0:
		out_dir = a[0]
	if a.size() > 1:
		tag = a[1]
	if not web:
		DirAccess.make_dir_recursive_absolute(out_dir)
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred()

func _wait(n: int = 6) -> void:
	for i in n:
		await process_frame

func _wait_ms(ms: int) -> void:
	var t := Time.get_ticks_msec()
	while Time.get_ticks_msec() - t < ms:
		await _wait(4)

func _shot(name_: String) -> void:
	await _wait(10)
	for t in app.get_node("Toasts").get_children():
		t.queue_free()
	if app.advisor_panel != null:
		app.advisor_panel.modulate.a = 0.0   # the advisor card would cover the workforce rows
	await _wait(3)
	if web:
		print("WEBSHOT %s-%s" % [tag, name_])
		await _wait_ms(4000)
		return
	var p := out_dir.path_join("%s-%s.png" % [tag, name_])
	root.get_viewport().get_texture().get_image().save_png(p)
	print("SHOT ", p)

func _run() -> void:
	var saved := {}
	AppSaves.load_settings(saved)
	await _wait(6)
	app.settings["tutorial"] = -1
	app.settings["uiEra"] = "tng"
	UiTokens.era = "tng"
	app.start_campaign({"faction": 1, "seed": 424242, "era": 3, "ruleset": "reference", "width": 22, "height": 16, "systems": 48,
		"preset": "remastered", "events": true, "aiAggression": true})
	app.close_all_modals()
	await _wait_ms(1500)
	app.close_all_modals()
	app.goto("galaxy")
	await _wait(12)
	await _shot("galaxy")
	app.goto("system")
	await _wait(6)
	var sc = app.current_screen()
	var n := 0
	for ch in sc.vm.choices(false):
		if n >= 3:
			break
		if sc.vm.choice_block(ch) == "" and sc.vm.enqueue(ch):
			n += 1
	print("QUEUED ", n)
	sc.tab = "installed"
	sc.refresh()
	await _wait(8)
	await _shot("system")
	sc.tab = "structures"
	sc.refresh()
	await _wait(8)
	await _shot("system-structures")
	AppSaves.store_settings(saved)
	quit(0)
