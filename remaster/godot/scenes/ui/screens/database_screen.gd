extends Control
## Database (encyclopedia): ships with the expanded roster and faction/role/era/source filters,
## technologies, structures, races and minor races. Port of screens/Encyclopedia.tsx.

var app: App
var tab := "ships"
var query := ""
var faction := "__init"
var role := ""
var era := ""
var source := ""
var field := -1
var selected := -1
var _list_box: VBoxContainer
var _detail_host: Control
var _count: Label

const ROLE_NAMES := {0: "Scout", 1: "Destroyer", 2: "Cruiser", 3: "Strike cruiser", 4: "Command ship", 5: "Colony ship", 6: "Outpost", 7: "Starbase", 8: "Station", 9: "Troop transport"}
const RACE_TO_EMPIRE := {1: 1, 3: 2, 4: 3, 0: 4, 2: 5}
const RACE_TO_FACTION := {1: "Federation", 3: "Klingon", 4: "Romulan", 0: "Cardassian", 2: "Ferengi"}
const MAJOR_FACTIONS := ["Federation", "Klingon", "Romulan", "Cardassian", "Ferengi"]

func _data() -> Dictionary:
	return app.get_encyclopedia()

func _race_name(id: int) -> String:
	for r in _data().get("races", []):
		if int(r["id"]) == id:
			return str(r["name"])
	return "-"

func facets(s: Dictionary) -> Dictionary:
	var r: Variant = s.get("roster")
	if r is Dictionary:
		return {"faction": str(r["faction"]), "role": Roster.role_label(str(r["hullRole"])), "era": str(r["era"]), "source": str(r["sourceKind"])}
	return {"faction": RACE_TO_FACTION.get(int(s["race"]), _race_name(int(s["race"]))), "role": ROLE_NAMES.get(int(s["role"]), "Role %d" % int(s["role"])), "era": "Legacy", "source": "Legacy"}

func refresh() -> void:
	for c in get_children():
		c.queue_free()
	var d := _data()
	if d.is_empty():
		add_child(Lcars.empty_note("Database unavailable."))
		return
	if faction == "__init":
		faction = ""
		if not app.view.is_empty():
			for e in app.meta.get("empires", []):
				if int(e["id"]) == app.player():
					faction = RACE_TO_FACTION.get(int(e["race"]), "")
	var m := MarginContainer.new()
	m.set_anchors_preset(Control.PRESET_FULL_RECT)
	for side in ["left", "right", "top", "bottom"]:
		m.add_theme_constant_override("margin_" + side, 16)
	add_child(m)
	var row := Lcars.hbox(14)
	m.add_child(row)
	var f := Lcars.frame("Database", "reference catalogue", "primary")
	f.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	f.size_flags_stretch_ratio = 1.1
	row.add_child(f)
	var b := Lcars.body(f)
	var tabs := Lcars.hbox(6)
	for t in [["ships", "Ships"], ["technologies", "Tech"], ["structures", "Structures"], ["races", "Races"], ["minors", "Minor races"]]:
		var id: String = t[0]
		var tg := Lcars.toggle(t[1], tab == id, func(_on): tab = id; selected = -1; refresh())
		tg.name = "Tab_" + id
		tabs.add_child(tg)
	tabs.add_child(Lcars.spacer())
	_count = Lcars.label("", "Mono", 13, Lcars.TEXT3)
	tabs.add_child(_count)
	b.add_child(tabs)
	var filters := HFlowContainer.new()
	filters.add_theme_constant_override("h_separation", 8)
	var search := LineEdit.new()
	search.name = "Search"
	search.placeholder_text = "Search..."
	search.text = query
	search.custom_minimum_size.x = 260
	search.text_changed.connect(func(t): query = t; _fill_list())
	filters.add_child(search)
	if tab == "ships":
		var fx := _ship_facets()
		filters.add_child(_select("Faction", "All factions", fx["factions"], faction, func(v): faction = v; _fill_list(), func(x): return Roster.faction_label(x)))
		filters.add_child(_select("Role", "All roles", fx["roles"], role, func(v): role = v; _fill_list()))
		filters.add_child(_select("Era", "All eras", fx["eras"], era, func(v): era = v; _fill_list(), func(x): return "Legacy catalogue" if x == "Legacy" else Roster.era_label(x)))
		filters.add_child(_select("Source", "All sources", ["Legacy", "Canon", "ModDerived", "Kelvin"], source, func(v): source = v; _fill_list(), func(x): return "Legacy catalogue" if x == "Legacy" else Roster.source_label(x)))
	elif tab in ["technologies", "structures"]:
		var ob := OptionButton.new()
		ob.add_item("All fields", 0)
		var fields: Array = app.meta.get("researchFields", [])
		for i in fields.size():
			ob.add_item(str(fields[i]), i + 1)
		ob.selected = field + 1
		ob.item_selected.connect(func(ix): field = ix - 1; _fill_list())
		filters.add_child(ob)
	b.add_child(filters)
	_list_box = Lcars.vbox(3)
	b.add_child(Lcars.scroll(_list_box))
	_detail_host = Lcars.vbox(0)
	_detail_host.custom_minimum_size.x = 720
	_detail_host.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(_detail_host)
	_fill_list()

func _select(nm: String, all: String, items: Array, current: String, on_pick: Callable, fmt: Callable = Callable()) -> OptionButton:
	var ob := OptionButton.new()
	ob.name = nm
	ob.add_item(all, 0)
	ob.set_item_metadata(0, "")
	for i in items.size():
		ob.add_item(fmt.call(items[i]) if fmt.is_valid() else str(items[i]), i + 1)
		ob.set_item_metadata(i + 1, items[i])
		if items[i] == current:
			ob.selected = i + 1
	ob.item_selected.connect(func(ix): on_pick.call(str(ob.get_item_metadata(ix))))
	return ob

func _ship_facets() -> Dictionary:
	var factions := []
	var roles := []
	var eras := {}
	for s in _data().get("ships", []):
		var fc := facets(s)
		if not factions.has(fc["faction"]):
			factions.append(fc["faction"])
		if not roles.has(fc["role"]):
			roles.append(fc["role"])
		eras[fc["era"]] = true
	var extra: Array = factions.filter(func(x): return not MAJOR_FACTIONS.has(x))
	extra.sort()
	roles.sort()
	var era_list := ["Legacy"]
	for e in Roster.ERA_ORDER:
		if eras.has(e):
			era_list.append(e)
	return {"factions": MAJOR_FACTIONS + extra, "roles": roles, "eras": era_list}

func rows() -> Array:
	var d := _data()
	var q := query.strip_edges().to_lower()
	var out := []
	var fields: Array = app.meta.get("researchFields", [])
	match tab:
		"ships":
			for s in d.get("ships", []):
				var fc := facets(s)
				if faction != "" and fc["faction"] != faction:
					continue
				if role != "" and fc["role"] != role:
					continue
				if era != "" and fc["era"] != era:
					continue
				if source != "" and fc["source"] != source:
					continue
				var ro: Variant = s.get("roster")
				if q != "" and not ("%s %s %s %s" % [s["designClass"], s["designName"], s["description"], ro["source"] if ro is Dictionary else ""]).to_lower().contains(q):
					continue
				var badge := Roster.source_label(ro["sourceKind"]) if ro is Dictionary and ro["sourceKind"] != "Canon" else "%s ind" % Lcars.fmt(s["industryCost"])
				out.append({"id": int(s["id"]), "title": str(s["designClass"]), "sub": "%s · %s%s" % [s["designName"], Roster.faction_label(fc["faction"]), (" · " + Roster.era_label(fc["era"])) if ro is Dictionary else ""], "badge": badge})
		"technologies":
			for t in d.get("technologies", []):
				if (field < 0 or int(t["field"]) == field) and (q == "" or ("%s %s" % [t["name"], t["description"]]).to_lower().contains(q)):
					out.append({"id": int(t["id"]), "title": str(t["name"]), "sub": "%s level %d" % [fields[int(t["field"])] if int(t["field"]) < fields.size() else "-", int(t["level"])], "badge": "%s pts" % Lcars.fmt(t["cost"])})
		"structures":
			for s2 in d.get("buildings", []):
				if (field < 0 or int(s2["field"]) == field) and (q == "" or ("%s %s" % [s2["name"], s2["description"]]).to_lower().contains(q)):
					out.append({"id": int(s2["id"]), "title": str(s2["name"]), "sub": "%s %d" % [fields[int(s2["field"])] if int(s2["field"]) >= 0 and int(s2["field"]) < fields.size() else "-", int(s2["level"])], "badge": "%s ind" % Lcars.fmt(s2["industryCost"])})
		"minors":
			for r in d.get("minorRaces", []):
				if q == "" or ("%s %s %s %s" % [r["raceName"], r["homeworld"], r["temperament"], r["description"]]).to_lower().contains(q):
					out.append({"id": int(r["raceId"]), "title": str(r["raceName"]), "sub": "%s · %s" % [r["temperament"], r["homeworld"]], "badge": str(r["bonus"])})
		_:
			for r in d.get("races", []):
				if q == "" or str(r["name"]).to_lower().contains(q):
					out.append({"id": int(r["id"]), "title": str(r["name"]), "sub": "Major power" if int(r["id"]) <= 4 else "Minor race", "badge": ""})
	return out

func _fill_list() -> void:
	for c in _list_box.get_children():
		c.queue_free()
	var rs := rows()
	_count.text = "%d entries" % rs.size()
	var current := {}
	for r in rs:
		if int(r["id"]) == selected:
			current = r
	if current.is_empty() and rs.size() > 0:
		current = rs[0]
	if rs.is_empty():
		_list_box.add_child(Lcars.empty_note("No matches."))
	for r in rs.slice(0, 400):
		var btn := Button.new()
		btn.theme_type_variation = "GhostButton"
		btn.custom_minimum_size.y = 44
		var active: bool = not current.is_empty() and int(current["id"]) == int(r["id"])
		if active:
			btn.add_theme_stylebox_override("normal", Lcars.box(Color(Lcars.accent(), 0.2), 10, Lcars.accent(), 1))
		var h := Lcars.hbox(10)
		h.set_anchors_preset(Control.PRESET_FULL_RECT)
		h.offset_left = 12
		h.offset_right = -12
		var t := Lcars.vbox(-3)
		t.alignment = BoxContainer.ALIGNMENT_CENTER
		t.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		t.add_child(Lcars.label(str(r["title"]), "", 15, Lcars.TEXT))
		var sub := Lcars.label(str(r["sub"]), "Muted", 12)
		sub.clip_text = true
		t.add_child(sub)
		h.add_child(t)
		if str(r["badge"]) != "":
			h.add_child(Lcars.label(str(r["badge"]), "Mono", 12, Lcars.accent2()))
		for n in [h] + h.find_children("*", "Control", true, false):
			n.mouse_filter = Control.MOUSE_FILTER_IGNORE
		btn.add_child(h)
		var id := int(r["id"])
		btn.pressed.connect(func(): selected = id; _fill_list())
		_list_box.add_child(btn)
	_show_detail(current)

func _show_detail(r: Dictionary) -> void:
	for c in _detail_host.get_children():
		c.queue_free()
	var f := Lcars.frame(str(r.get("title", "Entry")), str(r.get("sub", "")), "secondary")
	f.name = "Detail"
	f.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_detail_host.add_child(f)
	var b := Lcars.body(f)
	if r.is_empty():
		b.add_child(Lcars.empty_note("Select an entry."))
		return
	var id := int(r["id"])
	match tab:
		"ships": b.add_child(Lcars.scroll(_ship_entry(id)))
		"technologies": b.add_child(_tech_entry(id))
		"structures": b.add_child(_structure_entry(id))
		"minors": b.add_child(_minor_entry(id))
		_:
			var col := Lcars.vbox(10)
			if RACE_TO_EMPIRE.has(id):
				col.add_child(Lcars.emblem(RACE_TO_EMPIRE[id], 90))
			col.add_child(Lcars.para("One of the five major powers of the quadrant." if id <= 4 else "A minor race that can be befriended, courted for membership, or absorbed into an empire."))
			var n: int = _data().get("ships", []).filter(func(s): return int(s["race"]) == id).size()
			col.add_child(Lcars.label("%d ship designs in the reference catalogue." % n, "Muted"))
			b.add_child(col)

func _spec(pairs: Array) -> GridContainer:
	var g := Lcars.grid(5)
	g.add_theme_constant_override("h_separation", 30)
	g.add_theme_constant_override("v_separation", 10)
	for p in pairs:
		g.add_child(Lcars.stat(p[0], p[1]))
	return g

func _ship_entry(id: int) -> Control:
	var s := {}
	for x in _data().get("ships", []):
		if int(x["id"]) == id:
			s = x
	var col := Lcars.vbox(10)
	col.name = "ShipEntry"
	var r: Variant = s.get("roster")
	var h := Lcars.hbox(14)
	if RACE_TO_EMPIRE.has(int(s["race"])):
		h.add_child(Lcars.emblem(RACE_TO_EMPIRE[int(s["race"])], 54))
	var t := Lcars.vbox(0)
	t.add_child(Lcars.label("%s CLASS" % str(s["designClass"]).to_upper(), "Heading", 30))
	t.add_child(Lcars.caption("%s · %s%s" % [s["designName"], Roster.role_label(r["hullRole"]) if r is Dictionary else ROLE_NAMES.get(int(s["role"]), ""), (" · " + Roster.faction_label(r["faction"])) if r is Dictionary else ""]))
	h.add_child(t)
	col.add_child(h)
	var silhouette := ShipSilhouette.new()
	silhouette.seed_id = id
	silhouette.color = Lcars.owner_color(RACE_TO_EMPIRE.get(int(s["race"]), 0))
	silhouette.custom_minimum_size = Vector2(0, 150)
	col.add_child(silhouette)
	var tags := HFlowContainer.new()
	if r is Dictionary:
		tags.add_child(Lcars.badge(Roster.era_label(r["era"]), "primary"))
		tags.add_child(Lcars.badge(Roster.source_label(r["sourceKind"]), "good" if r["sourceKind"] == "Canon" else "warn"))
		tags.add_child(Lcars.badge("tier %d" % int(r["tier"]), "dim"))
		tags.add_child(Lcars.badge("buildable (Remastered)" if r.get("buildable", false) else "reference only", "good" if r.get("buildable", false) else "dim"))
		if r.get("cloak", false):
			tags.add_child(Lcars.badge("cloak", "warn"))
	else:
		tags.add_child(Lcars.badge("Legacy catalogue", "dim"))
	col.add_child(tags)
	var specs := [["Hull", s["hull"]], ["Shields", s["shields"]], ["Attack", Lcars.fmt(s["attack"])], ["Speed", s["speed"]], ["Range", s["range"]],
		["Sensors", r["sensors"]] if r is Dictionary else ["Troops", s["troops"] if int(s["troops"]) >= 0 else "-"], ["Industry", Lcars.fmt(s["industryCost"])], ["Upkeep", s["maintenance"]]]
	if r is Dictionary:
		specs.append(["Torpedo share", "%d%%" % int(r["torpedoSharePercent"])])
	col.add_child(_spec(specs))
	if r is Dictionary and r["weapons"].size() > 0:
		col.add_child(Lcars.caption("Weapon mounts"))
		for w in r["weapons"]:
			col.add_child(Lcars.label("%dx  %s  ·  %s  ·  %s" % [int(w["count"]), Roster.WEAPON_LABEL.get(w["type"], Roster.split_camel(str(w["type"]))), Roster.ARC_LABEL.get(w["arc"], w["arc"]), w["key"]], "", 14, Lcars.ALERT.lerp(Lcars.WARN, 0.5) if w.get("torpedo", false) else Lcars.SHIELD))
	col.add_child(Lcars.caption("Technology requirements"))
	var req := HFlowContainer.new()
	var fields: Array = app.meta.get("researchFields", [])
	var reqs: Array = s.get("requires", [])
	for i in reqs.size():
		req.add_child(Lcars.badge("%s %d" % [fields[i] if i < fields.size() else "?", int(reqs[i])], "primary" if int(reqs[i]) > 0 else "dim"))
	col.add_child(req)
	col.add_child(Lcars.para(str(s.get("description", "")), Lcars.TEXT2, 14))
	if r is Dictionary:
		col.add_child(Lcars.label("Source: %s · asset %s" % [r["source"], r["assetKey"]], "Mono", 12, Lcars.TEXT3))
	return col

func _tech_entry(id: int) -> Control:
	var t := {}
	for x in _data().get("technologies", []):
		if int(x["id"]) == id:
			t = x
	var col := Lcars.vbox(10)
	var fields: Array = app.meta.get("researchFields", [])
	col.add_child(Lcars.label(str(t["name"]).to_upper(), "Heading", 30))
	col.add_child(Lcars.caption("%s · level %d · %s points" % [fields[int(t["field"])], int(t["level"]), Lcars.fmt(t["cost"])]))
	if not app.view.is_empty():
		var have: bool = int(app.view["empire"]["techs"][int(t["field"])]["level"]) >= int(t["level"])
		col.add_child(Lcars.badge("researched" if have else "not yet", "good" if have else "dim"))
	col.add_child(Lcars.para(str(t["description"]), Lcars.TEXT2, 15))
	return col

func _structure_entry(id: int) -> Control:
	var s := {}
	for x in _data().get("buildings", []):
		if int(x["id"]) == id:
			s = x
	var col := Lcars.vbox(10)
	col.add_child(Lcars.label(str(s["name"]).to_upper(), "Heading", 30))
	col.add_child(_spec([["Industry", Lcars.fmt(s["industryCost"])], ["Energy", s["energyCost"]], ["Population", s["populationCost"]], ["Morale", s["moraleBonus"]]]))
	col.add_child(Lcars.para(str(s["description"]), Lcars.TEXT2, 15))
	col.add_child(Lcars.para("Reference catalogue entry. The remake's colony economy currently builds the core's own structure set.", Lcars.TEXT3, 13))
	return col

func _minor_entry(id: int) -> Control:
	var r := {}
	for x in _data().get("minorRaces", []):
		if int(x["raceId"]) == id:
			r = x
	var col := Lcars.vbox(10)
	col.name = "MinorEntry"
	col.add_child(Lcars.label("◇ " + str(r["raceName"]).to_upper(), "Heading", 30))
	col.add_child(Lcars.caption("Homeworld %s · %s" % [r["homeworld"], r["temperament"]]))
	col.add_child(Lcars.para(str(r["description"]), Lcars.TEXT2, 15))
	var sp := [["Temperament", r["temperament"]], ["Join bonus", r["bonus"]], ["Tech level", "%d (+ era)" % int(r["techLevel"])], ["Population", r["population"]]]
	if r.has("signatureShip"):
		sp.append(["Signature ship", r["signatureShip"]])
	col.add_child(_spec(sp))
	col.add_child(Lcars.caption("Affinity toward the major powers"))
	var aff := Lcars.hbox(16)
	var af: Array = r.get("affinity", [])
	for i in af.size():
		var h := Lcars.hbox(4)
		h.add_child(Lcars.emblem(i + 1, 18))
		var a := int(af[i])
		h.add_child(Lcars.label(("+%d" % a) if a > 0 else str(a), "Mono", 14, Lcars.GOOD if a > 0 else (Lcars.ALERT if a < 0 else Lcars.TEXT3)))
		aff.add_child(h)
	col.add_child(aff)
	return col
