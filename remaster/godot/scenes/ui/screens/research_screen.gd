extends Control
## Research: allocation sliders that keep the total at 100, progress with the core forecast,
## and the technology ladder with unlocks. Port of screens/Research.tsx.

var app: App
var research: Dictionary = {}
var forecast: Dictionary = {}
var alloc: Array = []
var locks := [false, false, false, false, false, false]
var focus := 0
var _alloc_labels := []
var _sliders := []
var _total: Control
var _apply: Button

const FIELD_COLORS := ["#5fd98f", "#7fb3ff", "#f2b45a", "#c38bff", "#6fd3ff", "#ff7a6a"]

static func rebalance(a: Array, field: int, value: int, locked: Array) -> Array:
	var nxt := a.duplicate()
	var locked_total := 0
	for i in nxt.size():
		if i != field and locked[i]:
			locked_total += int(nxt[i])
	nxt[field] = clamp(value, 0, 100 - locked_total)
	var others := []
	for i in nxt.size():
		if i != field and not locked[i]:
			others.append(i)
	var remaining := 100 - locked_total - int(nxt[field])
	if others.is_empty():
		nxt[field] = 100 - locked_total
		return nxt
	var current := 0
	for i in others:
		current += int(nxt[i])
	for i in others:
		nxt[i] = int(floor(float(nxt[i]) / current * remaining)) if current > 0 else remaining / others.size()
	var diff := remaining
	for i in others:
		diff -= int(nxt[i])
	var k := 0
	while diff > 0:
		nxt[others[k % others.size()]] += 1
		diff -= 1
		k += 1
	return nxt

func _current() -> Array:
	return app.view["empire"]["techs"].map(func(t): return int(t["allocation"]))

func refresh() -> void:
	for c in get_children():
		c.queue_free()
	var r: Variant = app.core.request("research")
	research = r if r is Dictionary else {}
	var f: Variant = app.core.request("forecast")
	forecast = f if f is Dictionary else {}
	alloc = _current()
	if research.is_empty():
		add_child(Lcars.empty_note("Research data unavailable."))
		return
	var m := MarginContainer.new()
	m.set_anchors_preset(Control.PRESET_FULL_RECT)
	for side in ["left", "right", "top", "bottom"]:
		m.add_theme_constant_override("margin_" + side, 16)
	add_child(m)
	var root := Lcars.vbox(14)
	m.add_child(root)
	var h := Lcars.hbox(24)
	var t := Lcars.vbox(0)
	t.add_child(Lcars.heading("Research", 40))
	t.add_child(Lcars.caption("Science output is pooled empire-wide and divided by allocation"))
	h.add_child(t)
	h.add_child(Lcars.spacer())
	h.add_child(Lcars.stat("Output", "%d/turn" % int(research["output"]), "info", "Sum of every colony's laboratory output."))
	h.add_child(Lcars.stat("Next turn", ("+" + Lcars.fmt(forecast.get("research", 0))) if str(forecast.get("error", "x")) == "" else "-", "good", "Research gained when this turn resolves (core forecast of the turn with your current orders)."))
	h.add_child(Lcars.stat("Reserve", Lcars.fmt(research.get("reserve", 0)), "", "Points banked in fields that have reached the final level."))
	_total = Lcars.hbox(0)
	h.add_child(_total)
	var lock := "Finish the battle in progress first" if app.view.get("session", {}).get("active", false) else ""
	h.add_child(Lcars.pill("Even split", "ghost", func(): _set_alloc([17, 17, 17, 17, 16, 16]), lock))
	h.add_child(Lcars.pill("Revert", "ghost", func(): _set_alloc(_current())))
	_apply = Lcars.pill("Apply allocation", "primary", func(): apply())
	_apply.name = "ApplyResearch"
	h.add_child(_apply)
	root.add_child(h)
	var grid := Lcars.grid(3)
	grid.add_theme_constant_override("h_separation", 12)
	grid.add_theme_constant_override("v_separation", 12)
	_alloc_labels.clear()
	_sliders.clear()
	var fields: Array = research["fields"]
	for i in fields.size():
		grid.add_child(_field_card(i, fields[i], lock))
	root.add_child(grid)
	root.add_child(_ladder(fields[focus]))
	_update_total()

func _field_card(i: int, fd: Dictionary, lock: String) -> Control:
	var col_c := Color(FIELD_COLORS[i % FIELD_COLORS.size()])
	var card := PanelContainer.new()
	card.name = "Field%d" % i
	card.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var sb := Lcars.box(Color(Lcars.PANEL2, 0.92), 16, col_c if focus == i else Lcars.LINE2, 2 if focus == i else 1, Vector4(14, 12, 14, 12))
	sb.border_width_top = 5
	sb.border_color = col_c if focus == i else Color(col_c, 0.6)
	card.add_theme_stylebox_override("panel", sb)
	var v := Lcars.vbox(6)
	card.add_child(v)
	var top := Lcars.hbox(8)
	var nm := Lcars.label(str(fd["name"]).to_upper(), "SubHeading", 22, col_c)
	nm.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	nm.tooltip_text = Lcars.RESEARCH_BLURBS[i] if i < Lcars.RESEARCH_BLURBS.size() else ""
	nm.mouse_filter = Control.MOUSE_FILTER_PASS
	top.add_child(nm)
	top.add_child(Lcars.label("L%d/10" % int(fd["level"]), "Mono", 18, Lcars.TEXT))
	v.add_child(top)
	var levels: Array = fd["levels"]
	var lvl := int(fd["level"])
	var nxt: Dictionary = levels[lvl + 1] if lvl + 1 < levels.size() else {}
	v.add_child(Lcars.para("%s  →  %s" % [fd["current"], nxt.get("name", "Mastered")], Lcars.TEXT2, 13))
	var cost := int(nxt.get("cost", 0))
	var gained := 0
	for x in forecast.get("fields", []):
		if int(x["field"]) == i:
			gained = int(x["gained"])
	var turns := int(ceil(float(cost - int(fd["progress"])) / gained)) if cost > 0 and gained > 0 else 0
	var pr := Lcars.hbox(8)
	pr.add_child(Lcars.meter(fd["progress"], max(1, cost), col_c, 6))
	pr.add_child(Lcars.label("%s/%s%s" % [Lcars.fmt(fd["progress"]), Lcars.fmt(cost), (" · %dT" % turns) if turns > 0 else ""], "Mono", 12))
	pr.tooltip_text = "%s of %s points toward level %d.\n%s" % [Lcars.fmt(fd["progress"]), Lcars.fmt(cost), lvl + 1, ("+%d expected next turn" % gained) if gained > 0 else "No points expected next turn"]
	pr.mouse_filter = Control.MOUSE_FILTER_PASS
	v.add_child(pr)
	var ar := Lcars.hbox(8)
	var sl := HSlider.new()
	sl.name = "Alloc%d" % i
	sl.max_value = 100
	sl.step = 1
	sl.value = int(alloc[i])
	sl.editable = lock == "" and not locks[i]
	sl.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	sl.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	sl.value_changed.connect(func(val): _set_alloc(rebalance(alloc, i, int(val), locks)))
	_sliders.append(sl)
	ar.add_child(sl)
	var al := Lcars.label("%d%%" % int(alloc[i]), "Mono", 18, Lcars.TEXT)
	al.custom_minimum_size.x = 50
	_alloc_labels.append(al)
	ar.add_child(al)
	var lk := Lcars.toggle("Locked" if locks[i] else "Lock", locks[i], func(on): locks[i] = on; _sliders[i].editable = not on, "Lock this share while adjusting others")
	ar.add_child(lk)
	v.add_child(ar)
	var foot := Lcars.hbox(6)
	foot.add_child(Lcars.pill("Focus", "ghost", func():
		var a := []
		for k in 6:
			a.append(50 if k == i else 10)
		_set_alloc(a), lock, "50% here, 10% to every other field"))
	foot.add_child(Lcars.toggle("Details", focus == i, func(_on): focus = i; refresh()))
	v.add_child(foot)
	return card

func _set_alloc(a: Array) -> void:
	alloc = a
	for i in _sliders.size():
		_sliders[i].set_value_no_signal(int(alloc[i]))
		_alloc_labels[i].text = "%d%%" % int(alloc[i])
	_update_total()

func _update_total() -> void:
	if _total == null:
		return
	for c in _total.get_children():
		c.queue_free()
	var sum := 0
	for a in alloc:
		sum += int(a)
	_total.add_child(Lcars.stat("Allocated", "%d%%" % sum, "good" if sum == 100 else "alert"))
	var dirty := alloc != _current()
	var lock: bool = app.view.get("session", {}).get("active", false)
	_apply.disabled = not dirty or lock
	_apply.tooltip_text = "Finish the battle first" if lock else ("No changes" if not dirty else "Order this allocation for the turn")

func apply() -> void:
	var ids := []
	for p in app.view.get("pending", []):
		if p["type"] == "AllocateResearch":
			ids.append(int(p["id"]))
	app.issue({"type": "AllocateResearch", "researchAllocation": alloc}, ids, "Research allocation ordered for this turn.")

func _ladder(fd: Dictionary) -> Control:
	var f := Lcars.frame("%s - technology ladder" % fd["name"], "level %d of 10" % int(fd["level"]), "secondary")
	f.size_flags_vertical = Control.SIZE_EXPAND_FILL
	var b := Lcars.body(f)
	var rungs := Lcars.hbox(4)
	var lvl := int(fd["level"])
	for l in fd["levels"]:
		var n := int(l["level"])
		var p := PanelContainer.new()
		var c := Lcars.accent() if n <= lvl else (Lcars.accent2() if n == lvl + 1 else Lcars.PANEL3)
		p.add_theme_stylebox_override("panel", Lcars.box(Color(c, 0.85 if n <= lvl + 1 else 0.6), 10, Color.TRANSPARENT, 0, Vector4(8, 4, 8, 4)))
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
		st.add_child(Lcars.label("%s  ·  %s ind · ⚡%d" % [s["name"], Lcars.fmt(s["cost"]), int(s["energy"])], "", 14))
	cols.add_child(st)
	b.add_child(cols)
	var blocked: Array = research.get("blockers", []).filter(func(x): return str(x.get("blocker", "")) != "")
	if blocked.size() > 0:
		b.add_child(Lcars.para("Still locked in this ruleset: " + " · ".join(blocked.slice(0, 6).map(func(x): return "%s (%s)" % [x["name"], x["blocker"]])), Lcars.TEXT3, 12))
	return f
