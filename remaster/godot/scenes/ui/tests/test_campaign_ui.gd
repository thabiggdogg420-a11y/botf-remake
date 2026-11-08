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
	# Browser target: run_ui_compat.ps1 runs this suite windowed with --rendering-method gl_compatibility (WebGL2's
	# renderer) and sets BOTF_EXPECT_RENDERER so the run proves which renderer it used.
	var want := OS.get_environment("BOTF_EXPECT_RENDERER")
	var got := str(ProjectSettings.get_setting_with_override("rendering/renderer/rendering_method"))
	if DisplayServer.get_name() != "headless":
		got = RenderingServer.get_current_rendering_method()
	print("renderer: %s (display %s)" % [got, DisplayServer.get_name()])
	if want != "":
		check(got == want, "the UI suite runs on the %s renderer (got %s)" % [want, got])
	var backends := ["fixture"]
	if Engine.has_singleton("BotfCore"):
		backends.append("native")
	else:
		print("BotfCore singleton not loaded: native backend run skipped")
	for b in backends:
		print("== backend: ", b)
		await _workflow(b)
	await _new_app("fixture")
	await _r35_display_and_quit()
	await _new_app("fixture")
	await _m1_ui()
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
	# M1-INT: the galaxy HUD is GalaxyHud over GalaxyVM, in the design system's frames.
	check(gs.hud is GalaxyHud and find(gs, "Alerts") != null and find(gs, "OrdersCard") != null and find(gs, "LayerBar") != null and find(gs, "ChartStatus") != null, "galaxy HUD shows alerts, orders, layers and the status strip")
	check(find(gs, "Legend") != null and find(gs, "Layer_fog") != null and find(gs, "Layer_fog").button_pressed, "HUD has the legend and the fog layer toggle")
	find(gs, "Layer_grid").button_pressed = false
	await frames()
	check(app.map_layers.get("grid", true) == false, "a layer toggle reaches the chart")
	find(gs, "OrdersToggle").pressed.emit()
	await frames()
	check(find(gs, "OrdersCard") != null and gs.hud.orders_open, "the orders frame opens")
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

	# ---- R2 gameplay surface
	await _r2_surface(backend)

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
		# Rules 4: the ship systems panel (six facings, five gauges, crew badge, aim selector) shows for a WEGO ship.
		var wego_ship := false
		for sd in app.encounter.get("ships", []):
			wego_ship = wego_ship or (sd.get("mine", false) and sd.has("wego"))
		if wego_ship:
			check(find(cs2, "ShipSystems") != null and find(cs2, "CrewBadge") != null and find(cs2, "AimSelector") != null, "rules 4 systems panel is present")
			check(find(cs2, "Facing_fore") != null and find(cs2, "Facing_ventral") != null and find(cs2, "Sys_warpCore") != null, "facing bars and subsystem gauges are present")
			cs2.vm.set_subsystem("engines")
			check(cs2.vm.order("Engage") != "", "an aimed order is accepted")
			cs2.vm.set_subsystem("none")
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

## R2: trade routes, retreat, terraform preview, end-turn check, empire managers.
func _r2_surface(backend: String) -> void:
	app.close_all_modals()
	# Empire tabs all render with their panels
	app.goto("empire")
	await frames()
	var es := app.current_screen()
	for pair in [["trade", "TradePanel"], ["governors", "GovernorManager"], ["templates", "TemplateManager"], ["stations", "StationManager"], ["members", "MemberManager"]]:
		es.tab = pair[0]
		es.refresh()
		await frames()
		check(find(es, pair[1]) != null, "empire tab %s shows %s" % [pair[0], pair[1]])
	# Governors: set every colony to Balanced workforce from the empire screen
	es.tab = "governors"
	es.refresh()
	await frames()
	var allb: Button = find(es, "All_workforce_Balanced")
	check(allb != null and not allb.disabled, "empire governors offers a set-all button")
	if allb:
		allb.pressed.emit()
		await frames()
		check(app.view["pending"].any(func(p): return p["type"] == "SetColonyGovernor"), "SetColonyGovernor order is pending from the empire screen")
	# Templates: save a queue as a template
	es = app.current_screen()
	es.tab = "templates"
	es.refresh()
	await frames()
	var tn: LineEdit = find(es, "TemplateName")
	var sv: Button = find(es, "SaveTemplate")
	check(tn != null and sv != null, "empire templates has a save-from-queue form")
	if tn and sv:
		tn.text = "R2 test"
		sv.pressed.emit()
		await frames()
		check(app.view["pending"].any(func(p): return p["type"] == "DefineBuildTemplate"), "DefineBuildTemplate order is pending from the empire screen")

	# Trade routes: the query and the command wire format
	var tr: Variant = app.core.request("tradeRoutes")
	check(tr is Dictionary and tr.has("routes") and tr.has("candidates"), "tradeRoutes query answers with routes and candidates")
	var contact := 0
	for e in app.view["empires"]:
		if e.get("contact", false):
			contact = int(e["empire"])
	if backend == "native" and contact != 0:
		var cmd := {"type": "EstablishTradeRoute", "systemId": int(app.view["colonies"][0]["systemId"]), "targetSystemId": 999, "targetEmpire": contact}
		var why := app.dry_run(cmd)
		check(why != "" and not why.begins_with("Invalid"), "core accepts the EstablishTradeRoute shape (answers: %s)" % why)
		var cc := {"type": "CancelTradeRoute", "systemId": int(app.view["colonies"][0]["systemId"]), "targetEmpire": contact}
		var why2 := app.dry_run(cc)
		check(why2.contains("No trade route"), "core accepts the CancelTradeRoute shape (answers: %s)" % why2)
	if backend == "fixture":
		for e in app.core._view["empires"]:
			e["tradeTreaty"] = true
			e["contact"] = true
		app.refresh()
		app.goto("empire")
		await frames()
		es = app.current_screen()
		es.tab = "trade"
		es.refresh()
		await frames()
		var est: Button = find(es, "EstablishRoute")
		check(est != null and not est.disabled, "trade panel offers a live Establish button with its value")
		if est:
			est.pressed.emit()
			await frames()
			check(app.view["pending"].any(func(p): return p["type"] == "EstablishTradeRoute"), "EstablishTradeRoute order is pending")
			check(app.view["tradeRoutes"].size() > 0, "established route is listed")
			es = app.current_screen()
			es.tab = "trade"
			es.refresh()
			await frames()
			check(find(es, "CancelRoute_%d" % int(app.view["tradeRoutes"][0]["from"])) != null, "established route offers Cancel")

	# Retreat order from the unit's order list
	app.goto("galaxy")
	await frames()
	var fl: Array = app.view.get("fleets", [])
	if fl.size() > 0:
		var f: Dictionary = fl[0]
		for cand in fl:
			if str(GalaxyOrders.retreat_action(app, cand, "")["disabled"]) == "":
				f = cand
				break
		app.select_fleet(int(f["id"]))
		var acts := GalaxyOrders.cell_actions(app, int(f["y"]) * app.width() + int(f["x"]))
		var ret: Dictionary = {}
		for a in acts:
			if a["id"] == "retreat":
				ret = a
		check(not ret.is_empty(), "order list offers Retreat")
		if not ret.is_empty():
			check(str(ret["detail"]).contains("falls back to") or str(ret["disabled"]) != "", "retreat names where the ship falls back to (%s)" % ret["detail"])
			if str(ret["disabled"]) == "":
				ret["run"].call()
				await frames()
				check(app.view["pending"].any(func(p): return p["type"] == "RetreatFleet"), "RetreatFleet order is pending")
				var uf := app.find_by_id("fleets", int(f["id"]))
				if backend == "native":
					check(uf.get("retreat", false), "core marks the fleet retreating")
				check(GalaxyOrders.order_text(app, uf).to_lower().contains("retreat") or backend == "fixture", "order text reads: %s" % GalaxyOrders.order_text(app, uf))
				app.cancel_orders(app.unit_order_ids(int(f["id"]), 0))
	if backend == "native" and fl.size() > 0:
		# Every ship starts at home, where Retreat is refused as pointless; drive the core order itself.
		check(app.issue({"type": "RetreatFleet", "fleetId": int(fl[0]["id"])}, app.unit_order_ids(int(fl[0]["id"]), int(fl[0].get("taskForceId", 0)))), "core accepts RetreatFleet for an own ship")
		check(app.find_by_id("fleets", int(fl[0]["id"])).get("retreat", false), "core marks the fleet retreating")
		app.cancel_orders(app.unit_order_ids(int(fl[0]["id"]), 0))
	# Terraform preview: every planet of the colony ship's system gets an action, with a reason when refused
	var ship: Dictionary = {}
	for f2 in fl:
		if f2.get("colonyShip", false):
			ship = f2
	if not ship.is_empty():
		app.select_fleet(int(ship["id"]))
		var acts2 := GalaxyOrders.cell_actions(app, int(ship["y"]) * app.width() + int(ship["x"]))
		var sysd := app.system_at(int(ship["x"]), int(ship["y"]))
		var tf_acts := acts2.filter(func(a): return str(a["id"]).begins_with("terraform-"))
		if not sysd.is_empty() and app.view["meta"].get("campaignExpansion", false):
			check(tf_acts.size() == sysd["planets"].size(), "terraform preview lists every planet (%d)" % tf_acts.size())
			var refused := tf_acts.filter(func(a): return str(a["disabled"]) != "")
			check(tf_acts.all(func(a): return str(a["detail"]).contains("supports")), "terraform preview states capacity gain")
			print("    terraform: %d refused with reasons: %s" % [refused.size(), ", ".join(refused.map(func(a): return a["disabled"]))])
			if backend == "native":
				for a in tf_acts:
					var idx := int(str(a["id"]).trim_prefix("terraform-"))
					var allowed: Variant = app.core.request("canTerraform", {"fleetId": int(ship["id"]), "systemId": int(sysd["id"]), "planetIndex": idx})
					check(allowed == true or str(a["disabled"]) != "", "terraform-%d: core says %s, UI says '%s'" % [idx, str(allowed), a["disabled"]])
	# Minor race standing: membership checklist / secession margin on the dossier
	var dip: Variant = app.core.request("diplomacy")
	if dip is Dictionary and dip["minors"].size() > 0:
		app.set_meta("diplomacy_focus", {"minor": true, "id": int(dip["minors"][0]["systemId"])})
		app.goto("diplomacy")
		await frames()
		check(find(app.current_screen(), "MinorStanding") != null, "minor dossier shows membership / secession numbers")
	# End-turn check: forecast, describeTurn and dryRunMany reach the panel
	var chk := EndTurnCheck.build(app)
	check(chk["forecast"].has("credits") and chk["forecast"].has("creditsAfter"), "end-turn check carries the core forecast")
	check(chk["warnings"] is Array, "end-turn check lists warnings (%d)" % chk["warnings"].size())
	app.preview_turn()
	await frames()
	check(find(app, "TurnForecast") != null and find(app, "EndTurnCheck") != null, "turn preview shows the forecast")
	app.close_all_modals()
	var fake := {"warnings": [{"kind": "x", "tone": "alert", "text": "x", "screen": "empire", "system": 0, "fleet": 0}], "forecast": chk["forecast"], "last_turn": {}, "alerts": 1}
	check(EndTurnCheck.should_prompt(app, fake), "an alert stops the end-turn button with the check")
	var calm := {"warnings": [{"kind": "x", "tone": "warn", "text": "x", "screen": "empire", "system": 0, "fleet": 0}], "forecast": {}, "last_turn": {}, "alerts": 0}
	app.settings["confirmEndTurn"] = false
	check(not EndTurnCheck.should_prompt(app, calm), "a warning alone does not stop the turn when confirmation is off")
	app.settings["confirmEndTurn"] = true
	check(EndTurnCheck.should_prompt(app, calm), "a warning stops the turn when confirmation is on")
	app.settings["confirmEndTurn"] = false
	app.close_all_modals()


## R3.5 (Simon, 8 Nov 2026): display settings persist and apply; Quit on the title and Quit to title both confirm.
func _r35_display_and_quit() -> void:
	DisplaySettings.dry_run = true
	app.start_drill("newDiplomacyDrill", "Diplomacy drill")
	await frames()
	app.close_all_modals()
	app.open_overlay("settings")
	await frames()
	for key in ["display.resolution", "display.windowMode", "display.vsync", "display.fpsCap", "display.renderScale", "display.antiAlias", "display.shadows"]:
		check(find(app, "Setting_" + key.replace(".", "_")) != null, "settings row %s" % key)
	var fps_row := find(app, "Setting_display_fpsCap")
	var ob: OptionButton = fps_row.get_node("Value") if fps_row != null else null
	if ob != null:
		ob.select(1)
		ob.item_selected.emit(1)
	check(int(app.settings["display.fpsCap"]) == 60 and Engine.max_fps == 60, "FPS cap 60 applies (max_fps %d)" % Engine.max_fps)
	for pair in [["display.windowMode", 1], ["display.antiAlias", 1], ["display.renderScale", 2], ["display.shadows", 0]]:
		var row := find(app, "Setting_" + str(pair[0]).replace(".", "_"))
		if row != null:
			var o: OptionButton = row.get_node("Value")
			o.select(pair[1])
			o.item_selected.emit(pair[1])
	check(app.settings["display.windowMode"] == "borderless" and app.settings["display.antiAlias"] == "fxaa", "window mode and AA chosen")
	check(root.screen_space_aa == Viewport.SCREEN_SPACE_AA_FXAA and absf(root.scaling_3d_scale - 0.75) < 0.001, "FXAA and 75% render scale on the viewport")
	check(not DisplaySettings.shadows_enabled, "shadows off")
	var saved: Variant = JSON.parse_string(FileAccess.get_file_as_string(App.SETTINGS_PATH))
	check(saved is Dictionary and int(saved.get("display.fpsCap", 0)) == 60 and saved.get("display.windowMode") == "borderless", "display settings persist to user://")
	# Startup: a fresh App reads them back and applies them.
	Engine.max_fps = 0
	var again: App = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(again)
	await frames()
	check(Engine.max_fps == 60 and DisplaySettings.last_applied.get("windowMode") == "borderless", "settings apply at startup")
	again.queue_free()
	App.i = app
	# Restore defaults so other runs start clean.
	for k in DisplaySettings.DEFAULTS:
		app.settings[k] = DisplaySettings.DEFAULTS[k]
	DisplaySettings.apply(app.settings, self)
	app.save_settings()
	app.close_all_modals()
	# Quit to title (game menu) asks first; Cancel keeps the campaign.
	app.open_menu()
	await frames()
	var qt := find(app, "Quittotitle") as Button
	check(qt != null, "menu has Quit to title")
	if qt != null:
		qt.pressed.emit()
		await frames()
		check(find(app, "Confirm") != null and not app.view.is_empty(), "quit to title asks before leaving")
		app.close_all_modals()
		check(not app.view.is_empty(), "cancel keeps the campaign")
		app.open_menu()
		await frames()
		(find(app, "Quittotitle") as Button).pressed.emit()
		await frames()
		(find(app, "ConfirmButton") as Button).pressed.emit()
		await frames()
		check(app.screen == "title" and app.view.is_empty(), "confirming returns to the title")
	# Title Quit: confirm, then quit (hooked).
	var quits := [0]
	app.quit_hook = func(): quits[0] += 1
	var qb := find(app.current_screen(), "Quit") as Button
	check(qb != null, "title has a Quit button")
	if qb != null:
		qb.pressed.emit()
		await frames()
		check(find(app, "Confirm") != null and quits[0] == 0, "Quit asks first")
		(find(app, "ConfirmButton") as Button).pressed.emit()
		await frames()
		check(quits[0] == 1, "confirming Quit quits the game")
	app.quit_hook = Callable()
	DisplaySettings.dry_run = false


func _key(code: Key, ctrl: bool = false) -> void:
	var ev := InputEventKey.new()
	ev.keycode = code
	ev.pressed = true
	ev.ctrl_pressed = ctrl
	app._unhandled_key_input(ev)

## M1-UI (docs/UI-DESIGN-SYSTEM.md): WS02 options and preview, the two-click rule from the galaxy, the command
## palette and hotkeys, the Empire dashboard, the hail panel, explained figures and the era setting.
func _m1_ui() -> void:
	print("== M1-UI")
	app.goto("new")
	await frames()
	var nc := app.current_screen()
	check(find(nc, "GalaxyPreview") is GalaxyPreview, "new campaign shows the galaxy preview")
	var shape_btn := nc.find_children("*", "Button", true, false).filter(func(b): return (b as Button).text == "Ring")
	check(shape_btn.size() == 1, "new campaign offers the ring shape")
	if shape_btn.size() == 1:
		(shape_btn[0] as Button).toggled.emit(true)
		await frames()
		nc = app.current_screen()
		check(nc.vm.shape == "ring" and (find(nc, "GalaxyPreview") as GalaxyPreview).shape == "ring", "picking a shape redraws the preview")
	var labels := nc.find_children("*", "Button", true, false).map(func(b): return (b as Button).text)
	check(labels.has("Many") and labels.has("Impossible") and labels.has("Elliptical") and labels.has("Voyager"), "minor density, difficulty, shape and interface era are offered")
	nc.seed_value = 424242
	nc.start()
	await frames(3)
	check(app.screen == "galaxy", "campaign starts on the galaxy")
	# Two-click rule: every game screen is one rail click from the galaxy; every overlay one rail or key press.
	var reached := []
	for r in AppRoutes.GAME_SCREENS:
		app.goto("galaxy")
		await frames()
		var rb := find(app, "Rail_" + r["id"]) as Button
		if rb:
			rb.pressed.emit()
			await frames()
			if app.screen == r["id"]:
				reached.append(r["id"])
	check(reached.size() == AppRoutes.GAME_SCREENS.size(), "every screen is one click from the galaxy (%d of %d)" % [reached.size(), AppRoutes.GAME_SCREENS.size()])
	app.goto("galaxy")
	await frames()
	for kind in [[KEY_E, "events"], [KEY_V, "victory"], [KEY_C, "briefing"]]:
		_key(kind[0])
		await frames()
		check(app.has_modal(), "%s opens from one key on the galaxy" % kind[1])
		app.close_all_modals()
		await frames()
	_key(KEY_F10)
	await frames()
	check(find(app, "SettingsPanel") != null, "F10 opens settings")
	app.close_all_modals()
	await frames()
	# Command palette
	_key(KEY_K, true)
	await frames()
	var pal := find(app, "CommandPalette") as CommandPalette
	check(pal != null, "Ctrl+K opens the command palette")
	if pal:
		pal.field.text = "resea"
		pal.field.text_changed.emit("resea")
		await frames()
		pal.run_selected()
		await frames()
		check(app.screen == "research" and not app.has_modal(), "the palette runs its first match (Research)")
	app.run_palette("colony:%d" % int(app.view["colonies"][0]["systemId"]))
	await frames()
	check(app.screen == "system", "the palette opens a colony's System screen")
	check(find(app.current_screen(), "Orrery") != null and find(app.current_screen(), "ColonyKpis") != null, "the System screen leads with the planets and key figures")
	var kp := find(app.current_screen(), "ColonyKpis")
	if kp:
		var tile := kp.get_child(0) as KpiTile
		check(tile != null and tile.tooltip_text.find(char(10)) > 0 and tile._make_custom_tooltip(tile.tooltip_text) is Control, "a key figure explains itself on hover")
	app.goto("empire")
	await frames()
	var es := app.current_screen()
	check(find(es, "Banner") != null and find(es, "EmpireKpis") != null and find(es, "ColonyCards") != null, "the Empire screen is filled: banner, figures, colony cards")
	check(find(es, "Table") != null and find(es, "PreviewTurn") != null, "the Empire registry and Preview turn remain")
	app.goto("diplomacy")
	await frames()
	var ds := app.current_screen()
	var contacted: Array = app.view["empires"].filter(func(e): return e.get("contact", false))
	if contacted.size() > 0:
		check(find(ds, "HailPanel") != null and find(ds, "Portrait") is PortraitSlot, "diplomacy shows the hail panel with a portrait slot")
		var hb := find(ds, "Hail") as Button
		if hb:
			hb.pressed.emit()
			await frames()
			check(app.current_screen().vm.dossier_tab == "propose", "Hail opens negotiation")
	# Era setting
	var before := str(app.settings.get("uiEra", "tng"))
	app.settings["uiEra"] = "tos"
	app.save_settings()
	await frames()
	check(UiTokens.era == "tos" and UiTokens.radius() == 4, "the TOS interface look applies its tokens")
	app.settings["uiEra"] = before
	app.save_settings()
	await frames()
	check(UiTokens.era == before, "the interface look restores")
