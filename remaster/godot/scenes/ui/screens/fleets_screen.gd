extends Control
## Fleets presentation: the registry grouped by sector, task-force forming, and the unit detail with send-to,
## stance and orders. Data and orders: FleetsVM (scripts/viewmodels/fleets_vm.gd); map orders: GalaxyOrders.

var app: App
var vm: FleetsVM:
	get:
		if vm == null:
			vm = FleetsVM.new(app.session)
		return vm
var marks: Array:
	get: return vm.marks
	set(v): vm.marks = v
var tf_name: String:
	get: return vm.tf_name
	set(v): vm.tf_name = v
var dest: int:
	get: return vm.dest
	set(v): vm.dest = v

func refresh() -> void:
	for c in get_children():
		c.queue_free()
	var v := app.view
	var list := vm.units()
	var current := vm.current(list)
	var row := Lcars.hbox(14)
	Lcars.screen_margin(self).add_child(row)
	var reg := Lcars.frame("Fleet registry", "%d ships · %d task forces" % [v["fleets"].size(), v["taskForces"].size()], "primary", "fleets")
	reg.name = "Registry"
	reg.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(reg)
	var rb := Lcars.body(reg)
	var lst := Lcars.vbox(6)
	if list.is_empty():
		lst.add_child(Lcars.empty_note("You have no ships."))
	for sector in vm.by_sector(list):
		var sh := Lcars.hbox(8)
		sh.add_child(Lcars.label(str(sector[0]).to_upper(), "SubHeading", 18, Lcars.accent2()))
		sh.add_child(Lcars.label("%d ships" % int(sector[2]), "Muted", 12))
		lst.add_child(sh)
		for u in sector[1]:
			lst.add_child(_row(u, u == current))
	rb.add_child(Lcars.scroll(lst))
	var form := Lcars.hbox(8)
	var le := LineEdit.new()
	le.placeholder_text = "Task force name (optional)"
	le.text = vm.tf_name
	le.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	le.text_changed.connect(func(t): vm.tf_name = t)
	form.add_child(le)
	var fb := Lcars.pill("Form task force (%d)" % vm.marks.size(), "secondary", func():
		if vm.form(le.text):
			refresh(), vm.form_block())
	fb.name = "FormTaskForce"
	form.add_child(fb)
	form.add_child(Lcars.pill("Clear marks", "ghost", func(): vm.marks = []; refresh(), "" if vm.marks.size() > 0 else "Nothing marked"))
	rb.add_child(form)
	var detail := _detail(current) if not current.is_empty() else Lcars.frame("Unit")
	detail.custom_minimum_size.x = 640
	row.add_child(detail)

func _row(u: Dictionary, active: bool) -> Control:
	var lead: Dictionary = u["lead"]
	var tf: Dictionary = u["tf"]
	var r := Lcars.hbox(8)
	if tf.is_empty():
		var cb := CheckBox.new()
		cb.button_pressed = vm.marks.has(int(lead["id"]))
		var fid := int(lead["id"])
		cb.toggled.connect(func(on): vm.toggle_mark(fid, on); refresh())
		cb.tooltip_text = "Mark for a task force"
		r.add_child(cb)
	else:
		r.add_child(Lcars.badge("TF", "secondary"))
	var b := Lcars.row_button(active, 46)
	b.name = "FleetRow%d" % int(lead["id"])
	b.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	if active:
		b.add_theme_stylebox_override("normal", Lcars.box(Color(Lcars.accent(), 0.22), 12, Lcars.accent(), 1))
	var inner := Lcars.row_inner(b, 12)
	inner.add_child(ShipPicture.make(int(lead.get("owner", app.player())), ShipGlyph.class_of(lead), 34, "iso"))
	var nm := Lcars.vbox(-2)
	nm.alignment = BoxContainer.ALIGNMENT_CENTER
	nm.add_child(Lcars.label(str(tf["name"]) if not tf.is_empty() else str(lead["name"]), "", 15, Lcars.TEXT))
	nm.add_child(Lcars.label(("%d ships" % u["members"].size()) if not tf.is_empty() else "%s #%d" % [lead.get("designClass", lead["className"]), int(lead["id"])], "Muted", 12))
	nm.custom_minimum_size.x = 260
	inner.add_child(nm)
	var t := FleetsVM.totals(u["members"])
	inner.add_child(Lcars.meter(t["hull"], t["max_hull"], Color("#8fd18a"), 4))
	var st := vm.status(lead)
	var sl := Lcars.label(st, "Mono", 13, Lcars.accent2() if st != "idle" else Lcars.TEXT3)
	sl.custom_minimum_size.x = 200
	inner.add_child(sl)
	Lcars.ignore_mouse(inner)
	var fid2 := int(lead["id"])
	b.pressed.connect(func(): app.select_fleet(fid2); refresh())
	r.add_child(b)
	return r

func _show_on_chart(fid: int, x: int, y: int) -> void:
	app.select_fleet(fid)
	app.goto("galaxy")
	var s := app.current_screen()
	if s and s.has_method("focus_cell"):
		s.focus_cell(x, y)

func _detail(u: Dictionary) -> Control:
	var v := app.view
	var lead: Dictionary = u["lead"]
	var tf: Dictionary = u["tf"]
	var members: Array = u["members"]
	var f := Lcars.frame(str(tf["name"]) if not tf.is_empty() else str(lead["name"]), ("task force · %d ships" % members.size()) if not tf.is_empty() else "%s · #%d" % [lead.get("designClass", lead["className"]), int(lead["id"])], "secondary", "ship")
	f.name = "UnitDetail"
	var b := Lcars.body(f)
	var lock := vm.lock_reason()
	var top := Lcars.hbox(8)
	if tf.is_empty() and lead.get("roster") is Dictionary:
		var ro: Dictionary = lead["roster"]
		top.add_child(Lcars.badge(Roster.role_label(ro["hullRole"]), "primary"))
		top.add_child(Lcars.badge(Roster.era_label(ro["era"]), "dim"))
		top.add_child(Lcars.badge(Roster.source_label(ro["sourceKind"]), "good" if ro["sourceKind"] == "Canon" else "warn"))
	top.add_child(Lcars.spacer())
	var lx := int(lead["x"])
	var ly := int(lead["y"])
	var lid := int(lead["id"])
	top.add_child(Lcars.pill("Show on chart", "ghost", func(): _show_on_chart(lid, lx, ly)))
	b.add_child(top)
	var hero := ShipSilhouette.new()
	hero.seed_id = int(lead.get("referenceShipId", lead["id"]))
	hero.owner_id = int(lead.get("owner", app.player()))
	hero.klass = ShipGlyph.class_of(lead)
	hero.view = "iso"
	hero.color = Lcars.owner_color(app.player())
	hero.custom_minimum_size = Vector2(0, 210)
	hero.tooltip_text = "%s %s, hull picture." % [lead.get("designClass", ""), lead.get("designName", lead["className"])]
	b.add_child(hero)
	if tf.is_empty() and lead.get("roster") is Dictionary:
		b.add_child(Lcars.para(Roster.weapon_summary(lead["roster"].get("weapons", [])), Lcars.TEXT3, 13))
	var t := FleetsVM.totals(members)
	b.add_child(Lcars.bar_row("Hull", t["hull"], t["max_hull"], Color("#8fd18a")))
	b.add_child(Lcars.bar_row("Shields", t["shields"], t["max_shields"], Lcars.SHIELD))
	var g := Lcars.hbox(8)
	for k in [["attack", "Attack", Lcars.fmt(tf.get("attack", lead["attack"])), "", "Combined weapon strength. Stance and range decide how it is used in battle."],
			["speed", "Speed", "%d/t" % int(tf.get("speed", lead["speed"])), "", "Sectors per turn. A task force moves at its slowest ship's speed."],
			["range", "Range", str(t["range"]), "", "Supply range from a powered shipyard (shortest-ranged member). Outside it the unit cannot move further out."],
			["credits", "Upkeep", str(t["upkeep"]), "warn", "Credits per turn for these ships. Scrapping or disbanding removes the cost."]]:
		var kt := Lcars.kpi(k[0], k[1], k[2], "", k[3], {"title": k[1], "value": k[2], "effect": k[4]})
		kt.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		kt.custom_minimum_size.x = 0
		g.add_child(kt)
	b.add_child(g)
	# Send to
	var send := Lcars.hbox(8)
	send.add_child(Lcars.caption("Send to"))
	var ob := OptionButton.new()
	ob.name = "Destination"
	ob.add_item("Choose a known system...", 0)
	for s in vm.destinations(lead):
		ob.add_item("%s (%d.%d)" % [s["name"], int(s["x"]), int(s["y"])], int(s["id"]))
	ob.selected = max(0, ob.get_item_index(vm.dest)) if vm.dest != 0 else 0
	ob.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	ob.item_selected.connect(func(idx): vm.dest = ob.get_item_id(idx); refresh())
	send.add_child(ob)
	var plan := vm.send_plan(lead)
	var target: Dictionary = plan["target"]
	var eta_txt: String = plan["eta_text"]
	send.add_child(Lcars.label(eta_txt, "Mono", 14, Lcars.ALERT if eta_txt == "out of range" else Lcars.accent2()))
	send.add_child(Lcars.pill("Set course", "primary", func():
		if GalaxyOrders.plot_course(app, lead, int(target["x"]), int(target["y"])):
			refresh(), plan["block"]))
	b.add_child(send)
	if v["meta"].get("advancedRules", false):
		var srow := Lcars.hbox(4)
		srow.add_child(Lcars.caption("Stance"))
		var cur := vm.stance(u)
		var ids := GalaxyOrders.replace_ids(app, lead)
		for s in ["Balanced", "Aggressive", "Defensive", "Withdraw"]:
			var st: String = s
			var tg := Lcars.toggle(s, cur == s, func(_on): vm.set_stance(u, st, ids))
			tg.disabled = lock != ""
			srow.add_child(tg)
		b.add_child(srow)
	for a in GalaxyOrders.cell_actions(app, ly * app.width() + lx):
		if a["group"] == "orders":
			b.add_child(Lcars.pill(a["label"], a["tone"], a["run"], a["disabled"], a["detail"]))
	b.add_child(Lcars.caption("Orders this turn"))
	var drafted := vm.drafted(GalaxyOrders.replace_ids(app, lead))
	if drafted.is_empty():
		b.add_child(Lcars.label("None - %s." % ("continuing on course" if lead.get("route", []).size() > 0 else "holding position"), "Muted"))
	for d in drafted:
		b.add_child(Lcars.para("•  " + str(d["summary"]), Lcars.TEXT2, 13))
	b.add_child(Lcars.caption("Ships"))
	for m in members:
		var r2 := Lcars.hbox(8)
		var l2 := Lcars.label("%s  #%d" % [m["name"], int(m["id"])], "", 14)
		l2.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		r2.add_child(l2)
		r2.add_child(Lcars.meter(m["hull"], m["maxHull"], Color("#8fd18a"), 4, 120))
		if m.get("colonyShip", false):
			r2.add_child(Lcars.badge("colony", "good"))
		elif m.get("shipClass", "") == "Transport":
			r2.add_child(Lcars.badge("troops", "warn"))
		Lcars.tip(r2, "%s %s\nHull %d/%d · shields %d/%d · attack %d" % [m.get("designClass", ""), m.get("designName", m["className"]), int(m["hull"]), int(m["maxHull"]), int(m["shields"]), int(m["maxShields"]), int(m["attack"])])
		b.add_child(r2)
	if not tf.is_empty():
		var sp := Lcars.pill("Split task force", "tertiary", func(): vm.split(tf), lock)
		sp.name = "SplitTaskForce"
		b.add_child(sp)
	return f
