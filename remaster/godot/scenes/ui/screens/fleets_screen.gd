extends Control
## Fleets and task forces: registry grouped by sector, task-force forming, unit detail with
## send-to (route ETA from the core), stance and orders. Port of screens/Fleets.tsx.

var app: App
var marks: Array = []
var tf_name := ""
var dest := 0

func _units() -> Array:
	var v := app.view
	var out := []
	var seen := {}
	for f in v.get("fleets", []):
		var tf := int(f.get("taskForceId", 0))
		if tf != 0:
			if seen.has(tf):
				continue
			seen[tf] = true
			var members: Array = v["fleets"].filter(func(m): return int(m.get("taskForceId", 0)) == tf)
			out.append({"lead": f, "members": members, "tf": app.find_by_id("taskForces", tf)})
		else:
			out.append({"lead": f, "members": [f], "tf": {}})
	return out

func _place(x: int, y: int) -> String:
	var s := app.system_at(x, y)
	return str(s["name"]) if not s.is_empty() else "Sector %d.%d" % [x, y]

func refresh() -> void:
	for c in get_children():
		c.queue_free()
	var v := app.view
	var list := _units()
	var current := {}
	for u in list:
		for m in u["members"]:
			if int(m["id"]) == int(app.selection.get("fleetId", 0)):
				current = u
	if current.is_empty() and list.size() > 0:
		current = list[0]
	var m := MarginContainer.new()
	m.set_anchors_preset(Control.PRESET_FULL_RECT)
	for side in ["left", "right", "top", "bottom"]:
		m.add_theme_constant_override("margin_" + side, 16)
	add_child(m)
	var row := Lcars.hbox(14)
	m.add_child(row)
	var reg := Lcars.frame("Fleet registry", "%d ships · %d task forces" % [v["fleets"].size(), v["taskForces"].size()], "primary")
	reg.name = "Registry"
	reg.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(reg)
	var rb := Lcars.body(reg)
	var lst := Lcars.vbox(6)
	if list.is_empty():
		lst.add_child(Lcars.empty_note("You have no ships."))
	var sectors := {}
	var order := []
	for u in list:
		var key := _place(int(u["lead"]["x"]), int(u["lead"]["y"]))
		if not sectors.has(key):
			sectors[key] = []
			order.append(key)
		sectors[key].append(u)
	for key in order:
		var n := 0
		for u in sectors[key]:
			n += u["members"].size()
		var sh := Lcars.hbox(8)
		sh.add_child(Lcars.label(str(key).to_upper(), "SubHeading", 18, Lcars.accent2()))
		sh.add_child(Lcars.label("%d ships" % n, "Muted", 12))
		lst.add_child(sh)
		for u in sectors[key]:
			lst.add_child(_row(u, u == current))
	rb.add_child(Lcars.scroll(lst))
	var form := Lcars.hbox(8)
	var le := LineEdit.new()
	le.placeholder_text = "Task force name (optional)"
	le.text = tf_name
	le.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	le.text_changed.connect(func(t): tf_name = t)
	form.add_child(le)
	var cmd := {"type": "CreateTaskForce", "memberFleetIds": marks, "taskForceName": tf_name.strip_edges()}
	var reason := "Tick at least two ungrouped ships in the same sector" if marks.size() < 2 else app.dry_run(cmd)
	var fb := Lcars.pill("Form task force (%d)" % marks.size(), "secondary", func():
		var c2 := {"type": "CreateTaskForce", "memberFleetIds": marks, "taskForceName": le.text.strip_edges()}
		if app.issue(c2, [], "Task force formed from %d ships." % marks.size()):
			marks = []
			tf_name = ""
			refresh(), reason)
	fb.name = "FormTaskForce"
	form.add_child(fb)
	form.add_child(Lcars.pill("Clear marks", "ghost", func(): marks = []; refresh(), "" if marks.size() > 0 else "Nothing marked"))
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
		cb.button_pressed = marks.has(int(lead["id"]))
		var fid := int(lead["id"])
		cb.toggled.connect(func(on):
			if on: marks.append(fid)
			else: marks.erase(fid)
			refresh())
		cb.tooltip_text = "Mark for a task force"
		r.add_child(cb)
	else:
		r.add_child(Lcars.badge("TF", "secondary"))
	var b := Button.new()
	b.name = "FleetRow%d" % int(lead["id"])
	b.theme_type_variation = "GhostButton"
	b.custom_minimum_size.y = 46
	b.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	if active:
		b.add_theme_stylebox_override("normal", Lcars.box(Color(Lcars.accent(), 0.22), 12, Lcars.accent(), 1))
	var inner := Lcars.hbox(10)
	inner.set_anchors_preset(Control.PRESET_FULL_RECT)
	inner.offset_left = 12
	inner.offset_right = -12
	inner.mouse_filter = Control.MOUSE_FILTER_IGNORE
	inner.add_child(Lcars.emblem(app.player(), 18))
	var nm := Lcars.vbox(-2)
	nm.alignment = BoxContainer.ALIGNMENT_CENTER
	nm.add_child(Lcars.label(str(tf["name"]) if not tf.is_empty() else str(lead["name"]), "", 15, Lcars.TEXT))
	nm.add_child(Lcars.label(("%d ships" % u["members"].size()) if not tf.is_empty() else "%s #%d" % [lead.get("designClass", lead["className"]), int(lead["id"])], "Muted", 12))
	nm.custom_minimum_size.x = 260
	inner.add_child(nm)
	var hull := 0
	var mh := 0
	for mm in u["members"]:
		hull += int(mm["hull"])
		mh += int(mm["maxHull"])
	inner.add_child(Lcars.meter(hull, mh, Color("#8fd18a"), 4))
	var st := "idle"
	if lead.get("route", []).size() > 0:
		st = "→ %s · %dT" % [_place(int(lead["targetX"]), int(lead["targetY"])), int(lead["eta"])]
	elif lead.get("hasOrders", false):
		st = "ordered"
	elif lead.get("terraforming", false):
		st = "terraforming"
	var sl := Lcars.label(st, "Mono", 13, Lcars.accent2() if st != "idle" else Lcars.TEXT3)
	sl.custom_minimum_size.x = 200
	inner.add_child(sl)
	for n in inner.find_children("*", "Control", true, false):
		n.mouse_filter = Control.MOUSE_FILTER_IGNORE
	b.add_child(inner)
	var fid2 := int(lead["id"])
	b.pressed.connect(func(): app.select_fleet(fid2); refresh())
	r.add_child(b)
	return r

func _detail(u: Dictionary) -> Control:
	var v := app.view
	var lead: Dictionary = u["lead"]
	var tf: Dictionary = u["tf"]
	var members: Array = u["members"]
	var f := Lcars.frame(str(tf["name"]) if not tf.is_empty() else str(lead["name"]), ("task force · %d ships" % members.size()) if not tf.is_empty() else "%s · #%d" % [lead.get("designClass", lead["className"]), int(lead["id"])], "secondary")
	f.name = "UnitDetail"
	var b := Lcars.body(f)
	var lock := "Finish the battle in progress first" if v.get("session", {}).get("active", false) else ""
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
	top.add_child(Lcars.pill("Show on chart", "ghost", func():
		app.select_fleet(lid)
		app.goto("galaxy")
		var s := app.current_screen()
		if s and s.has_method("focus_cell"):
			s.focus_cell(lx, ly)))
	b.add_child(top)
	if tf.is_empty() and lead.get("roster") is Dictionary:
		b.add_child(Lcars.para(Roster.weapon_summary(lead["roster"].get("weapons", [])), Lcars.TEXT3, 13))
	var hull := 0
	var mh := 0
	var sh := 0
	var ms := 0
	var upkeep := 0
	var rng := 999
	for m in members:
		hull += int(m["hull"])
		mh += int(m["maxHull"])
		sh += int(m["shields"])
		ms += int(m["maxShields"])
		upkeep += int(m.get("maintenance", 0))
		rng = min(rng, int(m.get("range", 0)))
	for bar in [["Hull", hull, mh, Color("#8fd18a")], ["Shields", sh, ms, Lcars.SHIELD]]:
		var r := Lcars.hbox(8)
		var l := Lcars.label(bar[0], "Muted", 13)
		l.custom_minimum_size.x = 60
		r.add_child(l)
		r.add_child(Lcars.meter(bar[1], bar[2], bar[3], 8))
		r.add_child(Lcars.label("%d/%d" % [bar[1], bar[2]], "Mono", 13))
		b.add_child(r)
	var g := Lcars.grid(4)
	g.add_theme_constant_override("h_separation", 40)
	g.add_child(Lcars.stat("Attack", Lcars.fmt(tf.get("attack", lead["attack"]))))
	g.add_child(Lcars.stat("Speed", "%d/turn" % int(tf.get("speed", lead["speed"])), "", "A task force moves at its slowest ship's speed."))
	g.add_child(Lcars.stat("Range", rng, "", "Supply range from a powered shipyard (shortest-ranged member)."))
	g.add_child(Lcars.stat("Upkeep", upkeep, "warn"))
	b.add_child(g)
	# Send to
	var send := Lcars.hbox(8)
	send.add_child(Lcars.caption("Send to"))
	var ob := OptionButton.new()
	ob.name = "Destination"
	ob.add_item("Choose a known system...", 0)
	var dests: Array = v["systems"].filter(func(s): return not (int(s["x"]) == lx and int(s["y"]) == ly))
	for s in dests:
		ob.add_item("%s (%d.%d)" % [s["name"], int(s["x"]), int(s["y"])], int(s["id"]))
	ob.selected = max(0, ob.get_item_index(dest)) if dest != 0 else 0
	ob.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	ob.item_selected.connect(func(idx): dest = ob.get_item_id(idx); refresh())
	send.add_child(ob)
	var route := {}
	var target := app.find_by_id("systems", dest) if dest != 0 else {}
	if not target.is_empty():
		var rr: Variant = app.core.request("route", {"fleetId": lid, "x": int(target["x"]), "y": int(target["y"])})
		route = rr if rr is Dictionary else {}
	var eta_txt := ""
	if not route.is_empty():
		eta_txt = "out of range" if int(route["steps"]) < 0 else "%d turn%s" % [int(route["eta"]), "" if int(route["eta"]) == 1 else "s"]
	send.add_child(Lcars.label(eta_txt, "Mono", 14, Lcars.ALERT if eta_txt == "out of range" else Lcars.accent2()))
	var sreason := lock
	if sreason == "":
		sreason = "Choose a destination" if route.is_empty() else ("Outside supply range" if int(route["steps"]) < 0 else "")
	send.add_child(Lcars.pill("Set course", "primary", func():
		if GalaxyOrders.plot_course(app, lead, int(target["x"]), int(target["y"])):
			refresh(), sreason))
	b.add_child(send)
	if v["meta"].get("advancedRules", false):
		var srow := Lcars.hbox(4)
		srow.add_child(Lcars.caption("Stance"))
		var cur: String = tf.get("stance", lead.get("stance", "Balanced"))
		var ids := GalaxyOrders.replace_ids(app, lead)
		for s in ["Balanced", "Aggressive", "Defensive", "Withdraw"]:
			var st: String = s
			var t := Lcars.toggle(s, cur == s, func(_on):
				if not tf.is_empty():
					app.issue({"type": "SetTaskForceOrder", "taskForceId": int(tf["id"]), "targetX": int(tf["targetX"]), "targetY": int(tf["targetY"]), "stance": st, "targetFleetId": int(tf.get("attackTarget", 0))}, ids, "Stance updated.")
				else:
					app.issue({"type": "SetTacticalStance", "fleetId": lid, "stance": st}, ids, "Stance updated."))
			t.disabled = lock != ""
			srow.add_child(t)
		b.add_child(srow)
	for a in GalaxyOrders.cell_actions(app, ly * app.width() + lx):
		if a["group"] == "orders":
			b.add_child(Lcars.pill(a["label"], a["tone"], a["run"], a["disabled"], a["detail"]))
	b.add_child(Lcars.caption("Orders this turn"))
	var ids2 := GalaxyOrders.replace_ids(app, lead)
	var drafted: Array = v["pending"].filter(func(p): return ids2.has(int(p["id"])))
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
		r2.tooltip_text = "%s %s\nHull %d/%d · shields %d/%d · attack %d" % [m.get("designClass", ""), m.get("designName", m["className"]), int(m["hull"]), int(m["maxHull"]), int(m["shields"]), int(m["maxShields"]), int(m["attack"])]
		r2.mouse_filter = Control.MOUSE_FILTER_PASS
		b.add_child(r2)
	if not tf.is_empty():
		var tid := int(tf["id"])
		var tname: String = tf["name"]
		var sp := Lcars.pill("Split task force", "tertiary", func(): app.issue({"type": "DisbandTaskForce", "taskForceId": tid}, [], "%s split into individual ships." % tname), lock)
		sp.name = "SplitTaskForce"
		b.add_child(sp)
	return f
