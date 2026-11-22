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
	var reg := Lcars.frame("Fleet registry", "%d · TF %d" % [v["fleets"].size(), v["taskForces"].size()], "primary", "fleets")
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
		sh.add_child(Pic.num("fleet", "%d" % int(sector[2]), "Ships in this sector", Lcars.TEXT3, 16, 13))
		lst.add_child(sh)
		for u in sector[1]:
			lst.add_child(_row(u, u == current))
	rb.add_child(Lcars.scroll(lst))
	var form := Lcars.hbox(8)
	var le := LineEdit.new()
	le.placeholder_text = "TF name"
	le.tooltip_text = "Task force name (optional)"
	le.text = vm.tf_name
	le.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	le.text_changed.connect(func(t): vm.tf_name = t)
	form.add_child(le)
	var fb := Lcars.pill("Form task force (%d)" % vm.marks.size(), "secondary", func():
		if vm.form(le.text):
			refresh(), vm.form_block())
	fb.name = "FormTaskForce"
	form.add_child(fb)
	form.add_child(Lcars.pill("Clear", "ghost", func(): vm.marks = []; refresh(), "" if vm.marks.size() > 0 else "Nothing marked", "Clear the marks"))
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
	var b := Lcars.row_button(active, 66)
	b.name = "FleetRow%d" % int(lead["id"])
	b.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	if active:
		b.add_theme_stylebox_override("normal", Lcars.box_lcars(Color(Lcars.accent(), 0.22), 12, Lcars.accent(), 1))
	var inner := Lcars.row_inner(b, 12)
	inner.add_child(ShipPicture.make(int(lead.get("owner", app.player())), ShipGlyph.class_of(lead), 58, "iso"))
	var nm := Lcars.vbox(-2)
	nm.alignment = BoxContainer.ALIGNMENT_CENTER
	nm.add_child(Lcars.label(str(tf["name"]) if not tf.is_empty() else str(lead["name"]), "", 15, Lcars.TEXT))
	nm.add_child(Lcars.label(("TF x%d" % u["members"].size()) if not tf.is_empty() else "%s #%d" % [lead.get("designClass", lead["className"]), int(lead["id"])], "Muted", 12))
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
	var f := Lcars.frame(str(tf["name"]) if not tf.is_empty() else str(lead["name"]), ("TF x%d" % members.size()) if not tf.is_empty() else "%s · #%d" % [lead.get("designClass", lead["className"]), int(lead["id"])], "secondary", "ship")
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
	hero.custom_minimum_size = Vector2(0, 260)
	hero.tooltip_text = "%s %s, hull picture." % [lead.get("designClass", ""), lead.get("designName", lead["className"])]
	b.add_child(hero)
	if tf.is_empty() and lead.get("roster") is Dictionary:
		hero.tooltip_text += "
" + Roster.weapon_summary(lead["roster"].get("weapons", []))
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
	send.add_child(Pic.tipped(Lcars.icon("route" if UiIcon.NAMES.has("route") else "routes", 24, Lcars.accent2()), "Send to a known system"))
	var ob := OptionButton.new()
	ob.name = "Destination"
	ob.add_item("Destination...", 0)
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
		var cur := vm.stance(u)
		var sicons := {"Balanced": "fleet", "Aggressive": "attack", "Defensive": "shield", "Withdraw": "delta_left"}
		var ids := GalaxyOrders.replace_ids(app, lead)
		for s in ["Balanced", "Aggressive", "Defensive", "Withdraw"]:
			var st: String = s
			var tg := Pic.icon_toggle(sicons[s], cur == s, func(_on): vm.set_stance(u, st, ids), "%s: how this unit behaves when battle is joined." % s, 20.0, Vector2(52, 38))
			tg.disabled = lock != ""
			srow.add_child(tg)
		srow.add_child(Lcars.label(cur, "Muted", 14))
		b.add_child(srow)
	if v["meta"].get("rules", {}).get("crewExperience", false):
		var xp := int(lead.get("experience", 25))
		var crow := Lcars.hbox(8)
		crow.name = "CrewExperience"
		crow.add_child(Pic.tipped(Lcars.icon("morale", 22, Color("#e8b44a")), "Crew: " + str(lead.get("crewRating", "Regular"))))
		crow.add_child(Lcars.badge(str(lead.get("crewRating", "Regular")), "primary"))
		var xm := Lcars.meter(xp, 100, Color("#e8b44a"), 8)
		xm.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		crow.add_child(xm)
		crow.add_child(Lcars.label("%d" % xp, "Mono", 13))
		Lcars.tip(crow, "Crew experience %d of 100. Battles, raids and academies raise it; it sets accuracy and repair in battle." % xp)
		b.add_child(crow)
	if vm.flagships_on() and members.size() == 1 and int(lead.get("attack", 0)) > 0 and not lead.get("colonyShip", false) and lead.get("shipClass", "") != "Transport":
		var frow := Lcars.hbox(8)
		frow.name = "Flagship"
		frow.add_child(Pic.tipped(Lcars.icon("victory", 22, Lcars.WARN), "Flagship"))
		if bool(lead.get("flagship", false)):
			frow.add_child(Lcars.badge("Flagship · " + str(lead.get("flagshipName", "")), "good"))
			if lead.has("renameBlocker"):   # E4: the player names their flagship
				var ne := LineEdit.new()
				ne.name = "FlagshipName"
				ne.text = str(lead.get("flagshipName", ""))
				ne.max_length = vm.flagship_name_limit()
				ne.custom_minimum_size.x = 190
				ne.tooltip_text = "Rename the flagship (up to %d characters). The new name shows in battle logs and is saved with the game; an empty name restores the original." % vm.flagship_name_limit()
				ne.editable = str(lead.get("renameBlocker", "")) == "" and lock == ""
				frow.add_child(ne)
				var rn := Lcars.pill("Rename", "tertiary", func(): vm.rename_flagship(lead, ne.text), str(lead.get("renameBlocker", "")) if str(lead.get("renameBlocker", "")) != "" else lock, "Give the flagship this name")
				rn.name = "RenameFlagship"
				frow.add_child(rn)
		else:
			var blocker := str(lead.get("flagshipBlocker", ""))
			var cm := Lcars.pill("Commission flagship", "tertiary", func(): vm.commission_flagship(lead), blocker if blocker != "" else lock, vm.flagship_tip())
			cm.name = "CommissionFlagship"
			frow.add_child(cm)
		b.add_child(frow)
	if vm.can_order(lead):
		var orow := Lcars.hbox(4)
		orow.name = "StandingOrders"
		orow.add_child(Pic.tipped(Lcars.icon("threat", 22, Lcars.ALERT), "Standing order"))
		var oids := GalaxyOrders.replace_ids(app, lead)
		var cur_o := int(lead.get("order", 0))
		var rules := vm.rules()
		if rules.get("interceptOrders", false):
			var tgi := Lcars.toggle("Intercept", cur_o == 1, func(_on): vm.set_standing(lead, 1, 0, oids), "Hold this sector as a post: hostile ships that come within range are attacked, and the ship returns when none is.")
			tgi.name = "OrderIntercept"
			tgi.disabled = lock != ""
			orow.add_child(tgi)
		if rules.get("raidOrders", false):
			var rb := OptionButton.new()
			rb.name = "RaidTarget"
			rb.add_item("Raid a colony...", 0)
			for s2 in vm.raid_targets(lead):
				rb.add_item(str(s2["name"]), int(s2["id"]))
			rb.item_selected.connect(func(idx): if rb.get_item_id(idx) != 0: vm.set_standing(lead, 2, rb.get_item_id(idx), oids))
			orow.add_child(rb)
		if cur_o != 0:
			orow.add_child(Lcars.pill("Stand down", "ghost", func(): vm.set_standing(lead, 0, 0, oids), lock))
		b.add_child(orow)
		orow.tooltip_text = "Current: " + vm.standing_text(lead)
		if cur_o != 0:
			orow.add_child(Lcars.label(vm.standing_text(lead), "Muted", 13))
	for a in GalaxyOrders.cell_actions(app, ly * app.width() + lx):
		if a["group"] == "orders":
			b.add_child(Lcars.pill(a["label"], a["tone"], a["run"], a["disabled"], a["detail"]))
	var drafted := vm.drafted(GalaxyOrders.replace_ids(app, lead))
	var ordr := Lcars.vbox(2)
	var oh := Lcars.hbox(8)
	oh.add_child(Lcars.icon("orders", 20, Lcars.accent2()))
	if drafted.is_empty():
		oh.add_child(Lcars.label("-", "Muted"))
		oh.tooltip_text = "No orders - %s." % ("continuing on course" if lead.get("route", []).size() > 0 else "holding position")
		oh.mouse_filter = Control.MOUSE_FILTER_PASS
	ordr.add_child(oh)
	for d in drafted:
		var dl := Lcars.label(str(d["summary"]), "", 13, Lcars.TEXT2)
		dl.clip_text = true
		dl.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
		ordr.add_child(dl)
	b.add_child(ordr)
	for m in members:
		var r2 := Lcars.hbox(8)
		r2.add_child(ShipPicture.make(int(m.get("owner", app.player())), ShipGlyph.class_of(m), 44, "iso"))
		var l2 := Lcars.label("#%d" % int(m["id"]), "Mono", 13, Lcars.TEXT3)
		l2.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		r2.add_child(l2)
		r2.add_child(Lcars.meter(m["hull"], m["maxHull"], Color("#8fd18a"), 4, 120))
		if m.get("colonyShip", false):
			r2.add_child(Pic.tipped(Lcars.icon("colony", 20, Lcars.GOOD), "Colony ship"))
		elif m.get("shipClass", "") == "Transport":
			r2.add_child(Pic.tipped(Lcars.icon("population", 20, Lcars.WARN), "Troop transport"))
		Lcars.tip(r2, "%s %s\nHull %d/%d · shields %d/%d · attack %d" % [m.get("designClass", ""), m.get("designName", m["className"]), int(m["hull"]), int(m["maxHull"]), int(m["shields"]), int(m["maxShields"]), int(m["attack"])])
		b.add_child(r2)
	if not tf.is_empty():
		var sp := Lcars.pill("Split task force", "tertiary", func(): vm.split(tf), lock)
		sp.name = "SplitTaskForce"
		b.add_child(sp)
	return f
