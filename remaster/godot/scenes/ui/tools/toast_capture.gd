extends SceneTree
## Round 4 (21 Nov 2026, Barclay): toasts keep clear of every modal panel. Opens each modal with toasts up and checks no
## toast rect meets a panel rect; saves a frame of each.
##   godot --path remaster/godot --audio-driver Dummy --resolution 1920x1080 --position -10000,-10000 \
##     -s res://scenes/ui/tools/toast_capture.gd -- <out_dir> [width height]
## Prints "TOAST ok|FAIL ..." lines; exit 1 on any failure. Saves and settings go to a capture sandbox.

var app: App
var out_dir := ""
var failures := 0


func _initialize() -> void:
	AppSaves.sandbox("user://capture_sandbox/toast/%d" % int(Time.get_unix_time_from_system()))
	root.set_flag(Window.FLAG_NO_FOCUS, true)
	var args := OS.get_cmdline_user_args()
	var w := int(args[1]) if args.size() > 2 else 1920
	var h := int(args[2]) if args.size() > 2 else 1080
	DisplayServer.window_set_size(Vector2i(w, h))
	root.size = Vector2i(w, h)
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred()


func check(cond: bool, what: String) -> void:
	if not cond:
		failures += 1
	print("TOAST ", "ok   " if cond else "FAIL ", what)


func _wait(n: int = 6) -> void:
	for i in n:
		await process_frame


func _shot(name_: String) -> void:
	await _wait(2)
	await RenderingServer.frame_post_draw
	root.get_texture().get_image().save_png("%s/%s.png" % [out_dir, name_])


func _overlap() -> String:
	var tl := root.find_child("Toasts", true, false)
	var bad := ""
	for m in app._modals:
		if not is_instance_valid(m) or m.get_child_count() == 0:
			continue
		var pr := (m.get_child(0).get_child(0) as Control).get_global_rect()
		for t in tl.get_children():
			if t is Control and not t.is_queued_for_deletion() and (t as Control).get_global_rect().intersects(pr):
				bad += " toast %s meets panel %s" % [(t as Control).get_global_rect(), pr]
	return bad


func _run() -> void:
	var args := OS.get_cmdline_user_args()
	out_dir = str(args[0]) if args.size() > 0 else OS.get_user_data_dir() + "/toast"
	DirAccess.make_dir_recursive_absolute(out_dir)
	await _wait(40)
	check(app.start_campaign({"faction": 1, "domination": false}), "campaign starts")
	await _wait(30)
	var kinds := ["victory", "briefing", "events", "menu", "settings", "help", "aftermath", "confirm", "palette", "victory+menu"]
	for k: String in kinds:
		app.close_all_modals()
		app.clear_toasts()
		await _wait(3)
		match k:
			"confirm": app.confirm("Abandon campaign?", "A long confirmation body to give the panel some height.", "Abandon", func(): pass)
			"palette": app.open_palette()
			"victory+menu":
				app.open_overlay("victory")
				app.open_overlay("menu")
			_: app.open_overlay(k)
		app.toast("Campaign begins. Stardate 41000. A toast to test the stack.", "good", 30.0)
		app.toast("A second toast, so the stack has height.", "info", 30.0)
		app.toast("A third toast with a warning tone and longer words in it.", "warn", 30.0)
		await create_timer(0.8).timeout
		await _wait(4)
		if app._modals.is_empty():
			print("TOAST skip %s (no modal opened)" % k)
			continue
		var bad := _overlap()
		check(bad == "", "%s: no toast over the panel%s" % [k, bad])
		await _shot("toast-" + k.replace("+", "-"))
	app.close_all_modals()
	await _wait(4)
	var tl := root.find_child("Toasts", true, false) as Control
	check(absf(tl.get_global_rect().end.y - (app.get_viewport_rect().size.y - 90)) < 4, "with no modal the toasts return to bottom centre (%s)" % tl.get_global_rect())
	print("TOAST summary: %d failures" % failures)
	quit(1 if failures > 0 else 0)
