extends SceneTree
## Hotseat frame proof (MULTIPLAYER.md): a two-human campaign (Federation and Klingon) through the real screens.
##   godot --path remaster/godot --audio-driver Dummy --resolution 1920x1080 --position 4000,0 -s res://scenes/ui/tools/hotseat_capture.gd -- <out_dir>
## Frames: 1-fed-galaxy, 2-card-klingon, 3-klingon-galaxy, 4-card-fed (the next turn), 5-fed-briefing. Prints "HOTSEAT ..." lines.
## Saves and settings go to a scratch sandbox; the window takes no focus.

var app: App
var out_dir := "user://hotseat"
var code := 0


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() > 0:
		out_dir = str(args[0])
	DirAccess.make_dir_recursive_absolute(out_dir)
	AppSaves.sandbox("user://capture_sandbox/hotseat/%d" % int(Time.get_unix_time_from_system()))
	root.set_flag(Window.FLAG_NO_FOCUS, true)
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred()


func _wait(n: int) -> void:
	for i in n:
		await process_frame


func _shot(name: String) -> void:
	await create_timer(1.2).timeout
	await _wait(2)
	var img := root.get_viewport().get_texture().get_image()
	img.resize(960, 540, Image.INTERPOLATE_LANCZOS)
	img.save_png("%s/%s.png" % [out_dir, name])


func _meta() -> Dictionary:
	return app.view.get("meta", {})


func _run() -> void:
	await _wait(10)
	app.goto("new")
	await _wait(10)
	var nc = app.current_screen()
	nc.faction = 1
	nc.seed_value = 515151
	nc.preset = "remastered"
	nc.vm.toggle_hotseat(2)
	nc.refresh()
	await _shot("0-new-campaign")
	app.start_campaign(nc.options())
	await _wait(30)
	app.close_all_modals()
	await _shot("1-fed-galaxy")
	var turn0 := int(_meta()["turn"])
	print("HOTSEAT start: humans %s, chair %d, turn %d" % [str(_meta().get("hotseat")), int(_meta()["player"]), int(_meta()["turn"])])
	var st := app.end_turn()
	await _wait(20)
	await _shot("2-card-klingon")
	print("HOTSEAT %s: chair %d, turn %d, card %s" % [st, int(_meta()["player"]), int(_meta()["turn"]), str(app.has_node("HotseatCard"))])
	if st != "handover" or int(_meta()["player"]) != 2:
		code = 1
	app._hotseat_take(app.get_node("HotseatCard"))
	await _wait(20)
	app.close_all_modals()
	await _shot("3-klingon-galaxy")
	st = app.end_turn()
	await _wait(30)
	if st == "battle":
		app.current_screen()._auto()
		await _wait(10)
		st = app.end_turn()
		await _wait(30)
	await _shot("4-card-fed")
	print("HOTSEAT %s: chair %d, turn %d, card %s" % [st, int(_meta()["player"]), int(_meta()["turn"]), str(app.has_node("HotseatCard"))])
	if st != "complete" or int(_meta()["player"]) != 1 or int(_meta()["turn"]) != turn0 + 1:
		code = 1
	if app.has_node("HotseatCard"):
		app._hotseat_take(app.get_node("HotseatCard"))
	await _wait(20)
	await _shot("5-fed-briefing")
	print("HOTSEAT done, code ", code)
	quit(code)
