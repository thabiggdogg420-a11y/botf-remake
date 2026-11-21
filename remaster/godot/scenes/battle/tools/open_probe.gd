extends SceneTree
## Battle-open probe (RC round, 20 Nov 2026): the first battle open on a GPU window, as pc_smoke measures it (title, a
## drill, End turn), with each frame of the first second split into process and draw and the traced steps inside it.
##   godot --path remaster/godot --resolution 1920x1080 --audio-driver Dummy -s res://scenes/battle/tools/open_probe.gd -- [runs] [loaded]
## `loaded` (21 Nov): as pc_smoke's drill instead: a campaign is started, saved, the client relaunched and the save loaded
## (galaxy up, its spare battle view warmed) before the drill opens; saves go to a capture sandbox.
## Prints OPEN lines: "OPEN run <n> worst <ms> ms" and one "OPEN frame" line per frame over 34 ms.
var app: Node

func _initialize() -> void:
	LoadTrace.on = true
	LoadTrace.FRAME_MS = 34
	_run.call_deferred()

func _run() -> void:
	var runs := 1
	for a in OS.get_cmdline_user_args():
		if a.is_valid_int():
			runs = int(a)
	var loaded := OS.get_cmdline_user_args().has("loaded")
	if loaded:
		AppSaves.sandbox("user://capture_sandbox/open-probe/%d" % int(Time.get_unix_time_from_system()))
	LoadTrace.split_frames(self)
	# As pc_smoke: a first client boots and is freed (its title fleet loaded), then a fresh client opens the drill.
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	for i in 900:
		await process_frame
	app.queue_free()
	for i in 20:
		await process_frame
	for r in runs:
		app = load("res://scenes/ui/app.tscn").instantiate()
		root.add_child(app)
		for i in 200:
			await process_frame
		if loaded:
			await _load_campaign()
		app.start_drill("newBattleDrill", "Tactical drill")
		for i in 30:
			await process_frame
		LoadTrace.min_override = 3
		print("OPEN start run %d frame %d" % [r, Engine.get_process_frames()])
		var t_open := Time.get_ticks_usec()
		app.end_turn()
		var last := t_open
		var worst := 0.0
		var n := 0
		while Time.get_ticks_usec() - t_open < 1000000:
			await process_frame
			n += 1
			var now := Time.get_ticks_usec()
			var ms := (now - last) / 1000.0
			worst = maxf(worst, ms)
			if ms > 34.0:
				print("OPEN frame %d (#%d after end_turn): %.0f ms" % [Engine.get_process_frames(), n, ms])
			last = now
		LoadTrace.min_override = -1
		print("OPEN run %d worst %.0f ms" % [r, worst])
		app.queue_free()
		for i in 20:
			await process_frame
	quit(0)

## pc_smoke's route to its drill: a Federation campaign, saved, a fresh client, the save loaded.
func _load_campaign() -> void:
	app.goto("new")
	for i in 10:
		await process_frame
	var nc: Node = app.current_screen()
	nc.faction = 1
	nc.preset = "remastered"
	nc.refresh()
	for i in 10:
		await process_frame
	nc.start()
	for i in 60:
		await process_frame
	app.close_all_modals()
	app.save_game("OpenProbe")
	app.queue_free()
	for i in 6:
		await process_frame
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	for i in 10:
		await process_frame
	for s in app.list_saves():
		if s.get("name", "") == "OpenProbe":
			app.load_game(s)
			for i in 120:
				await process_frame
			app.delete_save(str(s["file"]))
			break
