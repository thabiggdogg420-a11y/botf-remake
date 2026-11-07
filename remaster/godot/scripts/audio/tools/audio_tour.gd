extends SceneTree
## Drives the real app through every screen with the audio system live and prints what each one sounds like:
## the music context, the track playing, and the cues fired. Evidence that every screen has sound.
##   godot --headless --path remaster/godot -s res://scripts/audio/tools/audio_tour.gd
## (BOTF_AUDIO_DIR=<LocalAssets/audio> to play the rebuilt BOTF files; without it the generated sounds play.)

func _initialize() -> void:
	_run.call_deferred()

func frames(n := 3) -> void:
	for k in n:
		await process_frame

func _run() -> void:
	var sys := BotfAudioSystem.instance
	App.forced_backend = ""
	var app: App = load("res://scenes/ui/app.tscn").instantiate()
	app.size = Vector2(1920, 1080)
	root.add_child(app)
	await frames()
	var report := func(label: String):
		var m := sys._music[sys._music_front]
		print("%-14s music=%-8s key=%-34s playing=%s  cues=%s" % [label, sys.music_context, sys.music_key, m.playing,
			sys.recent.slice(-4).map(func(r): return "%s>%s" % [r.cue if r.cue != "" else "-", r.key])])
	report.call("title")
	app.start_campaign({"empire": 3, "era": 3, "seed": 7, "preset": "remastered"})
	await frames()
	report.call("new game (rom)")
	for id in ["galaxy", "system", "empire", "research", "fleets", "diplomacy", "intel", "database"]:
		sys.clock_override = sys.now() + 5.0
		app.goto(id)
		await frames()
		report.call(id)
	sys.clock_override = sys.now() + 5.0
	app.request_end_turn()
	await frames(6)
	report.call("end turn")
	app.queue_free()
	await frames()
	quit(0)
