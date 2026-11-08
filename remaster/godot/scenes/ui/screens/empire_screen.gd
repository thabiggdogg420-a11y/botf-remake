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
	var e: Dictionary = v["empire"]
	var tot := vm.totals()
	var m := Lcars.screen_margin(self)
	var root := Lcars.vbox(14)
	m.add_child(root)
	var h := Lcars.hbox(22)
	h.add_child(Lcars.emblem(app.player(), 56))
	h.add_child(Lcars.title_block(str(v["meta"]["playerName"]), vm.subtitle()))
	h.add_child(Lcars.spacer())
	var pv := Lcars.pill("Preview turn", "secondary", func(): app.preview_turn(), "", "What the turn will do, and what is left idle")
	pv.name = "PreviewTurn"
	pv.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	h.add_child(pv)
	var inc: Dictionary = e["income"]
	h.add_child(Lcars.stat("Treasury", "%s  %s" % [Lcars.fmt(e["credits"]), Lcars.signed(inc["net"])], "secondary", "Colonies %s · trade %s · ship upkeep -%d" % [Lcars.signed(inc["colonies"]), Lcars.signed(inc["trade"]), int(inc["ships"])]))
	h.add_child(Lcars.stat("Population", "%s/%s" % [Lcars.fmt(tot["pop"]), Lcars.fmt(tot["cap"])]))
	h.add_child(Lcars.stat("Industry", Lcars.fmt(tot["industry"])))
	h.add_child(Lcars.stat("Research", Lcars.fmt(tot["research"]), "info"))
	h.add_child(Lcars.stat("Food balance", Lcars.signed(tot["food"]), "alert" if tot["food"] < 0 else "good"))
	h.add_child(Lcars.stat("Intelligence", Lcars.fmt(e["intelligence"]), "tertiary"))
	root.add_child(h)
	root.add_child(_members(v))
	var f := Lcars.frame(vm.tab_title(), "click a row to open it" if tab in ["colonies", "fleets"] else "", "primary")
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
