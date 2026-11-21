extends SceneTree
## C7 domination setting end to end (21 Nov 2026, Barclay): the New Campaign switch, a campaign started with it off, the
## victory goals showing it, and a save/load keeping it.
##   godot --path remaster/godot --audio-driver Dummy --resolution 1920x1080 --position -10000,-10000 \
##     -s res://scenes/ui/tools/c7_capture.gd -- <out_dir>
## Prints "C7 ok|FAIL ..." lines; exit 1 on any failure. Saves and settings go to a capture sandbox.

var app: App
var out_dir := ""
var failures := 0


func _initialize() -> void:
	AppSaves.sandbox("user://capture_sandbox/c7/%d" % int(Time.get_unix_time_from_system()))
	root.set_flag(Window.FLAG_NO_FOCUS, true)
	DisplayServer.window_set_size(Vector2i(1920, 1080))
	root.size = Vector2i(1920, 1080)
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred()


func check(cond: bool, what: String) -> void:
	if not cond:
		failures += 1
	print("C7 ", "ok   " if cond else "FAIL ", what)


func _wait(n: int = 6) -> void:
	for i in n:
		await process_frame


func _shot(name_: String) -> void:
	await create_timer(1.5).timeout
	await _wait(2)
	await RenderingServer.frame_post_draw
	root.get_texture().get_image().save_png("%s/%s.png" % [out_dir, name_])


func _texts(n: Node) -> String:
	var t := ""
	for l in n.find_children("*", "Label", true, false):
		t += (l as Label).text + "|"
	return t


func _run() -> void:
	var args := OS.get_cmdline_user_args()
	out_dir = str(args[0]) if args.size() > 0 else OS.get_user_data_dir() + "/c7"
	DirAccess.make_dir_recursive_absolute(out_dir)
	await _wait(40)
	# New Campaign: the switch is there, on by default, and its state reaches the options.
	app.goto("new")
	await _wait(20)
	var cb := root.find_child("Domination", true, false) as CheckBox
	check(cb != null and cb.button_pressed, "New Campaign shows the Domination victory switch, on by default")
	if cb != null:
		cb.button_pressed = false
		await _wait(2)
		var vm = app.current_screen().get("vm")
		check(vm != null and vm.options().get("domination", true) == false, "switching it off reaches the campaign options")
	await _shot("c7-1-new-campaign")
	# A campaign started with it off.
	check(app.start_campaign({"faction": 1, "domination": false}), "campaign starts with domination off")
	await _wait(30)
	check(app.view.get("meta", {}).get("domination", true) == false, "meta.domination is false")
	app.close_all_modals()
	app.open_overlay("victory")
	await _wait(10)
	var t := _texts(root)
	check(t.to_lower().find("off in this campaign") >= 0 and t.find("conquest is the military victory") >= 0, "the victory goals show domination off in this campaign")
	await _shot("c7-2-victory-goals")
	app.close_all_modals()
	# Save and load keep it.
	var blob := app.session.save_blob()
	check(str(blob.get("data", "")).find("NO_DOMINATION 1") >= 0, "the save carries NO_DOMINATION")
	check(app.load_game({"data": blob.get("data", ""), "name": "c7.sav"}), "the save loads")
	await _wait(20)
	check(app.view.get("meta", {}).get("domination", true) == false, "domination is still off after loading")
	print("C7 summary: %d failures" % failures)
	quit(1 if failures > 0 else 0)
