extends Control
## Empire overview presentation: treasury and totals, member worlds, sortable colony and fleet tables and the
## manager tabs. Data and orders: EmpireVM (scripts/viewmodels/empire_vm.gd).

var app: App
var vm: EmpireVM:
	get:
		if vm == null:
			vm = EmpireVM.new(app.session)
		return vm
var tab: String:
	get: return vm.tab
	set(v): vm.tab = v
var tab_state: Dictionary:
	get: return vm.tab_state
	set(v): vm.tab_state = v

const TABS := EmpireVM.TABS

func refresh() -> void:
	for c in get_children():
		c.queue_free()
	var v := app.view
	var m := Lcars.screen_margin(self, Vector4(16, 16, 10, 16))
	var root := Lcars.vbox(12)
	m.add_child(root)
	root.add_child(_banner(v))
	var kr := Lcars.hbox(10)
	kr.name = "EmpireKpis"
	var kp := vm.kpis()
	if not vm.dilithium_kpi().is_empty():
		kp.append(vm.dilithium_kpi())
	for k in kp:
		var t := Lcars.kpi(k["icon"], k["cap"], k["value"], k["delta"], k["tone"], k["explain"])
		t.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		t.custom_minimum_size.x = 0
		kr.add_child(t)
	root.add_child(kr)
	var dash := Lcars.hbox(12)
	dash.name = "Dashboard"
	dash.custom_minimum_size.y = 268
	var cols := _colonies_panel()
	cols.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	cols.size_flags_stretch_ratio = 2.2
	dash.add_child(cols)
	var eco := _economy_panel()
	eco.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	dash.add_child(eco)
	var con := _contacts_panel()
	con.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	dash.add_child(con)
	root.add_child(dash)
	Motion.stagger(dash)
	var f := Lcars.frame("Registry · " + vm.tab_title(), "click a row to open it" if tab in ["colonies", "fleets"] else "", "primary", "database")
	f.size_flags_vertical = Control.SIZE_EXPAND_FILL
	var b := Lcars.body(f)
	b.add_child(Lcars.tab_bar(vm.tab_labels(), tab, func(id): tab = id; refresh()))
	if not tab in ["colonies", "fleets"]:
		var pane: Control
		match tab:
			"trade": pane = TradePanel.make(app, vm)
			"governors": pane = EmpireTabs.governors(app, vm)
			"templates": pane = EmpireTabs.templates(app, vm)
			"stations": pane = EmpireTabs.stations(app, vm)
			"programs": pane = EmpireTabs.programs(app, vm)
			_: pane = EmpireTabs.members(app, vm)
		pane.size_flags_vertical = Control.SIZE_EXPAND_FILL
		b.add_child(pane if pane is ScrollContainer or pane is TradePanel else Lcars.scroll(pane))
		root.add_child(f)
		return
	var table: DataTable
	if tab == "colonies":
		table = DataTable.make([
			{"key": "name", "label": "System", "value": func(c): return c["name"], "ratio": 1.6},
			{"key": "pop", "label": "Pop", "num": true, "value": func(c): return int(c["population"]), "text": func(c): return "%d/%d" % [int(c["population"]), int(c["capacity"])]},
			{"key": "growth", "label": "Growth", "num": true, "value": func(c): return int(c["growth"]["max"]), "text": func(c): return Lcars.signed(c["growth"]["max"]), "color": func(c): return Lcars.ALERT if int(c["growth"]["max"]) < 0 else Lcars.GOOD},
			{"key": "morale", "label": "Morale", "num": true, "value": func(c): return int(c["morale"])},
			{"key": "food", "label": "Food", "num": true, "value": func(c): return int(c["economy"]["food"]) - int(c["population"]), "text": func(c): return Lcars.signed(int(c["economy"]["food"]) - int(c["population"])), "color": func(c): return Lcars.ALERT if int(c["economy"]["foodDeficit"]) > 0 else Lcars.GOOD},
			{"key": "industry", "label": "Industry", "num": true, "value": func(c): return int(c["economy"]["industry"])},
			{"key": "research", "label": "Research", "num": true, "value": func(c): return int(c["economy"]["research"])},
			{"key": "energy", "label": "Energy", "num": true, "value": func(c): return int(c["economy"]["energyUsed"]), "text": func(c): return "%d/%d" % [int(c["economy"]["energyUsed"]), int(c["economy"]["energyAvailable"])]},
			{"key": "credits", "label": "Credits", "num": true, "value": func(c): return int(c["economy"]["net"]), "text": func(c): return Lcars.signed(c["economy"]["net"])},
			{"key": "building", "label": "Building", "ratio": 2.0, "value": func(c): return c["building"]["name"] if c.get("building") is Dictionary else "", "text": func(c): return ("%s · %dT" % [c["building"]["name"], int(c["building"]["eta"])]) if c.get("building") is Dictionary else "idle"},
			{"key": "gov", "label": "Governor", "value": func(c): return c["governor"]["workforce"]},
		], v["colonies"], "name")
		table.row_activated.connect(func(c): app.select_system(int(c["systemId"])); app.goto("system"))
	else:
		table = DataTable.make([
			{"key": "name", "label": "Ship", "ratio": 2.0, "value": func(f2): return f2["name"], "text": func(f2): return "%s #%d" % [f2["name"], int(f2["id"])]},
			{"key": "class", "label": "Role", "value": func(f2): return f2["className"]},
			{"key": "loc", "label": "Location", "value": func(f2): return _place(int(f2["x"]), int(f2["y"]))},
			{"key": "hull", "label": "Hull", "num": true, "value": func(f2): return float(f2["hull"]) / max(1, int(f2["maxHull"])), "text": func(f2): return "%d/%d" % [int(f2["hull"]), int(f2["maxHull"])]},
			{"key": "shields", "label": "Shields", "num": true, "value": func(f2): return int(f2["shields"])},
			{"key": "attack", "label": "Attack", "num": true, "value": func(f2): return int(f2["attack"])},
			{"key": "speed", "label": "Speed", "num": true, "value": func(f2): return int(f2["speed"])},
			{"key": "upkeep", "label": "Upkeep", "num": true, "value": func(f2): return int(f2.get("maintenance", 0))},
			{"key": "orders", "label": "Orders", "ratio": 1.6, "value": func(f2): return int(f2["eta"]) if f2.get("route", []).size() > 0 else 999, "text": func(f2): return ("→ %s · %dT" % [_place(int(f2["targetX"]), int(f2["targetY"])), int(f2["eta"])]) if f2.get("route", []).size() > 0 else ("ordered" if f2.get("hasOrders", false) else "idle")},
		], v["fleets"], "name")
		table.row_activated.connect(func(f2):
			app.select_fleet(int(f2["id"]))
			app.goto("galaxy")
			var s := app.current_screen()
			if s and s.has_method("focus_cell"):
				s.focus_cell(int(f2["x"]), int(f2["y"])))
	table.name = "Table"
	b.add_child(Lcars.scroll(table))
	root.add_child(f)

func _place(x: int, y: int) -> String:
	return vm.place(x, y)

## The banner: the empire's emblem as a large card, its name and motto, member worlds and stations, Preview turn.
func _banner(v: Dictionary) -> Control:
	var band := PanelContainer.new()
	band.name = "Banner"
	var sb := Lcars.panel_box(Lcars.accent())
	sb.bg_color = Color(Lcars.accent().darkened(0.82), 0.92)
	sb.content_margin_top = 10
	sb.content_margin_bottom = 10
	band.add_theme_stylebox_override("panel", sb)
	var h := Lcars.hbox(22)
	band.add_child(h)
	h.add_child(PortraitSlot.make(app.player(), Vector2(112, 112), "", ""))
	var tb := Lcars.vbox(2)
	tb.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	var nm := Lcars.label(str(v["meta"]["playerName"]).to_upper(), "", 54, Lcars.accent())
	nm.add_theme_font_override("font", Lcars.font("display", 700))
	tb.add_child(nm)
	var motto := ""
	for fc in Lcars.FACTIONS:
		if int(fc["id"]) == app.player():
			motto = str(fc["motto"])
	tb.add_child(Lcars.label(motto, "", 16, Lcars.TEXT2))
	tb.add_child(Lcars.caption(vm.subtitle()))
	h.add_child(tb)
	h.add_child(Lcars.spacer())
	var right := Lcars.vbox(8)
	right.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	right.add_child(_members(v))
	var pv := Lcars.pill("Preview turn", "secondary", func(): app.preview_turn(), "", "What the turn will do, and what is left idle")
	pv.name = "PreviewTurn"
	pv.size_flags_horizontal = Control.SIZE_SHRINK_END
	pv.custom_minimum_size = Vector2(200, 44)
	right.add_child(pv)
	h.add_child(right)
	return band

## Colony cards: planet sphere, population ring, outputs and what it is building. Click opens the colony.
func _colonies_panel() -> Control:
	var cards := vm.colony_cards()
	var f := Lcars.frame("Colonies", "%d worlds" % cards.size(), "primary", "colony")
	f.name = "ColonyCards"
	var flow := HFlowContainer.new()
	flow.add_theme_constant_override("h_separation", 10)
	flow.add_theme_constant_override("v_separation", 10)
	for cc in cards:
		var b := Lcars.row_button(false, 96)
		b.custom_minimum_size = Vector2(300, 96)
		b.tooltip_text = "%s: population %d/%d, food %s, industry %d, research %d, morale %d.%s Click to manage." % [cc["name"], cc["pop"], cc["cap"], Fmt.signed(cc["food"]), cc["industry"], cc["research"], cc["morale"], (" Building %s (%dT)." % [cc["building"], cc["eta"]]) if cc["building"] != "" else " Queue idle."]
		var row := Lcars.row_inner(b, 8, 10)
		row.add_child(PlanetOrb.make(cc["cls"], 74, cc["id"] * 13))
		var col := Lcars.vbox(2)
		col.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		col.alignment = BoxContainer.ALIGNMENT_CENTER
		var nm := Lcars.label(str(cc["name"]).to_upper(), "", 20, Lcars.TEXT)
		nm.add_theme_font_override("font", Lcars.font("display", 600))
		col.add_child(nm)
		var outs := Lcars.hbox(10)
		for o in [["food", Fmt.signed(cc["food"]), Lcars.ALERT if cc["short"] else Lcars.GOOD], ["industry", str(cc["industry"]), Lcars.accent2()], ["research", str(cc["research"]), Lcars.INFO]]:
			outs.add_child(UiIcon.make(o[0], 14, o[2]))
			outs.add_child(Lcars.label(o[1], "Mono", 13, Lcars.TEXT2))
		col.add_child(outs)
		var st := ("Building %s · %dT" % [cc["building"], cc["eta"]]) if cc["building"] != "" else "Queue idle"
		col.add_child(Lcars.label(st, "", 12, Lcars.TEXT3 if cc["building"] != "" else Lcars.WARN))
		row.add_child(col)
		row.add_child(RingGauge.make(58, cc["pop"], max(1, cc["cap"]), Lcars.GOOD if not cc["short"] else Lcars.ALERT, str(cc["pop"]), "POP"))
		Lcars.ignore_mouse(row)
		var sid: int = cc["id"]
		b.pressed.connect(func(): app.select_system(sid); app.goto("system"))
		flow.add_child(b)
	Lcars.body(f).add_child(Lcars.scroll(flow))
	return f

## Income bar, fleet roles ring and victory progress.
func _economy_panel() -> Control:
	var f := Lcars.frame("Economy & fleet", "", "secondary", "credits")
	var b := Lcars.body(f)
	b.add_child(Lcars.caption("Income per turn"))
	var parts := vm.income_parts().map(func(p): return [p[0], p[1], Lcars.tone_color(p[2])])
	b.add_child(SegmentBar.make(parts, {"title": "Income per turn", "parts": parts.map(func(p): return [p[0], Fmt.signed(p[1]), "alert" if p[1] < 0 else "good"]), "effect": "Colony income net of structure upkeep, plus trade, less ship upkeep."}))
	var row := Lcars.hbox(14)
	var roles := vm.fleet_roles()
	var palette := [Lcars.accent(), Lcars.accent2(), Lcars.INFO, Lcars.GOOD, Lcars.WARN, Lcars.accent3()]
	var ring := RingGauge.make(104, 0, 1, Lcars.accent(), str(app.view["fleets"].size()), "SHIPS", {"title": "Ships by role", "parts": roles.map(func(r): return [r[0], str(r[1])])})
	var segs := []
	for n in roles.size():
		segs.append([roles[n][1], palette[n % palette.size()]])
	ring.segments = segs
	row.add_child(ring)
	var leg := Lcars.vbox(3)
	leg.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	for n in mini(roles.size(), 5):
		var it := Lcars.hbox(6)
		var dot := ColorRect.new()
		dot.color = palette[n % palette.size()]
		dot.custom_minimum_size = Vector2(10, 10)
		dot.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		it.add_child(dot)
		it.add_child(Lcars.label("%s  %d" % [roles[n][0], roles[n][1]], "", 13, Lcars.TEXT2))
		leg.add_child(it)
	row.add_child(leg)
	b.add_child(row)
	return f

## Every major power: emblem, name, standing and trust. Click opens Diplomacy.
func _contacts_panel() -> Control:
	var f := Lcars.frame("Powers", "", "tertiary", "diplomacy")
	var col := Lcars.vbox(6)
	for r in vm.contacts():
		var b := Lcars.row_button(false, 42)
		b.tooltip_text = "%s: %s%s. Click for diplomacy." % [r["name"], r["status"], (", trust %d" % r["trust"]) if r["met"] else ""]
		var row := Lcars.row_inner(b, 8, 8)
		row.add_child(Lcars.emblem(r["empire"], 24, null if r["met"] else Lcars.TEXT3))
		var nm := Lcars.label(str(r["name"]), "", 14, Lcars.TEXT if r["met"] else Lcars.TEXT3)
		nm.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		nm.clip_text = true
		nm.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		row.add_child(nm)
		if r["met"]:
			var tm := Lcars.meter(r["trust"], 100, Lcars.tone_color(r["tone"]), 4, 50)
			tm.size_flags_horizontal = Control.SIZE_SHRINK_END
			row.add_child(tm)
		row.add_child(Lcars.badge(r["status"], r["tone"]))
		Lcars.ignore_mouse(row)
		b.pressed.connect(func(): app.goto("diplomacy"))
		col.add_child(b)
	Lcars.body(f).add_child(Lcars.scroll(col))
	return f

func _members(v: Dictionary) -> Control:
	var b := vm.member_bonuses()
	var row := PanelContainer.new()
	row.theme_type_variation = "Inset"
	var h := Lcars.hbox(12)
	row.add_child(h)
	var l := Lcars.caption("Member worlds")
	l.tooltip_text = "Minor races that joined your empire. Each adds its join bonus to every colony."
	l.mouse_filter = Control.MOUSE_FILTER_PASS
	h.add_child(l)
	if int(b.get("members", 0)) > 0:
		var parts: Array = b["parts"]
		h.add_child(Lcars.label(" · ".join(parts) if parts.size() > 0 else "no bonus", "", 14, Lcars.GOOD))
		for mm in b["minors"]:
			var bd := Lcars.badge("◇ " + str(mm["name"]), "primary")
			bd.tooltip_text = str(mm["bonus"])
			h.add_child(bd)
	else:
		h.add_child(Lcars.label("None yet - befriend minor races in Diplomacy and invite them to join for their bonuses.", "Muted", 14))
	h.add_child(Lcars.spacer())
	var sc := vm.station_counts()
	var sb: int = sc["starbases"]
	var op: int = sc["outposts"]
	if sb + op > 0:
		h.add_child(Lcars.label("%d starbases · %d outposts" % [sb, op], "Mono", 13))
	return row
