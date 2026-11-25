extends SceneTree
## The Quick Start tutorial in Mobile mode (onboarding lane + mobile lane, together). Plays the five beats from the title with
## real touch taps on whatever the spotlight lights, in a phone-sized window (844x390 and 915x412), and checks that every
## beat's target is on screen, reachable by a tap (the beat advances), and that the beat line is readable (on-screen font size).
## Then the same beats on a desktop window with mouse clicks, and a Custom game (no dimming, no tutorial).
##   godot --headless --path remaster/godot --audio-driver Dummy -s res://scenes/ui/tests/test_mobile_beats.gd
## Exit code 0 on success, 1 on any failure.

var app: App
var failures := 0
var checks := 0

func check(cond: bool, what: String) -> void:
	checks += 1
	if cond:
		print("  ok  ", what)
	else:
		failures += 1
		printerr("  FAIL ", what)

func frames(n: int = 4) -> void:
	for k in n:
		await process_frame

func _initialize() -> void:
	_run.call_deferred()

func _new_app(size_: Vector2i, mobile: bool) -> void:
	if app:
		app.queue_free()
		await frames()
	App.forced_backend = "" if Engine.has_singleton("BotfCore") else "fixture"
	if mobile:
		PlatformMode.force("mobile")
	else:
		PlatformMode.force("desktop")
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	await frames(6)
	app.settings["tutorial"] = 0
	app.settings.erase("tutorialBeats")

func _run() -> void:
	var native := Engine.has_singleton("BotfCore")
	print("mobile beats (%s backend)" % ("native" if native else "fixture"))
	# The window is the one the engine was started with (--resolution): a headless window cannot be resized by a script.
	# User args: [mobile|desktop]. Default: whatever size this window is, in the mode its size suggests.
	var args := Array(OS.get_cmdline_user_args())
	var mobile := (str(args[0]) == "mobile") if args.size() > 0 else (root.size.x < 1200)
	await _quick_start_beats(root.size, mobile)
	await _custom_game()
	print("%d checks, %d failures" % [checks, failures])
	quit(1 if failures > 0 else 0)

func _tap(logical: Vector2, mobile: bool) -> void:
	var win := root.get_final_transform() * logical
	if mobile:
		for down in [true, false]:
			var e := InputEventScreenTouch.new()
			e.index = 0
			e.position = win
			e.pressed = down
			Input.parse_input_event(e)
			await frames(3)
	else:
		var mm := InputEventMouseMotion.new()
		mm.position = logical
		mm.global_position = logical
		root.push_input(mm, true)
		await frames(2)
		for down in [true, false]:
			var e := InputEventMouseButton.new()
			e.button_index = MOUSE_BUTTON_LEFT
			e.pressed = down
			e.position = logical
			e.global_position = logical
			e.button_mask = MOUSE_BUTTON_MASK_LEFT if down else 0
			root.push_input(e, true)
			await frames(3)
	await frames(8)

## The first ancestor whose clipping cuts the control's centre off (a ScrollContainer or clip_contents Control), or "".
func _clipped_by(c: Control) -> String:
	var centre := c.get_global_rect().get_center()
	var p := c.get_parent()
	while p != null and p != app:
		if p is Control and ((p as Control).clip_contents or p is ScrollContainer) and not (p as Control).get_global_rect().has_point(centre):
			return str(p.name)
		p = p.get_parent()
	return ""

func _where(c: Control) -> String:
	var s := ""
	var p: Node = c
	while p != null and p != app:
		if p is ScrollContainer:
			s += " [scroll %s at %s size %s v=%d]" % [p.name, str((p as Control).get_global_rect().position), str((p as Control).size), (p as ScrollContainer).scroll_vertical]
		p = p.get_parent()
	return s

func _quick_start_beats(size_: Vector2i, mobile: bool) -> void:
	var tag := "%dx%d %s" % [size_.x, size_.y, "mobile" if mobile else "desktop"]
	print("== Quick Start, ", tag)
	await _new_app(size_, mobile)
	app.goto("title", false)
	await frames(4)
	var title := app.current_screen()
	title.quick_seed = 31337
	title.quick_start()
	await frames(10)
	check(app.tutorial_game, tag + ": Quick Start is the tutorial game")
	var adv := app.advisor_panel.advisor
	var win := Rect2(Vector2.ZERO, root.get_visible_rect().size)
	print("  window %s, content scale %.2f (%s), visible %s, mobile %s" % [str(root.size), root.content_scale_factor, str(root.content_scale_mode), str(win.size), str(PlatformMode.is_mobile())])
	var seen := {}
	var guard := 0
	while adv.tutorial_active() and guard < 30:
		guard += 1
		app.advisor_panel.update()
		await frames(10)
		app.advisor_panel.update()   # the layout may have moved (a screen that pans): the panel's own poll does the same
		await frames(30)
		var step := adv.tutorial_step()
		var id := str(adv.STEPS[step]["id"]) if step >= 0 and step < adv.STEPS.size() else "?"
		var rects := app.advisor_panel.target_rects()
		if rects.is_empty() and app.screen != "galaxy":
			app.goto("galaxy")   # the colony ship's beat points at the star chart
			await frames(30)
			app.advisor_panel.update()
			await frames(20)
			rects = app.advisor_panel.target_rects()
		if rects.is_empty():
			check(false, "%s: beat %d (%s) has a spotlit target" % [tag, step + 1, id])
			break
		var r: Rect2 = rects[0]
		var key := "%d" % step
		var on_screen := win.encloses(r) or (win.has_point(r.get_center()) and win.intersection(r).get_area() >= 0.6 * r.get_area())
		check(on_screen, "%s: beat %d (%s) target is on screen (rect %s, window %s)" % [tag, step + 1, id, str(r), str(win.size)])
		if not on_screen:
			var host: Node = app.current_screen()
			print("    target lies off screen; scrolls above it:")
			for n in adv.targets(app.advisor_panel.current):
				if not n is Rect2:
					var c: Control = app.advisor_panel._find_target(host, str(n))
					print("      ", n, " ", _where(c) if c else "(not found)")
			break
		if not seen.has(key):
			seen[key] = true
			# the beat line is readable: its font, as the player sees it, is at least 11 CSS px (UI scale and device pixel ratio counted)
			var fs := _beat_font_px()
			check(fs >= 11.0, "%s: beat %d line is readable (%.1f px on screen)" % [tag, step + 1, fs])
		var before := step
		var tries := 0
		while adv.tutorial_step() == before and tries < 8:
			tries += 1
			app.advisor_panel.update()
			await frames(6)
			var rs := app.advisor_panel.target_rects()
			if rs.is_empty():
				break
			var rr: Rect2 = rs[0]
			if not (win.has_point(rr.get_center()) and win.intersection(rr).get_area() >= 0.6 * rr.get_area()):
				check(false, "%s: beat %d (%s) second target is on screen (rect %s)" % [tag, before + 1, id, str(rr)])
				var host2: Node = app.current_screen()
				for n2 in adv.targets(app.advisor_panel.current):
					if not n2 is Rect2:
						var c2: Control = app.advisor_panel._find_target(host2, str(n2))
						print("      ", n2, " ", _where(c2) if c2 else "(not found)")
				break
			await _tap(rr.get_center(), mobile)
			if app.screen == "combat":
				break
		check(adv.tutorial_step() != before or not adv.tutorial_active(), "%s: beat %d (%s) was done by tapping what the spotlight lit (%d taps)" % [tag, before + 1, id, tries])
		if adv.tutorial_step() == before and adv.tutorial_active():
			var host3: Node = app.current_screen()
			var rsx := app.advisor_panel.target_rects()
			if not rsx.is_empty():
				var mm2 := InputEventMouseMotion.new()
				mm2.position = rsx[0].get_center()
				mm2.global_position = mm2.position
				root.push_input(mm2, true)
				await frames(3)
				var hov := root.gui_get_hovered_control()
				print("    control under the target's centre: ", (str(hov.get_path()) + " " + str(hov.get_global_rect()) + " filter " + str(hov.mouse_filter)) if hov else "none")
			print("    stuck: screen ", app.screen, ", spotlight rects ", app.advisor_panel.target_rects())
			for n3 in adv.targets(app.advisor_panel.current):
				if not n3 is Rect2:
					var c3: Control = app.advisor_panel._find_target(host3, str(n3))
					print("      ", n3, " ", (str(c3.get_global_rect()) + _where(c3) + " clip " + str(_clipped_by(c3))) if c3 else "(not found)")
			break
	check(not adv.tutorial_active() or guard < 30, tag + ": the five beats finish")

## The on-screen height of the beat card's text in CSS px: its font size times the content scale, over the page's device pixel ratio.
func _beat_font_px() -> float:
	var card := app.advisor_panel
	for l in card.find_children("*", "Label", true, false):
		var lab := l as Label
		if lab.visible and lab.text.length() > 20:
			var fs := float(lab.get_theme_font_size("font_size"))
			return fs * root.content_scale_factor
	return 0.0

func _custom_game() -> void:
	print("== Custom game, window ", root.size)
	await _new_app(root.size, PlatformMode.is_mobile())
	app.goto("title", false)
	await frames(3)
	app.goto("new", false)
	await frames(4)
	var ncs := app.current_screen()
	ncs.faction = 1
	ncs.seed_value = 31337
	ncs.start()
	await frames(10)
	check(not app.tutorial_game, "Custom game: not the tutorial game")
	var dimmed := 0
	for id in TabReveal.TABS:
		if TabReveal.dimmed(app, id):
			dimmed += 1
	check(dimmed == 0, "Custom game: no tab is dimmed")
	var rail_ok := true
	for b in app.find_children("Rail_*", "Button", true, false):
		rail_ok = rail_ok and (b as Button).modulate.a > 0.99
	check(rail_ok, "Custom game: every side tab at full strength (no dimming, no new marks)")
	check(app.find_children("NewMark", "", true, false).is_empty(), "Custom game: no new marks")
