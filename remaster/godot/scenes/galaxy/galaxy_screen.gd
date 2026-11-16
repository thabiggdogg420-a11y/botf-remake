extends Control
## Galaxy screen: the star chart plus HUD (alerts, galactic events, layer toggles, drafted orders,
## legend), hover tooltip, right-click order menu and the inspector. The HUD is GalaxyHud (scenes/ui/screens/galaxy_hud.gd)
## fed by GalaxyVM; this script owns the chart, the inspector, the hover card and the order menu.

var app: App
var map: GalaxyMap
var hud: GalaxyHud
var _inspector: PanelContainer
var _inspector_body: VBoxContainer
var _hover: PanelContainer
var _menu: PanelContainer
var _framed := false
var _warmed := false
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
	hud = GalaxyHud.new()
	hud.app = app
	hud.focus_requested.connect(focus_cell)
	hud.layer_changed.connect(func(key, on): map.set_layer(key, on))
	add_child(hud)
	_inspector = PanelContainer.new()
	_inspector.name = "Inspector"
	_inspector.add_theme_stylebox_override("panel", Lcars.panel_box(Lcars.accent()))
	add_child(_inspector)
	var sc := ScrollContainer.new()
	sc.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	_inspector.add_child(sc)
	_inspector_body = Lcars.vbox(10)
	_inspector_body.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	sc.add_child(_inspector_body)
	_hover = PanelContainer.new()
	_hover.add_theme_stylebox_override("panel", Lcars.box_lcars(Color(Lcars.PANEL_SOLID, 0.94), maxi(4, UiTokens.radius() - 4), Color(Lcars.accent(), 0.6), 1, Vector4(12, 8, 12, 8)))
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
	if not _warmed:
		# Parse the hero models of the player's own ship classes on worker threads now, so the first battle opens
		# without a multi-second stall (BattleAssets.warm; a no-op without local hero models).
		_warmed = true
		var mine: Array = []
		for f in app.view.get("fleets", []):
			if int(f.get("owner", 0)) == int(app.view.get("meta", {}).get("player", 0)):
				mine.append(f)
		BattleAssets.shared().warm(BattleAssets.ships_keys(mine))
		# The visible foes too (a battle is fought against them): their hulls parse in the background as well, after ours.
		var theirs: Array = []
		for f in app.view.get("fleets", []):
			if int(f.get("owner", 0)) != int(app.view.get("meta", {}).get("player", 0)):
				theirs.append(f)
		BattleAssets.shared().warm(BattleAssets.ships_keys(theirs))
	if not _framed:
		_framed = true
		map.frame_known.call_deferred()
	hud.refresh()
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
		# Double-click flies the camera one level closer (galaxy -> sector -> system close-up). Once the view is
		# already on that system's close-up, double-clicking your own system opens it, as on the 2D chart.
		var s := app.system_at(x, y)
		if not s.is_empty() and int(s.get("owner", 0)) == app.player() and map.zoom_level() == "system" and map.is_focused_on(x, y):
			app.select_system(int(s["id"]))
			app.goto("system")
			return
		app.select_cell(cell)
		map.fly_to_cell(x, y)
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
	var p := map.cell_screen_pos(x, y)
	open_context_menu(y * app.width() + x, p)

func open_context_menu(cell: int, pos: Vector2) -> void:
	close_menu()
	var actions := GalaxyOrders.cell_actions(app, cell)
	_menu = PanelContainer.new()
	_menu.name = "ContextMenu"
	_menu.add_theme_stylebox_override("panel", Lcars.box_lcars(Color(Lcars.PANEL_SOLID, 0.97), UiTokens.radius(), Lcars.accent(), 1, Vector4(8, 8, 8, 8)))
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
		var uc := app.session.uncharted_at(x, y)
		var col := Lcars.vbox(2)
		var title := Lcars.hbox(8)
		title.add_child(Lcars.label((str(s["name"]) if not s.is_empty() else (str(uc["name"]) if not uc.is_empty() else ("Uncharted space" if fog == "0" else "Open space"))).to_upper(), "SubHeading", 18, Lcars.TEXT))
		title.add_child(Lcars.label("%d.%d" % [x, y], "Mono", 12, Lcars.TEXT3))
		col.add_child(title)
		if not uc.is_empty():
			col.add_child(Lcars.label("Uncharted star system", "Muted"))
		if not s.is_empty() and int(s.get("owner", 0)) != 0:
			col.add_child(Lcars.label("Your colony" if int(s["owner"]) == app.player() else app.empire_name(int(s["owner"])), "", 13, Lcars.owner_color(int(s["owner"]))))
		if not s.is_empty() and s.has("minor"):
			col.add_child(Lcars.marked(UiIcon.minor_name(str(s["minor"].get("temperament", ""))), "%s%s" % [s["minor"]["name"], (" · " + str(s["minor"]["temperament"])) if s["minor"].has("temperament") else ""], "", 13, Color("#d8c3ff"), 14))
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
		e.add_child(Lcars.label("Drag pan · Right-drag orbit · Wheel zoom\nWASD pan · Q/E rotate · PgUp/PgDn tilt\nDouble-click fly to · F fit · H home\nTab next ship · Enter end turn", "Mono", 13))
		_inspector_body.add_child(e)

func _card_head(empire: int, title: String, sub: String) -> HBoxContainer:
	var h := Lcars.hbox(10)
	h.add_child(Lcars.emblem(empire, 30))
	var t := Lcars.vbox(0)
	var tl := Lcars.label(title.to_upper(), "SubHeading", 21, Lcars.TEXT)
	tl.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	tl.custom_minimum_size.x = 150
	t.add_child(tl)
	t.add_child(Lcars.caption(sub))
	h.add_child(t)
	return h

## A meter led by its icon (hull, shield): the numbers sit beside it, the word is the tooltip.
func _bar_row(label: String, value: float, max_value: float, color: Color) -> HBoxContainer:
	var r := Lcars.hbox(8)
	var ic := UiIcon.make("hull" if label == "Hull" else "shield", 20, color)
	r.add_child(Pic.tipped(ic, label))
	r.add_child(Lcars.meter(value, max_value, color, 8))
	var n := Lcars.label("%d/%d" % [value, max_value], "Mono", 13)
	n.custom_minimum_size.x = 72
	n.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	r.add_child(n)
	return r

## The ship's own picture on a plate: the baked icon when there is one, else its hull family silhouette.
func _ship_pic(owner_id: int, klass: String, w: float, h: float) -> Control:
	var pic := ShipPicture.make(owner_id, klass, h, "iso", Lcars.owner_color(owner_id))
	pic.plate = true
	pic.custom_minimum_size = Vector2(w, h)
	pic.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	return pic

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
	var klass := str(fleet.get("designClass", "")) if str(fleet.get("designClass", "")) != "" else str(fleet.get("className", ""))
	var top := Lcars.hbox(12)
	top.add_child(_ship_pic(app.player(), klass, 132, 100))
	var tv := Lcars.vbox(6)
	tv.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	tv.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	var head := _card_head(app.player(), str(tf["name"]) if not tf.is_empty() else str(fleet["name"]),
		("x%d" % members.size()) if not tf.is_empty() else "%s · #%d" % [klass, int(fleet["id"])])
	head.tooltip_text = ("Task force of %d ships" % members.size()) if not tf.is_empty() else "%s%s" % [(str(fleet["designClass"]) + " class · ") if fleet.get("designClass", "") != "" else "", fleet["className"]]
	tv.add_child(head)
	tv.add_child(_bar_row("Hull", hull, mh, Color("#8fd18a")))
	tv.add_child(_bar_row("Shields", sh, ms, Lcars.SHIELD))
	top.add_child(tv)
	col.add_child(top)
	var g := Lcars.hbox(18)
	g.add_child(Pic.num("attack", Lcars.fmt(tf.get("attack", fleet["attack"])), "Attack: combined strategic attack rating.", null, 22, 18))
	g.add_child(Pic.num("speed", "%d" % int(tf.get("speed", fleet["speed"])), "Speed: sectors moved per turn.", null, 22, 18))
	g.add_child(Pic.num("range", "%d" % int(fleet.get("range", 0)), "Range: base supply range in sectors from a powered shipyard.", null, 22, 18))
	g.add_child(Pic.num("credits", "%d" % upkeep, "Upkeep: credits deducted every turn for maintenance.", Lcars.WARN if upkeep > 0 else Lcars.TEXT3, 22, 18))
	col.add_child(g)
	var ol := Lcars.hbox(8)
	ol.add_child(UiIcon.make("orders", 20, Lcars.accent2()))
	var ot := Lcars.label(GalaxyOrders.order_text(app, fleet), "", 14, Lcars.TEXT)
	ot.clip_text = true
	ot.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	ot.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	ol.add_child(ot)
	if int(fleet.get("eta", 0)) > 0:
		ol.add_child(Pic.num("clock", "%d" % int(fleet["eta"]), "Turns to arrive", Lcars.accent2(), 18, 15))
	col.add_child(ol)
	if fleet.get("routeBlocked", false):
		col.add_child(Pic.tipped(Lcars.marked("warn", "Out of supply", "", 13, Lcars.WARN, 16), "Course leaves supply range - the unit will wait."))
	var lock := "Finish the battle in progress first" if v.get("session", {}).get("active", false) else ""
	if v["meta"].get("advancedRules", false):
		var sr := Lcars.hbox(4)
		var cur: String = tf.get("stance", fleet.get("stance", "Balanced"))
		var icons := {"Balanced": "fleet", "Aggressive": "attack", "Defensive": "shield", "Withdraw": "delta_left"}
		for s in ["Balanced", "Aggressive", "Defensive", "Withdraw"]:
			var st: String = s
			var b := Pic.icon_toggle(icons[s], cur == s, func(_on): _set_stance(fleet, tf, st), "%s: how this unit behaves when battle is joined." % s, 20, Vector2(46, 34))
			b.disabled = lock != ""
			sr.add_child(b)
		sr.add_child(Lcars.label(cur, "Muted", 14))
		col.add_child(sr)
	for a in GalaxyOrders.cell_actions(app, int(fleet["y"]) * app.width() + int(fleet["x"])):
		if a["group"] != "orders":
			continue
		var p := Lcars.pill(a["label"], a["tone"], a["run"], a["disabled"], a["detail"])
		p.name = "Action_" + str(a["id"])
		col.add_child(p)
	if not tf.is_empty():
		var mr := Lcars.hbox(6)
		for m in members:
			var mk := str(m.get("designClass", "")) if str(m.get("designClass", "")) != "" else str(m.get("className", ""))
			var mc := Lcars.vbox(2)
			mc.add_child(Pic.tipped(_ship_pic(app.player(), mk, 70, 46), "%s #%d" % [m["name"], int(m["id"])]))
			mc.add_child(Lcars.meter(m["hull"], m["maxHull"], Color("#8fd18a"), 4, 70))
			mr.add_child(mc)
		col.add_child(mr)
		var tid := int(tf["id"])
		var tname: String = tf["name"]
		col.add_child(Lcars.pill("Split task force", "tertiary", func(): app.issue({"type": "DisbandTaskForce", "taskForceId": tid}, [], "%s disbanded." % tname), lock))
	else:
		var here := []
		for f in v["fleets"]:
			if int(f["x"]) == int(fleet["x"]) and int(f["y"]) == int(fleet["y"]) and int(f.get("taskForceId", 0)) == 0 and int(f["id"]) != int(fleet["id"]):
				here.append(f)
		if here.size() > 0:
			col.add_child(Pic.tipped(Lcars.marked("fleets", "Task force", "Caption", 0, Lcars.TEXT2, 16), "Form a task force with the ships here: mark them, then press the button."))
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
	var war: bool = c.get("atWar", false)
	var top := Lcars.hbox(12)
	top.add_child(_ship_pic(int(c["owner"]), str(c.get("className", "")), 132, 100))
	var tv := Lcars.vbox(6)
	tv.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	tv.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	var h := _card_head(int(c["owner"]), str(c["name"]), "%s · #%d" % [c["ownerName"], int(c["id"])])
	h.add_child(Pic.tipped(Lcars.icon("war" if war else "peace", 24, Lcars.ALERT if war else Lcars.GOOD), ("At war with " if war else "At peace with ") + str(c["ownerName"]) + (". Select one of your warships, then right-click this sector to attack." if war else ". Declare war in Diplomacy before engaging.") + " Contacts are only tracked inside your sensor range."))
	tv.add_child(h)
	tv.add_child(_bar_row("Hull", c["hull"], c["maxHull"], Color("#8fd18a")))
	tv.add_child(_bar_row("Shields", c["shields"], c["maxShields"], Lcars.SHIELD))
	top.add_child(tv)
	col.add_child(top)
	col.add_child(Pic.num("attack", Lcars.fmt(c["attack"]), "Attack", null, 22, 18))
	return card

## The sector card: a picture of what is here. Numbers sit beside icons; the sentences that used to fill it are tooltips.
func _sector_card(cell: int, primary: bool) -> Control:
	var v := app.view
	var w := app.width()
	var x := cell % w
	var y := cell / w
	var s := app.system_at(x, y)
	var uc := app.session.uncharted_at(x, y)
	var fog: String = str(v["fog"])[cell]
	var card := Lcars.card()
	card.name = "SectorCard"
	var col := Lcars.vbox(8)
	card.add_child(col)
	var owner := int(s.get("owner", 0)) if not s.is_empty() else 0
	var sub := "%d.%d" % [x, y]
	if owner != 0 and owner != app.player():
		sub += " · " + app.empire_name(owner)
	elif owner == 0 and not s.is_empty() and s.has("minor"):
		sub += " · " + str(s["minor"]["name"])
	elif owner == 0 and not s.is_empty():
		sub += " · unclaimed"
	elif not uc.is_empty():
		sub += " · uncharted"
	elif fog == "0":
		sub += " · unexplored"
	var head := _card_head(owner, str(s["name"]) if not s.is_empty() else (str(uc["name"]) if not uc.is_empty() else ("Uncharted space" if fog == "0" else "Open space")), sub)
	head.tooltip_text = "Sector %d.%d · %s" % [x, y, "in sensor range" if fog == "2" else ("surveyed" if fog == "1" else "unexplored")]
	col.add_child(head)
	if not uc.is_empty():
		var up := Pic.tipped(Lcars.marked("unexplored", "Uncharted star", "", 14, Lcars.TEXT2, 18), "A star shows here on the chart, but nothing is known of it until a ship or a sensor reaches it. Send a ship to chart it: select one of yours and right-click this star.")
		up.name = "UnchartedNote"
		col.add_child(up)
	for f in _features_at(cell):
		var fc := PanelContainer.new()
		fc.theme_type_variation = "Inset"
		var fv := Lcars.vbox(3)
		fc.add_child(fv)
		var fh := Lcars.hbox(6)
		fh.add_child(Lcars.label(str(f["name"]).to_upper(), "SubHeading", 17, Color(0.55, 0.85, 1.0)))
		if not f.get("passable", true):
			fh.add_child(Lcars.badge("impassable", "alert"))
		fh.add_child(Pic.tipped(Lcars.icon("tick" if f.get("surveyed", false) else "unexplored", 18, Lcars.GOOD if f.get("surveyed", false) else Lcars.TEXT3), "Surveyed" if f.get("surveyed", false) else "Not surveyed yet. Survey reward: %s." % f.get("surveyReward", "")))
		fv.add_child(fh)
		fc.tooltip_text = str(f["effect"])
		if f["type"] == "Wormhole":
			if f.has("exit"):
				var ex: Dictionary = f["exit"]
				fv.add_child(Lcars.pill("Show exit %d.%d" % [int(ex["x"]), int(ex["y"])], "ghost", func(): focus_cell(int(ex["x"]), int(ex["y"]))))
			else:
				fv.add_child(Pic.tipped(Lcars.marked("unexplored", "Exit uncharted", "Muted", 12, Lcars.TEXT3, 14), "The exit is uncharted - survey or explore it to use the link."))
		col.add_child(fc)
	for st in v.get("stations", []):
		if int(st["x"]) == x and int(st["y"]) == y:
			col.add_child(_station_card(st))
	var colony := {}
	if not s.is_empty():
		colony = app.find_by_id("colonies", int(s["id"]), "systemId")
	if not colony.is_empty():
		var eco: Dictionary = colony["economy"]
		var g := Lcars.hbox(14)
		g.add_child(Pic.num("population", "%d/%d" % [int(colony["population"]), int(colony["capacity"])], "Population; the system currently supports %d." % int(colony["capacity"]), null, 22, 16))
		g.add_child(Pic.num("food", Lcars.signed(int(eco["food"]) - int(colony["population"])), "Farms produce %d food for %d people." % [int(eco["food"]), int(colony["population"])], Lcars.ALERT if int(eco.get("foodDeficit", 0)) > 0 else Lcars.GOOD, 22, 16))
		g.add_child(Pic.num("industry", "%d" % int(eco["industry"]), "Industry per turn, invested in the first build-queue item.", null, 22, 16))
		g.add_child(Pic.num("credits", Lcars.signed(eco["net"]), "Income %d - structure upkeep %d." % [int(eco["income"]), int(eco["upkeep"])], Lcars.ALERT if int(eco["net"]) < 0 else null, 22, 16))
		col.add_child(g)
		if colony.has("building") and colony["building"] is Dictionary:
			var b: Dictionary = colony["building"]
			var br := Lcars.hbox(8)
			br.add_child(Pic.tipped(Pic.structure(str(b["name"]), 34), "Building " + str(b["name"])))
			var bm := Lcars.meter(b["work"], b["cost"], Pic.tint("industry"), 8)
			bm.size_flags_vertical = Control.SIZE_SHRINK_CENTER
			br.add_child(bm)
			br.add_child(Pic.num("clock", ("%d" % int(b["eta"])) if int(b["eta"]) > 0 else "-", "Turns until it is built", Lcars.accent2(), 18, 15))
			col.add_child(br)
		else:
			col.add_child(Pic.tipped(Lcars.marked("warn", "Queue idle", "", 14, Lcars.WARN, 18), "Nothing under construction"))
		if str(colony.get("queueBlocker", "")) != "":
			col.add_child(Pic.tipped(Lcars.marked("warn", "Blocked", "", 13, Lcars.WARN, 16), str(colony["queueBlocker"])))
		var sid := int(s["id"])
		var mb := Lcars.pill("Manage system", "primary", func(): app.select_system(sid); app.goto("system"), "", "", "F2")
		mb.name = "OpenSystem"
		col.add_child(mb)
	if not s.is_empty() and s.get("planets", []).size() > 0:
		var pr := HFlowContainer.new()
		pr.add_theme_constant_override("h_separation", 12)
		pr.add_theme_constant_override("v_separation", 6)
		for p in s["planets"]:
			var pc := Lcars.vbox(2)
			var dot := PlanetDot.make(str(p["class"]), 30)
			dot.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
			pc.add_child(dot)
			var cap := Lcars.label(("%d/%d" % [int(p["capacity"]), int(p["maxCapacity"])]) if int(p["maxCapacity"]) > 0 else "-", "Mono", 12, Lcars.TEXT2)
			cap.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
			pc.add_child(cap)
			pc.tooltip_text = "%s - %s\nCapacity now %d, up to %d when terraformed." % [p["name"], p["className"], int(p["capacity"]), int(p["maxCapacity"])]
			pc.mouse_filter = Control.MOUSE_FILTER_PASS
			pr.add_child(pc)
		col.add_child(pr)
		col.add_child(Pic.num("population", "%d/%d" % [int(s["capacity"]), int(s["maxCapacity"])], "Capacity %d now, up to %d when terraformed." % [int(s["capacity"]), int(s["maxCapacity"])], Lcars.TEXT2, 18, 14))
	if not s.is_empty() and s.has("minor"):
		var m: Dictionary = s["minor"]
		if m.get("unknown", false):
			col.add_child(Pic.tipped(Lcars.marked("minor", "Inhabited world", "", 14, Color("#d8c3ff"), 18), "An inhabited world. Make contact to learn more."))
		else:
			var mr := Lcars.hbox(12)
			mr.add_child(Lcars.marked(UiIcon.minor_name(str(m.get("temperament", ""))), "%s" % m["name"], "", 14, Color("#d8c3ff"), 18))
			if m.has("attitude"):
				mr.add_child(Pic.num("diplomacy", "%d" % int(m["attitude"]), "Relations", Color("#d8c3ff"), 18, 14))
			if m.has("techLevel"):
				mr.add_child(Pic.num("research", "%d" % int(m["techLevel"]), "Tech level", Color("#d8c3ff"), 18, 14))
			var txt := ("Member of " + app.empire_name(int(m["memberOf"]))) if int(m.get("memberOf", 0)) != 0 else "Independent"
			if m.has("temperament"):
				txt += " · " + str(m["temperament"])
			if m.has("bonus"):
				txt += "\nMembers give " + str(m["bonus"])
			mr.tooltip_text = txt
			mr.mouse_filter = Control.MOUSE_FILTER_PASS
			col.add_child(mr)
			if m.has("guard"):
				var gd: Dictionary = m["guard"]
				var gr := Lcars.hbox(8)
				gr.add_child(Lcars.icon("threat", 18, Lcars.ALERT))
				gr.add_child(Pic.num("attack", "%d" % int(gd["attack"]), "Guard ship %s attack" % gd["name"], null, 18, 14))
				gr.add_child(Pic.num("hull", "%d/%d" % [int(gd["hull"]), int(gd["maxHull"])], "Hull", null, 18, 14))
				gr.add_child(Pic.num("shield", "%d/%d" % [int(gd["shields"]), int(gd["maxShields"])], "Shields", null, 18, 14))
				gr.tooltip_text = "Guard ship: " + str(gd["name"])
				gr.mouse_filter = Control.MOUSE_FILTER_PASS
				col.add_child(gr)
	var units := HFlowContainer.new()
	units.add_theme_constant_override("h_separation", 6)
	units.add_theme_constant_override("v_separation", 6)
	var sel_id := int(app.selection.get("fleetId", 0))
	for f in v.get("fleets", []):
		if int(f["x"]) == x and int(f["y"]) == y:
			var fid := int(f["id"])
			var fk := str(f.get("designClass", "")) if str(f.get("designClass", "")) != "" else str(f.get("className", ""))
			units.add_child(_unit_button(app.player(), fk, "%s%s%s" % [f["name"], " (task force)" if int(f.get("taskForceId", 0)) != 0 else "", ", orders given" if f.get("hasOrders", false) else ""], fid == sel_id, f.get("hasOrders", false), func(): app.select_fleet(fid)))
	for c in v.get("contacts", []):
		if int(c["x"]) == x and int(c["y"]) == y:
			var cid := int(c["id"])
			units.add_child(_unit_button(int(c["owner"]), str(c.get("className", "")), "%s%s" % [c["name"], " (hostile)" if c.get("atWar", false) else ""], cid == sel_id, false, func(): app.select_fleet(cid)))
	if units.get_child_count() > 0:
		col.add_child(units)
	return card

## A ship as a button with just its picture; the name is the tooltip.
func _unit_button(owner_id: int, klass: String, tip: String, selected: bool, ordered: bool, on: Callable) -> Button:
	var b := Button.new()
	b.theme_type_variation = "GhostButton"
	b.custom_minimum_size = Vector2(84, 62)
	b.tooltip_text = tip
	if selected:
		b.add_theme_stylebox_override("normal", Lcars.box_tab(Color(Lcars.accent(), 0.2), 12, 3, Lcars.accent(), 1, Vector4(4, 4, 4, 4)))
	var pic := ShipPicture.make(owner_id, klass, 50, "iso", Lcars.owner_color(owner_id))
	pic.set_anchors_preset(Control.PRESET_FULL_RECT)
	pic.offset_left = 6
	pic.offset_right = -6
	pic.offset_top = 4
	pic.offset_bottom = -4
	b.add_child(pic)
	if ordered:
		var tk := UiIcon.make("tick", 14, Lcars.GOOD)
		tk.position = Vector2(66, 4)
		tk.size = Vector2(14, 14)
		b.add_child(tk)
	b.pressed.connect(on)
	return b

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
	var g := Lcars.hbox(14)
	g.add_child(Pic.num("attack", "%d" % (int(st["beam"]) + int(st["torpedo"])), "Attack: beams %d · torpedoes %d. Fights whenever an enemy at war enters its sector." % [int(st["beam"]), int(st["torpedo"])], null, 20, 15))
	g.add_child(Pic.num("supply", "%d" % int(st["supplyRadius"]), "Supply radius: sectors within it count as supplied for your ships.", null, 20, 15))
	g.add_child(Pic.num("sensor", "%d" % int(st["sensorRadius"]), "Sensor radius", null, 20, 15))
	g.add_child(Pic.num("credits", "%d" % int(st["upkeep"]), "Upkeep per turn", Lcars.WARN, 20, 15))
	col.add_child(g)
	if st.has("upgrade"):
		var ur := Lcars.hbox(8)
		ur.add_child(Pic.tipped(Lcars.icon("starbase", 20, Lcars.GOOD), "Starbase upgrade, funded by " + str(st["upgrade"]["funding"])))
		ur.add_child(Lcars.meter(st["upgrade"]["work"], st["upgrade"]["cost"], Lcars.GOOD, 6))
		col.add_child(ur)
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
