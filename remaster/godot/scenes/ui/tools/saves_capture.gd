extends SceneTree
## F5 autosave proof: a campaign played four turns in a real window (each End turn autosaves, slots rotate), then the
## Saved campaigns screen with the thumbnails. Saves and settings go to a fresh sandbox (user://capture_sandbox/saves/<time>);
## the player's user://saves is never touched (the old move-aside-and-restore was not safe if a run died mid-way).
##   godot --path remaster/godot --audio-driver Dummy --resolution 1920x1080 -s res://scenes/ui/tools/saves_capture.gd -- <out_dir>

var app: App
var out_dir := "user://saves_cap"
var code := 0


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() > 0:
		out_dir = str(args[0])
	DirAccess.make_dir_recursive_absolute(out_dir)
	AppSaves.sandbox("user://capture_sandbox/saves/%d" % int(Time.get_unix_time_from_system()))
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred()


func _wait(n: int) -> void:
	for i in n:
		await process_frame


func _shot(name: String) -> void:
	await _wait(3)
	var img := root.get_viewport().get_texture().get_image()
	img.resize(960, 540, Image.INTERPOLATE_LANCZOS)
	img.save_png("%s/%s.png" % [out_dir, name])


func _run() -> void:
	await _wait(10)
	app.start_campaign({"faction": 2, "seed": 515151, "era": 1, "ruleset": "reference", "preset": "remastered"})
	app.goto("galaxy")
	await _wait(30)
	for t in 4:
		app.end_turn()
		await _wait(40)
		for m in app._modals.duplicate():
			app.close_modal(m)
		await _wait(10)
	var names := []
	for s in app.list_saves():
		if str(s.get("name", "")).begins_with("Autosave "):
			names.append("%s turn %d thumb %d B" % [s["name"], int(s.get("turn", 0)), str(s.get("thumb", "")).length()])
			if str(s.get("thumb", "")) == "":
				code = 1
	print("SAVES autosaves: ", names)
	if names.size() != 3:
		code = 1
	app.goto("saves")
	await _wait(20)
	await _shot("saves-screen")
	print("SAVES done, code ", code)
	quit(code)
