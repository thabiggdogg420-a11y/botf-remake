extends SceneTree
## Advisor "Not now" (round 9 browser drive: the Battle Joined card stayed after the button was clicked). Situational
## cards (battle, battle over, captain's log, aftermath) were not hints, so the per-turn dismissed list never applied to
## them and the card came straight back. Checks, in a real battle: the click on "Not now" (a real mouse event) closes the
## card, it stays closed across polls and re-layouts, and a different situation (battle over) shows its own card again.
##   godot --headless --path remaster/godot --audio-driver Dummy -s res://scenes/ui/tests/test_advisor_notnow.gd
## Exit code 0 on success, 1 on any failure.

var failures := 0
var checks := 0
var app: App

func _initialize() -> void:
	_run.call_deferred()

func check(cond: bool, what: String) -> void:
	checks += 1
	if cond:
		print("  ok  ", what)
	else:
		failures += 1
		printerr("  FAIL ", what)

func frames(n: int = 2) -> void:
	for k in n:
		await process_frame

func _click(c: Control) -> void:
	var at: Vector2 = c.get_global_transform_with_canvas() * (c.size * 0.5)   # the transform, not get_global_rect(): the app may be scaled
	var mm := InputEventMouseMotion.new()
	mm.position = at
	mm.global_position = at
	root.push_input(mm, true)
	await frames(2)
	for pressed in [true, false]:
		var e := InputEventMouseButton.new()
		e.button_index = MOUSE_BUTTON_LEFT
		e.pressed = pressed
		e.position = at
		e.global_position = at
		e.button_mask = MOUSE_BUTTON_MASK_LEFT if pressed else 0
		root.push_input(e, true)
		await frames(2)

func _run() -> void:
	var saved := {}
	AppSaves.load_settings(saved)
	var backend := "native" if Engine.has_singleton("BotfCore") else "fixture"
	App.forced_backend = "fixture" if backend == "fixture" else ""
	root.size = Vector2i(1920, 1080)
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	await frames(2)
	app.settings["tutorial"] = -1
	app.start_drill("newBattleDrill", "Starbase drill", {"station": true})
	app.end_turn()
	for n in 600:
		await frames(1)
		if app.screen == "combat" and app.advisor_panel.visible:
			break
	var panel := app.advisor_panel
	check(app.screen == "combat", "a drill battle is open (%s backend)" % backend)
	check(panel.visible and str(panel.current.get("id", "")) == "battle", "the Battle Joined card is up")
	var nn: Button = panel.find_child("AdvisorDismiss", true, false) as Button
	check(nn != null and nn.text == "Not now", "the card has a Not now button")
	if nn != null:
		await _click(nn)
		await frames(2)
		check(not panel.visible and panel.current.is_empty(), "clicking Not now with the mouse closes the card")
		# the card must not come back on the next polls, a re-layout, or a new view of the same battle
		var back := false
		for k in 30:
			await create_timer(0.1).timeout
			if panel.visible or not panel.current.is_empty():
				back = true
		check(not back, "the card stays closed for 3 s (poll interval 0.4 s)")
		panel._sig = ""
		panel.update()
		check(not panel.visible, "an explicit rebuild (view change) keeps it closed")
		# a different situation shows its own card
		var sess: Dictionary = app.view.get("session", {})
		sess["battleFinished"] = true
		panel.update()
		check(panel.visible and str(panel.current.get("id", "")) == "battle_done", "the battle over shows its own card")
		var nn2: Button = panel.find_child("AdvisorDismiss", true, false) as Button
		check(nn2 != null, "the battle-over card has Not now too")
		if nn2 != null:
			nn2.pressed.emit()
			check(not panel.visible, "Not now closes the battle-over card")
			panel.update()
			check(not panel.visible, "and it stays closed")
		sess["battleFinished"] = false
		panel.update()
		check(panel.visible and str(panel.current.get("id", "")) == "battle", "a new situation is not held back by the last one's Not now")
	AppSaves.store_settings(saved)
	print("%d checks, %d failures" % [checks, failures])
	quit(1 if failures > 0 else 0)
