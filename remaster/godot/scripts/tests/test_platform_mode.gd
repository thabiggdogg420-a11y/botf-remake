extends SceneTree
## Mobile mode, stage M-A: mode detection (query string, remembered choice, touch + narrow auto-detect; the same cases the page's
## JavaScript is tested against), the phone UI scale, tap-as-click, long-press tooltips, pinch, bloom off, the slow-frame
## render scale; and that desktop mode is left exactly as it was.
##   godot --headless --path remaster/godot --audio-driver Dummy -s res://scripts/tests/test_platform_mode.gd
## Exit code 0 on success, 1 on any failure.

var failures := 0
var checks := 0

func check(cond: bool, what: String) -> void:
	checks += 1
	if cond:
		print("  ok  ", what)
	else:
		failures += 1
		printerr("  FAIL ", what)

func frames(n := 3) -> void:
	for k in n:
		await process_frame

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	_test_decide()
	_test_sources()
	_test_scale_and_render_logic()
	await _test_gestures()
	await _test_desktop_untouched()
	await _test_touch_click_and_tooltip()
	await _test_app_in_mobile()
	print("%d checks, %d failed" % [checks, failures])
	quit(1 if failures > 0 else 0)


# ---- 1. mode detection ---------------------------------------------------------------------------------

func _test_decide() -> void:
	var cases: Array = []
	var doc: Variant = JSON.parse_string(FileAccess.get_file_as_string("res://fixtures/platform-mode-cases.json"))
	check(doc is Dictionary and doc.get("cases", []).size() >= 15, "the shared case table loads")
	cases = doc["cases"]
	for c in cases:
		var d := PlatformMode.decide(str(c["query"]), str(c["stored"]), bool(c["touch"]), int(c["short"]))
		check(d["mode"] == c["mode"] and d["source"] == c["source"], "decide: %s -> %s (%s)" % [c["name"], d["mode"], d["source"]])


func _test_sources() -> void:
	check(PlatformMode.from_args(["--mobile"]) == "mobile", "--mobile on the command line")
	check(PlatformMode.from_args(["--desktop"]) == "desktop", "--desktop on the command line")
	check(PlatformMode.from_args(["--platform-mode=mobile"]) == "mobile", "--platform-mode=mobile")
	check(PlatformMode.from_args(["--platform-mode=tablet"]) == "", "an unknown --platform-mode value is ignored")
	check(PlatformMode.from_args(["--mobile", "--desktop"]) == "desktop", "the last switch wins")
	check(PlatformMode.from_args(["--audio-driver", "Dummy"]) == "", "no switch, no mode")
	# The PC setting (Settings, "auto" | "mobile" | "desktop"), with no switch and no environment variable set.
	PlatformMode.reset()
	if OS.get_environment("BOTF_MODE") == "" and PlatformMode.from_args(OS.get_cmdline_args()) == "":
		PlatformMode.init_from({"platformMode": "mobile"})
		check(PlatformMode.is_mobile() and PlatformMode.source() == "setting", "the platformMode setting 'mobile' turns mobile on")
		PlatformMode.init_from({"platformMode": "desktop"})
		check(not PlatformMode.is_mobile(), "the setting 'desktop' keeps desktop")
		PlatformMode.init_from({})
		check(not PlatformMode.is_mobile() and PlatformMode.source() == "auto", "no setting: Auto is desktop on a PC")
	# The web route, with the page's inputs supplied (what PlatformMode reads from JavaScript in the browser).
	PlatformMode.reset()
	PlatformMode.web_inputs = {"query": "", "stored": "", "touch": true, "short": 412}
	check(PlatformMode.is_mobile() and PlatformMode.source() == "auto", "web: touch + a 412 px screen is mobile")
	PlatformMode.reset()
	PlatformMode.web_inputs = {"query": "desktop", "stored": "mobile", "touch": true, "short": 412}
	check(not PlatformMode.is_mobile() and PlatformMode.source() == "query", "web: ?mode=desktop beats the stored choice")
	PlatformMode.reset()


# ---- 2. scale and render-scale logic ----------------------------------------------------------------------

func _test_scale_and_render_logic() -> void:
	check(is_equal_approx(PlatformMode.ui_factor(390.0, 1.0), clampf(PlatformMode.UI_TARGET * 1080.0 / 390.0, 1.0, 3.0)), "a 390 px tall phone window gets a %.2fx UI factor" % PlatformMode.ui_factor(390.0))
	check(PlatformMode.ui_factor(390.0) > 1.7, "that factor is large enough to read (>1.7)")
	check(is_equal_approx(PlatformMode.ui_factor(390.0 * 3.0, 3.0), PlatformMode.ui_factor(390.0, 1.0)), "a 3x device pixel ratio phone gets the same factor as its CSS size")
	check(PlatformMode.ui_factor(1080.0) == 1.0, "a 1080 px window needs no extra scale")
	check(PlatformMode.ui_factor(2160.0) == 1.0, "a big window never shrinks the UI (min 1.0)")
	check(PlatformMode.ui_factor(0.0) == 1.0, "no window size, no scale")
	check(PlatformMode.ui_factor(120.0) <= PlatformMode.UI_FACTOR_MAX, "a tiny window is capped")
	check(PlatformRuntime.next_scale(60.0, 1.0) < 1.0, "a slow frame average (60 ms) lowers the render scale")
	check(PlatformRuntime.next_scale(16.0, 1.0) == 1.0, "a fast average (16 ms) leaves it alone")
	var s := 1.0
	for i in 20:
		s = PlatformRuntime.next_scale(80.0, s)
	check(is_equal_approx(s, PlatformRuntime.RENDER_MIN), "it stops at the floor (%.2f)" % s)
	PlatformMode.reset()
	PlatformMode.force("mobile")
	PlatformMode.render_cap = 0.7
	check(is_equal_approx(PlatformMode.effective_render_scale(1.0), 0.7), "mobile: the render scale is capped by the slow-frame cap")
	check(is_equal_approx(PlatformMode.effective_render_scale(0.5), 0.5), "mobile: a lower setting stays lower")
	PlatformMode.force("desktop")
	check(is_equal_approx(PlatformMode.effective_render_scale(1.0), 1.0), "desktop: the cap is ignored")
	check(PlatformMode.glow_allowed() == true, "desktop: bloom allowed")
	PlatformMode.force("mobile")
	check(PlatformMode.glow_allowed() == false, "mobile: bloom off")
	PlatformMode.reset()


# ---- 3. finger gestures ---------------------------------------------------------------------------------------

func _touch(idx: int, pos: Vector2, down: bool) -> InputEventScreenTouch:
	var e := InputEventScreenTouch.new()
	e.index = idx
	e.position = pos
	e.pressed = down
	return e

## A finger going down or up, as the engine delivers it: the touch event, and (finger 0, Input.emulate_mouse_from_touch) the
## mouse button it stands for. Headless has no display server, so Input.parse_input_event never dispatches; the events are
## pushed into the window the way the engine would deliver them.
func _finger(idx: int, pos: Vector2, down: bool) -> void:
	root.push_input(_touch(idx, pos, down), true)
	if idx == 0 and Input.emulate_mouse_from_touch:
		var m := InputEventMouseButton.new()
		m.button_index = MOUSE_BUTTON_LEFT
		m.pressed = down
		m.position = pos
		m.global_position = pos
		m.button_mask = MOUSE_BUTTON_MASK_LEFT if down else 0
		root.push_input(m, true)

func _move(idx: int, pos: Vector2, rel: Vector2) -> void:
	root.push_input(_drag(idx, pos, rel), true)

func _drag(idx: int, pos: Vector2, rel := Vector2.ZERO) -> InputEventScreenDrag:
	var e := InputEventScreenDrag.new()
	e.index = idx
	e.position = pos
	e.relative = rel
	return e

func _test_gestures() -> void:
	var g := TouchGestures.new()
	var fired := []
	g.long_press.connect(func(p: Vector2): fired.append(p))
	g.feed(_touch(0, Vector2(100, 100), true))
	g.tick(0.3)
	check(fired.is_empty(), "no long-press at 0.3 s")
	g.tick(0.3)
	check(fired.size() == 1 and fired[0] == Vector2(100, 100), "a finger held still 0.6 s long-presses at its position")
	g.tick(1.0)
	check(fired.size() == 1, "one long-press per touch")
	g.feed(_touch(0, Vector2(100, 100), false))
	g.feed(_touch(0, Vector2(50, 50), true))
	g.feed(_drag(0, Vector2(120, 50), Vector2(70, 0)))
	g.tick(0.9)
	check(fired.size() == 1, "a finger that moved is a drag, not a long-press")
	g.feed(_touch(0, Vector2(50, 50), false))
	# pinch
	var pinches := []
	g.pinch.connect(func(f: float, c: Vector2): pinches.append([f, c]))
	g.feed(_touch(0, Vector2(100, 100), true))
	g.feed(_touch(1, Vector2(200, 100), true))
	g.feed(_drag(1, Vector2(300, 100)))
	check(pinches.size() == 1 and is_equal_approx(pinches[0][0], 2.0), "fingers moving 100 -> 200 px apart report a 2.0x pinch")
	check(pinches[0][1] == Vector2(200, 100), "centred between the fingers")
	g.feed(_drag(1, Vector2(250, 100)))
	check(is_equal_approx(pinches[1][0], 150.0 / 200.0), "the next step is relative to the last (0.75x)")
	g.tick(2.0)
	check(fired.size() == 1, "two fingers never long-press")
	g.feed(_touch(1, Vector2(250, 100), false))
	check(not g.is_pinching(), "lifting a finger ends the pinch")


# ---- 4. desktop is untouched -----------------------------------------------------------------------------------

func _test_desktop_untouched() -> void:
	PlatformMode.force("desktop")
	root.size = Vector2i(1920, 1080)
	var aspect_before := root.content_scale_aspect
	PlatformMode.apply_window(root, 1.25)
	check(is_equal_approx(root.content_scale_factor, 1.25), "desktop: the window scale is exactly the player's own setting")
	check(root.content_scale_aspect == aspect_before, "desktop: the scale aspect is not touched")
	var holder := Node.new()
	root.add_child(holder)
	PlatformMode.sync_runtime(holder)
	check(holder.get_node_or_null("PlatformRuntime") == null, "desktop: no mobile runtime is added")
	holder.queue_free()
	root.content_scale_factor = 1.0
	PlatformMode.reset()


# ---- 5. tap = click, long-press = tooltip, pinch reaches the control under the fingers -----------------------------

class PinchBox extends Control:
	var got := []
	func on_pinch(f: float, c: Vector2) -> void:
		got.append([f, c])

func _test_touch_click_and_tooltip() -> void:
	PlatformMode.force("mobile")
	root.size = Vector2i(844, 390)
	PlatformMode.apply_window(root, 1.0)
	check(Input.emulate_mouse_from_touch, "mobile: touch emulates the mouse (a tap is a click)")
	check(root.content_scale_aspect == Window.CONTENT_SCALE_ASPECT_EXPAND, "mobile: extra screen width goes to the layout, not bars")
	check(is_equal_approx(root.content_scale_factor, PlatformMode.ui_factor(390.0)), "mobile: the content scale is the phone factor (%.2f)" % root.content_scale_factor)
	var holder := Control.new()
	holder.set_anchors_preset(Control.PRESET_FULL_RECT)
	root.add_child(holder)
	var clicks := [0]
	var b := Button.new()
	b.text = "Build"
	b.position = Vector2(100, 100)
	b.custom_minimum_size = Vector2(160, 60)
	b.tooltip_text = "Queues a ship at this system."
	b.pressed.connect(func(): clicks[0] += 1)
	holder.add_child(b)
	var noTip := Button.new()
	noTip.text = "Plain"
	noTip.position = Vector2(100, 220)
	noTip.custom_minimum_size = Vector2(160, 60)
	noTip.pressed.connect(func(): clicks[0] += 100)
	holder.add_child(noTip)
	var pb := PinchBox.new()
	pb.position = Vector2(400, 50)
	pb.size = Vector2(300, 200)
	holder.add_child(pb)
	PlatformMode.sync_runtime(holder)
	await frames(3)
	var rt: PlatformRuntime = holder.get_node_or_null("PlatformRuntime")
	check(rt != null, "mobile: the runtime (gestures, tooltips, render scale) is added")
	if rt == null:
		return
	# A tap: touch down and up on the button, through the engine's touch-to-mouse step.
	var at: Vector2 = b.get_global_transform_with_canvas() * (b.size * 0.5)
	_finger(0, at, true)
	await frames(2)
	_finger(0, at, false)
	await frames(3)
	check(clicks[0] == 1, "a tap on a button clicks it (clicks=%d)" % clicks[0])
	check(not rt.tooltip_open(), "a quick tap opens no tooltip")
	# A long-press on the button with a tooltip: the tooltip opens, the lift does not click.
	_finger(0, at, true)
	var t0 := Time.get_ticks_msec()
	while not rt.tooltip_open() and Time.get_ticks_msec() - t0 < 4000:
		await frames(1)
	check(rt.tooltip_open(), "a long-press on a control with a tooltip opens it")
	await frames(3)
	check(rt.tooltip_text() == "Queues a ship at this system.", "it shows that control's tooltip text: '%s'" % rt.tooltip_text())
	_finger(0, at, false)
	await frames(4)
	check(clicks[0] == 1, "lifting after a long-press does not also click (clicks=%d)" % clicks[0])
	# The next touch closes the tooltip and clicks normally.
	_finger(0, at, true)
	await frames(2)
	check(not rt.tooltip_open(), "touching again closes the tooltip")
	_finger(0, at, false)
	await frames(3)
	check(clicks[0] == 2, "and that tap clicks (clicks=%d)" % clicks[0])
	# A long-press where there is no tooltip: nothing opens, and the lift still clicks as a tap.
	var at2: Vector2 = noTip.get_global_transform_with_canvas() * (noTip.size * 0.5)
	_finger(0, at2, true)
	await create_timer(0.8).timeout
	await frames(2)
	check(not rt.tooltip_open(), "a long-press on a control without a tooltip opens nothing")
	_finger(0, at2, false)
	await frames(3)
	check(clicks[0] == 102, "a slow tap on it still clicks (clicks=%d)" % clicks[0])
	# Lookup helpers: the topmost control with a tooltip, climbing parents.
	check(PlatformRuntime.control_at(root, at) == b, "control_at finds the button under a point")
	check(PlatformRuntime.tooltip_owner(b, at) == b, "tooltip_owner is the button itself")
	check(PlatformRuntime.tooltip_owner(noTip, at2) == null, "no tooltip anywhere above a plain button")
	# Pinch: delivered to the control under the fingers' centre that defines on_pinch.
	var c0: Vector2 = pb.get_global_transform_with_canvas() * Vector2(100, 100)
	_finger(0, c0, true)
	_finger(1, c0 + Vector2(100, 0), true)
	await frames(2)
	_move(1, c0 + Vector2(200, 0), Vector2(100, 0))
	await frames(2)
	check(pb.got.size() >= 1 and is_equal_approx(pb.got[0][0], 2.0), "a pinch reaches the control under the fingers (factor %s)" % [pb.got[0][0] if pb.got.size() > 0 else "none"])
	_finger(1, c0 + Vector2(200, 0), false)
	_finger(0, c0, false)
	await frames(2)
	# Drag-to-scroll: a one-finger drag that starts on a button inside a scroller scrolls it and clicks nothing; a slider keeps its drag.
	var sc := ScrollContainer.new()
	sc.position = Vector2(800, 40)
	sc.custom_minimum_size = Vector2(300, 200)
	sc.size = Vector2(300, 200)
	holder.add_child(sc)
	var col := VBoxContainer.new()
	col.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	sc.add_child(col)
	var inside := [0]
	for i in 10:
		var rb := Button.new()
		rb.text = "Row %d" % i
		rb.custom_minimum_size = Vector2(0, 60)
		rb.pressed.connect(func(): inside[0] += 1)
		col.add_child(rb)
	var slider := HSlider.new()
	slider.custom_minimum_size = Vector2(280, 40)
	col.add_child(slider)
	await frames(4)
	check(PlatformRuntime.can_scroll(sc, false) and not PlatformRuntime.can_scroll(sc, true), "the scroller can scroll down, not sideways")
	var p0: Vector2 = sc.get_global_transform_with_canvas() * Vector2(150, 100)
	_finger(0, p0, true)
	await frames(1)
	for k in 8:
		_move(0, p0 - Vector2(0, 25.0 * (k + 1)), Vector2(0, -25))
		await frames(1)
	_finger(0, p0 - Vector2(0, 200), false)
	await frames(4)
	check(sc.scroll_vertical > 100, "a drag that began on a button scrolls the list (scroll_vertical=%d)" % sc.scroll_vertical)
	check(inside[0] == 0, "and the lift does not click the row it began on (clicks=%d)" % inside[0])
	var before_scroll := sc.scroll_vertical
	sc.scroll_vertical = int(sc.get_v_scroll_bar().max_value)
	await frames(2)
	var sp: Vector2 = slider.get_global_transform_with_canvas() * Vector2(100, 20)
	_finger(0, sp, true)
	await frames(1)
	var v0 := sc.scroll_vertical
	for k in 6:
		_move(0, sp + Vector2(0, -25.0 * (k + 1)), Vector2(0, -25))
		await frames(1)
	_finger(0, sp + Vector2(0, -150), false)
	await frames(3)
	check(sc.scroll_vertical == v0, "a drag that begins on a slider does not scroll the list (it is the slider's)")
	check(before_scroll > 0, "(scroll worked before the slider drag)")
	holder.queue_free()
	await frames(2)
	root.content_scale_factor = 1.0
	root.content_scale_aspect = Window.CONTENT_SCALE_ASPECT_KEEP
	PlatformMode.reset()


# ---- 6. the real app in mobile mode --------------------------------------------------------------------------

func _test_app_in_mobile() -> void:
	OS.set_environment("BOTF_MODE", "mobile")
	PlatformMode.reset()
	App.forced_backend = "fixture"
	root.size = Vector2i(844, 390)
	var app: App = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	await frames(4)
	check(PlatformMode.is_mobile() and PlatformMode.source() == "env", "the app starts in mobile mode from BOTF_MODE")
	check(root.content_scale_factor > 1.7, "app: the UI scale is up (%.2f)" % root.content_scale_factor)
	check(app.get_node_or_null("PlatformRuntime") != null, "app: the mobile runtime is running")
	app.start_campaign({"faction": 1, "seed": 424242, "era": 3, "ruleset": "reference", "width": 22, "height": 16, "systems": 48, "preset": "remastered", "events": true, "aiAggression": true})
	app.close_all_modals()
	app.goto("galaxy")
	await frames(6)
	var gs := app.current_screen()
	var map: GalaxyMap = gs.map
	check(map != null and map._env.glow_enabled == false, "app: bloom (glow) is off on the star chart")
	# Pinch zooms the chart about the fingers; a long-press on a star opens its order menu.
	var z0 := map.zoom
	map.on_pinch(1.5, map.size / 2.0)
	check(map.zoom > z0 * 1.4, "the chart zooms on a pinch (%.2f -> %.2f)" % [z0, map.zoom])
	var menu := [-2]
	map.cell_context.connect(func(cell: int, _p: Vector2): menu[0] = cell)
	var star_pos := Vector2(-1, -1)
	var star_cell := -1
	for sid in map.systems3d:
		var p := map.world_to_screen(map.systems3d[sid]["pos"])
		if Rect2(Vector2.ZERO, map.size).grow(-40).has_point(p) and map.cell_at(p) >= 0:
			star_pos = p
			star_cell = map.cell_at(p)
			break
	check(star_cell >= 0, "a star is on screen to press")
	if star_cell >= 0:
		check(map.on_long_press(star_pos) and menu[0] == star_cell, "a long-press on a star opens its order menu, as a right-click does (cell %d)" % star_cell)
	var mv := InputEventMouseMotion.new()
	mv.position = star_pos
	var hovered := [-2]
	map.hover_changed.connect(func(cell: int, _p: Vector2): hovered[0] = cell)
	map._gui_input(mv)
	check(hovered[0] == -2, "a moving finger raises no hover card (no hover on a phone)")
	# The chrome fits a phone window: End turn and Menu are on screen (they were pushed off the right edge by the figures), the rail scrolls.
	var win := root.get_visible_rect()
	var endb: Control = app.find_child("EndTurn", true, false)
	check(endb != null and win.encloses(Rect2(endb.get_global_transform_with_canvas().origin, endb.size)), "End turn is on screen in a phone window (%s in %s)" % [endb.get_global_transform_with_canvas().origin if endb else "-", win.size])
	check(app.find_child("StatsScroll", true, false) != null, "the top bar's figures scroll so End turn stays in view")
	check(app.find_child("RailScroll", true, false) != null, "the rail scrolls (ten tabs are taller than a phone)")
	# One-finger drag pans; the chart's tap threshold is wider for a fingertip.
	check(map._tap_slop() > 6.0, "the chart's tap slop is wider on a phone")
	await frames(2)
	# A screen wider than the window pans instead of running off it; the chart and battle stay fixed.
	app.goto("research")
	await frames(8)
	var rs := app.current_screen()
	var wrap := rs.get_parent() as ScrollContainer
	check(wrap != null and wrap.has_meta("pan_wrap"), "the research screen is in a pan wrapper (its content is wider than a phone window)")
	if wrap != null:
		check(rs.custom_minimum_size.x > wrap.size.x, "its content (%d) is wider than the window (%d): drag scrolls it" % [rs.custom_minimum_size.x, wrap.size.x])
	app.goto("galaxy")
	await frames(8)
	check(app.current_screen().get_parent() == app.get_node("Stage"), "the star chart is not wrapped (it takes the drags itself)")
	check(app.get_node("Stage").find_child("PanWrap", true, false) == null, "the research pan wrapper went with its screen")
	app.queue_free()
	await frames(3)
	root.content_scale_factor = 1.0
	root.content_scale_aspect = Window.CONTENT_SCALE_ASPECT_KEEP
	OS.unset_environment("BOTF_MODE")
	PlatformMode.reset()
