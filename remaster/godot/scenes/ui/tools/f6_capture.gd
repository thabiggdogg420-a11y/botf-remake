extends SceneTree
## F6 frame proof: a tactical drill fought through the real combat screen in real time and then at the helm.
##   godot --path remaster/godot --audio-driver Dummy --resolution 1920x1080 --position 4000,0 -s res://scenes/ui/tools/f6_capture.gd -- <out_dir> [ships]
## Frames: realtime-<n>.png while real time runs (no plan phase between rounds), helm-<n>.png at the helm of one ship,
## paused.png after Space. Prints "F6 round <r> ..." lines and exits 1 when real time did not run on by itself.
## Saves and settings go to a scratch sandbox (AppSaves.sandbox); the window takes no focus.

var app: App
var out_dir := "user://f6"
var code := 0


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() > 0:
		out_dir = str(args[0])
	DirAccess.make_dir_recursive_absolute(out_dir)
	AppSaves.sandbox("user://capture_sandbox/f6/%d" % int(Time.get_unix_time_from_system()))
	root.set_flag(Window.FLAG_NO_FOCUS, true)
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred(int(args[1]) if args.size() > 1 else 3)


func _wait(n: int) -> void:
	for i in n:
		await process_frame


func _secs(s: float) -> void:
	await create_timer(s).timeout


func _shot(name: String) -> void:
	await _wait(2)
	var img := root.get_viewport().get_texture().get_image()
	img.resize(960, 540, Image.INTERPOLATE_LANCZOS)
	img.save_png("%s/%s.png" % [out_dir, name])


func _key(code_: Key) -> InputEventKey:
	var k := InputEventKey.new()
	k.keycode = code_
	k.pressed = true
	return k


func _run(ships: int) -> void:
	await _wait(10)
	app.goto("title")
	await _wait(6)
	app.start_drill("newBattleDrill", "Tactical drill", {"ships": ships})
	await _wait(20)
	app.end_turn()
	await _wait(30)
	app.close_all_modals()
	await _wait(10)
	var cs = app.current_screen()
	cs.refresh()
	await _secs(2.0)
	await _shot("plan")
	# Real time: Engage for all ships, then the toggle; rounds run on with no plan phase.
	cs.order("Engage")
	cs.set_realtime(true)
	var last := int(app.encounter.get("round", 0))
	var n := 0
	for i in 16:
		await _secs(1.5)
		await _shot("realtime-%02d" % n)
		n += 1
		var r := int(app.encounter.get("round", 0))
		if r != last:
			print("F6 round %d realtime (plan phase skipped: %s)" % [r, str(cs._playing_layout)])
			last = r
		if app.encounter.get("finished", false):
			break
	if last < 3 and not app.encounter.get("finished", false):
		print("F6 FAIL: real time did not run on (round %d)" % last)
		code = 1
	# The helm: one of our ships, key 1 (Engage), Tab for a target.
	if not app.encounter.get("finished", false):
		var mine: Array = app.encounter.get("ships", []).filter(func(sd): return sd.get("mine", false) and CombatVM.is_active(sd) and not sd.get("station", false))
		if not mine.is_empty():
			cs.vm.pick(int(mine[0]["id"]))
			cs.toggle_helm()
			cs._realtime_key(_key(KEY_TAB))
			cs._realtime_key(_key(KEY_1))
			for i in 10:
				await _secs(1.5)
				await _shot("helm-%02d" % i)
				var r := int(app.encounter.get("round", 0))
				if r != last:
					print("F6 round %d helm %d (%s)" % [r, cs.helm, CombatVM.short_name(cs._ship_by_id(cs.helm))])
					last = r
				if app.encounter.get("finished", false):
					break
	if not app.encounter.get("finished", false):
		cs._realtime_key(_key(KEY_SPACE))
		for i in 30:
			await _secs(0.5)
			if not cs.realtime:
				break
		await _secs(1.5)
		await _shot("paused")
		print("F6 paused: realtime %s helm %d round %d" % [str(cs.realtime), cs.helm, int(app.encounter.get("round", 0))])
		if cs.realtime:
			code = 1
	print("F6 done, code ", code)
	quit(code)
