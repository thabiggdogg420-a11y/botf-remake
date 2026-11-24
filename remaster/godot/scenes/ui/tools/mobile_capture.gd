extends SceneTree
## Frames of the game in phone mode (mobile stage M-A) and, with BOTF_MODE=desktop, the same frames in desktop mode. Needs a window:
##   BOTF_MODE=mobile godot --path remaster/godot --resolution 844x390 --audio-driver Dummy -s res://scenes/ui/tools/mobile_capture.gd -- <out_dir> <tag> [shot ...]
## Shots: title galaxy system research tooltip ordermenu pinch   (default: all). Touches are real engine input events
## (Input.parse_input_event of InputEventScreenTouch, which the engine turns into mouse clicks), not clicks on nodes.
## Writes <out_dir>/<tag>-<shot>.png at the window's own size and prints "saved <path>".

var app: App
var out_dir := ""
var tag := "m"
var shots: Array = []

func _initialize() -> void:
	var args := Array(OS.get_cmdline_user_args())
	out_dir = str(args[0]) if args.size() > 0 else "user://mobile-frames"
	tag = str(args[1]) if args.size() > 1 else "m"
	shots = args.slice(2) if args.size() > 2 else ["title", "galaxy", "tooltip", "ordermenu", "pinch", "system", "research"]
	DirAccess.make_dir_recursive_absolute(out_dir)
	_run.call_deferred()

func _wait(n := 8) -> void:
	for i in n:
		await process_frame

func _save(shot: String) -> void:
	if shots.has("notut"):
		shot += "-notut"
	await _wait(10)
	var img := root.get_viewport().get_texture().get_image()
	var path := "%s/%s-%s.png" % [out_dir, tag, shot]
	img.save_png(path)
	print("saved ", path, " ", img.get_size(), "  mode=", PlatformMode.mode(), " content_scale=", snappedf(root.content_scale_factor, 0.01), " logical=", root.get_visible_rect().size)

func _touch(idx: int, pos: Vector2, down: bool) -> void:
	var e := InputEventScreenTouch.new()
	e.index = idx
	e.position = pos
	e.pressed = down
	Input.parse_input_event(e)

func _tap(pos: Vector2) -> void:
	_touch(0, pos, true)
	await _wait(3)
	_touch(0, pos, false)
	await _wait(4)

## Input.parse_input_event takes window pixels (the engine applies the content scale on the way in); layout is in logical pixels.
func _phys(logical: Vector2) -> Vector2:
	return root.get_final_transform() * logical

func _center(c: Control) -> Vector2:
	return c.get_global_transform_with_canvas() * (c.size * 0.5)

func _run() -> void:
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	await _wait(40)
	print("window ", root.size, " mode ", PlatformMode.mode(), " (", PlatformMode.source(), ") content_scale ", root.content_scale_factor, " aspect ", root.content_scale_aspect)
	if shots.has("title"):
		await _save("title")
	app.settings["cinematics"] = false
	if shots.has("notut"):
		app.settings["tutorial"] = -1   # no advisor card over the chart
	app.start_campaign({"faction": 1, "seed": 424242, "era": 3, "ruleset": "reference", "width": 22, "height": 16, "systems": 48, "preset": "remastered", "events": true, "aiAggression": true})
	app.close_all_modals()
	app.goto("galaxy")
	await _wait(60)
	if shots.has("galaxy"):
		await _save("galaxy")
	if shots.has("tooltip"):
		# A long-press on a control that has a tooltip (the first visible one in the top bar or the rail).
		var target: Control = null
		for c in app.find_children("*", "Control", true, false):
			var ctl := c as Control
			if ctl.visible and ctl.is_visible_in_tree() and ctl.tooltip_text != "" and ctl.size.x > 20 and ctl.size.y > 12 and ctl.mouse_filter != Control.MOUSE_FILTER_IGNORE:
				var p := _center(ctl)
				if Rect2(Vector2.ZERO, root.get_visible_rect().size).has_point(p) and p.y < 60:
					target = ctl
					break
		if target != null:
			var at := _phys(_center(target))
			print("long-press on ", target.get_path(), " tip='", target.tooltip_text.left(60).replace("\n", " / "), "'")
			_touch(0, at, true)
			await create_timer(0.9).timeout
			await _wait(8)
			await _save("tooltip")
			_touch(0, at, false)
			await _wait(6)
		else:
			print("no control with a tooltip found in the top bar")
	var gs := app.current_screen()
	var map: GalaxyMap = gs.map if gs != null and gs.get("map") != null else null
	if map != null and shots.has("ordermenu"):
		var best := Vector2(-1, -1)
		for sid in map.systems3d:
			var p := map.world_to_screen(map.systems3d[sid]["pos"])
			var lp: Vector2 = map.get_global_transform_with_canvas() * p
			if Rect2(Vector2.ZERO, map.size).grow(-60).has_point(p) and map.cell_at(p) >= 0 and PlatformRuntime.control_at(root, lp) == map:
				best = _phys(lp)   # a star with nothing (panel, strip) drawn over it
				print("long-press on star at ", lp, " cell ", map.cell_at(p))
				break
		if best.x >= 0:
			_touch(0, best, true)
			await create_timer(0.9).timeout
			await _wait(8)
			await _save("ordermenu")
			_touch(0, best, false)
			await _wait(6)
			gs.close_menu()
	if map != null and shots.has("pinch"):
		var c := _phys(map.get_global_transform_with_canvas() * (map.size * 0.5))
		_touch(0, c - Vector2(40, 0), true)
		_touch(1, c + Vector2(40, 0), true)
		await _wait(3)
		for k in 8:
			for idx in [0, 1]:
				var d := InputEventScreenDrag.new()
				d.index = idx
				var sgn := -1.0 if idx == 0 else 1.0
				d.position = c + Vector2(sgn * (40.0 + 12.0 * (k + 1)), 0)
				d.relative = Vector2(sgn * 12.0, 0)
				Input.parse_input_event(d)
			await _wait(2)
		_touch(1, c + Vector2(136, 0), false)
		_touch(0, c - Vector2(136, 0), false)
		await _wait(20)
		await _save("pinch")
	if map != null and shots.has("scale"):
		# The slow-frame path by hand: the render scale cap down to the floor, applied to every 3D view, then a frame.
		var rt: PlatformRuntime = app.get_node_or_null("PlatformRuntime")
		PlatformMode.render_cap = PlatformRuntime.RENDER_MIN
		if rt != null:
			rt.reapply_render()
		await _wait(20)
		print("render cap ", PlatformMode.render_cap, " map viewport scaling_3d_scale ", map._viewport.scaling_3d_scale, " mode ", map._viewport.get("scaling_3d_mode"), " renderer ", RenderingServer.get_current_rendering_method())
		await _save("scale")
	if shots.has("probe"):
		# How far each screen's content reaches past the window (logical pixels), screen by screen.
		var win := root.get_visible_rect().size
		print("PROBE window ", win)
		for def in App.GAME_SCREENS:
			var sid := str(def["id"])
			app.goto(sid)
			await _wait(40)
			var cur := app.current_screen()
			var reach := Vector2.ZERO
			var worst := ""
			for ch in cur.find_children("*", "Control", true, false):
				var cc := ch as Control
				if not cc.is_visible_in_tree():
					continue
				var r := cc.get_global_transform_with_canvas() * cc.size
				if r.x > reach.x:
					reach.x = r.x
				if r.y > reach.y:
					reach.y = r.y
					worst = str(cc.get_path()).get_slice("Stage/", 1).left(80)
			print("PROBE ", sid, " reaches ", reach.round(), " over x ", maxf(0.0, reach.x - win.x), " over y ", maxf(0.0, reach.y - win.y), " worst-y ", worst)
	if shots.has("probe2"):
		app.goto("research")
		await _wait(40)
		var cur2 := app.current_screen()
		var rows: Array = []
		for ch in cur2.find_children("*", "Control", true, false):
			var cc := ch as Control
			if cc.is_visible_in_tree():
				var rr: Vector2 = cc.get_global_transform_with_canvas() * cc.size
				rows.append([rr.x, cc.get_class(), str(cc.get_path()).get_slice("Screen_research/", 1).right(70), cc.size, cc.get_combined_minimum_size()])
		rows.sort_custom(func(x, y): return x[0] > y[0])
		for r in rows.slice(0, 8):
			print("REACH ", r)
		print("reach fn ", PlatformMode.content_reach(cur2), " screen size ", cur2.size, " parent ", cur2.get_parent().name)
	if shots.has("panned"):
		# A one-finger drag on a screen wider than the window: it scrolls (touch drag on the pan wrapper).
		app.goto("research")
		await _wait(40)
		var wrap := app.current_screen().get_parent() as ScrollContainer
		var from := _phys(Vector2(700, 420))
		_touch(0, from, true)
		await _wait(3)
		for k in 12:
			var d := InputEventScreenDrag.new()
			d.index = 0
			d.position = from - Vector2(30.0 * (k + 1), 0)
			d.relative = Vector2(-30, 0)
			Input.parse_input_event(d)
			await _wait(2)
		_touch(0, from - Vector2(360, 0), false)
		await _wait(20)
		print("PAN wrap ", wrap != null, " scroll_horizontal ", wrap.scroll_horizontal if wrap else -1, " of content ", app.current_screen().custom_minimum_size.x if wrap else -1)
		await _save("research-panned")
	for s in ["system", "research"]:
		if shots.has(s):
			app.goto(s)
			await _wait(40)
			await _save(s)
	quit(0)
