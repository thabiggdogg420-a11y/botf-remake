extends SceneTree
## Headless UI workflow test: drives a campaign through the real screens with the fixture backend,
## then again with the BotfCore GDExtension when it is loaded.
##   godot --headless --path remaster/godot -s res://scenes/ui/tests/test_campaign_ui.gd
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

func find(node: Node, name_: String) -> Node:
	return node.find_child(name_, true, false)

func _run() -> void:
	var backends := ["fixture"]
	if Engine.has_singleton("BotfCore"):
		backends.append("native")
	else:
		print("BotfCore singleton not loaded: native backend run skipped")
	for b in backends:
		print("== backend: ", b)
		await _workflow(b)
	print("%d checks, %d failures" % [checks, failures])
	quit(1 if failures > 0 else 0)

func _new_app(backend: String) -> void:
	if app:
		app.queue_free()
		await frames()
	App.forced_backend = "fixture" if backend == "fixture" else ""
	app = load("res://scenes/ui/app.tscn").instantiate()
	app.size = Vector2(1920, 1080)
	root.add_child(app)
	await frames()
	check(app.core.backend == backend, "core backend is %s" % backend)

func _workflow(backend: String) -> void:
	await _new_app(backend)
	check(not app.meta.is_empty() and app.meta.get("empires", []).size() == 5, "meta lists five major powers")
	check(app.screen == "title" and find(app, "NewCampaign") != null, "title screen shows New campaign")

	# ---- New campaign
	app.goto("new")
	await frames()
	var nc := app.current_screen()
	check(find(nc, "Faction2") != null and find(nc, "PresetClassic") != null, "new campaign shows faction cards and presets")
	nc.faction = 2
	nc.seed_value = 424242
	nc.preset = "remastered"
	nc.start()
	await frames(3)
	check(not app.view.is_empty(), "campaign started")
	check(app.screen == "galaxy", "galaxy screen opens after launch")
	if backend == "native":
		check(int(app.view["meta"]["player"]) == 2, "player is the Klingon Empire")
		check(app.view["meta"]["rules"]["preset"] == "remastered", "Remastered preset applied")

	# ---- Galaxy: select a unit, open the order menu, set course
	var gs := app.current_screen()
	check(gs.map != null and gs.map.view().size() > 0, "galaxy map has the view")
	var fleets: Array = app.view.get("fleets", [])
	check(fleets.size() > 0, "player has ships")
	if fleets.size() > 0:
		var f: Dictionary = fleets[0]
		app.select_fleet(int(f["id"]))
		await frames()
		check(find(gs, "UnitCard") != null, "inspector shows the unit card")
		var w := app.width()
		var moved := false
		for d in [Vector2i(1, 0), Vector2i(0, 1), Vector2i(-1, 0), Vector2i(0, -1), Vector2i(1, 1)]:
			var x: int = int(f["x"]) + d.x
			var y: int = int(f["y"]) + d.y
			if x < 0 or y < 0 or x >= w or y >= int(app.view["meta"]["height"]):
				continue
			gs.open_context_menu_at_cell(x, y)
			await frames()
			var menu: Node = find(gs, "ContextMenu")
			var mv: Button = find(menu, "Action_move") if menu else null
			check(mv != null, "order menu offers Set course at %d.%d" % [x, y])
			if mv and not mv.disabled:
				mv.pressed.emit()
				await frames()
				moved = true
				break
			gs.close_menu()
		check(moved, "a course was plotted from the order menu")
		if moved:
			check(app.view["pending"].any(func(p): return p["type"] in ["MoveFleet", "SetTaskForceOrder"]), "MoveFleet order is pending")

	# ---- System screen: workforce forecast, enqueue construction
	app.goto("system")
	await frames()
	var ss := app.current_screen()
	check(find(ss, "Workforce") != null and find(ss, "BuildQueue") != null and find(ss, "Construction") != null, "system screen shows workforce, queue, construction")
	check(find(ss, "Forecast") != null, "workforce forecast is shown")
	var choice: Button = null
	for n in ss.find_children("Choice_*", "Button", true, false):
		if not n.disabled:
			choice = n
			break
	check(choice != null, "a structure can be built")
	if choice:
		var before: int = app.view["pending"].size()
		choice.pressed.emit()
		await frames()
		check(app.view["pending"].size() > before and app.view["pending"].any(func(p): return p["type"] == "EnqueueBuilding"), "EnqueueBuilding order is pending")

	# ---- Research: allocation keeps 100%, apply
	app.goto("research")
	await frames()
	var rs := app.current_screen()
	var alloc: Array = rs.rebalance([20, 20, 20, 20, 10, 10], 0, 50, [false, false, false, false, false, false])
	var total := 0
	for a in alloc:
		total += int(a)
	check(total == 100 and int(alloc[0]) == 50, "rebalance keeps the allocation at 100%")
	rs._set_alloc([50, 10, 10, 10, 10, 10])
	rs.apply()
	await frames()
	check(app.view["pending"].any(func(p): return p["type"] == "AllocateResearch"), "AllocateResearch order is pending")

	# ---- Other screens render
	for id in ["fleets", "empire", "diplomacy", "intel", "database"]:
		app.goto(id)
		await frames()
		check(app.current_screen() != null and app.current_screen().get_child_count() > 0, "%s screen renders" % id)
	check(find(app.current_screen(), "ShipEntry") != null, "database shows a ship entry")

	# ---- Overlays
	for o in ["events", "victory", "menu", "settings", "help"]:
		app.open_overlay(o)
		await frames()
		check(app.has_modal(), "%s overlay opens" % o)
		app.close_all_modals()

	# ---- End turn (fight any battle through the combat screen)
	var turn := int(app.view["meta"]["turn"])
	var status := app.end_turn()
	await frames()
	var guard := 0
	while status == "battle" and guard < 10:
		guard += 1
		var cs := app.current_screen()
		cs.order("Engage")
		cs._auto()
		await frames()
		status = app.end_turn()
		await frames()
	check(status == "complete", "turn resolved")
	check(int(app.view["meta"]["turn"]) == turn + 1, "turn advanced to %d" % (turn + 1))
	check(app.has_modal(), "briefing is shown after the turn")
	app.close_all_modals()

	# ---- Save / load
	check(app.save_game("UITest"), "campaign saved")
	var saves := app.list_saves()
	var blob: Dictionary = {}
	for s in saves:
		if s.get("name", "") == "UITest":
			blob = s
	check(not blob.is_empty(), "save listed")
	if not blob.is_empty():
		check(app.load_game(blob), "campaign loaded")
		await frames()
		if backend == "native":
			check(int(app.view["meta"]["turn"]) == turn + 1, "loaded turn matches")
		app.delete_save(str(blob["file"]))

	# ---- Tactical drill through the combat orders UI
	app.start_drill("newBattleDrill", "Tactical drill")
	await frames()
	var st := app.end_turn()
	await frames()
	check(st == "battle" and app.screen == "combat", "battle drill opens the combat screen")
	if st == "battle":
		app.close_all_modals()
		var cs2 := app.current_screen()
		check(find(cs2, "BattleHost") != null, "BattleHost container is present")
		check(find(cs2, "Maneuver_Engage") != null and find(cs2, "Maneuver_Retreat") != null, "maneuver buttons are present")
		cs2.order("Charge")
		await frames()
		cs2._round()
		await frames()
		check(app.encounter is Dictionary and int(app.encounter.get("round", 0)) >= 1, "a round resolved")
		cs2 = app.current_screen()
		cs2._auto()
		await frames()
		check(app.encounter.get("finished", false), "battle finished")
		check(app.end_turn() == "complete", "turn completes after the battle")
		await frames()
