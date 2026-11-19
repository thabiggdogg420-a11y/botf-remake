extends SceneTree
## F5 autosave proof: a campaign played four turns in a real window (each End turn autosaves, slots rotate), then the
## Saved campaigns screen with the thumbnails. Writes into the real user://saves, so it first moves any slot it would
## overwrite (Autosave 1-3) aside to user://saves_before_capture and puts it back at the end.
##   godot --path remaster/godot --audio-driver Dummy --resolution 1920x1080 -s res://scenes/ui/tools/saves_capture.gd -- <out_dir>

var app: App
var out_dir := "user://saves_cap"
var code := 0


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() > 0:
		out_dir = str(args[0])
	DirAccess.make_dir_recursive_absolute(out_dir)
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


func _slots(move_to: String, move_from: String) -> void:
	DirAccess.make_dir_recursive_absolute(move_to)
	for i in 3:
		var fn := "Autosave %d.json" % (i + 1)
		if FileAccess.file_exists(move_from + "/" + fn):
			DirAccess.rename_absolute(move_from + "/" + fn, move_to + "/" + fn)


func _run() -> void:
	await _wait(10)
	_slots("user://saves_before_capture", AppSaves.DIR)
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
	# Put the player's own slots back (the capture's own autosaves go to the capture folder's archive).
	_slots("user://saves_capture_runs/%d" % Time.get_unix_time_from_system(), AppSaves.DIR)
	_slots(AppSaves.DIR, "user://saves_before_capture")
	print("SAVES done, code ", code)
	quit(code)
