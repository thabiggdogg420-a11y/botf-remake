extends Control
## Research presentation: allocation sliders, field cards with progress and the technology ladder.
## State and orders live in ResearchVM (scripts/viewmodels/research_vm.gd).

var app: App
var vm: ResearchVM:
	get:
		if vm == null:
			vm = ResearchVM.new(app.session)
		return vm
var _alloc_labels := []
var _sliders := []
var _total: Control
var _apply: Button

const FIELD_COLORS := ["#5fd98f", "#7fb3ff", "#f2b45a", "#c38bff", "#6fd3ff", "#ff7a6a"]

static func rebalance(a: Array, field: int, value: int, locked: Array) -> Array:
	return ResearchVM.rebalance(a, field, value, locked)

func refresh() -> void:
	for c in get_children():
		c.queue_free()
	vm.fetch()
	if not vm.available():
		add_child(Lcars.empty_note("Research data unavailable."))
		return
	var root := Lcars.vbox(14)
	Lcars.screen_margin(self).add_child(root)
	var h := Lcars.hbox(18)
	var fields0 := vm.fields()
	var split := RingGauge.make(92, 0, 1, Lcars.INFO, "%d" % int(vm.research["output"]), "PER TURN",
		{"title": "Allocation", "value": "%d points per turn" % int(vm.research["output"]), "parts": range(fields0.size()).map(func(i): return [str(fields0[i]["name"]), "%d%%" % int(vm.alloc[i])]), "effect": "Output is pooled across the empire and split by these shares. Drag a field's slider; locked shares stay put."})
	split.segments = range(fields0.size()).map(func(i): return [int(vm.alloc[i]), Color(FIELD_COLORS[i % FIELD_COLORS.size()])])
	h.add_child(split)
	h.add_child(Lcars.title_block("Research", "Science output is pooled empire-wide and divided by allocation"))
	h.add_child(Lcars.spacer())
	h.add_child(Lcars.kpi("research", "Output", "%d/turn" % int(vm.research["output"]), "", "info", {"title": "Research output", "value": "%d per turn" % int(vm.research["output"]), "effect": "Sum of every colony's laboratory output. More lab workers or research structures raise it."}))
	h.add_child(Lcars.kpi("growth", "Next turn", ("+" + Lcars.fmt(vm.forecast.get("research", 0))) if vm.forecast_ok() else "-", "", "good", {"title": "Next turn", "value": ("+" + Lcars.fmt(vm.forecast.get("research", 0))) if vm.forecast_ok() else "-", "effect": "Research gained when this turn resolves.", "forecast": "Core forecast of the turn with your current orders."}))
	h.add_child(Lcars.kpi("save", "Reserve", Lcars.fmt(vm.research.get("reserve", 0)), "", "", {"title": "Reserve", "value": Lcars.fmt(vm.research.get("reserve", 0)), "effect": "Points banked in fields that have reached the final level."}))
	_total = Lcars.hbox(0)
	h.add_child(_total)
	var lock := vm.lock_reason()
	h.add_child(Lcars.pill("Even split", "ghost", func(): _set_alloc(ResearchVM.EVEN.duplicate()), lock))
	h.add_child(Lcars.pill("Revert", "ghost", func(): _set_alloc(vm.current())))
	_apply = Lcars.pill("Apply allocation", "primary", func(): apply())
	_apply.name = "ApplyResearch"
	h.add_child(_apply)
	root.add_child(h)
	var grid := Lcars.grid(3)
	grid.add_theme_constant_override("h_separation", 12)
	grid.add_theme_constant_override("v_separation", 12)
	_alloc_labels.clear()
	_sliders.clear()
	var fields := vm.fields()
	for i in fields.size():
		grid.add_child(_field_card(i, fields[i], lock))
	root.add_child(grid)
	root.add_child(_ladder(fields[vm.focus]))
	_update_total()

func _field_card(i: int, fd: Dictionary, lock: String) -> Control:
	var col_c := Color(FIELD_COLORS[i % FIELD_COLORS.size()])
	var focus := vm.focus
	var card := PanelContainer.new()
	card.name = "Field%d" % i
	card.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var sb := Lcars.box_lcars(Color(Lcars.PANEL2, 0.92), 16, col_c if focus == i else Lcars.LINE2, 2 if focus == i else 1, Vector4(14, 12, 14, 12))
	sb.border_width_top = 5
	sb.border_color = col_c if focus == i else Color(col_c, 0.6)
	card.add_theme_stylebox_override("panel", sb)
	var outer := Lcars.hbox(14)
	card.add_child(outer)
	var pg0 := vm.progress(i)
	var ring := RingGauge.make(96, float(fd["progress"]), max(1, int(pg0["cost"])), col_c, "L%d" % int(fd["level"]), "OF %d" % vm.cap(),
		{"title": str(fd["name"]), "value": "Level %d of %d" % [int(fd["level"]), vm.cap()], "parts": [["Progress", "%s / %s" % [Lcars.fmt(fd["progress"]), Lcars.fmt(pg0["cost"])], ""], ["Share", "%d%%" % int(vm.alloc[i]), ""], ["Next turn", "+%d" % int(pg0["gained"]), "good"]],
			"effect": Lcars.RESEARCH_BLURBS[i] if i < Lcars.RESEARCH_BLURBS.size() else "",
			"forecast": ("Next level in %d turns at this share." % int(pg0["turns"])) if int(pg0["turns"]) > 0 else "No progress at this share."})
	outer.add_child(ring)
	var v := Lcars.vbox(6)
	v.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	outer.add_child(v)
	var top := Lcars.hbox(8)
	top.add_child(UiIcon.make(UiIcon.TECH_FIELDS[i % UiIcon.TECH_FIELDS.size()], 26, col_c))
	var nm := Lcars.label(str(fd["name"]).to_upper(), "SubHeading", 22, col_c)
	nm.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	Lcars.tip(nm, Lcars.RESEARCH_BLURBS[i] if i < Lcars.RESEARCH_BLURBS.size() else "")
	top.add_child(nm)
	top.add_child(Lcars.label("L%d/%d" % [int(fd["level"]), vm.cap()], "Mono", 18, Lcars.TEXT))
	v.add_child(top)
	var pg := vm.progress(i)
	var nxt: Dictionary = pg["next"]
	var lvl := int(fd["level"])
	v.add_child(Lcars.para("%s  →  %s" % [fd["current"], nxt.get("name", "Mastered")], Lcars.TEXT2, 13))
	var cost := int(pg["cost"])
	var gained := int(pg["gained"])
	var turns := int(pg["turns"])
	var pr := Lcars.hbox(8)
	pr.add_child(Lcars.meter(fd["progress"], max(1, cost), col_c, 6))
	pr.add_child(Lcars.label("%s/%s%s" % [Lcars.fmt(fd["progress"]), Lcars.fmt(cost), (" · %dT" % turns) if turns > 0 else ""], "Mono", 12))
	Lcars.tip(pr, "%s of %s points toward level %d.
%s" % [Lcars.fmt(fd["progress"]), Lcars.fmt(cost), lvl + 1, ("+%d expected next turn" % gained) if gained > 0 else "No points expected next turn"])
	v.add_child(pr)
	var ar := Lcars.hbox(8)
	var sl := HSlider.new()
	sl.name = "Alloc%d" % i
	sl.max_value = 100
	sl.step = 1
	sl.value = int(vm.alloc[i])
	sl.editable = lock == "" and not vm.locks[i]
	sl.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	sl.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	sl.value_changed.connect(func(val): vm.slide(i, int(val)); _set_alloc(vm.alloc))
	_sliders.append(sl)
	ar.add_child(sl)
	var al := Lcars.label("%d%%" % int(vm.alloc[i]), "Mono", 18, Lcars.TEXT)
	al.custom_minimum_size.x = 50
	_alloc_labels.append(al)
	ar.add_child(al)
	var lk := Lcars.toggle("Locked" if vm.locks[i] else "Lock", vm.locks[i], func(on): vm.locks[i] = on; _sliders[i].editable = not on, "Lock this share while adjusting others")
	ar.add_child(lk)
	v.add_child(ar)
	var foot := Lcars.hbox(6)
	foot.add_child(Lcars.pill("Focus", "ghost", func(): _set_alloc(ResearchVM.focus_alloc(i)), lock, "50% here, 10% to every other field"))
	foot.add_child(Lcars.toggle("Details", focus == i, func(_on): vm.focus = i; refresh()))
	v.add_child(foot)
	return card

func _set_alloc(a: Array) -> void:
	vm.alloc = a
	for i in _sliders.size():
		_sliders[i].set_value_no_signal(int(vm.alloc[i]))
		_alloc_labels[i].text = "%d%%" % int(vm.alloc[i])
	_update_total()

func _update_total() -> void:
	if _total == null:
		return
	for c in _total.get_children():
		c.queue_free()
	var sum := vm.total()
	_total.add_child(Lcars.stat("Allocated", "%d%%" % sum, "good" if sum == 100 else "alert"))
	var block := vm.apply_block()
	_apply.disabled = block != ""
	_apply.tooltip_text = block if block != "" else "Order this allocation for the turn"

func apply() -> void:
	vm.apply()

func _ladder(fd: Dictionary) -> Control:
	var f := Lcars.frame("%s - technology ladder" % fd["name"], "level %d of %d" % [int(fd["level"]), vm.cap()], "secondary", "research")
	f.size_flags_vertical = Control.SIZE_EXPAND_FILL
	var b := Lcars.body(f)
	var rungs := Lcars.hbox(4)
	var lvl := int(fd["level"])
	for l in fd["levels"]:
		var n := int(l["level"])
		var p := PanelContainer.new()
		var c := Lcars.accent() if n <= lvl else (Lcars.accent2() if n == lvl + 1 else Lcars.PANEL3)
		p.add_theme_stylebox_override("panel", Lcars.box_lcars(Color(c, 0.85 if n <= lvl + 1 else 0.6), 10, Color.TRANSPARENT, 0, Vector4(8, 4, 8, 4)))
		p.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		var lb := Lcars.label("%d  %s" % [n, l["name"]], "", 13, Lcars.ink() if n <= lvl + 1 else Lcars.TEXT2)
		lb.clip_text = true
		lb.add_theme_font_override("font", Lcars.font("display", 560))
		p.add_child(lb)
		p.tooltip_text = "%s\nCost %s points." % [l.get("description", l["name"]), Lcars.fmt(l["cost"])]
		rungs.add_child(p)
	b.add_child(rungs)
	var levels: Array = fd["levels"]
	if lvl + 1 >= levels.size():
		b.add_child(Lcars.empty_note("This field is fully researched."))
		return f
	var nxt: Dictionary = levels[lvl + 1]
	var nh := Lcars.hbox(10)
	nh.add_child(Lcars.label("NEXT: " + str(nxt["name"]).to_upper(), "SubHeading", 22, Lcars.TEXT))
	nh.add_child(Lcars.badge("%s pts" % Lcars.fmt(nxt["cost"]), "primary"))
	b.add_child(nh)
	if nxt.has("description"):
		b.add_child(Lcars.para(str(nxt["description"]), Lcars.TEXT2, 14))
	var cols := Lcars.hbox(30)
	var ships := Lcars.vbox(4)
	ships.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	ships.add_child(Lcars.caption("Starships requiring %s %d" % [fd["name"], int(nxt["level"])]))
	if nxt["ships"].is_empty():
		ships.add_child(Lcars.label("None for your empire.", "Muted"))
	for s in nxt["ships"]:
		var r := Lcars.hbox(8)
		r.add_child(Lcars.label(str(s["name"]), "", 14))
		r.add_child(Lcars.label("%s ind" % Lcars.fmt(s["cost"]), "Muted", 12))
		r.add_child(Lcars.badge("ready" if s.get("othersMet", false) else "needs more", "good" if s.get("othersMet", false) else "dim"))
		ships.add_child(r)
	cols.add_child(ships)
	var st := Lcars.vbox(4)
	st.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	st.add_child(Lcars.caption("Reference structures at this level"))
	if nxt["structures"].is_empty():
		st.add_child(Lcars.label("None catalogued.", "Muted"))
	for s in nxt["structures"].slice(0, 8):
		var sr := Lcars.hbox(4)
		sr.add_child(Lcars.label("%s  ·  %s ind ·" % [s["name"], Lcars.fmt(s["cost"])], "", 14))
		sr.add_child(UiIcon.make("energy", 14, Lcars.TEXT2))
		sr.add_child(Lcars.label("%d" % int(s["energy"]), "", 14))
		st.add_child(sr)
	cols.add_child(st)
	b.add_child(cols)
	var blocked: Array = vm.blocked()
	if blocked.size() > 0:
		b.add_child(Lcars.para("Still locked in this ruleset: " + " · ".join(blocked.slice(0, 6).map(func(x): return "%s (%s)" % [x["name"], x["blocker"]])), Lcars.TEXT3, 12))
	return f
