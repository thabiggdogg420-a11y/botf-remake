extends Control
## Database (encyclopedia) presentation: the filtered list and the entry detail for ships (expanded roster),
## technologies, structures, races and minor races. Filters and lookups: DatabaseVM (scripts/viewmodels).

var app: App
var vm: DatabaseVM:
	get:
		if vm == null:
			vm = DatabaseVM.new(app.session)
		return vm
var tab: String:
	get: return vm.tab
	set(v): vm.tab = v
var query: String:
	get: return vm.query_text
	set(v): vm.query_text = v
var faction: String:
	get: return vm.faction
	set(v): vm.faction = v
var role: String:
	get: return vm.role
	set(v): vm.role = v
var era: String:
	get: return vm.era
	set(v): vm.era = v
var source: String:
	get: return vm.source
	set(v): vm.source = v
var field: int:
	get: return vm.field
	set(v): vm.field = v
var selected: int:
	get: return vm.selected
	set(v): vm.selected = v
var _list_box: VBoxContainer
var _detail_host: Control
var _count: Label

const ROLE_NAMES := DatabaseVM.ROLE_NAMES
const RACE_TO_EMPIRE := DatabaseVM.RACE_TO_EMPIRE

func _data() -> Dictionary:
	return vm.data()

func facets(s: Dictionary) -> Dictionary:
	return vm.facets(s)

func rows() -> Array:
	return vm.rows()

func refresh() -> void:
	for c in get_children():
		c.queue_free()
	var d := _data()
	if d.is_empty():
		add_child(Lcars.empty_note("Database unavailable."))
		return
	vm.init_faction()
	var row := Lcars.hbox(14)
	Lcars.screen_margin(self).add_child(row)
	var f := Lcars.frame("Database", "reference catalogue", "primary", "database")
	f.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	f.size_flags_stretch_ratio = 1.1
	row.add_child(f)
	var b := Lcars.body(f)
	var tabs := Lcars.tab_bar(DatabaseVM.TABS, tab, func(id): vm.pick_tab(id); refresh())
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
		var fx := vm.ship_facets()
		filters.add_child(_select("Faction", "All factions", fx["factions"], faction, func(v): faction = v; _fill_list(), func(x): return Roster.faction_label(x)))
		filters.add_child(_select("Role", "All roles", fx["roles"], role, func(v): role = v; _fill_list()))
		filters.add_child(_select("Era", "All eras", fx["eras"], era, func(v): era = v; _fill_list(), func(x): return "Legacy catalogue" if x == "Legacy" else Roster.era_label(x)))
		filters.add_child(_select("Source", "All sources", DatabaseVM.SOURCES, source, func(v): source = v; _fill_list(), func(x): return "Legacy catalogue" if x == "Legacy" else Roster.source_label(x)))
	elif tab in ["technologies", "structures"]:
		var ob := OptionButton.new()
		ob.add_item("All fields", 0)
		var fields := vm.fields()
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

func _fill_list() -> void:
	for c in _list_box.get_children():
		c.queue_free()
	var rs := rows()
	_count.text = "%d entries" % rs.size()
	var current := vm.current(rs)
	if rs.is_empty():
		_list_box.add_child(Lcars.empty_note("No matches."))
	for r in rs.slice(0, 400):
		var active: bool = not current.is_empty() and int(current["id"]) == int(r["id"])
		var btn := Lcars.row_button(active, 44, 10)
		var h := Lcars.row_inner(btn, 12)
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
		Lcars.ignore_mouse(h)
		var id := int(r["id"])
		btn.pressed.connect(func(): selected = id; _fill_list())
		_list_box.add_child(btn)
	_show_detail(current)

func _show_detail(r: Dictionary) -> void:
	for c in _detail_host.get_children():
		c.queue_free()
	var f := Lcars.frame(str(r.get("title", "Entry")), str(r.get("sub", "")), "secondary", {"ships": "ship", "technologies": "research", "structures": "build", "minors": "population"}.get(tab, "empire"))
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
				col.add_child(PortraitSlot.make(RACE_TO_EMPIRE[id], Vector2(420, 236), "", str(r.get("title", ""))))
			col.add_child(Lcars.para("One of the five major powers of the quadrant." if id <= 4 else "A minor race that can be befriended, courted for membership, or absorbed into an empire."))
			var n := vm.ships_of_race(id)
			col.add_child(Lcars.label("%d ship designs in the reference catalogue." % n, "Muted"))
			b.add_child(col)

const SPEC_ICONS := {"Hull": "hull", "Shields": "shield", "Attack": "attack", "Speed": "speed", "Range": "range", "Sensors": "eye", "Troops": "population",
	"Industry": "industry", "Upkeep": "credits", "Torpedo share": "combat", "Energy": "energy", "Population": "population", "Morale": "morale",
	"Temperament": "diplomacy", "Join bonus": "good", "Tech level": "research", "Signature ship": "ship"}
const SPEC_TIPS := {"Hull": "Damage the hull absorbs before the ship is lost.", "Shields": "Absorb damage first and recharge between rounds.", "Attack": "Combined weapon strength per round.",
	"Speed": "Sectors per turn on the galaxy map.", "Range": "Sectors from a powered shipyard the ship can operate.", "Industry": "Industry to build it.", "Upkeep": "Credits per turn while in service.",
	"Energy": "Power the structure draws.", "Population": "Workers the structure needs.", "Morale": "Morale it adds to the colony."}

## Key figures as tiles with icons (docs/UI-DESIGN-SYSTEM.md §5).
func _spec(pairs: Array) -> GridContainer:
	var g := Lcars.grid(4)
	g.add_theme_constant_override("h_separation", 8)
	g.add_theme_constant_override("v_separation", 8)
	for p in pairs:
		var t := Lcars.kpi(SPEC_ICONS.get(p[0], "info"), p[0], p[1], "", "", {"title": p[0], "value": Lcars.num(p[1]), "effect": SPEC_TIPS.get(p[0], "")})
		t.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		t.custom_minimum_size.x = 0
		g.add_child(t)
	return g

func _ship_entry(id: int) -> Control:
	var s := vm.entry("ships", id)
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
	silhouette.custom_minimum_size = Vector2(0, 190)
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
	var fields := vm.fields()
	var reqs: Array = s.get("requires", [])
	for i in reqs.size():
		req.add_child(Lcars.badge("%s %d" % [fields[i] if i < fields.size() else "?", int(reqs[i])], "primary" if int(reqs[i]) > 0 else "dim"))
	col.add_child(req)
	col.add_child(Lcars.para(str(s.get("description", "")), Lcars.TEXT2, 14))
	if r is Dictionary:
		col.add_child(Lcars.label("Source: %s · asset %s" % [r["source"], r["assetKey"]], "Mono", 12, Lcars.TEXT3))
	return col

func _tech_entry(id: int) -> Control:
	var t := vm.entry("technologies", id)
	var col := Lcars.vbox(10)
	var fields := vm.fields()
	col.add_child(Lcars.label(str(t["name"]).to_upper(), "Heading", 30))
	col.add_child(Lcars.caption("%s · level %d · %s points" % [fields[int(t["field"])], int(t["level"]), Lcars.fmt(t["cost"])]))
	if not app.view.is_empty():
		var have := vm.researched(t)
		col.add_child(Lcars.badge("researched" if have else "not yet", "good" if have else "dim"))
	col.add_child(Lcars.para(str(t["description"]), Lcars.TEXT2, 15))
	return col

func _structure_entry(id: int) -> Control:
	var s := vm.entry("buildings", id)
	var col := Lcars.vbox(10)
	col.add_child(Lcars.label(str(s["name"]).to_upper(), "Heading", 30))
	col.add_child(_spec([["Industry", Lcars.fmt(s["industryCost"])], ["Energy", s["energyCost"]], ["Population", s["populationCost"]], ["Morale", s["moraleBonus"]]]))
	col.add_child(Lcars.para(str(s["description"]), Lcars.TEXT2, 15))
	col.add_child(Lcars.para("Reference catalogue entry. The remake's colony economy currently builds the core's own structure set.", Lcars.TEXT3, 13))
	return col

func _minor_entry(id: int) -> Control:
	var r := vm.entry("minorRaces", id, "raceId")
	var col := Lcars.vbox(10)
	col.name = "MinorEntry"
	col.add_child(PortraitSlot.make(0, Vector2(420, 236), PortraitSlot.minor_key(str(r["raceName"])), str(r["raceName"])))
	col.add_child(Lcars.label(str(r["raceName"]).to_upper(), "Heading", 30))
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
