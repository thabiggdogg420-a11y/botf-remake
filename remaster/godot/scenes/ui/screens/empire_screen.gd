extends Control
## Empire overview: treasury and totals, member worlds, sortable colony and fleet tables.
## Port of screens/Empire.tsx.

var app: App
var tab := "colonies"

func refresh() -> void:
	for c in get_children():
		c.queue_free()
	var v := app.view
	var e: Dictionary = v["empire"]
	var tot := {"food": 0, "industry": 0, "research": 0, "net": 0, "pop": 0, "cap": 0}
	for c in v["colonies"]:
		tot["food"] += int(c["economy"]["food"]) - int(c["population"])
		tot["industry"] += int(c["economy"]["industry"])
		tot["research"] += int(c["economy"]["research"])
		tot["pop"] += int(c["population"])
		tot["cap"] += int(c["capacity"])
	var m := MarginContainer.new()
	m.set_anchors_preset(Control.PRESET_FULL_RECT)
	for side in ["left", "right", "top", "bottom"]:
		m.add_theme_constant_override("margin_" + side, 16)
	add_child(m)
	var root := Lcars.vbox(14)
	m.add_child(root)
	var h := Lcars.hbox(22)
	h.add_child(Lcars.emblem(app.player(), 56))
	var t := Lcars.vbox(0)
	t.add_child(Lcars.heading(str(v["meta"]["playerName"]), 40))
	var contacts: int = v["empires"].filter(func(r): return r.get("contact", false)).size()
	t.add_child(Lcars.caption("Turn %d · %d colonies · %d ships · %d contacts" % [int(v["meta"]["turn"]), v["colonies"].size(), v["fleets"].size(), contacts]))
	h.add_child(t)
	h.add_child(Lcars.spacer())
	var inc: Dictionary = e["income"]
	h.add_child(Lcars.stat("Treasury", "%s  %s" % [Lcars.fmt(e["credits"]), Lcars.signed(inc["net"])], "secondary", "Colonies %s · trade %s · ship upkeep -%d" % [Lcars.signed(inc["colonies"]), Lcars.signed(inc["trade"]), int(inc["ships"])]))
	h.add_child(Lcars.stat("Population", "%s/%s" % [Lcars.fmt(tot["pop"]), Lcars.fmt(tot["cap"])]))
	h.add_child(Lcars.stat("Industry", Lcars.fmt(tot["industry"])))
	h.add_child(Lcars.stat("Research", Lcars.fmt(tot["research"]), "info"))
	h.add_child(Lcars.stat("Food balance", Lcars.signed(tot["food"]), "alert" if tot["food"] < 0 else "good"))
	h.add_child(Lcars.stat("Intelligence", Lcars.fmt(e["intelligence"]), "tertiary"))
	root.add_child(h)
	root.add_child(_members(v))
	var f := Lcars.frame("Colonies" if tab == "colonies" else "Fleets", "click a row to open it", "primary")
	f.size_flags_vertical = Control.SIZE_EXPAND_FILL
	var b := Lcars.body(f)
	var tabs := Lcars.hbox(6)
	tabs.add_child(Lcars.toggle("Colonies  %d" % v["colonies"].size(), tab == "colonies", func(_on): tab = "colonies"; refresh()))
	tabs.add_child(Lcars.toggle("Fleets  %d" % v["fleets"].size(), tab == "fleets", func(_on): tab = "fleets"; refresh()))
	b.add_child(tabs)
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
	var s := app.system_at(x, y)
	return str(s["name"]) if not s.is_empty() else "%d.%d" % [x, y]

func _members(v: Dictionary) -> Control:
	var b: Dictionary = v["empire"].get("memberBonuses", {}) if v["empire"].get("memberBonuses") is Dictionary else {}
	var row := PanelContainer.new()
	row.theme_type_variation = "Inset"
	var h := Lcars.hbox(12)
	row.add_child(h)
	var l := Lcars.caption("Member worlds")
	l.tooltip_text = "Minor races that joined your empire. Each adds its join bonus to every colony."
	l.mouse_filter = Control.MOUSE_FILTER_PASS
	h.add_child(l)
	if int(b.get("members", 0)) > 0:
		var parts := []
		for pair in [["research", "% research"], ["industry", "% industry"], ["food", "% food"], ["credits", "% credit income"], ["morale", " morale"], ["groundDefense", "% ground defense"]]:
			if int(b.get(pair[0], 0)) != 0:
				parts.append("+%d%s" % [int(b[pair[0]]), pair[1]])
		h.add_child(Lcars.label(" · ".join(parts) if parts.size() > 0 else "no bonus", "", 14, Lcars.GOOD))
		for mm in b.get("minors", []):
			var bd := Lcars.badge("◇ " + str(mm["name"]), "primary")
			bd.tooltip_text = str(mm["bonus"])
			h.add_child(bd)
	else:
		h.add_child(Lcars.label("None yet - befriend minor races in Diplomacy and invite them to join for their bonuses.", "Muted", 14))
	h.add_child(Lcars.spacer())
	var sb := 0
	var op := 0
	for s in v.get("stations", []):
		if s.get("mine", false):
			if s["type"] == "Starbase":
				sb += 1
			else:
				op += 1
	if sb + op > 0:
		h.add_child(Lcars.label("%d starbases · %d outposts" % [sb, op], "Mono", 13))
	return row
