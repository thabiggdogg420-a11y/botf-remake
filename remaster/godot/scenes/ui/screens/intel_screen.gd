extends Control
## Intelligence presentation: the budget split between security and per-rival espionage and sabotage, department
## focus, missions and reports. Data and orders: IntelVM (scripts/viewmodels/intel_vm.gd).

var app: App
var vm: IntelVM:
	get:
		if vm == null:
			vm = IntelVM.new(app.session)
		return vm
var tab: String:
	get: return vm.tab
	set(v): vm.tab = v
var _sec_label: Label
var _sec_meter: ProgressBar
var _apply: Button
var _labels := {}
var _sliders := {}

func refresh() -> void:
	for c in get_children():
		c.queue_free()
	vm.fetch()
	if not vm.available():
		add_child(Lcars.empty_note("Intelligence unavailable."))
		return
	if app.has_meta("intel_focus"):
		vm.highlight = int(app.get_meta("intel_focus"))
		app.remove_meta("intel_focus")
	var intel := vm.intel
	var root := Lcars.vbox(14)
	Lcars.screen_margin(self).add_child(root)
	var h := Lcars.hbox(18)
	var sec := int(vm.budget["security"])
	var ring := RingGauge.make(92, 0, 1, Lcars.GOOD, "%d%%" % sec, "SECURITY",
		{"title": "Budget split", "value": "%d%% security" % sec, "parts": [["Internal security", "%d%%" % sec, "good"], ["Operations against rivals", "%d%%" % (100 - sec), "warn"]],
			"effect": "Security receives whatever is not assigned to operations. Raising a rival's espionage or sabotage share takes it from security."})
	ring.segments = [[sec, Lcars.GOOD], [100 - sec, Lcars.WARN]]
	h.add_child(ring)
	h.add_child(Lcars.title_block("Intelligence", "Divide the budget between internal security and operations against each rival"))
	h.add_child(Lcars.spacer())
	h.add_child(Lcars.kpi("save", "Reserve", Lcars.fmt(intel["reserve"]), "", "", {"title": "Reserve", "value": Lcars.fmt(intel["reserve"]), "effect": "Intelligence points available to classic operations (Gather intelligence, Sabotage)."}))
	h.add_child(Lcars.kpi("intel", "Output", "%s/t" % Lcars.fmt(intel["output"]), "", "tertiary", {"title": "Output", "value": "%s per turn" % Lcars.fmt(intel["output"]), "effect": "Intelligence generated each turn, split by the budget below."}))
	h.add_child(Lcars.kpi("shield", "Security", vm.security_text(), "", "good", {"title": "Security", "value": vm.security_text(), "effect": "Counter-intelligence strength. Rival operations are measured against it; more security share raises it."}))
	h.add_child(Lcars.pill("Revert", "ghost", func(): refresh()))
	_apply = Lcars.pill("Apply budget", "primary", func(): vm.apply())
	_apply.name = "ApplyIntel"
	h.add_child(_apply)
	root.add_child(h)
	var tabs := Lcars.hbox(6)
	tabs.add_child(Lcars.toggle("Operations", tab == "operations", func(_on): tab = "operations"; refresh()))
	tabs.add_child(Lcars.toggle("Reports  %d" % vm.reports_count(), tab == "reports", func(_on): tab = "reports"; refresh()))
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
	_sec_meter = Lcars.meter(vm.budget["security"], 100, Lcars.GOOD, 10)
	v.add_child(_sec_meter)
	_sec_label = Lcars.label("", "", 44, Lcars.GOOD)
	_sec_label.add_theme_font_override("font", Lcars.font("display", 600))
	v.add_child(_sec_label)
	v.add_child(Lcars.para("Security receives whatever is not assigned to operations.", Lcars.TEXT3, 13))
	# MOD-3: the race structures that feed intelligence (the Dominion's Vorta Directorate and Changeling Shapeshifter Network).
	var sb: Dictionary = vm.intel.get("structureBonus", {})
	if int(sb.get("securityPercent", 0)) > 0:
		v.add_child(Lcars.para("Security structures and traits add %d%% to internal security output." % int(sb["securityPercent"]), Lcars.GOOD, 13))
	if int(sb.get("networks", 0)) > 0:
		var n := Lcars.para("Changeling Shapeshifter Network x%d: +%d%% sabotage output, +%d sabotage damage." % [int(sb["networks"]), int(sb["sabotagePercent"]), int(sb["sabotageDamage"])], Lcars.WARN, 13)
		n.name = "ShapeshifterNetwork"
		v.add_child(n)
	return c

func _rival_card(r: Dictionary) -> Control:
	var id := int(r["id"])
	var hl := vm.highlight == id
	var c := PanelContainer.new()
	c.name = "Rival%d" % id
	c.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	c.add_theme_stylebox_override("panel", Lcars.box_lcars(Color(Lcars.PANEL2, 0.92), 14, Lcars.owner_color(id) if hl else Lcars.LINE2, 2 if hl else 1, Vector4(12, 10, 12, 10)))
	var v := Lcars.vbox(6)
	c.add_child(v)
	var h := Lcars.hbox(10)
	h.add_child(Lcars.emblem(id, 30))
	var t := Lcars.vbox(0)
	t.add_child(Lcars.label(str(r["name"]).to_upper(), "SubHeading", 20, Lcars.owner_color(id)))
	t.add_child(Lcars.caption(IntelVM.rival_status(r)))
	h.add_child(t)
	v.add_child(h)
	var lock := vm.lock_reason()
	for kind in ["espionage", "sabotage"]:
		var row := Lcars.hbox(8)
		var l := Lcars.label(kind.capitalize(), "Muted", 13)
		l.custom_minimum_size.x = 76
		row.add_child(l)
		var sl := HSlider.new()
		sl.max_value = 100
		sl.step = 1
		sl.value = int(vm.budget["rivals"][id][kind])
		sl.editable = lock == ""
		sl.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		sl.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		var k: String = kind
		sl.value_changed.connect(func(val): _set_share(id, k, int(val)))
		row.add_child(sl)
		var pl := Lcars.label("%d%%" % int(vm.budget["rivals"][id][kind]), "Mono", 15)
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
			var tg := Lcars.toggle(nm, r["focus"] == key, func(_on): vm.set_focus(id, key, nm))
			tg.add_theme_font_size_override("font_size", 13)
			tg.disabled = lock != ""
			fr.add_child(tg)
		v.add_child(fr)
	if r.has("espionageStockpile"):
		v.add_child(Lcars.label("Stockpiles: espionage %s · sabotage %s%s" % [Lcars.fmt(r["espionageStockpile"]), Lcars.fmt(r.get("sabotageStockpile", 0)), (" · strength %.1f" % (float(r["attackStrengthMilli"]) / 1000.0)) if r.has("attackStrengthMilli") else ""], "Mono", 12, Lcars.TEXT3))
	var ops := Lcars.hbox(6)
	var gather := vm.gather_command(id)
	ops.add_child(Lcars.pill("Gather intelligence", "secondary", func(): vm.issue(gather, [], "Espionage mission ordered."), vm.gather_block(id)))
	var cols: Array = r.get("visibleColonies", [])
	var target := vm.target_of(r)
	if cols.size() > 0:
		var ob := OptionButton.new()
		for col in cols:
			ob.add_item(str(col["name"]), int(col["systemId"]))
		ob.selected = max(0, ob.get_item_index(target))
		ob.item_selected.connect(func(ix): vm.targets[id] = ob.get_item_id(ix); refresh())
		ops.add_child(ob)
	var sab := vm.sabotage_command(r)
	ops.add_child(Lcars.pill("Sabotage", "alert", func(): vm.issue(sab, [], "Sabotage mission ordered."), vm.sabotage_block(r)))
	v.add_child(ops)
	return c

func _set_share(id: int, kind: String, value: int) -> void:
	var clamped := vm.set_share(id, kind, value)
	var key := "%d:%s" % [id, kind]
	if int(_sliders[key].value) != clamped:
		_sliders[key].set_value_no_signal(clamped)
	_labels[key].text = "%d%%" % clamped
	_update()

func _update() -> void:
	if _sec_label:
		_sec_label.text = "%d%%" % int(vm.budget["security"])
		_sec_meter.value = int(vm.budget["security"])
	if _apply:
		var reason := vm.apply_block()
		_apply.disabled = reason != ""
		_apply.tooltip_text = ("Unavailable: " + reason) if reason != "" else "Order this budget"

func _reports() -> Control:
	var list := Lcars.vbox(6)
	for o in vm.operation_rows():
		var p := PanelContainer.new()
		var sb := Lcars.box_lcars(Color(Lcars.PANEL2, 0.7), 10, Lcars.tone_color(o["tone"]), 0, Vector4(14, 8, 14, 8))
		sb.border_width_left = 5
		p.add_theme_stylebox_override("panel", sb)
		var h := Lcars.hbox(10)
		p.add_child(h)
		h.add_child(Lcars.label("T%d" % int(o["turn"]), "Mono", 14, Lcars.TEXT3))
		var l := Lcars.para(o["text"], Lcars.TEXT, 14)
		l.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		h.add_child(l)
		if o["badge"] != "":
			h.add_child(Lcars.badge(o["badge"], "alert"))
		list.add_child(p)
	for r in vm.report_rows():
		var h2 := Lcars.hbox(10)
		h2.add_child(Lcars.label("T%d" % int(r["turn"]), "Mono", 14, Lcars.TEXT3))
		h2.add_child(Lcars.para(r["text"], Lcars.GOOD if r["ok"] else Lcars.WARN, 14))
		list.add_child(h2)
	if list.get_child_count() == 0:
		list.add_child(Lcars.empty_note("No operations have completed yet."))
	return list
