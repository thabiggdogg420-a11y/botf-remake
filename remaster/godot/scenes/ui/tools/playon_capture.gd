extends SceneTree
## C4 "Continue playing" end to end (21 Nov 2026, Barclay): a real domination win, the button pressed, the campaign runs on,
## a later win still ends it, and a LAN game is not offered the button.
##   godot --path remaster/godot --audio-driver Dummy --resolution 1920x1080 --position -10000,-10000 \
##     -s res://scenes/ui/tools/playon_capture.gd -- a <domination-eve.sav> <out_dir>
##     -s res://scenes/ui/tools/playon_capture.gd -- b <conquest-eve.sav> <out_dir>
## Phase a loads a save one end of turn from a Federation domination win, ends the turn, captures the end screen, checks the
## LAN case, clicks Continue playing, plays 5 more turns and writes <out_dir>/after-playon.sav. The conquest-eve save is that
## file with every colony given to the player (rc-captures/c4/c4_fixture conquer). Phase b ends one turn: a conquest win ends
## the played-on campaign and offers no Continue playing. Prints "PLAYON ok|FAIL ..." lines; exit 1 on any failure.
## Saves and settings go to a capture sandbox.

var app: App
var out_dir := ""
var failures := 0


func _initialize() -> void:
	AppSaves.sandbox("user://capture_sandbox/playon/%d" % int(Time.get_unix_time_from_system()))
	root.set_flag(Window.FLAG_NO_FOCUS, true)
	DisplayServer.window_set_size(Vector2i(1920, 1080))
	root.size = Vector2i(1920, 1080)
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred()


func check(cond: bool, what: String) -> void:
	if not cond:
		failures += 1
	print("PLAYON ", "ok   " if cond else "FAIL ", what)


func _wait(n: int = 6) -> void:
	for i in n:
		await process_frame


func _shot(name_: String) -> void:
	await create_timer(1.5).timeout
	await _wait(2)
	await RenderingServer.frame_post_draw
	root.get_texture().get_image().save_png("%s/%s.png" % [out_dir, name_])


## The text of a visible toast whose rect overlaps the button row of `b`, or "".
func _toast_over(b: Control) -> String:
	if b == null:
		return ""
	var row := b.get_parent() as Control
	var r := (row if row != null else b).get_global_rect()
	var tl := root.find_child("Toasts", true, false)
	if tl == null:
		return ""
	for t in tl.get_children():
		if t is Control and not t.is_queued_for_deletion() and (t as Control).is_visible_in_tree() and (t as Control).get_global_rect().intersects(r):
			for l in t.find_children("*", "Label", true, false):
				return "(%s)" % (l as Label).text
			return "(a toast)"
	return ""


func _meta() -> Dictionary:
	return app.view.get("meta", {})


func _button(text: String) -> Button:
	for b in root.find_children("*", "Button", true, false):
		if (b as Button).is_visible_in_tree() and (b as Button).text.strip_edges().to_lower() == text.to_lower():
			return b
	return null


func _click(c: Control) -> void:
	var at := c.get_global_rect().get_center()
	var mv := InputEventMouseMotion.new()
	mv.position = at
	mv.global_position = at
	Input.parse_input_event(mv)
	await process_frame
	for pressed in [true, false]:
		var e := InputEventMouseButton.new()
		e.button_index = MOUSE_BUTTON_LEFT
		e.position = at
		e.global_position = at
		e.pressed = pressed
		Input.parse_input_event(e)
		await process_frame


## Ends the turn, auto-resolving any battle on the way. Returns the final status.
func _end_turn() -> String:
	app.close_all_modals()
	var st := app.end_turn()
	await _wait()
	var guard := 0
	while st == "battle" and guard < 12:
		guard += 1
		app.close_all_modals()
		app.current_screen()._auto()
		await _wait()
		st = app.end_turn()
		await _wait()
	return st


func _load(path: String) -> bool:
	var f := FileAccess.open(path, FileAccess.READ)
	if f == null:
		return false
	var ok := app.load_game({"data": f.get_as_text(), "name": path.get_file()})
	await _wait(20)
	return ok


func _run() -> void:
	var args := OS.get_cmdline_user_args()
	var phase := str(args[0]) if args.size() > 0 else ""
	var save := str(args[1]) if args.size() > 1 else ""
	out_dir = str(args[2]) if args.size() > 2 else OS.get_user_data_dir() + "/playon"
	DirAccess.make_dir_recursive_absolute(out_dir)
	await _wait(40)
	check(await _load(save), "loaded %s (turn %d)" % [save.get_file(), int(_meta().get("turn", 0))])
	if phase == "a":
		await _phase_a()
	elif phase == "b":
		await _phase_b()
	else:
		check(false, "phase a or b")
	print("PLAYON summary: %d failures" % failures)
	quit(1 if failures > 0 else 0)


func _phase_a() -> void:
	var turn0 := int(_meta().get("turn", 0))
	await _wait(30)
	check(not app.advisor_panel.advisor.tutorial_active() and _button("Skip tutorial") == null, "no tutorial after loading a save")
	# The "Loaded ..." toast is still up here (3.4 s); it must be gone from over the end screen's buttons once the win lands.
	app.toast("Loaded test toast over the button row.", "good", 30.0)
	check(str(_meta().get("outcome", "")) == "ongoing", "the campaign is ongoing before the win")
	await _end_turn()
	await _wait(30)
	check(str(_meta().get("outcome", "")) == "victory", "the end of turn is a victory (outcome %s)" % str(_meta().get("outcome", "")))
	check(root.find_child("EndScreen", true, false) != null, "the end screen is up")
	check(bool(_meta().get("canPlayOn", false)), "the core offers play-on (meta canPlayOn)")
	var cont := _button("Continue playing")
	check(cont != null, "end screen shows Continue playing")
	check(_toast_over(cont) == "", "no toast over the end screen's buttons %s" % _toast_over(cont))
	await _shot("c4-1-domination-end")
	# LAN: the same win in a LAN game offers no Continue playing (the choice would be one machine's alone).
	app.lan.in_game = true
	app.close_all_modals()
	app.open_overlay("end")
	await _wait(10)
	check(root.find_child("EndScreen", true, false) != null and _button("Continue playing") == null, "LAN game: end screen without Continue playing")
	await _shot("c4-2-lan-no-continue")
	app.lan.in_game = false
	app.close_all_modals()
	app.open_overlay("end")
	await _wait(10)
	cont = _button("Continue playing")
	check(cont != null, "Continue playing back once the LAN flag is off")
	if cont == null:
		return
	await _click(cont)
	await _wait(20)
	check(str(_meta().get("outcome", "")) == "ongoing", "after Continue playing the campaign is ongoing")
	check(int(_meta().get("playingOnFrom", 0)) > 0, "playingOnFrom set (%d)" % int(_meta().get("playingOnFrom", 0)))
	check(app.screen == "galaxy" and not app.has_modal(), "back on the galaxy with no modal")
	await _shot("c4-3-continued-galaxy")
	for i in 5:
		var st := await _end_turn()
		check(st == "complete" and str(_meta().get("outcome", "")) == "ongoing", "played-on turn %d resolves and the campaign goes on (%s, %s)" % [int(_meta().get("turn", 0)), st, str(_meta().get("outcome", ""))])
	check(int(_meta().get("turn", 0)) >= turn0 + 6, "turn %d reached after playing on" % int(_meta().get("turn", 0)))
	await _shot("c4-4-played-on-turns")
	var blob := app.session.save_blob()
	var f := FileAccess.open(out_dir + "/after-playon.sav", FileAccess.WRITE)
	f.store_string(str(blob.get("data", "")))
	f.close()
	check(str(blob.get("data", "")).length() > 0, "wrote after-playon.sav")


func _phase_b() -> void:
	check(int(_meta().get("playingOnFrom", 0)) > 0, "the save is a played-on campaign (playingOnFrom %d)" % int(_meta().get("playingOnFrom", 0)))
	await _end_turn()
	await _wait(30)
	check(str(_meta().get("outcome", "")) == "victory", "a later win ends the played-on campaign (outcome %s)" % str(_meta().get("outcome", "")))
	check(root.find_child("EndScreen", true, false) != null, "the end screen is up")
	check(_button("Continue playing") == null and not bool(_meta().get("canPlayOn", true)), "no Continue playing after the conquest win")
	await _shot("c4-5-conquest-end")
