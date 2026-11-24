extends SceneTree
## Onboarding measure (ONBOARDING-AND-MOBILE-PLAN.md section 1.5): a scripted first-time player goes from the title to the
## first End Turn with real input events only, doing exactly what the screen asks, and the run records
##   clicks   mouse clicks from the title to the first End Turn           (target 6 or fewer)
##   words    words of guidance text shown before it: the advisor card, toasts and any dialog, each distinct text once
##            (target under 60)
##   seconds  the run's wall time plus the pacing a person needs: 1.5 s per click (aim and press) and 0.3 s per word
##            read                                                          (target under 60)
## The player takes the defaults: the empire the title shows, no menu browsing. Where the advisor spotlights a control the
## player clicks it; where an old-style card offers a button the player presses it.
## The same script runs on the pre-onboarding code (title New campaign + Launch, then the advisor's buttons), so the before
## and after numbers are the same measure.
##   godot --headless --path remaster/godot --audio-driver Dummy -s res://scenes/ui/tests/test_first_time_drive.gd
## Exit code 0 when all three targets are met, 1 otherwise. Prints one DRIVE line and the click log.

const MAX_CLICKS := 6
const MAX_WORDS := 60
const MAX_SECONDS := 60.0
const SECONDS_PER_CLICK := 1.5
const SECONDS_PER_WORD := 0.3

var app: App
var clicks := 0
var log_lines: Array[String] = []
var seen := {}          ## distinct guidance texts -> true
var words := 0
var failures := 0
var start_turn := 0
var _alnum := RegEx.new()

func _initialize() -> void:
	_run.call_deferred()

func frames(n: int = 2) -> void:
	for k in n:
		await process_frame

func _fail(what: String) -> void:
	failures += 1
	printerr("  FAIL ", what)

## Counts the words of every distinct text the player was shown this frame.
func _harvest() -> void:
	var holders: Array[Node] = []
	if app.advisor_panel != null and app.advisor_panel.visible:
		holders.append(app.advisor_panel)
	var toasts := app.get_node_or_null("Toasts")
	if toasts != null:
		holders.append(toasts)
	for m in app._modals:
		holders.append(m)
	for h in holders:
		for n in h.find_children("*", "", true, false):
			if not (n is Control) or not (n as Control).is_visible_in_tree():
				continue
			var txt := ""
			if n is Label:
				txt = (n as Label).text
			elif n is Button:
				txt = (n as Button).text
			elif n is RichTextLabel:
				txt = (n as RichTextLabel).get_parsed_text()
			txt = txt.strip_edges()
			if txt == "" or seen.has(txt):
				continue
			seen[txt] = true
			var w := 0
			for tok in txt.split(" ", false):
				if _alnum.search(String(tok)) != null:   # a token with a letter or digit, not a lone mark
					w += 1
			words += w

func _click_at(at: Vector2, what: String) -> void:
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
	clicks += 1
	log_lines.append("  click %d: %s" % [clicks, what])
	await frames(2)
	if app.view.is_empty() or int(app.view["meta"]["turn"]) == start_turn:
		_harvest()   # what the click ended is the next turn's: the log after the first End Turn is not counted

func _click(c: Control, what: String) -> void:
	await _click_at(c.get_global_transform_with_canvas() * (c.size * 0.5), what)

func _run() -> void:
	_alnum.compile("[A-Za-z0-9]")
	OS.set_environment("BOTF_TITLE_EMPIRE", "1")
	var backend := "native" if Engine.has_singleton("BotfCore") else "fixture"
	App.forced_backend = "fixture" if backend == "fixture" else ""
	root.size = Vector2i(1920, 1080)
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	await frames(6)
	print("first-time drive (%s backend)" % backend)
	var title: Control = app.current_screen()
	if title == null or app.screen != "title":
		_fail("the app opens on the title")
		return _finish(0.0)
	# A deterministic galaxy for the run; the player has no say in the seed either way.
	if "quick_seed" in title:
		title.quick_seed = 31337
	var t0 := Time.get_ticks_msec()
	var quick: Button = title.find_child("QuickStart", true, false) as Button
	if quick != null:
		await _click(quick, "Quick Start")
	else:
		var nb: Button = title.find_child("NewCampaign", true, false) as Button
		await _click(nb, "New campaign")
		var nc := app.current_screen()
		nc.seed_value = 31337
		var launch: Button = nc.find_child("Launch", true, false) as Button
		await _click(launch, "Launch campaign")
	await frames(8)
	if app.view.is_empty():
		_fail("the campaign started")
		return _finish(float(Time.get_ticks_msec() - t0) / 1000.0)
	start_turn = int(app.view["meta"]["turn"])
	var steps := 0
	while int(app.view["meta"]["turn"]) == start_turn and steps < 30:
		steps += 1
		_harvest()
		var panel := app.advisor_panel
		panel.update()
		await frames(2)
		_harvest()
		var g: Dictionary = panel.current
		if app.has_modal() and not panel.visible:
			_fail("a dialog is open and no guidance is shown (step %d)" % steps)
			break
		if g.is_empty():
			var et: Button = app.find_child("EndTurn", true, false) as Button
			if et == null:
				_fail("no guidance and no End turn button (step %d)" % steps)
				break
			await _click(et, "End turn (no guidance)")
			continue
		if g.get("beat", false):
			var rects := panel.target_rects()
			if rects.is_empty():
				_fail("beat '%s' spotlights nothing (screen %s)" % [str(g.get("id", "")), app.screen])
				break
			await _click_at(rects[0].get_center(), "%s -> %s" % [str(g.get("id", "")), str(g.get("text", ""))])
		else:
			var btn: Button = panel.find_child("AdvisorAct", true, false) as Button
			if btn == null:
				_fail("guidance '%s' has no action to take" % str(g.get("id", "")))
				break
			await _click(btn, "%s -> %s" % [str(g.get("id", "")), str(g.get("act_label", ""))])
	if int(app.view["meta"]["turn"]) == start_turn:
		_fail("the first End Turn was reached in %d steps" % steps)
	_finish(float(Time.get_ticks_msec() - t0) / 1000.0)

func _finish(wall: float) -> void:
	var model := float(clicks) * SECONDS_PER_CLICK + float(words) * SECONDS_PER_WORD
	var total := wall + model
	for l in log_lines:
		print(l)
	for k in seen.keys():
		print("  shown: ", str(k).replace("
", " / "))
	print("DRIVE clicks=%d words=%d wall_s=%.1f paced_s=%.1f total_s=%.1f" % [clicks, words, wall, model, total])
	var ok_clicks := clicks <= MAX_CLICKS
	var ok_words := words < MAX_WORDS
	var ok_time := total < MAX_SECONDS
	print("  %s clicks %d (target %d or fewer)" % ["ok  " if ok_clicks else "MISS", clicks, MAX_CLICKS])
	print("  %s words %d (target under %d)" % ["ok  " if ok_words else "MISS", words, MAX_WORDS])
	print("  %s time %.1f s (target under %.0f s)" % ["ok  " if ok_time else "MISS", total, MAX_SECONDS])
	var bad := failures > 0 or not (ok_clicks and ok_words and ok_time)
	print("DRIVE %s" % ("FAIL" if bad else "PASS"))
	quit(1 if bad else 0)
