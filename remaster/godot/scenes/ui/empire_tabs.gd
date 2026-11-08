class_name EmpireTabs
extends RefCounted
## Empire-wide managers shown as tabs on the Empire screen: governors for every colony, build-queue
## templates, stations (with the starbase upgrade), and member minor worlds (with secession risk).
## Each builder returns a Control (presentation only); data and orders come from the screen's EmpireVM.

const SECESSION_LINE := EmpireVM.SECESSION_LINE
const JOIN_LINE := EmpireVM.JOIN_LINE

static func _lock(app: App) -> String:
	return app.session.lock_reason()

# ---------------------------------------------------------------------------------------------
# Governors
# ---------------------------------------------------------------------------------------------
static func governors(app: App, vm: EmpireVM = null) -> Control:
	if vm == null:
		vm = EmpireVM.new(app.session)
	var col := Lcars.vbox(8)
	col.name = "GovernorManager"
	var lock := _lock(app)
	var opts: Array = app.meta.get("governors", [])
	col.add_child(Lcars.para("A governor staffs a colony's citizens (workforce) or refills its build queue when it runs empty (queue) every turn. Manual staffing overrides the workforce governor until you resume it.", Lcars.TEXT3, 13))
	var all := Lcars.hbox(6)
	all.add_child(Lcars.caption("Set every colony"))
	for which in ["workforce", "queue"]:
		var w: String = which
		var l := Lcars.label(w.capitalize() + ":", "Muted", 13)
		all.add_child(l)
		for o in opts:
			var key: String = o["key"]
			var b := Lcars.pill(str(o["name"]), "ghost", func():
				var n := vm.set_all_governors(w, key)
				app.toast("All %d colonies: %s governor %s." % [n, w, key], "good"), lock, "Apply to all %d colonies" % app.view["colonies"].size())
			b.name = "All_%s_%s" % [w, key]
			all.add_child(b)
	col.add_child(all)
	var list := Lcars.vbox(4)
	for c in app.view["colonies"]:
		var sid := int(c["systemId"])
		var g: Dictionary = c["governor"]
		var row := Lcars.hbox(6)
		row.name = "GovRow_%d" % sid
		var nm := Lcars.label("%s  %d pop" % [c["name"], int(c["population"])], "", 15, Lcars.TEXT)
		nm.custom_minimum_size.x = 210
		row.add_child(nm)
		for which in ["workforce", "queue"]:
			var w2: String = which
			var cap := Lcars.caption(w2.capitalize())
			cap.custom_minimum_size.x = 76
			row.add_child(cap)
			for o in opts:
				var key2: String = o["key"]
				var t := Lcars.toggle(str(o["name"]), str(g[w2]) == key2, func(_on):
					vm.set_governor(sid, key2 if w2 == "workforce" else str(g["workforce"]), key2 if w2 == "queue" else str(g["queue"])))
				t.add_theme_font_size_override("font_size", 13)
				t.disabled = lock != ""
				t.name = "Gov_%s_%d_%s" % [w2, sid, key2]
				row.add_child(t)
		if g.get("override", false) and str(g["workforce"]) != "Manual":
			row.add_child(Lcars.pill("Resume governor", "primary", func(): vm.resume_governor(sid), lock, "Manual staffing is overriding the governor"))
		list.add_child(row)
		var note := EmpireVM.governor_note(g)
		var why: String = note[0]
		if why != "":
			var p := Lcars.para(why, Lcars.WARN if note[1] else Lcars.TEXT3, 12)
			p.custom_minimum_size.x = 400
			list.add_child(p)
	col.add_child(Lcars.scroll(list))
	return col

# ---------------------------------------------------------------------------------------------
# Build templates
# ---------------------------------------------------------------------------------------------
static func templates(app: App, vm: EmpireVM = null) -> Control:
	if vm == null:
		vm = EmpireVM.new(app.session)
	var state := vm.tab_state
	var col := Lcars.vbox(8)
	col.name = "TemplateManager"
	var v := app.view
	var lock := _lock(app)
	var temps: Array = v.get("templates", [])
	col.add_child(Lcars.para("Templates are saved build queues. Apply one to any colony; the default template is applied to every new colony with an empty queue.", Lcars.TEXT3, 13))
	for t in temps:
		var tid := int(t["id"])
		var tname: String = t["name"]
		var card := Lcars.card()
		card.name = "Template_%d" % tid
		var cv := Lcars.vbox(4)
		card.add_child(cv)
		var h := Lcars.hbox(6)
		h.add_child(Lcars.label(tname, "SubHeading", 17, Lcars.TEXT))
		if t.get("isDefault", false):
			h.add_child(Lcars.badge("default", "good"))
		cv.add_child(h)
		cv.add_child(Lcars.para(" · ".join(t["items"].map(func(i): return i["name"])), Lcars.TEXT3, 12))
		var acts := Lcars.hbox(4)
		var accepted := vm.accepted_applies(tid)  # dryRunMany: which idle colonies would accept this template
		var reason := vm.apply_block(accepted)
		var ap := Lcars.pill("Apply to %d idle colon%s" % [accepted.size(), "y" if accepted.size() == 1 else "ies"], "secondary", func():
			vm.apply_template_to(accepted)
			app.toast("“%s” applied to %d colonies." % [tname, accepted.size()], "good"), reason, "Appends the template to every colony whose build queue is empty (checked with the core first)")
		ap.name = "ApplyIdle_%d" % tid
		acts.add_child(ap)
		var is_def: bool = t.get("isDefault", false)
		acts.add_child(Lcars.pill("Clear default" if is_def else "Make default", "ghost", func(): vm.set_default_template(tid, is_def), lock))
		acts.add_child(Lcars.pill("Delete", "alert", func(): app.confirm("Delete “%s”?" % tname, "Queues it was applied to are not affected.", "Delete", func(): vm.delete_template(tid)), lock))
		cv.add_child(acts)
		col.add_child(card)
	if temps.is_empty():
		col.add_child(Lcars.empty_note("No templates yet. Save a colony's queue below."))
	# New template from a colony's queue
	var nr := Lcars.hbox(6)
	nr.add_child(Lcars.caption("Save queue of"))
	var ob := OptionButton.new()
	ob.name = "TemplateSource"
	var sel := 0
	for i in v["colonies"].size():
		var c: Dictionary = v["colonies"][i]
		ob.add_item("%s (%d queued)" % [c["name"], int(c.get("queueLength", 0))], int(c["systemId"]))
		if int(c["systemId"]) == int(state.get("template_source", 0)):
			sel = i
	ob.selected = sel
	ob.item_selected.connect(func(i): state["template_source"] = ob.get_item_id(i))
	nr.add_child(ob)
	var le := LineEdit.new()
	le.placeholder_text = "Template name"
	le.max_length = 48
	le.name = "TemplateName"
	le.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	nr.add_child(le)
	var save := Lcars.pill("Save as template", "secondary", func():
		var n := le.text.strip_edges()
		if n == "":
			app.toast("Name the template", "warn")
			return
		var sid := ob.get_selected_id()
		var its := vm.queue_items(sid)
		if its.is_empty():
			app.toast("That colony's build queue is empty.", "warn")
			return
		vm.define_template(0, n, its, "Template “%s” saved." % n), lock)
	save.name = "SaveTemplate"
	nr.add_child(save)
	col.add_child(nr)
	return col

# ---------------------------------------------------------------------------------------------
# Stations and the starbase upgrade
# ---------------------------------------------------------------------------------------------
static func stations(app: App, vm: EmpireVM = null) -> Control:
	if vm == null:
		vm = EmpireVM.new(app.session)
	var col := Lcars.vbox(8)
	col.name = "StationManager"
	var mine := vm.my_stations()
	var cost: Dictionary = app.view.get("stationUpgradeCost", {})
	col.add_child(Lcars.para("Outposts extend supply (radius 3) and sensors (2). A starbase fights harder, repairs ships in its sector and has supply radius 5, sensors 3. Upgrading costs ¢%s up front and %s industry from one colony within 6 sectors, whose build queue pauses meanwhile; it also needs Construction level 2." % [str(cost.get("credits", "?")), str(cost.get("industry", "?"))], Lcars.TEXT3, 13))
	if mine.is_empty():
		col.add_child(Lcars.empty_note("You have no stations. A transport can build an outpost at the end of a turn (Orders menu: Build outpost)."))
	var battle := _lock(app)
	for st in mine:
		var card := Lcars.card()
		card.name = "Station_%d" % int(st["id"])
		var cv := Lcars.vbox(4)
		card.add_child(cv)
		var h := Lcars.hbox(8)
		h.add_child(Lcars.label(str(st["name"]), "SubHeading", 17, Lcars.TEXT))
		h.add_child(Lcars.badge(str(st["typeName"]), "primary" if st["type"] == "Starbase" else "dim"))
		h.add_child(Lcars.spacer())
		var sx := int(st["x"])
		var sy := int(st["y"])
		h.add_child(Lcars.pill("Show on chart", "ghost", func():
			app.goto("galaxy")
			var s := app.current_screen()
			if s and s.has_method("focus_cell"):
				s.focus_cell(sx, sy)))
		cv.add_child(h)
		var g := Lcars.grid(5)
		g.add_theme_constant_override("h_separation", 30)
		g.add_child(Lcars.stat("Hull", "%d/%d" % [int(st["hull"]), int(st["maxHull"])]))
		g.add_child(Lcars.stat("Attack", "%d beam · %d torp" % [int(st.get("beam", 0)), int(st.get("torpedo", 0))]))
		g.add_child(Lcars.stat("Supply / sensors", "%d / %d" % [int(st["supplyRadius"]), int(st["sensorRadius"])], "", "Sectors around the station inside which your ships are supplied / which you can see."))
		g.add_child(Lcars.stat("Upkeep", "¢%d" % int(st["upkeep"]), "warn", "Charged every turn as military upkeep."))
		g.add_child(Lcars.stat("Repairs", "%d%% hull/turn" % int(st.get("repairPercent", 0)), "", "Friendly ships in this sector that did not fight this turn, while no enemy at war is present."))
		cv.add_child(g)
		var up := GalaxyOrders.upgrade_action(app, st, battle)
		if not up.is_empty():
			var u := Lcars.pill(str(up["label"]), "good", up["run"], str(up["disabled"]), str(up["detail"]))
			u.name = "UpgradeStation_%d" % int(st["id"])
			cv.add_child(u)
			cv.add_child(Lcars.para(str(up["detail"]) if str(up["disabled"]) == "" else str(up["disabled"]), Lcars.TEXT3 if str(up["disabled"]) == "" else Lcars.WARN, 12))
		col.add_child(card)
	return col

# ---------------------------------------------------------------------------------------------
# Member and candidate minor worlds
# ---------------------------------------------------------------------------------------------
const LOYALTY := EmpireVM.LOYALTY

static func members(app: App, vm: EmpireVM = null) -> Control:
	if vm == null:
		vm = EmpireVM.new(app.session)
	var col := Lcars.vbox(8)
	col.name = "MemberManager"
	var mw := vm.minor_worlds()
	if mw.is_empty():
		col.add_child(Lcars.empty_note("Minor-race data unavailable: %s" % app.core.last_error))
		return col
	var mem: Array = mw["members"]
	var cand: Array = mw["candidates"]
	col.add_child(Lcars.para("A member world adds its bonus to every colony. It secedes, and the bonus is lost, if its attitude toward you falls below %d (and you hold another colony). Every 5 turns its attitude drifts: -2 for rising expectations, +2 while its morale is 70+, -2 per extra war you fight." % SECESSION_LINE, Lcars.TEXT3, 13))
	if mem.is_empty():
		col.add_child(Lcars.empty_note("No member worlds. Befriend a minor race, then offer membership."))
	for m in mem:
		col.add_child(_member_card(app, m, true))
	if cand.size() > 0:
		col.add_child(Lcars.caption("Independent minor races"))
		for m in cand:
			col.add_child(_member_card(app, m, false))
	return Lcars.scroll(col)

static func _member_card(app: App, m: Dictionary, is_member: bool) -> Control:
	var card := Lcars.card()
	card.name = "Minor_%d" % int(m["systemId"])
	var cv := Lcars.vbox(4)
	card.add_child(cv)
	var att := int(m["attitude"])
	var h := Lcars.hbox(8)
	h.add_child(Lcars.label("◇ " + str(m["name"]), "SubHeading", 17, Lcars.TEXT))
	h.add_child(Lcars.label(str(m.get("system", "")), "Muted", 12))
	if m.get("race") is Dictionary:
		h.add_child(Lcars.badge(str(m["race"]["temperament"]), "dim"))
	h.add_child(Lcars.spacer())
	var stand := EmpireVM.minor_standing(m, is_member)
	var margin: int = stand["margin"]
	var bad: bool = stand["bad"]
	var tip: String = stand["tip"]
	var bar := Lcars.meter(att, 100, Lcars.ALERT if bad and is_member else Lcars.GOOD, 8, 160)
	bar.tooltip_text = tip
	h.add_child(bar)
	h.add_child(Lcars.label(str(att), "Mono", 15))
	cv.add_child(h)
	if is_member:
		var risk := "Safe" if margin >= 15 else ("AT RISK - %d above the secession line" % margin if margin >= 0 else "Seceding")
		cv.add_child(Lcars.label("Secession: %s (line %d)" % [risk, SECESSION_LINE], "", 13, Lcars.ALERT if margin < 15 else Lcars.GOOD))
	else:
		var checks := [
			["Attitude %d of %d" % [att, JOIN_LINE], att >= JOIN_LINE],
			["Friendship signed", bool(m.get("friendship", false))],
		]
		var parts := []
		for ck in checks:
			parts.append(("✓ " if ck[1] else "✗ ") + str(ck[0]))
		cv.add_child(Lcars.label("Membership: " + "   ".join(parts), "", 13, Lcars.GOOD if checks.all(func(c): return c[1]) else Lcars.TEXT2))
	if m.get("race") is Dictionary:
		var race: Dictionary = m["race"]
		cv.add_child(Lcars.label("Join bonus: %s" % race["bonus"], "", 13, Lcars.GOOD))
		cv.add_child(Lcars.label(LOYALTY.get(str(race["temperament"]), ""), "Muted", 12))
	if m.has("techLevel"):
		cv.add_child(Lcars.label("Tech level %d (%d/%d to the next)" % [int(m["techLevel"]), int(m.get("progress", 0)), int(m.get("progressNeeded", 0))], "Muted", 12))
	var sid := int(m["systemId"])
	cv.add_child(Lcars.pill("Open in Diplomacy", "ghost", func(): app.set_meta("diplomacy_focus", {"minor": true, "id": sid}); app.goto("diplomacy")))
	return card
