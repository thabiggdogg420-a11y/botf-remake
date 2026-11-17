extends SceneTree
## Probe: which buttons play a UI cue when pressed. Presses every rail button and the Quit to title confirm with the audio system
## running and prints what played (the cues recorded in BotfAudioSystem.recent).
##   godot --headless --audio-driver Dummy --path remaster/godot -s res://scenes/ui/tools/click_probe.gd

var app: App
var clock := 1000.0

func frames(n := 3) -> void:
	for k in n:
		await process_frame

func _initialize() -> void:
	_run.call_deferred()

func _press(label: String, b: BaseButton) -> void:
	var sys := BotfAudioSystem.instance
	clock += 30.0
	sys.clock_override = clock
	sys.recent.clear()
	b.pressed.emit()
	await frames(3)
	print("PROBE %-34s -> %s" % [label, str(sys.recent.map(func(r): return r.cue + ":" + r.key))])

func _run() -> void:
	App.forced_backend = "fixture"
	app = load("res://scenes/ui/app.tscn").instantiate()
	app.size = Vector2(1920, 1080)
	root.add_child(app)
	await frames()
	app.start_campaign({"faction": 1, "seed": 424242, "era": 3, "ruleset": "reference", "width": 22, "height": 16, "systems": 48, "preset": "remastered", "events": true, "aiAggression": true})
	app.close_all_modals()
	await frames(4)
	var sys := BotfAudioSystem.instance
	print("PROBE audio system: %s, screen %s" % [str(sys != null), app.screen])
	for id in ["research", "galaxy", "system", "fleets", "empire", "system", "system"]:
		var b := app.find_child("Rail_" + id, true, false)
		if b == null:
			print("PROBE Rail_%s missing" % id)
			continue
		await _press("rail " + id + " (from " + app.screen + ")", b)
		await frames(2)
	# Quit to title confirm
	Overlays.confirm_quit_to_title(app)
	await frames(3)
	var ok := app.find_child("ConfirmButton", true, false)
	print("PROBE confirm button: %s" % str(ok != null))
	if ok != null:
		await _press("quit-to-title confirm", ok)
	print("PROBE screen after: %s" % app.screen)
	quit(0)
