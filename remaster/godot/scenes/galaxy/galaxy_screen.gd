extends Control
## Galaxy screen: the star chart plus HUD (alerts, galactic events, layer toggles, drafted orders,
## legend), hover tooltip, right-click order menu and the inspector. Port of screens/Galaxy.tsx.

var app: App
var map: GalaxyMap
var _hud: Control
var _inspector: PanelContainer
var _inspector_body: VBoxContainer
var _hover: PanelContainer
var _menu: PanelContainer
var _orders_open := false
var _alerts_open := true
var _framed := false
var _marks: Array = []

func _ready() -> void:
	map = GalaxyMap.new()
	map.name = "GalaxyMap"
	map.app = app
	map.set_anchors_preset(Control.PRESET_FULL_RECT)
	add_child(map)
	map.cell_clicked.connect(_on_click)
	map.cell_context.connect(open_context_menu)
	map.hover_changed.connect(_on_hover)
	_hud = Control.new()
	_hud.set_anchors_preset(Control.PRESET_FULL_RECT)
	_hud.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_hud)
	_inspector = PanelContainer.new()
	_inspector.name = "Inspector"
	_inspector.add_theme_stylebox_override("panel", Lcars.box(Color(Lcars.PANEL_SOLID, 0.9), 18, Lcars.LINE2, 1, Vector4(16, 14, 16, 14)))
	add_child(_inspector)
	var sc := ScrollContainer.new()
	sc.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	_inspector.add_child(sc)
	_inspector_body = Lcars.vbox(10)
	_inspector_body.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	sc.add_child(_inspector_body)
	_hover = PanelContainer.new()
	_hover.add_theme_stylebox_override("panel", Lcars.box(Color(Lcars.PANEL_SOLID, 0.94), 10, Color(Lcars.accent(), 0.6), 1, Vector4(12, 8, 12, 8)))
	_hover.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_hover.visible = false
	add_child(_hover)
	app.selection_changed.connect(_on_selection)
	resized.connect(_layout)

func _exit_tree() -> void:
	if app.selection_changed.is_connected(_on_selection):
		app.selection_changed.disconnect(_on_selection)

func _layout() -> void:
	if _inspector:
		_inspector.position = Vector2(size.x - 440, 14)
		_inspector.size = Vector2(426, size.y - 28)
	if map:
		# Keep framed sectors clear of the inspector (right) and the layer toolbar (bottom).
		map.view_insets = Vector4(0, 0, 440, 80)

func focus_cell(x: int, y: int) -> void:
	map.focus_cell(x, y, max(map.zoom, 1.3))

func refresh() -> void:
	if app.view.is_empty():
		return
	_layout()
	map.rebuild()
	if not _framed:
		_framed = true
		map.frame_known.call_deferred()
	_build_hud()
	_build_inspector()

func _on_selection() -> void:
	_marks.clear()
	map.update_supply()
	_build_inspector()

func _on_click(cell: int, double: bool) -> void:
	close_menu()
	var w := app.width()
	var x := cell % w
	var y := cell / w
	if double:
		var s := app.system_at(x, y)
		if not s.is_empty() and int(s.get("owner", 0)) == app.player():
			app.select_system(int(s["id"]))
			app.goto("system")
		return
	# Clicking a sector with own ships cycles through them; otherwise select the sector.
	var units := []
	for f in app.view.get("fleets", []):
		if int(f["x"]) == x and int(f["y"]) == y:
			units.append(f)
	for c in app.view.get("contacts", []):
		if int(c["x"]) == x and int(c["y"]) == y:
			units.append(c)
	var cur := int(app.selection.get("fleetId", 0))
	if units.size() > 0 and int(app.selection.get("cell", -1)) == cell:
		var idx := -1
		for n in units.size():
			if int(units[n]["id"]) == cur:
				idx = n
		if idx + 1 < units.size():
			app.select_fleet(int(units[idx + 1]["id"]))
			return
		app.select_cell(cell)
		return
	var own: Array = app.view.get("fleets", []).filter(func(f): return int(f["x"]) == x and int(f["y"]) == y)
	if own.size() > 0 and app.system_at(x, y).is_empty():
		app.select_fleet(int(own[0]["id"]))
	else:
		app.select_cell(cell)

# ---------------------------------------------------------------------------------------------
# Context menu
# ---------------------------------------------------------------------------------------------
func open_context_menu_at_cell(x: int, y: int) -> void:
	var p := map.map_to_screen(map.cell_center(x, y))
	open_context_menu(y * app.width() + x, p)

func open_context_menu(cell: int, pos: Vector2) -> void:
	close_menu()
	var actions := GalaxyOrders.cell_actions(app, cell)
	_menu = PanelContainer.new()
	_menu.name = "ContextMenu"
	_menu.add_theme_stylebox_override("panel", Lcars.box(Color(Lcars.PANEL_SOLID, 0.97), 14, Lcars.accent(), 1, Vector4(8, 8, 8, 8)))
	var col := Lcars.vbox(2)
	col.custom_minimum_size.x = 330
	_menu.add_child(col)
	var head := Lcars.hbox(8)
	var band := ColorRect.new()
	band.color = Lcars.accent()
	band.custom_minimum_size = Vector2(14, 18)
	head.add_child(band)
	head.add_child(Lcars.label(GalaxyOrders.sector_label(app, cell).to_upper(), "SubHeading", 19, Lcars.TEXT))
	head.add_child(Lcars.spacer())
	head.add_child(Lcars.label("%d.%d" % [cell % app.width(), cell / app.width()], "Mono", 13, Lcars.TEXT3))
	col.add_child(head)
	var last_group := ""
	for a in actions:
		if a["group"] != last_group:
			if last_group != "":
				col.add_child(Lcars.separator())
			last_group = a["group"]
		var b := Button.new()
		b.name = "Action_" + str(a["id"])
		b.theme_type_variation = "GhostButton"
		b.alignment = HORIZONTAL_ALIGNMENT_LEFT
		b.add_theme_font_override("font", Lcars.font("ui", 520))
		b.add_theme_font_size_override("font_size", 15)
		var tone_c := Lcars.tone_color(a["tone"])
		var sb := Lcars.box(Color(0, 0, 0, 0), 8, Color.TRANSPARENT, 0, Vector4(12, 5, 12, 5))
		sb.border_width_left = 4
		sb.border_color = tone_c
		b.add_theme_stylebox_override("normal", sb)
		var hov := sb.duplicate() as StyleBoxFlat
		hov.bg_color = Color(tone_c, 0.2)
		b.add_theme_stylebox_override("hover", hov)
		b.add_theme_color_override("font_color", Lcars.TEXT)
		var reason: String = a["disabled"]
		b.text = str(a["label"])
		if reason != "":
			b.disabled = true
			b.text = "%s\n%s" % [a["label"], reason]
			b.add_theme_stylebox_override("disabled", sb)
			b.add_theme_color_override("font_disabled_color", Color(Lcars.TEXT3, 0.9))
			b.tooltip_text = "Unavailable: " + reason
		else:
			b.tooltip_text = str(a["detail"])
		var run: Callable = a["run"]
		b.pressed.connect(func():
			close_menu()
			run.call())
		col.add_child(b)
	add_child(_menu)
	_menu.reset_size()
	var sz := _menu.get_combined_minimum_size()
	_menu.position = Vector2(clamp(pos.x + 8, 8, size.x - sz.x - 8), clamp(pos.y + 8, 8, size.y - sz.y - 8))
	_hover.visible = false
	for c in col.get_children():
		if c is Button and not c.disabled:
			c.grab_focus()
			break

func close_menu() -> void:
	if _menu and is_instance_valid(_menu):
		_menu.queue_free()
	_menu = null

func _unhandled_key_input(event: InputEvent) -> void:
	var k := event as InputEventKey
	if k and k.pressed and k.keycode == KEY_ESCAPE and _menu:
		close_menu()
		get_viewport().set_input_as_handled()

func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed and _menu:
		close_menu()

# ---------------------------------------------------------------------------------------------
# Hover tooltip
# ---------------------------------------------------------------------------------------------
var _hover_cell := -2
func _on_hover(cell: int, pos: Vector2) -> void:
	if cell < 0 or _menu:
		_hover.visible = false
		_hover_cell = -2
		return
	if cell != _hover_cell:
		_hover_cell = cell
		for c in _hover.get_children():
			c.queue_free()
		var v := app.view
		var w := app.width()
		var x := cell % w
		var y := cell / w
		var fog: String = str(v["fog"])[cell]
		var s := app.system_at(x, y)
		var col := Lcars.vbox(2)
		var title := Lcars.hbox(8)
		title.add_child(Lcars.label((str(s["name"]) if not s.is_empty() else ("Uncharted space" if fog == "0" else "Open space")).to_upper(), "SubHeading", 18, Lcars.TEXT))
		title.add_child(Lcars.label("%d.%d" % [x, y], "Mono", 12, Lcars.TEXT3))
		col.add_child(title)
		if not s.is_empty() and int(s.get("owner", 0)) != 0:
			col.add_child(Lcars.label("Your colony" if int(s["owner"]) == app.player() else app.empire_name(int(s["owner"])), "", 13, Lcars.owner_color(int(s["owner"]))))
		if not s.is_empty() and s.has("minor"):
			col.add_child(Lcars.label("◇ %s%s" % [s["minor"]["name"], (" · " + str(s["minor"]["temperament"])) if s["minor"].has("temperament") else ""], "", 13, Color("#d8c3ff")))
		if fog == "1":
			col.add_child(Lcars.label("Surveyed - outside sensor range", "Muted"))
		for st in v.get("stations", []):
			if int(st["x"]) == x and int(st["y"]) == y:
				col.add_child(Lcars.label("%s · %s · supply %d, sensors %d" % [st["name"], "yours" if st.get("mine", false) else st["ownerName"], int(st["supplyRadius"]), int(st["sensorRadius"])], "", 13, Lcars.owner_color(int(st["owner"]))))
		for f in _features_at(cell):
			col.add_child(Lcars.label("%s%s" % [f["name"], "" if f.get("passable", true) else " - impassable"], "", 14, Lcars.accent2()))
			var e := Lcars.para(str(f["effect"]), Lcars.TEXT3, 12)
			e.custom_minimum_size.x = 280
			col.add_child(e)
		var own := 0
		for f2 in v.get("fleets", []):
			if int(f2["x"]) == x and int(f2["y"]) == y:
				own += 1
		if own > 0:
			col.add_child(Lcars.label("%d of your ship%s" % [own, "" if own == 1 else "s"], "", 13))
		var hostile := 0
		var foreign := 0
		for c in v.get("contacts", []):
			if int(c["x"]) == x and int(c["y"]) == y:
				foreign += 1
				if c.get("atWar", false):
					hostile += 1
		if foreign > 0:
			col.add_child(Lcars.label("%d contact%s%s" % [foreign, "" if foreign == 1 else "s", " - hostile" if hostile > 0 else ""], "", 13, Lcars.WARN))
		var unit := GalaxyOrders.selected_unit(app)
		if not unit.is_empty() and not (int(unit["x"]) == x and int(unit["y"]) == y):
			var r := app.core.invoke("route", {"fleetId": int(unit["id"]), "x": x, "y": y})
			if r["ok"] and int(r["result"]["steps"]) >= 0:
				col.add_child(Lcars.label("Right-click: set course - %d turn%s" % [int(r["result"]["eta"]), "" if int(r["result"]["eta"]) == 1 else "s"], "", 13, Lcars.accent2()))
			else:
				col.add_child(Lcars.label("Out of supply range", "", 13, Lcars.ALERT))
		_hover.add_child(col)
		_hover.reset_size()
	_hover.visible = true
	var sz := _hover.get_combined_minimum_size()
	_hover.position = Vector2(min(pos.x + 20, size.x - sz.x - 450), min(pos.y + 16, size.y - sz.y - 8))

func _features_at(cell: int) -> Array:
	var w := app.width()
	var out := []
	for f in app.view.get("features", []):
		if int(f["x"]) + int(f["y"]) * w == cell or (f.has("cells") and f["cells"] is Array and (f["cells"].has(cell) or f["cells"].has(float(cell)))):
			out.append(f)
	return out

# ---------------------------------------------------------------------------------------------
# HUD
# ---------------------------------------------------------------------------------------------
func _build_hud() -> void:
	for c in _hud.get_children():
		c.queue_free()
	var v := app.view
	var top := Lcars.vbox(8)
	top.position = Vector2(16, 14)
	top.custom_minimum_size.x = 420
	_hud.add_child(top)
	# Alerts
	var alerts: Array = v.get("alerts", [])
	var tone := "good"
	for a in alerts:
		if a["severity"] == "alert":
			tone = "alert"
		elif tone != "alert":
			tone = "warn"
	var ac := PanelContainer.new()
	ac.name = "Alerts"
	ac.add_theme_stylebox_override("panel", Lcars.box(Color(Lcars.PANEL_SOLID, 0.88), 14, Color(Lcars.tone_color(tone), 0.7), 1, Vector4(12, 8, 12, 8)))
	var acol := Lcars.vbox(4)
	ac.add_child(acol)
	var ah := Lcars.pill(("%d alert%s" % [alerts.size(), "" if alerts.size() == 1 else "s"]) if alerts.size() > 0 else "All systems nominal", "ghost", func(): _alerts_open = not _alerts_open; _build_hud())
	ah.add_theme_color_override("font_color", Lcars.tone_color(tone))
	ah.alignment = HORIZONTAL_ALIGNMENT_LEFT
	acol.add_child(ah)
	if _alerts_open:
		for a in alerts.slice(0, 7):
			var b := Button.new()
			b.flat = true
			b.alignment = HORIZONTAL_ALIGNMENT_LEFT
			b.text = "●  " + str(a["text"])
			b.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
			b.add_theme_font_override("font", Lcars.font("ui", 440))
			b.add_theme_font_size_override("font_size", 14)
			b.add_theme_color_override("font_color", Lcars.tone_color("alert" if a["severity"] == "alert" else ("warn" if a["severity"] == "warn" else "info")))
			b.add_theme_color_override("font_hover_color", Lcars.TEXT)
			var alert: Dictionary = a
			b.pressed.connect(func(): _open_alert(alert))
			acol.add_child(b)
	top.add_child(ac)
	# Events HUD
	var evs: Array = v.get("events", [])
	if evs.size() > 0:
		var open := 0
		for e in evs:
			if e.get("awaitingChoice", false) and int(e.get("drafted", -1)) < 0:
				open += 1
		var eb := Lcars.pill(("%d decision%s needed" % [open, "" if open == 1 else "s"]) if open > 0 else "Galactic events: %s" % ", ".join(evs.slice(0, 2).map(func(e): return e["name"])), "warn" if open > 0 else "tertiary", func(): app.open_overlay("events"))
		eb.name = "EventsHud"
		top.add_child(eb)
	# Bottom bar: layers, orders, legend
	var bottom := Lcars.hbox(10)
	bottom.position = Vector2(16, size.y - 58)
	_hud.add_child(bottom)
	for pair in [["territory", "Territory", "Colony influence of every known empire"], ["supply", "Supply", "Supply range of the selected unit (from the core)"],
			["routes", "Routes", "Plotted courses and ETAs"], ["trade", "Trade", "Trade routes"], ["fog", "Fog", "Fog of war"], ["grid", "Grid", "Sector grid and coordinates"], ["labels", "Labels", "System names"]]:
		var key: String = pair[0]
		bottom.add_child(Lcars.toggle(pair[1], app.map_layers.get(key, true), func(on): map.set_layer(key, on), pair[2]))
	var pending: Array = v.get("pending", [])
	var oc := PanelContainer.new()
	oc.name = "OrdersCard"
	oc.add_theme_stylebox_override("panel", Lcars.box(Color(Lcars.PANEL_SOLID, 0.9), 14, Lcars.LINE2, 1, Vector4(10, 6, 10, 6)))
	var ocol := Lcars.vbox(4)
	oc.add_child(ocol)
	ocol.add_child(Lcars.pill("Orders this turn  %d  %s" % [pending.size(), "▾" if _orders_open else "▴"], "ghost", func(): _orders_open = not _orders_open; _build_hud()))
	if _orders_open:
		if pending.is_empty():
			ocol.add_child(Lcars.label("No orders drafted yet.", "Muted"))
		for p in pending:
			var r := Lcars.hbox(6)
			var l := Lcars.label(str(p["summary"]), "", 13)
			l.custom_minimum_size.x = 380
			l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
			r.add_child(l)
			var pid := int(p["id"])
			r.add_child(Lcars.pill("✕", "ghost", func(): app.cancel_orders([pid], "Order cancelled."), "Finish the battle in progress first" if v.get("session", {}).get("active", false) else "", "Cancel this order"))
			ocol.add_child(r)
	bottom.add_child(oc)
	oc.size_flags_vertical = Control.SIZE_SHRINK_END
	bottom.alignment = BoxContainer.ALIGNMENT_END
	bottom.reset_size()
	bottom.position.y = size.y - 16 - bottom.get_combined_minimum_size().y
	var legend := Lcars.hbox(14)
	for item in [["Your ships", Lcars.owner_color(app.player())], ["Contacts", Lcars.ALERT], ["Station", Lcars.accent2()], ["Anomaly", Color(0.45, 0.85, 1.0)], ["Unexplored", Color("#1a2238")]]:
		var sw := ColorRect.new()
		sw.color = item[1]
		sw.custom_minimum_size = Vector2(10, 10)
		sw.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		legend.add_child(sw)
		legend.add_child(Lcars.label(item[0], "Muted", 12))
	legend.position = Vector2(size.x - 440 - legend.get_combined_minimum_size().x - 20, size.y - 34)
	_hud.add_child(legend)

func _open_alert(a: Dictionary) -> void:
	if a.has("systemId"):
		app.select_system(int(a["systemId"]))
		var s := app.find_by_id("systems", int(a["systemId"]))
		if not s.is_empty():
			focus_cell(int(s["x"]), int(s["y"]))
	elif a.has("fleetId"):
		app.select_fleet(int(a["fleetId"]))
		var f := app.find_by_id("fleets", int(a["fleetId"]))
		if not f.is_empty():
			focus_cell(int(f["x"]), int(f["y"]))
	elif a["kind"] in ["proposal", "offer", "callToArms"]:
		app.goto("diplomacy")
	elif a["kind"] == "event":
		app.open_overlay("events")

# ---------------------------------------------------------------------------------------------
# Inspector
# ---------------------------------------------------------------------------------------------
func _build_inspector() -> void:
	if _inspector_body == null:
		return
	for c in _inspector_body.get_children():
		c.queue_free()
	var v := app.view
	if v.is_empty():
		return
	var sel := app.selection
	var fleet := app.find_by_id("fleets", int(sel.get("fleetId", 0)))
	var contact := {}
	if fleet.is_empty() and int(sel.get("fleetId", 0)) != 0:
		contact = app.find_by_id("contacts", int(sel["fleetId"]))
	var cell := int(sel.get("cell", -1))
	if not fleet.is_empty():
		_inspector_body.add_child(_unit_card(fleet))
	if not contact.is_empty():
		_inspector_body.add_child(_contact_card(contact))
	if cell >= 0:
		_inspector_body.add_child(_sector_card(cell, fleet.is_empty() and contact.is_empty()))
	else:
		var e := Lcars.vbox(8)
		e.add_child(Lcars.heading("Star chart", 26))
		e.add_child(Lcars.para("Select a system or ship. Right-click a sector for orders; with a ship selected, right-click open space to set course."))
		e.add_child(Lcars.label("Drag pan · Wheel zoom · F fit · H home\nTab next ship · Enter end turn", "Mono", 13))
		_inspector_body.add_child(e)

func _card_head(empire: int, title: String, sub: String) -> HBoxContainer:
	var h := Lcars.hbox(10)
	h.add_child(Lcars.emblem(empire, 30))
	var t := Lcars.vbox(0)
	var tl := Lcars.label(title.to_upper(), "SubHeading", 21, Lcars.TEXT)
	tl.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	tl.custom_minimum_size.x = 300
	t.add_child(tl)
	t.add_child(Lcars.caption(sub))
	h.add_child(t)
	return h

func _bar_row(label: String, value: float, max_value: float, color: Color) -> HBoxContainer:
	var r := Lcars.hbox(8)
	var l := Lcars.label(label, "Muted", 13)
	l.custom_minimum_size.x = 56
	r.add_child(l)
	r.add_child(Lcars.meter(value, max_value, color, 7))
	var n := Lcars.label("%d/%d" % [value, max_value], "Mono", 13)
	n.custom_minimum_size.x = 80
	n.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	r.add_child(n)
	return r

func _unit_card(fleet: Dictionary) -> Control:
	var v := app.view
	var card := Lcars.card()
	card.name = "UnitCard"
	var col := Lcars.vbox(8)
	card.add_child(col)
	var tf := app.find_by_id("taskForces", int(fleet.get("taskForceId", 0))) if int(fleet.get("taskForceId", 0)) != 0 else {}
	var members := []
	if tf.is_empty():
		members = [fleet]
	else:
		for f in v["fleets"]:
			if tf["fleetIds"].has(f["id"]) or tf["fleetIds"].has(int(f["id"])):
				members.append(f)
	col.add_child(_card_head(app.player(), str(tf["name"]) if not tf.is_empty() else str(fleet["name"]),
		("Task force · %d ships" % members.size()) if not tf.is_empty() else "%s%s · #%d" % [(str(fleet["designClass"]) + " class · ") if fleet.get("designClass", "") != "" else "", fleet["className"], int(fleet["id"])]))
	var hull := 0
	var mh := 0
	var sh := 0
	var ms := 0
	var upkeep := 0
	for m in members:
		hull += int(m["hull"])
		mh += int(m["maxHull"])
		sh += int(m["shields"])
		ms += int(m["maxShields"])
		upkeep += int(m.get("maintenance", 0))
	col.add_child(_bar_row("Hull", hull, mh, Color("#8fd18a")))
	col.add_child(_bar_row("Shields", sh, ms, Lcars.SHIELD))
	var g := Lcars.grid(4)
	g.add_child(Lcars.stat("Attack", Lcars.fmt(tf.get("attack", fleet["attack"])), "", "Combined strategic attack rating."))
	g.add_child(Lcars.stat("Speed", "%d/t" % int(tf.get("speed", fleet["speed"])), "", "Sectors moved per turn."))
	g.add_child(Lcars.stat("Range", fleet.get("range", 0), "", "Base supply range in sectors from a powered shipyard."))
	g.add_child(Lcars.stat("Upkeep", upkeep, "warn", "Credits deducted every turn for maintenance."))
	col.add_child(g)
	var ol := Lcars.hbox(8)
	ol.add_child(Lcars.caption("Orders"))
	var ot := Lcars.para(GalaxyOrders.order_text(app, fleet), Lcars.TEXT, 14)
	ot.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	ol.add_child(ot)
	if int(fleet.get("eta", 0)) > 0:
		ol.add_child(Lcars.badge("%dT" % int(fleet["eta"]), "secondary"))
	col.add_child(ol)
	if fleet.get("routeBlocked", false):
		col.add_child(Lcars.para("Course leaves supply range - the unit will wait.", Lcars.WARN, 13))
	var lock := "Finish the battle in progress first" if v.get("session", {}).get("active", false) else ""
	if v["meta"].get("advancedRules", false):
		var sr := Lcars.hbox(4)
		sr.add_child(Lcars.caption("Stance"))
		var cur: String = tf.get("stance", fleet.get("stance", "Balanced"))
		for s in ["Balanced", "Aggressive", "Defensive", "Withdraw"]:
			var st: String = s
			var b := Lcars.toggle(s, cur == s, func(_on): _set_stance(fleet, tf, st), "How this unit behaves when battle is joined.")
			b.add_theme_font_size_override("font_size", 13)
			b.disabled = lock != ""
			sr.add_child(b)
		col.add_child(sr)
	for a in GalaxyOrders.cell_actions(app, int(fleet["y"]) * app.width() + int(fleet["x"])):
		if a["group"] != "orders":
			continue
		var p := Lcars.pill(a["label"], a["tone"], a["run"], a["disabled"], a["detail"])
		p.name = "Action_" + str(a["id"])
		col.add_child(p)
	if not tf.is_empty():
		for m in members:
			var r := Lcars.hbox(6)
			var l := Lcars.label("%s #%d" % [m["name"], int(m["id"])], "", 13)
			l.size_flags_horizontal = Control.SIZE_EXPAND_FILL
			r.add_child(l)
			r.add_child(Lcars.meter(m["hull"], m["maxHull"], Color("#8fd18a"), 4, 90))
			col.add_child(r)
		var tid := int(tf["id"])
		var tname: String = tf["name"]
		col.add_child(Lcars.pill("Split task force", "tertiary", func(): app.issue({"type": "DisbandTaskForce", "taskForceId": tid}, [], "%s disbanded." % tname), lock))
	else:
		var here := []
		for f in v["fleets"]:
			if int(f["x"]) == int(fleet["x"]) and int(f["y"]) == int(fleet["y"]) and int(f.get("taskForceId", 0)) == 0 and int(f["id"]) != int(fleet["id"]):
				here.append(f)
		if here.size() > 0:
			col.add_child(Lcars.caption("Form task force with ships here"))
			for f in here.slice(0, 8):
				var cb := CheckBox.new()
				cb.text = "%s #%d" % [f["name"], int(f["id"])]
				var fid := int(f["id"])
				cb.button_pressed = _marks.has(fid)
				cb.toggled.connect(func(on):
					if on: _marks.append(fid)
					else: _marks.erase(fid)
					_build_inspector())
				col.add_child(cb)
			var ids := [int(fleet["id"])] + _marks
			var cmd := {"type": "CreateTaskForce", "memberFleetIds": ids}
			var reason := app.dry_run(cmd) if _marks.size() > 0 else "Mark at least one more ungrouped ship in this sector"
			col.add_child(Lcars.pill("Form task force", "secondary", func():
				if app.issue(cmd, [], "Task force formed. It moves and fights as one."):
					_marks.clear(), reason))
	return card

func _set_stance(fleet: Dictionary, tf: Dictionary, stance: String) -> void:
	var ids := GalaxyOrders.replace_ids(app, fleet)
	if not tf.is_empty():
		app.issue({"type": "SetTaskForceOrder", "taskForceId": int(tf["id"]), "targetX": int(tf["targetX"]), "targetY": int(tf["targetY"]), "stance": stance, "targetFleetId": int(tf.get("attackTarget", 0))}, ids, "%s: %s stance." % [tf["name"], stance.to_lower()])
	else:
		app.issue({"type": "SetTacticalStance", "fleetId": int(fleet["id"]), "stance": stance}, ids, "Stance set to %s." % stance.to_lower())

func _contact_card(c: Dictionary) -> Control:
	var card := Lcars.card()
	card.name = "ContactCard"
	var col := Lcars.vbox(8)
	card.add_child(col)
	var h := _card_head(int(c["owner"]), str(c["name"]), "%s · %s · #%d" % [c["ownerName"], c["className"], int(c["id"])])
	h.add_child(Lcars.badge("At war" if c.get("atWar", false) else "At peace", "alert" if c.get("atWar", false) else "dim"))
	col.add_child(h)
	col.add_child(_bar_row("Hull", c["hull"], c["maxHull"], Color("#8fd18a")))
	col.add_child(_bar_row("Shields", c["shields"], c["maxShields"], Lcars.SHIELD))
	var g := Lcars.grid(2)
	g.add_child(Lcars.stat("Attack", Lcars.fmt(c["attack"])))
	g.add_child(Lcars.stat("Sector", "%d.%d" % [int(c["x"]), int(c["y"])]))
	col.add_child(g)
	col.add_child(Lcars.para(("Select one of your warships, then right-click this sector to attack." if c.get("atWar", false) else "Declare war in Diplomacy before engaging.") + " Contacts are only tracked inside your sensor range.", Lcars.TEXT3, 13))
	return card

func _sector_card(cell: int, primary: bool) -> Control:
	var v := app.view
	var w := app.width()
	var x := cell % w
	var y := cell / w
	var s := app.system_at(x, y)
	var fog: String = str(v["fog"])[cell]
	var card := Lcars.card()
	card.name = "SectorCard"
	var col := Lcars.vbox(8)
	card.add_child(col)
	var owner := int(s.get("owner", 0)) if not s.is_empty() else 0
	var sub := "Sector %d.%d · %s" % [x, y, "in sensor range" if fog == "2" else ("surveyed" if fog == "1" else "unexplored")]
	if owner != 0:
		sub += " · " + ("your colony" if owner == app.player() else app.empire_name(owner))
	elif not s.is_empty() and s.has("minor"):
		sub += " · " + str(s["minor"]["name"])
	elif not s.is_empty():
		sub += " · unclaimed"
	col.add_child(_card_head(owner, str(s["name"]) if not s.is_empty() else ("Uncharted space" if fog == "0" else "Open space"), sub))
	for f in _features_at(cell):
		var fc := PanelContainer.new()
		fc.theme_type_variation = "Inset"
		var fv := Lcars.vbox(3)
		fc.add_child(fv)
		var fh := Lcars.hbox(6)
		fh.add_child(Lcars.label(str(f["name"]).to_upper(), "SubHeading", 17, Color(0.55, 0.85, 1.0)))
		if not f.get("passable", true):
			fh.add_child(Lcars.badge("impassable", "alert"))
		fh.add_child(Lcars.badge("surveyed" if f.get("surveyed", false) else "unsurveyed", "good" if f.get("surveyed", false) else "dim"))
		fv.add_child(fh)
		fv.add_child(Lcars.para(str(f["effect"]), Lcars.TEXT2, 13))
		if f["type"] == "Wormhole":
			if f.has("exit"):
				var ex: Dictionary = f["exit"]
				fv.add_child(Lcars.pill("Show exit %d.%d" % [int(ex["x"]), int(ex["y"])], "ghost", func(): focus_cell(int(ex["x"]), int(ex["y"]))))
			else:
				fv.add_child(Lcars.para("The exit is uncharted - survey or explore it to use the link.", Lcars.TEXT3, 12))
		if not f.get("surveyed", false):
			fv.add_child(Lcars.para("Survey reward: %s." % f.get("surveyReward", ""), Lcars.TEXT3, 12))
		col.add_child(fc)
	for st in v.get("stations", []):
		if int(st["x"]) == x and int(st["y"]) == y:
			col.add_child(_station_card(st))
	var colony := {}
	if not s.is_empty():
		colony = app.find_by_id("colonies", int(s["id"]), "systemId")
	if not colony.is_empty():
		var eco: Dictionary = colony["economy"]
		var g := Lcars.grid(4)
		g.add_child(Lcars.stat("Pop", "%d/%d" % [int(colony["population"]), int(colony["capacity"])], "", "Population; the system currently supports %d." % int(colony["capacity"])))
		g.add_child(Lcars.stat("Food", Lcars.signed(int(eco["food"]) - int(colony["population"])), "alert" if int(eco.get("foodDeficit", 0)) > 0 else "good", "Farms produce %d food for %d people." % [int(eco["food"]), int(colony["population"])]))
		g.add_child(Lcars.stat("Industry", eco["industry"], "", "Industry per turn, invested in the first build-queue item."))
		g.add_child(Lcars.stat("Credits", Lcars.signed(eco["net"]), "alert" if int(eco["net"]) < 0 else "", "Income %d - structure upkeep %d." % [int(eco["income"]), int(eco["upkeep"])]))
		col.add_child(g)
		if colony.has("building") and colony["building"] is Dictionary:
			var b: Dictionary = colony["building"]
			var br := Lcars.hbox(8)
			br.add_child(Lcars.label("Building " + str(b["name"]), "", 14))
			br.add_child(Lcars.meter(b["work"], b["cost"], Color(0, 0, 0, 0), 5))
			br.add_child(Lcars.badge(("%dT" % int(b["eta"])) if int(b["eta"]) > 0 else "-", "secondary"))
			col.add_child(br)
		else:
			col.add_child(Lcars.para("Nothing under construction", Lcars.WARN, 13))
		if str(colony.get("queueBlocker", "")) != "":
			col.add_child(Lcars.para(str(colony["queueBlocker"]), Lcars.WARN, 13))
		var sid := int(s["id"])
		var mb := Lcars.pill("Manage system", "primary", func(): app.select_system(sid); app.goto("system"), "", "", "F2")
		mb.name = "OpenSystem"
		col.add_child(mb)
	if not s.is_empty() and s.get("planets", []).size() > 0:
		for p in s["planets"]:
			var pr := Lcars.hbox(8)
			pr.add_child(PlanetDot.make(str(p["class"]), 12))
			var pn := Lcars.label(str(p["name"]), "", 14)
			pn.size_flags_horizontal = Control.SIZE_EXPAND_FILL
			pr.add_child(pn)
			pr.add_child(Lcars.label("Gas giant" if p["class"] == "GasGiant" else ("Asteroids" if p["class"] == "Asteroids" else "Class " + str(p["class"])), "Muted", 13))
			pr.add_child(Lcars.label(("%d/%d" % [int(p["capacity"]), int(p["maxCapacity"])]) if int(p["maxCapacity"]) > 0 else "-", "Mono", 13))
			pr.tooltip_text = "%s\nCapacity now %d, up to %d when terraformed." % [p["className"], int(p["capacity"]), int(p["maxCapacity"])]
			pr.mouse_filter = Control.MOUSE_FILTER_PASS
			col.add_child(pr)
		col.add_child(Lcars.label("Capacity %d · max %d" % [int(s["capacity"]), int(s["maxCapacity"])], "Muted", 13))
	elif s.is_empty():
		col.add_child(Lcars.para("Send a scout to survey this region." if fog == "0" else "No star system in this sector.", Lcars.TEXT3, 13))
	if not s.is_empty() and s.has("minor"):
		var m: Dictionary = s["minor"]
		if m.get("unknown", false):
			col.add_child(Lcars.para("An inhabited world. Make contact to learn more.", Color("#d8c3ff"), 13))
		else:
			var txt := "◇ %s · %s" % [m["name"], ("member of " + app.empire_name(int(m["memberOf"]))) if int(m.get("memberOf", 0)) != 0 else "independent"]
			if m.has("attitude"):
				txt += " · relations %d" % int(m["attitude"])
			if m.has("temperament"):
				txt += "\n%s%s%s" % [m["temperament"], (" · tech %d" % int(m["techLevel"])) if m.has("techLevel") else "", (" · members give " + str(m["bonus"])) if m.has("bonus") else ""]
			col.add_child(Lcars.para(txt, Color("#d8c3ff"), 13))
	var units := Lcars.vbox(3)
	for f in v.get("fleets", []):
		if int(f["x"]) == x and int(f["y"]) == y:
			var fid := int(f["id"])
			var b2 := Lcars.pill("%s%s%s" % [f["name"], "  · TF" if int(f.get("taskForceId", 0)) != 0 else "", "  · ordered" if f.get("hasOrders", false) else ""], "ghost", func(): app.select_fleet(fid))
			b2.alignment = HORIZONTAL_ALIGNMENT_LEFT
			units.add_child(b2)
	for c in v.get("contacts", []):
		if int(c["x"]) == x and int(c["y"]) == y:
			var cid := int(c["id"])
			var b3 := Lcars.pill("%s%s" % [c["name"], "  · hostile" if c.get("atWar", false) else ""], "ghost", func(): app.select_fleet(cid))
			b3.alignment = HORIZONTAL_ALIGNMENT_LEFT
			b3.add_theme_color_override("font_color", Lcars.owner_color(int(c["owner"])))
			units.add_child(b3)
	if units.get_child_count() > 0:
		col.add_child(Lcars.caption("Units"))
		col.add_child(units)
	if colony.is_empty() and (not s.is_empty() or units.get_child_count() > 0):
		col.add_child(Lcars.para("Right-click the sector for available orders.", Lcars.TEXT3, 13))
	return card

func _station_card(st: Dictionary) -> Control:
	var c := PanelContainer.new()
	c.theme_type_variation = "Inset"
	var col := Lcars.vbox(4)
	c.add_child(col)
	var h := Lcars.hbox(6)
	h.add_child(Lcars.emblem(int(st["owner"]), 18))
	h.add_child(Lcars.label(str(st["name"]).to_upper(), "SubHeading", 18, Lcars.owner_color(int(st["owner"]))))
	h.add_child(Lcars.badge(str(st["typeName"]), "secondary"))
	col.add_child(h)
	col.add_child(_bar_row("Hull", st["hull"], st["maxHull"], Color("#8fd18a")))
	col.add_child(_bar_row("Shields", st["shields"], st["maxShields"], Lcars.SHIELD))
	var g := Lcars.grid(4)
	g.add_child(Lcars.stat("Attack", int(st["beam"]) + int(st["torpedo"]), "", "Beams %d · torpedoes %d. Fights whenever an enemy at war enters its sector." % [int(st["beam"]), int(st["torpedo"])]))
	g.add_child(Lcars.stat("Supply", st["supplyRadius"], "", "Sectors within this radius count as supplied for your ships."))
	g.add_child(Lcars.stat("Sensors", st["sensorRadius"]))
	g.add_child(Lcars.stat("Upkeep", st["upkeep"], "warn"))
	col.add_child(g)
	if st.has("upgrade"):
		col.add_child(Lcars.caption("Starbase upgrade · funded by " + str(st["upgrade"]["funding"])))
		col.add_child(Lcars.meter(st["upgrade"]["work"], st["upgrade"]["cost"], Lcars.GOOD, 6))
	return c

class PlanetDot extends Control:
	var cls := "M"
	static func make(c: String, s: float) -> PlanetDot:
		var d := PlanetDot.new()
		d.cls = c
		d.custom_minimum_size = Vector2(s, s)
		d.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		return d
	func _draw() -> void:
		var colors := {"M": Color("#4fae6a"), "L": Color("#7fa35a"), "O": Color("#3f7fd0"), "H": Color("#d0a060"), "K": Color("#9a8a80"), "J": Color("#d9a86a"), "GasGiant": Color("#d9a86a"), "Y": Color("#c0503a"), "N": Color("#b08040"), "D": Color("#80808a"), "Asteroids": Color("#707078")}
		var c: Color = colors.get(cls, Color("#8899aa"))
		draw_circle(size / 2.0, size.x / 2.0, c)
		draw_circle(size / 2.0 - Vector2(size.x * 0.15, size.y * 0.15), size.x * 0.18, Color(1, 1, 1, 0.35))
