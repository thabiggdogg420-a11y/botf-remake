extends SceneTree
## Event-to-cue trace of a real battle, no sound needed (run with --audio-driver Dummy):
##   BOTF_CUE_TRACE=1 godot --audio-driver Dummy --path remaster/godot -s res://scenes/battle/tools/cue_trace.gd [-- rounds [ships]]
## Plays the starbase drill (Remastered rules, a starbase in the fight) round by round, each to its end, so every cue
## fires at its time. BattleScene prints one "CUE" line per cue (event, key, resolved file, what played); this script
## ends with a "CUE SUMMARY" per event kind and exits 1 when any visual weapon, impact, explosion, warp or cloak job
## had no cue at all.

var app: App

func _initialize() -> void:
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred()

func _wait(n := 1) -> void:
	for i in n:
		await process_frame

func _run() -> void:
	await _wait(4)
	var args := OS.get_cmdline_user_args()
	var rounds := int(args[0]) if args.size() > 0 else 8
	var drill := {"station": true}
	if args.size() > 1:
		drill["ships"] = int(args[1])
	app.start_drill("newBattleDrill", "Starbase drill", drill)
	app.end_turn()
	await _wait(4)
	app.close_all_modals()
	var view: Node = app._current.host.view_node
	view.playback_speed = float(OS.get_environment("TRACE_SPEED")) if OS.get_environment("TRACE_SPEED") != "" else 1.0
	var counts := {}
	var jobs := {}
	view.cue.connect(func(key, _keys, _p, _f): counts[key] = counts.get(key, 0) + 1)
	for r in rounds:
		if app.encounter.get("finished", false):
			break
		app._current._round()
		await _wait(3)
		if view.timeline != null:
			for j in view.timeline.jobs:
				var n: String = BattleTimeline.JOB_NAMES[j.kind]
				jobs[n] = jobs.get(n, 0) + 1
		var guard := 0
		while view.is_playing() and guard < 3000:
			guard += 1
			await _wait(1)
	print("CUE SUMMARY jobs ", jobs)
	print("CUE SUMMARY cues ", counts)
	quit(0)
