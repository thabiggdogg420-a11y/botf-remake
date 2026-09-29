extends Control
## Intelligence: budget split between security and per-rival espionage/sabotage, department focus,
## missions and reports. Port of screens/Intel.tsx.

var app: App
var intel: Dictionary = {}
var budget := {"security": 100, "rivals": {}}
var original := {}
var tab := "operations"
var highlight := 0
var targets := {}
var _sec_label: Label
var _sec_meter: ProgressBar
var _apply: Button
var _labels := {}
var _sliders := {}

func _from_view() -> Dictionary:
	var b := {"security": int(intel.get("budget", {}).get("security", 100)), "rivals": {}}
	for r in intel.get("rivals", []):
		var a := {"espionage": 0, "sabotage": 0}
		for x in intel["budget"].get("rivals", []):
			if int(x["id"]) == int(r["id"]):
				a = {"espionage": int(x["espionage"]), "sabotage": int(x["sabotage"])}
		b["rivals"][int(r["id"])] = a
	return b

func refresh() -> void:
	for c in get_children():
		c.queue_free()
	var got: Variant = app.core.request("intel")
	intel = got if got is Dictionary else {}
	if intel.is_empty():
		add_child(Lcars.empty_note("Intelligence unavailable."))
		return
	if app.has_meta("intel_focus"):
		highlight = int(app.get_meta("intel_focus"))
		app.remove_meta("intel_focus")
	budget = _from_view()
	original = budget.duplicate(true)
	var m := MarginContainer.new()
	m.set_anchors_preset(Control.PRESET_FULL_RECT)
	for side in ["left", "right", "top", "bottom"]:
		m.add_theme_constant_override("margin_" + side, 16)
	add_child(m)
	var root := Lcars.vbox(14)
	m.add_child(root)
	var h := Lcars.hbox(24)
	var t := Lcars.vbox(0)
	t.add_child(Lcars.heading("Intelligence", 40))
	t.add_child(Lcars.caption("Divide the budget between internal security and operations against each rival"))
	h.add_child(t)
	h.add_child(Lcars.spacer())
	h.add_child(Lcars.stat("Reserve", Lcars.fmt(intel["reserve"]), "", "Intelligence points available to classic operations."))
	h.add_child(Lcars.stat("Output", "%s/turn" % Lcars.fmt(intel["output"]), "tertiary", "Intelligence generated each turn, split by the budget below."))
	var sec: String = ("%.1f" % (float(intel["defenseStrengthMilli"]) / 1000.0)) if intel.has("defenseStrengthMilli") else Lcars.fmt(intel.get("securityStockpile", 0))
	h.add_child(Lcars.stat("Security", sec, "good", "Counter-intelligence strength. Rival operations are measured against it."))
	h.add_child(Lcars.pill("Revert", "ghost", func(): refresh()))
	_apply = Lcars.pill("Apply budget", "primary", func(): _apply_budget())
	_apply.name = "ApplyIntel"
	h.add_child(_apply)
	root.add_child(h)
	var tabs := Lcars.hbox(6)
	tabs.add_child(Lcars.toggle("Operations", tab == "operations", func(_on): tab = "operations"; refresh()))
	tabs.add_child(Lcars.toggle("Reports  %d" % (intel["reports"].size() + intel["operations"].size()), tab == "reports", func(_on): tab = "reports"; refresh()))
	root.add_child(tabs)
	if tab == "operations":
		var grid := Lcars.grid(3)
		grid.add_theme_constant_override("h_separation", 12)
		grid.add_theme_constant_override("v_separation", 12)
		grid.add_child(_security_card())
		for r in intel["rivals"]:
			grid.add_child(_rival_card(r))
		root.add_child(Lcars.scroll(grid))
	else:
		root.add_child(Lcars.scroll(_reports()))
	_update()

func _lock() -> String:
	return "Finish the battle in progress first" if app.view.get("session", {}).get("active", false) else ""

func _security_card() -> Control:
	var c := Lcars.card()
	c.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var v := Lcars.vbox(8)
	c.add_child(v)
	var h := Lcars.hbox(10)
	h.add_child(Lcars.emblem(app.player(), 30))
	var t := Lcars.vbox(0)
	t.add_child(Lcars.label("INTERNAL SECURITY", "SubHeading", 20, Lcars.TEXT))
	t.add_child(Lcars.caption("Protects your colonies from rival operations"))
	h.add_child(t)
	v.add_child(h)
	_sec_meter = Lcars.meter(budget["security"], 100, Lcars.GOOD, 10)
	v.add_child(_sec_meter)
	_sec_label = Lcars.label("", "", 44, Lcars.GOOD)
	_sec_label.add_theme_font_override("font", Lcars.font("display", 600))
	v.add_child(_sec_label)
	v.add_child(Lcars.para("Security receives whatever is not assigned to operations.", Lcars.TEXT3, 13))
	return c

func _rival_card(r: Dictionary) -> Control:
	var id := int(r["id"])
	var c := PanelContainer.new()
	c.name = "Rival%d" % id
	c.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	c.add_theme_stylebox_override("panel", Lcars.box(Color(Lcars.PANEL2, 0.92), 14, Lcars.owner_color(id) if highlight == id else Lcars.LINE2, 2 if highlight == id else 1, Vector4(12, 10, 12, 10)))
	var v := Lcars.vbox(6)
	c.add_child(v)
	var h := Lcars.hbox(10)
	h.add_child(Lcars.emblem(id, 30))
	var t := Lcars.vbox(0)
	t.add_child(Lcars.label(str(r["name"]).to_upper(), "SubHeading", 20, Lcars.owner_color(id)))
	t.add_child(Lcars.caption("Eliminated - no longer a target" if r.get("eliminated", false) else ("No contact" if not r.get("contact", false) else ("At war - sabotage permitted" if r.get("atWar", false) else "At peace"))))
	h.add_child(t)
	v.add_child(h)
	var lock := _lock()
	for kind in ["espionage", "sabotage"]:
		var row := Lcars.hbox(8)
		var l := Lcars.label(kind.capitalize(), "Muted", 13)
		l.custom_minimum_size.x = 76
		row.add_child(l)
		var sl := HSlider.new()
		sl.max_value = 100
		sl.step = 1
		sl.value = int(budget["rivals"][id][kind])
		sl.editable = lock == ""
		sl.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		sl.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		var k: String = kind
		sl.value_changed.connect(func(val): _set_share(id, k, int(val)))
		row.add_child(sl)
		var pl := Lcars.label("%d%%" % int(budget["rivals"][id][kind]), "Mono", 15)
		pl.custom_minimum_size.x = 44
		row.add_child(pl)
		_labels["%d:%s" % [id, kind]] = pl
		_sliders["%d:%s" % [id, kind]] = sl
		v.add_child(row)
	if r.has("focus"):
		var fr := HFlowContainer.new()
		fr.add_child(Lcars.caption("Focus"))
		for d in app.meta.get("departments", []):
			var key: String = d["key"]
			var nm: String = d["name"]
			var tg := Lcars.toggle(nm, r["focus"] == key, func(_on): app.issue({"type": "SetIntelFocus", "targetEmpire": id, "intelDepartmentFocus": key}, [], "Focus on %s targets." % nm.to_lower()))
			tg.add_theme_font_size_override("font_size", 13)
			tg.disabled = lock != ""
			fr.add_child(tg)
		v.add_child(fr)
	if r.has("espionageStockpile"):
		v.add_child(Lcars.label("Stockpiles: espionage %s · sabotage %s%s" % [Lcars.fmt(r["espionageStockpile"]), Lcars.fmt(r.get("sabotageStockpile", 0)), (" · strength %.1f" % (float(r["attackStrengthMilli"]) / 1000.0)) if r.has("attackStrengthMilli") else ""], "Mono", 12, Lcars.TEXT3))
	var ops := Lcars.hbox(6)
	var gather := {"type": "GatherIntelligence", "targetEmpire": id}
	ops.add_child(Lcars.pill("Gather intelligence", "secondary", func(): app.issue(gather, [], "Espionage mission ordered."), lock if lock != "" else app.dry_run(gather)))
	var cols: Array = r.get("visibleColonies", [])
	if not targets.has(id):
		targets[id] = int(cols[0]["systemId"]) if cols.size() > 0 else 0
	if cols.size() > 0:
		var ob := OptionButton.new()
		for col in cols:
			ob.add_item(str(col["name"]), int(col["systemId"]))
		ob.selected = max(0, ob.get_item_index(int(targets[id])))
		ob.item_selected.connect(func(ix): targets[id] = ob.get_item_id(ix); refresh())
		ops.add_child(ob)
	var sab := {"type": "Sabotage", "targetEmpire": id, "targetSystemId": int(targets[id])}
	var sreason := lock
	if sreason == "":
		sreason = "No colony of theirs is within sensor range" if int(targets[id]) == 0 else app.dry_run(sab)
	ops.add_child(Lcars.pill("Sabotage", "alert", func(): app.issue(sab, [], "Sabotage mission ordered."), sreason))
	v.add_child(ops)
	return c

func _set_share(id: int, kind: String, value: int) -> void:
	var others := 0
	for rid in budget["rivals"]:
		var a: Dictionary = budget["rivals"][rid]
		if int(rid) == id:
			others += int(a["sabotage"] if kind == "espionage" else a["espionage"])
		else:
			others += int(a["espionage"]) + int(a["sabotage"])
	var clamped: int = clamp(value, 0, 100 - others)
	budget["rivals"][id][kind] = clamped
	budget["security"] = 100 - others - clamped
	var key := "%d:%s" % [id, kind]
	if int(_sliders[key].value) != clamped:
		_sliders[key].set_value_no_signal(clamped)
	_labels[key].text = "%d%%" % clamped
	_update()

func _command() -> Dictionary:
	var ids := []
	var esp := []
	var sab := []
	for r in intel["rivals"]:
		var id := int(r["id"])
		ids.append(id)
		esp.append(int(budget["rivals"][id]["espionage"]))
		sab.append(int(budget["rivals"][id]["sabotage"]))
	return {"type": "SetIntelBudget", "intelInternalSecurity": budget["security"], "intelRivalIds": ids, "intelEspionagePcts": esp, "intelSabotagePcts": sab}

func _cancel_ids() -> Array:
	var out := []
	for p in app.view.get("pending", []):
		if p["type"] == "SetIntelBudget":
			out.append(int(p["id"]))
	return out

func _update() -> void:
	if _sec_label:
		_sec_label.text = "%d%%" % int(budget["security"])
		_sec_meter.value = int(budget["security"])
	if _apply:
		var dirty := JSON.stringify(budget) != JSON.stringify(original)
		var reason := _lock()
		if reason == "":
			reason = app.dry_run(_command(), _cancel_ids()) if dirty else "No changes"
		_apply.disabled = reason != ""
		_apply.tooltip_text = ("Unavailable: " + reason) if reason != "" else "Order this budget"

func _apply_budget() -> void:
	app.issue(_command(), _cancel_ids(), "Intelligence budget ordered.")

func _reports() -> Control:
	var list := Lcars.vbox(6)
	for o in intel.get("operations", []):
		var tone := ("good" if o.get("success", false) else "warn") if o.get("mine", false) else "alert"
		var p := PanelContainer.new()
		var sb := Lcars.box(Color(Lcars.PANEL2, 0.7), 10, Lcars.tone_color(tone), 0, Vector4(14, 8, 14, 8))
		sb.border_width_left = 5
		p.add_theme_stylebox_override("panel", sb)
		var h := Lcars.hbox(10)
		p.add_child(h)
		h.add_child(Lcars.label("T%d" % int(o["turn"]), "Mono", 14, Lcars.TEXT3))
		var txt := "%s · %s · %s" % [("%s vs %s" % [o["area"], o["defenderName"]]) if o.get("mine", false) else "Intrusion by %s" % o["attackerName"], o["department"], o["outcome"]]
		if int(o.get("creditsStolen", 0)) > 0:
			txt += " · ¢%s taken" % Lcars.fmt(o["creditsStolen"])
		if int(o.get("researchStolen", 0)) > 0:
			txt += " · %s research taken" % Lcars.fmt(o["researchStolen"])
		if str(o.get("details", "")) != "":
			txt += "\n" + str(o["details"])
		var l := Lcars.para(txt, Lcars.TEXT, 14)
		l.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		h.add_child(l)
		if o.get("detected", false):
			h.add_child(Lcars.badge("exposed" if o.get("mine", false) else "detected", "alert"))
		list.add_child(p)
	for r in intel.get("reports", []):
		var h2 := Lcars.hbox(10)
		h2.add_child(Lcars.label("T%d" % int(r["turn"]), "Mono", 14, Lcars.TEXT3))
		h2.add_child(Lcars.para("%s · %s%s" % [r["targetName"], ("%d colonies, about ¢%s" % [int(r["knownColonies"]), Lcars.fmt(r["estimatedCredits"])]) if r.get("success", false) else "operation failed", (" · " + str(r["system"])) if str(r.get("system", "")) != "" else ""], Lcars.GOOD if r.get("success", false) else Lcars.WARN, 14))
		list.add_child(h2)
	if list.get_child_count() == 0:
		list.add_child(Lcars.empty_note("No operations have completed yet."))
	return list
