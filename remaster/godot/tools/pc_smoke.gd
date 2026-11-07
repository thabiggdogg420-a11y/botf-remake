extends SceneTree
## PC smoke run on a real GPU window: drives the client with synthetic key and mouse input where it can
## (F1-F8, C, button clicks), saves 1920x1080 screenshots and logs frame rates.
##   Godot_v4.5-stable_win64_console.exe --path . -s res://tools/pc_smoke.gd -- <out_dir> [--novsync]
## Prints "SMOKE ok|FAIL <what>" lines and a summary; exit code 1 on any failure.

var app: App
var out_dir := ""
var failures := 0
var fps := {}

func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	out_dir = args[0] if args.size() > 0 and not args[0].begins_with("--") else OS.get_user_data_dir() + "/pc-smoke"
	if "--novsync" in args:
		# Uncapped frame rates: the real frame cost rather than the display's refresh.
		DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
	DirAccess.make_dir_recursive_absolute(out_dir)
	DisplayServer.window_set_size(Vector2i(1920, 1080))
	root.size = Vector2i(1920, 1080)
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred()

func check(cond: bool, what: String) -> void:
	if not cond:
		failures += 1
	print("SMOKE ", "ok   " if cond else "FAIL ", what)

func wait(n: int = 6) -> void:
	for k in n:
		await process_frame

func shot(name_: String) -> void:
	for t in app.get_node("Toasts").get_children():
		t.queue_free()
	await wait(4)
	await RenderingServer.frame_post_draw
	root.get_texture().get_image().save_png("%s/%s.png" % [out_dir, name_])

func key(code: Key) -> void:
	for pressed in [true, false]:
		var e := InputEventKey.new()
		e.keycode = code
		e.physical_keycode = code
		e.pressed = pressed
		Input.parse_input_event(e)
		await process_frame

func click(c: Control, button: MouseButton = MOUSE_BUTTON_LEFT) -> void:
	var at := c.get_global_rect().get_center()
	var mv := InputEventMouseMotion.new()
	mv.position = at
	mv.global_position = at
	Input.parse_input_event(mv)
	await process_frame
	for pressed in [true, false]:
		var e := InputEventMouseButton.new()
		e.button_index = button
		e.position = at
		e.global_position = at
		e.pressed = pressed
		Input.parse_input_event(e)
		await process_frame

## Frame rate over `seconds` of real time: average fps, p99 and worst frame. `from_us` (a Time.get_ticks_usec() taken
## before the action under test) counts that action's own cost in the first frame, so a hitch in the call shows up.
func measure(label: String, seconds: float, from_us := 0) -> void:
	var t0 := from_us if from_us > 0 else Time.get_ticks_usec()
	var last := t0
	var times: Array = []
	while Time.get_ticks_usec() - t0 < int(seconds * 1e6):
		await process_frame
		var now := Time.get_ticks_usec()
		times.append((now - last) / 1000.0)
		last = now
	var sorted := times.duplicate()
	sorted.sort()
	var total := 0.0
	for v in times:
		total += v
	var avg := 1000.0 * times.size() / maxf(total, 1.0)
	var p99: float = sorted[mini(sorted.size() - 1, int(sorted.size() * 0.99))] if not sorted.is_empty() else 0.0
	var worst: float = sorted[sorted.size() - 1] if not sorted.is_empty() else 0.0
	fps[label] = "%.0f fps avg, p99 %.1f ms, worst frame %.1f ms" % [avg, p99, worst]
	print("SMOKE fps  ", label, ": ", fps[label])

func find(n: Node, name_: String) -> Node:
	return n.find_child(name_, true, false)

func _run() -> void:
	await wait(30)
	check(app.core.is_native(), "native BotfCore backend")
	await shot("pc-01-title")

	# ---- New campaign through the title button
	var nb: Control = find(app, "NewCampaign")
	check(nb != null, "title has New Campaign")
	if nb:
		await click(nb)
		await wait()
	check(app.screen == "new", "New Campaign opens the setup screen")
	var nc := app.current_screen()
	nc.faction = 1
	nc.preset = "remastered"
	if nc.has_method("refresh"):
		nc.refresh()
	await wait()
	await shot("pc-02-new-campaign")
	nc.start()
	await wait(10)
	app.close_all_modals()
	check(app.screen == "galaxy" and int(app.view["meta"]["player"]) == 1, "Federation campaign on the galaxy screen")
	check(app.view["meta"]["rules"]["preset"] == "remastered", "Remastered preset")
	await measure("galaxy", 4.0)
	await shot("pc-03-galaxy")

	# ---- Right-click order menu on the map
	var gs := app.current_screen()
	var fleets: Array = app.view.get("fleets", [])
	var moved := false
	if fleets.size() > 0:
		var f: Dictionary = fleets[0]
		app.select_fleet(int(f["id"]))
		await wait()
		for d in [Vector2i(2, 1), Vector2i(1, 0), Vector2i(0, 1), Vector2i(-1, 0), Vector2i(0, -1)]:
			var x: int = int(f["x"]) + d.x
			var y: int = int(f["y"]) + d.y
			if x < 0 or y < 0 or x >= app.width() or y >= int(app.view["meta"]["height"]):
				continue
			gs.open_context_menu_at_cell(x, y)
			await wait()
			var menu: Node = find(gs, "ContextMenu")
			var mv: Button = find(menu, "Action_move") if menu else null
			if mv and not mv.disabled:
				await shot("pc-04-order-menu")
				await click(mv)
				await wait()
				moved = true
				break
			gs.close_menu()
	check(moved, "Set course clicked in the right-click order menu")
	check(app.view["pending"].any(func(p): return p["type"] in ["MoveFleet", "SetTaskForceOrder"]), "move order pending")

	# ---- System screen (F2): queue a build by clicking a construction choice
	await key(KEY_F2)
	await wait()
	check(app.screen == "system", "F2 opens System")
	var choice: Button = null
	for n in app.current_screen().find_children("Choice_*", "Button", true, false):
		if not n.disabled and n.is_visible_in_tree():
			choice = n
			break
	if choice:
		await click(choice)
		await wait()
	check(app.view["pending"].any(func(p): return p["type"] == "EnqueueBuilding"), "build queued on the System screen")
	await shot("pc-05-system")

	# ---- Every game screen by its function key
	var keys := [KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8]
	for k in 8:
		await key(keys[k])
		await wait()
		var id: String = App.GAME_SCREENS[k]["id"]
		check(app.screen == id and app.current_screen().get_child_count() > 0, "F%d opens %s" % [k + 1, id])
		if id in ["empire", "research", "fleets", "diplomacy", "intel", "database"]:
			await shot("pc-06-%s" % id)
	await key(KEY_F1)
	await wait()

	# ---- Ten-plus turns (auto-resolve any battle here; the drill below is fought by hand)
	var start_turn := int(app.view["meta"]["turn"])
	var battles := 0
	for t in 12:
		var st := app.end_turn()
		await wait()
		var guard := 0
		while st == "battle" and guard < 10:
			guard += 1
			battles += 1
			app.close_all_modals()
			app.current_screen()._auto()
			await wait()
			st = app.end_turn()
			await wait()
		if st != "complete":
			check(false, "turn %d resolved (status %s)" % [start_turn + t + 1, st])
			break
		app.close_all_modals()
	check(int(app.view["meta"]["turn"]) >= start_turn + 12, "ended 12 turns (turn %d, %d battles auto-resolved)" % [int(app.view["meta"]["turn"]), battles])
	await measure("galaxy turn 12", 4.0)
	await shot("pc-07-galaxy-turn12")

	# ---- Overlays: events, victory, chronicle
	for o in ["events", "victory", "briefing"]:
		app.open_overlay(o)
		await wait()
		check(app.has_modal(), "%s overlay opens" % o)
		await shot("pc-08-%s" % ("chronicle" if o == "briefing" else o))
		app.close_all_modals()

	# ---- Save, "relaunch" (a fresh client), load
	var fp_before: Variant = app.core.request("fingerprint")
	var turn := int(app.view["meta"]["turn"])
	check(app.save_game("PCSmoke"), "saved")
	app.queue_free()
	await wait()
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	await wait(10)
	var blob: Dictionary = {}
	for s in app.list_saves():
		if s.get("name", "") == "PCSmoke":
			blob = s
	check(not blob.is_empty() and app.load_game(blob), "loaded after relaunch")
	await wait()
	check(int(app.view["meta"]["turn"]) == turn, "loaded turn %d" % turn)
	check(str(app.core.request("fingerprint")) == str(fp_before), "fingerprint identical after load")
	if not blob.is_empty():
		app.delete_save(str(blob["file"]))

	# ---- Battle drill: Take command, several rounds with the 3D view, cinematic camera
	app.start_drill("newBattleDrill", "Tactical drill")
	await wait()
	var t_open := Time.get_ticks_usec()
	var status := app.end_turn()
	await measure("battle open (ships built, models loaded)", 1.0, t_open)
	check(status == "battle" and app.screen == "combat", "drill opens the combat screen")
	var take: Control = find(app, "TakeCommand")
	check(take != null, "encounter offers Take command")
	if take:
		await shot("pc-09-enemy-contact")
		await click(take)
		await wait()
	var cs := app.current_screen()
	var rounds := 0
	for r in 5:
		if app.encounter.get("finished", false):
			break
		cs.order("Engage" if r > 0 else "Charge")
		await wait()
		var t_round := Time.get_ticks_usec()
		cs._round()
		rounds += 1
		await measure("battle round %d" % rounds, 3.0, t_round)
		if r == 1:
			await shot("pc-10-battle-round2")
		if r == 2:
			# The battle opens in the cinematic camera, so C toggles it OFF (to overview) and a second C back on.
			# (The first smoke runs pressed C once and filmed the overview as "cinematic": HANDOFF bug 4.)
			var cam: BattleCamera = cs.host.view_node.camera
			check(cam.mode == BattleCamera.Mode.CINEMATIC, "battle opens in the cinematic camera")
			await key(KEY_C)
			await wait(2)
			check(cam.mode == BattleCamera.Mode.OVERVIEW, "C key (real window) leaves cinematic")
			await shot("pc-11a-battle-overview")
			var cbtn: Control = find(cs, "CamCINEMATIC")
			check(cbtn != null, "CINEMATIC button on the camera bar")
			if cbtn:
				await click(cbtn)
				await wait(2)
			check(cam.mode == BattleCamera.Mode.CINEMATIC, "CINEMATIC button (clicked) selects cinematic")
			await key(KEY_C)
			await key(KEY_C)
			check(cam.mode == BattleCamera.Mode.CINEMATIC, "C twice returns to cinematic")
			await measure("battle cinematic", 3.0)
			await shot("pc-11-battle-cinematic")
		cs = app.current_screen()
	check(rounds >= 3, "%d rounds commanded" % rounds)
	if not app.encounter.get("finished", false):
		cs._auto()
		await wait()
	check(app.encounter.get("finished", false), "battle finished")
	await shot("pc-12-battle-end")
	check(app.end_turn() == "complete", "turn completes after the battle")

	# ---- 24 v 24 starbase drill: the large-battle frame cost (each round from the call that starts it)
	app.start_drill("newBattleDrill", "Starbase drill", {"station": true, "ships": 24})
	await wait()
	var t_big := Time.get_ticks_usec()
	var big_status := app.end_turn()
	await measure("24v24 open (ships built, models loaded)", 1.0, t_big)
	check(big_status == "battle" and app.screen == "combat", "24v24 drill opens the combat screen")
	app.close_all_modals()
	await wait()
	var big := app.current_screen()
	for r in 4:
		if app.encounter.get("finished", false):
			break
		var t_r := Time.get_ticks_usec()
		big._round()
		await measure("24v24 round %d" % (r + 1), 4.0, t_r)
		big = app.current_screen()
		if r == 2:
			await measure("24v24 cinematic", 4.0) # the opening mode is cinematic
			await shot("pc-13-24v24-cinematic")
	check(int(app.encounter.get("ships", []).size()) >= 48, "24v24 encounter has %d ships" % app.encounter.get("ships", []).size())

	print("SMOKE summary: %d failures" % failures)
	for k in fps:
		print("SMOKE fps ", k, ": ", fps[k])
	quit(1 if failures > 0 else 0)
