extends Control
## System management: planets, workforce with the core's forecast, governor, energy & shipyard,
## construction choices, build queue, build templates and the megaproject. Port of screens/System.tsx.

var app: App
var detail: Dictionary = {}
var draft := {"food": 0, "industry": 0, "research": 0}
var preview: Dictionary = {}
var tab := "structures"
var template_name := ""
var _sliders := {}
var _forecast: Control
var _wf_out := {}

const FIELDS := [
	{"key": "food", "label": "Food", "structure": "farms", "unit": "food"},
	{"key": "industry", "label": "Industry", "structure": "factories", "unit": "industry"},
	{"key": "research", "label": "Research", "structure": "laboratories", "unit": "research"},
]

func _locked() -> String:
	return "Finish the battle in progress first" if app.view.get("session", {}).get("active", false) else ""

func _system_id() -> int:
	var sid := int(app.selection.get("systemId", 0))
	for c in app.view.get("colonies", []):
		if int(c["systemId"]) == sid:
			return sid
	var cols: Array = app.view.get("colonies", [])
	return int(cols[0]["systemId"]) if cols.size() > 0 else 0

func cycle(d: int) -> void:
	var cols: Array = app.view.get("colonies", [])
	if cols.size() < 2:
		return
	var sid := _system_id()
	var idx := 0
	for n in cols.size():
		if int(cols[n]["systemId"]) == sid:
			idx = n
	app.select_system(int(cols[(idx + d + cols.size()) % cols.size()]["systemId"]))
	refresh()

func _unhandled_key_input(event: InputEvent) -> void:
	var k := event as InputEventKey
	if k and k.pressed and not k.echo and not app.has_modal() and not (get_viewport().gui_get_focus_owner() is LineEdit):
		if k.keycode == KEY_Q:
			cycle(-1)
			get_viewport().set_input_as_handled()
		elif k.keycode == KEY_E:
			cycle(1)
			get_viewport().set_input_as_handled()

func refresh() -> void:
	for c in get_children():
		c.queue_free()
	var sid := _system_id()
	if sid == 0:
		var f := Lcars.frame("System")
		f.set_anchors_preset(Control.PRESET_FULL_RECT)
		Lcars.body(f).add_child(Lcars.empty_note("You have no colonies."))
		add_child(f)
		return
	var d: Variant = app.core.request("colony", {"systemId": sid})
	if not d is Dictionary:
		app.report_error(app.core.last_error)
		return
	detail = d
	var c: Dictionary = detail["colony"]
	draft = {"food": int(c["workers"]["food"]), "industry": int(c["workers"]["industry"]), "research": int(c["workers"]["research"])}
	preview = {}
	var margin := MarginContainer.new()
	margin.set_anchors_preset(Control.PRESET_FULL_RECT)
	for side in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 16)
	add_child(margin)
	var root := Lcars.vbox(14)
	margin.add_child(root)
	root.add_child(_header(c))
	var cols := Lcars.hbox(14)
	cols.size_flags_vertical = Control.SIZE_EXPAND_FILL
	root.add_child(cols)
	var left := Lcars.vbox(12)
	left.custom_minimum_size.x = 560
	left.add_child(_planets())
	left.add_child(_workforce(c))
	left.add_child(_governor(c))
	left.add_child(_energy(c))
	cols.add_child(Lcars.scroll(left))
	var mid := _construction()
	mid.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	cols.add_child(mid)
	var right := Lcars.vbox(12)
	right.custom_minimum_size.x = 470
	if detail.has("fundsStation"):
		var fs: Dictionary = detail["fundsStation"]
		right.add_child(Lcars.para("All industry funds the starbase upgrade of %s (%d/%d); the construction queue is paused." % [fs["name"], int(fs["work"]), int(fs["cost"])], Lcars.WARN, 14))
	right.add_child(_queue(c))
	right.add_child(_templates())
	var mp := _megaproject()
	if mp:
		right.add_child(mp)
	var rs := Lcars.scroll(right)
	rs.custom_minimum_size.x = 480
	rs.size_flags_horizontal = Control.SIZE_FILL
	cols.add_child(rs)

func _header(c: Dictionary) -> Control:
	var cols: Array = app.view.get("colonies", [])
	var idx := 0
	for n in cols.size():
		if int(cols[n]["systemId"]) == int(c["systemId"]):
			idx = n
	var e: Dictionary = c["economy"]
	var h := Lcars.hbox(22)
	h.add_child(Lcars.pill("◂", "ghost", func(): cycle(-1), "" if cols.size() > 1 else "Only one colony", "Previous colony", "Q"))
	var t := Lcars.vbox(0)
	t.add_child(Lcars.heading(str(c["name"]), 40))
	t.add_child(Lcars.caption("Colony %d of %d · governor %s" % [idx + 1, cols.size(), "manual" if c["governor"]["workforce"] == "Manual" else c["governor"]["workforce"]]))
	h.add_child(t)
	h.add_child(Lcars.pill("▸", "ghost", func(): cycle(1), "" if cols.size() > 1 else "Only one colony", "Next colony", "E"))
	h.add_child(Lcars.spacer())
	var g: Dictionary = c["growth"]
	var gtxt := "fixed"
	if g.get("enabled", false):
		gtxt = Lcars.signed(g["min"]) if int(g["min"]) == int(g["max"]) else "%s to %s" % [Lcars.signed(g["min"]), Lcars.signed(g["max"])]
	h.add_child(Lcars.stat("Population", "%d/%d" % [int(c["population"]), int(c["capacity"])], "", "Population %d of capacity %d (max %d after terraforming every planet).\nNext turn: %s per turn.\nFood surplus drives growth; a shortfall shrinks the colony." % [int(c["population"]), int(c["capacity"]), int(detail["maxCapacity"]), gtxt]))
	h.add_child(Lcars.stat("Growth", gtxt, "alert" if int(g.get("max", 0)) < 0 else "good"))
	h.add_child(Lcars.stat("Morale", c["morale"], "warn" if int(c["morale"]) < 50 else "", "Morale drifts toward 70 each turn; events can raise or lower it."))
	h.add_child(Lcars.stat("Food", Lcars.signed(int(e["food"]) - int(c["population"])), "alert" if int(e["foodDeficit"]) > 0 else "good", "%d farms staffed by %d workers produce %d food." % [int(c["structures"]["farms"]), int(c["workers"]["food"]), int(e["food"])]))
	h.add_child(Lcars.stat("Industry", e["industry"], "", "Industry builds the first item in the queue each turn."))
	h.add_child(Lcars.stat("Research", e["research"], "info", "Pooled empire-wide and split by your research allocation."))
	h.add_child(Lcars.stat("Energy", "%d/%d" % [int(e["energyUsed"]), int(e["energyAvailable"])], "alert" if int(e["energyUsed"]) > int(e["energyAvailable"]) else "", "Power plants add capacity; unpowered structures go idle."))
	h.add_child(Lcars.stat("Credits", Lcars.signed(e["net"]), "alert" if int(e["net"]) < 0 else "secondary", "Income %d - structure upkeep %d." % [int(e["income"]), int(e["upkeep"])]))
	return h

func _planets() -> Control:
	var f := Lcars.frame("Planets", "capacity %d · max %d" % [int(detail["colony"]["capacity"]), int(detail["maxCapacity"])], "tertiary")
	var row := HFlowContainer.new()
	row.add_theme_constant_override("h_separation", 8)
	row.add_theme_constant_override("v_separation", 8)
	for p in detail.get("planets", []):
		var pc := Lcars.card()
		pc.custom_minimum_size = Vector2(98, 0)
		var v := Lcars.vbox(3)
		pc.add_child(v)
		var orb := PlanetOrb.new()
		orb.cls = str(p["class"])
		orb.custom_minimum_size = Vector2(0, 44)
		v.add_child(orb)
		var n := str(p["name"])
		var sp := n.rfind(" ")
		v.add_child(Lcars.label(n.substr(sp + 1) if sp > 0 else n, "SubHeading", 16, Lcars.TEXT))
		v.add_child(Lcars.label("Gas giant" if p["class"] == "GasGiant" else ("Belt" if p["class"] == "Asteroids" else "Class " + str(p["class"])), "Muted", 12))
		v.add_child(Lcars.meter(p["capacity"], max(1, int(p["maxCapacity"])), Lcars.GOOD, 3))
		v.add_child(Lcars.label(("%d/%d" % [int(p["capacity"]), int(p["maxCapacity"])]) if int(p["maxCapacity"]) > 0 else "-", "Mono", 12))
		pc.tooltip_text = "%s\n%s%s\nSupports %d now, %d when terraformed." % [p["className"], p["terraformState"], (" - %d/%d terraforming work" % [int(p["terraformWork"]), int(p["terraformNeeded"])]) if int(p["terraformNeeded"]) > 0 else "", int(p["capacity"]), int(p["maxCapacity"])]
		row.add_child(pc)
	Lcars.body(f).add_child(row)
	return f

func _field_out(eco: Dictionary, key: String) -> int:
	return int(eco.get(key, 0))

func _workforce(c: Dictionary) -> Control:
	var pop := int(c["population"])
	var used := int(draft["food"]) + int(draft["industry"]) + int(draft["research"])
	var f := Lcars.frame("Workforce", "%d unassigned" % (pop - used), "primary")
	f.name = "Workforce"
	var b := Lcars.body(f)
	var acts := Lcars.hbox(8)
	acts.add_child(Lcars.pill("Balance", "ghost", func(): _auto_balance(false), _locked(), "Feed the colony, then split industry and research"))
	acts.add_child(Lcars.pill("Industry", "ghost", func(): _auto_balance(true), _locked(), "Feed the colony, then maximise industry"))
	b.add_child(acts)
	var limits: Dictionary = detail["capacityLimits"]
	_sliders.clear()
	_wf_out.clear()
	for fd in FIELDS:
		var key: String = fd["key"]
		var row := Lcars.vbox(2)
		var head := Lcars.hbox(8)
		head.add_child(Lcars.label(str(fd["label"]).to_upper(), "SubHeading", 18, Lcars.TEXT))
		head.add_child(Lcars.label("%d %s" % [int(c["structures"][fd["structure"]]), fd["structure"]], "Muted", 13))
		head.add_child(Lcars.spacer())
		var out := Lcars.label("", "Mono", 14)
		_wf_out[key] = out
		head.add_child(out)
		row.add_child(head)
		var ctl := Lcars.hbox(10)
		var sl := HSlider.new()
		sl.name = "wf_" + key
		sl.max_value = max(1, int(limits[key]))
		sl.step = 1
		sl.value = int(draft[key])
		sl.editable = _locked() == "" and int(limits[key]) > 0
		sl.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		sl.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		sl.tooltip_text = "%s workers (up to %d jobs)" % [fd["label"], int(limits[key])]
		var cnt := Lcars.label("%d/%d" % [int(draft[key]), int(limits[key])], "Mono", 14)
		cnt.custom_minimum_size.x = 64
		sl.value_changed.connect(func(val): _on_slider(key, int(val), cnt))
		sl.drag_ended.connect(func(_changed): apply_workers())
		_sliders[key] = sl
		ctl.add_child(sl)
		ctl.add_child(cnt)
		row.add_child(ctl)
		b.add_child(row)
	_forecast = Lcars.hbox(16)
	_forecast.name = "Forecast"
	b.add_child(_forecast)
	_update_forecast()
	if not app.view["meta"].get("populationEnabled", true):
		b.add_child(Lcars.para("Population is fixed in this ruleset.", Lcars.TEXT3, 13))
	return f

func _on_slider(key: String, value: int, cnt: Label) -> void:
	var pop := int(detail["colony"]["population"])
	var others := int(draft["food"]) + int(draft["industry"]) + int(draft["research"]) - int(draft[key])
	var limit: int = min(int(detail["capacityLimits"][key]), pop - others)
	var v: int = clamp(value, 0, max(0, limit))
	draft[key] = v
	if int(_sliders[key].value) != v:
		_sliders[key].set_value_no_signal(v)
	cnt.text = "%d/%d" % [v, int(detail["capacityLimits"][key])]
	var p: Variant = app.core.request("workforcePreview", {"systemId": int(detail["colony"]["systemId"]), "food": draft["food"], "industry": draft["industry"], "research": draft["research"]})
	preview = p if p is Dictionary else {"error": app.core.last_error}
	_update_forecast()

## Shows the core's forecast for the drafted staffing.
func _update_forecast() -> void:
	if _forecast == null:
		return
	for ch in _forecast.get_children():
		ch.queue_free()
	var c: Dictionary = detail["colony"]
	var pop := int(c["population"])
	var ok := not preview.is_empty() and str(preview.get("error", "")) == ""
	var eco: Dictionary = preview["economy"] if ok and preview.has("economy") else c["economy"]
	var growth: Dictionary = preview["growth"] if ok and preview.has("growth") else c["growth"]
	for fd in FIELDS:
		var key: String = fd["key"]
		var now := _field_out(c["economy"], key)
		var nxt := _field_out(eco, key)
		var l: Label = _wf_out.get(key)
		if l:
			l.text = "%d%s%s" % [nxt, (" (%s)" % Lcars.signed(nxt - now)) if nxt != now else "", (" / %d needed" % pop) if key == "food" else ""]
			l.add_theme_color_override("font_color", Lcars.GOOD if nxt > now else (Lcars.ALERT if nxt < now else Lcars.TEXT2))
	if not preview.is_empty() and str(preview.get("error", "")) != "":
		_forecast.add_child(Lcars.label(str(preview["error"]), "", 14, Lcars.ALERT))
		return
	var gtxt := Lcars.signed(growth.get("min", 0)) if int(growth.get("min", 0)) == int(growth.get("max", 0)) else "%s..%s" % [Lcars.signed(growth["min"]), Lcars.signed(growth["max"])]
	_forecast.add_child(Lcars.stat("Growth", gtxt, "alert" if int(growth.get("max", 0)) < 0 else "good", "Population change next turn with this staffing (core population forecast)."))
	_forecast.add_child(Lcars.stat("Food", Lcars.signed(int(eco["food"]) - pop), "alert" if int(eco["food"]) < pop else "good"))
	_forecast.add_child(Lcars.stat("Credits", Lcars.signed(eco["net"])))
	var idle := pop - int(draft["food"]) - int(draft["industry"]) - int(draft["research"])
	_forecast.add_child(Lcars.stat("Idle", idle, "warn" if idle > 0 else ""))
	var dirty := int(draft["food"]) != int(c["workers"]["food"]) or int(draft["industry"]) != int(c["workers"]["industry"]) or int(draft["research"]) != int(c["workers"]["research"])
	if dirty:
		var ap := Lcars.pill("Apply", "primary", func(): apply_workers(), _locked())
		ap.name = "ApplyWorkers"
		ap.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		_forecast.add_child(ap)

func apply_workers() -> void:
	if _locked() != "":
		return
	var c: Dictionary = detail["colony"]
	var p: Variant = app.core.request("workforcePreview", {"systemId": int(c["systemId"]), "food": draft["food"], "industry": draft["industry"], "research": draft["research"]})
	if not p is Dictionary:
		app.report_error(app.core.last_error)
		return
	if str(p.get("error", "")) != "":
		preview = p
		_update_forecast()
		return
	app.issue({"type": "AssignWorkers", "systemId": int(c["systemId"]), "foodWorkers": draft["food"], "industryWorkers": draft["industry"], "researchWorkers": draft["research"]}, p.get("cancelIds", []), "Staffing updated.")

func _auto_balance(industry_first: bool) -> void:
	var c: Dictionary = detail["colony"]
	var pop := int(c["population"])
	var lim: Dictionary = detail["capacityLimits"]
	var food := 0
	for fv in range(0, min(pop, int(lim["food"])) + 1):
		food = fv
		var p: Variant = app.core.request("workforcePreview", {"systemId": int(c["systemId"]), "food": fv, "industry": 0, "research": 0})
		if p is Dictionary and str(p.get("error", "")) == "" and p.has("economy") and int(p["economy"]["foodDeficit"]) == 0:
			break
	var rest := pop - food
	var research: int = max(0, min(int(lim["research"]), rest - int(lim["industry"]))) if industry_first else min(int(lim["research"]), rest / 2)
	var industry: int = min(int(lim["industry"]), rest - research)
	draft = {"food": food, "industry": industry, "research": min(int(lim["research"]), rest - industry)}
	apply_workers()

func _governor(c: Dictionary) -> Control:
	var g: Dictionary = c["governor"]
	var f := Lcars.frame("Governor", "automation", "secondary")
	var b := Lcars.body(f)
	var sid := int(c["systemId"])
	for which in ["workforce", "queue"]:
		var row := Lcars.hbox(4)
		var l := Lcars.caption("Workforce" if which == "workforce" else "Queue")
		l.custom_minimum_size.x = 84
		l.tooltip_text = "Automatically assigns citizens each turn." if which == "workforce" else "Queues construction whenever the build queue runs empty."
		l.mouse_filter = Control.MOUSE_FILTER_PASS
		row.add_child(l)
		for o in app.meta.get("governors", []):
			var key: String = o["key"]
			var w: String = which
			var t := Lcars.toggle(o["name"], g[which] == key, func(_on):
				var wf: String = key if w == "workforce" else g["workforce"]
				var qu: String = key if w == "queue" else g["queue"]
				app.issue({"type": "SetColonyGovernor", "systemId": sid, "governorWorkforce": wf, "governorQueue": qu}, [], "Governor orders updated."))
			t.add_theme_font_size_override("font_size", 14)
			t.disabled = _locked() != ""
			row.add_child(t)
		b.add_child(row)
	if g.get("override", false) and g["workforce"] != "Manual":
		var r := Lcars.hbox(8)
		r.add_child(Lcars.para("Manual staffing is overriding the governor.", Lcars.WARN, 13))
		r.add_child(Lcars.pill("Resume governor", "primary", func(): app.issue({"type": "ClearGovernorOverride", "systemId": sid}, [], "Governor resumed.")))
		b.add_child(r)
	if str(g.get("warning", "")) != "":
		b.add_child(Lcars.para(str(g["warning"]), Lcars.WARN, 13))
	if g["workforce"] != "Manual" and str(g.get("workforceExplanation", "")) != "":
		b.add_child(Lcars.para(str(g["workforceExplanation"]), Lcars.TEXT3, 13))
	if g["queue"] != "Manual" and str(g.get("queueExplanation", "")) != "":
		b.add_child(Lcars.para(str(g["queueExplanation"]), Lcars.TEXT3, 13))
	return f

func _energy(c: Dictionary) -> Control:
	var e: Dictionary = c["economy"]
	var s: Dictionary = c["structures"]
	var f := Lcars.frame("Energy & shipyard", "", "tertiary")
	var b := Lcars.body(f)
	var r := Lcars.hbox(8)
	r.add_child(Lcars.label("Power", "Muted", 13))
	r.add_child(Lcars.meter(e["energyUsed"], max(1, int(e["energyAvailable"])), Lcars.ALERT if int(e["energyUsed"]) > int(e["energyAvailable"]) else Lcars.accent2(), 8))
	r.add_child(Lcars.label("%d/%d" % [int(e["energyUsed"]), int(e["energyAvailable"])], "Mono", 13))
	b.add_child(r)
	var lines := "%d power plant%s%s · %s" % [int(s["powerPlants"]), "" if int(s["powerPlants"]) == 1 else "s", (" · %d fusion" % int(s["fusionPlants"])) if int(s["fusionPlants"]) > 0 else "",
		("%d factories, %d labs idle" % [int(e["inactiveFactories"]), int(e["inactiveLabs"])]) if int(e["inactiveFactories"]) + int(e["inactiveLabs"]) > 0 else "All structures powered"]
	b.add_child(Lcars.para(lines, Lcars.TEXT2, 13))
	var sy := Lcars.hbox(8)
	var st := "No shipyard"
	if int(s["shipyards"]) > 0:
		st = "Shipyard powered - builds ships and extends range" if e.get("shipyardPowered", false) else "Shipyard offline"
	var sl := Lcars.para(st, Lcars.TEXT2, 13)
	sl.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	sy.add_child(sl)
	if int(s["shipyards"]) > 0:
		var sid := int(c["systemId"])
		sy.add_child(Lcars.pill("Switch %s" % ("off" if c.get("shipyardEnabled", false) else "on"), "ghost", func(): app.issue({"type": "ToggleShipyard", "systemId": sid}, [], "Shipyard power change ordered for turn end."),
			_locked() if _locked() != "" else str(detail.get("shipyardToggleBlocker", ""))))
	b.add_child(sy)
	return f

func _construction() -> Control:
	var f := Lcars.frame("Construction", "", "primary")
	f.name = "Construction"
	var b := Lcars.body(f)
	var choices: Array = detail.get("choices", [])
	var structures: Array = choices.filter(func(x): return not x["isShip"])
	var ships: Array = choices.filter(func(x): return x["isShip"])
	var tabs := Lcars.hbox(6)
	for t in [["structures", "Structures  %d" % structures.filter(func(x): return str(x["blocker"]) == "").size()], ["ships", "Starships  %d" % ships.filter(func(x): return str(x["blocker"]) == "").size()], ["installed", "Installed"]]:
		var id: String = t[0]
		tabs.add_child(Lcars.toggle(t[1], tab == id, func(_on): tab = id; refresh()))
	b.add_child(tabs)
	var list := Lcars.vbox(6)
	if tab == "installed":
		for s in detail.get("scrap", []):
			var r := Lcars.hbox(10)
			var n := Lcars.label(str(s["name"]), "", 15)
			n.size_flags_horizontal = Control.SIZE_EXPAND_FILL
			r.add_child(n)
			r.add_child(Lcars.label("x%d" % int(s["count"]), "Mono"))
			r.add_child(Lcars.label("⚡%d · ¢%d" % [int(s["energyCost"]), int(s["upkeep"])], "Muted"))
			var bld: String = s["building"]
			var nm: String = s["name"]
			var sid := int(detail["colony"]["systemId"])
			var reason := _locked() if _locked() != "" else (str(s["blocker"]) if str(s["blocker"]) != "" else ("None installed" if int(s["count"]) == 0 else ""))
			r.add_child(Lcars.pill("Scrap", "alert", func():
				app.confirm("Scrap a %s?" % nm.to_lower(), "The structure is demolished at turn end. Cancel the order to undo it before then.", "Scrap",
					func(): app.issue({"type": "ScrapStructure", "systemId": sid, "building": bld}, [], "%s will be scrapped." % nm)), reason))
			list.add_child(r)
	else:
		var src := structures if tab == "structures" else ships
		if src.is_empty():
			list.add_child(Lcars.empty_note("No starships available - build and power a shipyard." if tab == "ships" else "Nothing can be built here right now."))
		for ch in src:
			list.add_child(_choice(ch))
	b.add_child(Lcars.scroll(list))
	return f

func _choice(ch: Dictionary) -> Control:
	var reason := _locked() if _locked() != "" else str(ch.get("blocker", ""))
	var btn := Button.new()
	btn.name = "Choice_%s_%d" % [ch["product"], int(ch["referenceId"])]
	btn.theme_type_variation = "GhostButton"
	btn.custom_minimum_size.y = 58
	btn.disabled = reason != ""
	var sb := Lcars.box(Color(Lcars.PANEL2, 0.8), 12, Lcars.LINE2, 1, Vector4(12, 6, 12, 6))
	btn.add_theme_stylebox_override("disabled", Lcars.box(Color(Lcars.PANEL, 0.6), 12, Lcars.LINE, 1, Vector4(12, 6, 12, 6)))
	btn.add_theme_stylebox_override("normal", sb)
	var tip := ""
	if ch["isShip"] and ch.has("hull"):
		tip = "%s %s\nHull %d · shields %d · attack %d\nSpeed %d · range %d · upkeep %d\n%s" % [ch.get("designClass", ""), ch.get("designName", ""), int(ch["hull"]), int(ch["shields"]), int(ch["attack"]), int(ch.get("speed", 0)), int(ch.get("range", 0)), int(ch.get("maintenance", 0)), ch.get("description", "")]
	else:
		tip = "%s\nIndustry %d%s · upkeep %d credits/turn" % [ch["name"], int(ch["cost"]), (" · energy %d" % int(ch["energyCost"])) if ch.has("energyCost") else "", int(ch["upkeep"])]
	btn.tooltip_text = ("Unavailable: " + reason) if reason != "" else tip
	var row := Lcars.hbox(10)
	row.set_anchors_preset(Control.PRESET_FULL_RECT)
	row.offset_left = 12
	row.offset_right = -12
	row.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var icon := ChoiceIcon.new()
	icon.product = str(ch["product"])
	icon.ship = ch["isShip"]
	icon.custom_minimum_size = Vector2(34, 34)
	icon.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	row.add_child(icon)
	var main := Lcars.vbox(0)
	main.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	main.alignment = BoxContainer.ALIGNMENT_CENTER
	var nm := Lcars.label(str(ch["name"]), "", 15, Lcars.TEXT if reason == "" else Lcars.TEXT3)
	nm.add_theme_font_override("font", Lcars.font("ui", 560))
	main.add_child(nm)
	var meta := ""
	if ch["isShip"] and ch.has("hull"):
		meta = "H%d · S%d · A%d · " % [int(ch["hull"]), int(ch["shields"]), int(ch["attack"])]
	meta += "%s ind · %s" % [Lcars.fmt(ch["cost"]), ("¢%d/t" % int(ch["upkeep"])) if int(ch["upkeep"]) > 0 else "no upkeep"]
	main.add_child(Lcars.label(meta, "Muted", 12))
	if reason != "":
		main.add_child(Lcars.label(reason, "", 12, Lcars.WARN))
	row.add_child(main)
	row.add_child(Lcars.label(("%dT" % int(ch["turns"])) if int(ch["turns"]) > 0 else "∞", "Mono", 15, Lcars.accent2()))
	row.add_child(Lcars.label("+", "", 26, Lcars.accent()))
	for n in row.find_children("*", "Control", true, false):
		n.mouse_filter = Control.MOUSE_FILTER_IGNORE
	btn.add_child(row)
	var sid := int(detail["colony"]["systemId"])
	btn.pressed.connect(func(): app.issue({"type": "EnqueueBuilding", "systemId": sid, "building": ch["product"], "referenceId": int(ch["referenceId"])}, [], "%s added to the queue." % ch["name"]))
	return btn

func _queue(c: Dictionary) -> Control:
	var q: Array = detail.get("queue", [])
	var sid := int(c["systemId"])
	var f := Lcars.frame("Build queue", "%d item%s" % [q.size(), "" if q.size() == 1 else "s"], "secondary")
	f.name = "BuildQueue"
	var b := Lcars.body(f)
	if str(detail.get("queueBlocker", "")) != "":
		b.add_child(Lcars.para(str(detail["queueBlocker"]), Lcars.WARN, 13))
	if q.is_empty():
		b.add_child(Lcars.empty_note("The queue is empty. Choose a structure or starship to build."))
	var lock := _locked()
	for i in q.size():
		var it: Dictionary = q[i]
		var card := Lcars.card()
		var col := Lcars.vbox(3)
		card.add_child(col)
		var h := Lcars.hbox(8)
		h.add_child(Lcars.label(str(i + 1), "Mono", 16, Lcars.accent() if i == 0 else Lcars.TEXT3))
		var n := Lcars.label(str(it["name"]), "", 15, Lcars.TEXT)
		n.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		h.add_child(n)
		var eta := Lcars.label(("%dT" % int(it["eta"])) if int(it["eta"]) > 0 else "-", "Mono", 15, Lcars.accent2())
		eta.tooltip_text = "Estimated completion in %d turns at %d industry/turn." % [int(it["eta"]), int(c["economy"]["industry"])] if int(it["eta"]) > 0 else "No industry: this item makes no progress."
		eta.mouse_filter = Control.MOUSE_FILTER_PASS
		h.add_child(eta)
		col.add_child(h)
		col.add_child(Lcars.meter(it["work"], it["cost"], Lcars.accent() if i == 0 else Lcars.accent2(), 4))
		var foot := Lcars.hbox(4)
		var w := Lcars.label("%s / %s" % [Lcars.fmt(it["work"]), Lcars.fmt(it["cost"])], "Mono", 12)
		w.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		foot.add_child(w)
		var idx := i
		var pinned: bool = it.get("pinned", false)
		foot.add_child(_icon_btn("◆" if pinned else "◇", "Unpin" if pinned else "Pin (protect from governor changes)", lock, func(): app.issue({"type": "SetQueuePin", "systemId": sid, "queueIndex": idx, "queuePinned": not pinned})))
		foot.add_child(_icon_btn("▲", "Move up", lock if lock != "" else ("Already first" if i == 0 else ""), func(): app.issue({"type": "MoveConstruction", "systemId": sid, "queueIndex": idx, "queueDestination": idx - 1})))
		foot.add_child(_icon_btn("▼", "Move down", lock if lock != "" else ("Already last" if i == q.size() - 1 else ""), func(): app.issue({"type": "MoveConstruction", "systemId": sid, "queueIndex": idx, "queueDestination": idx + 1})))
		var nm: String = it["name"]
		foot.add_child(_icon_btn("✕", "Remove (invested work is lost)", lock, func(): app.issue({"type": "RemoveConstruction", "systemId": sid, "queueIndex": idx}, [], "%s removed." % nm)))
		col.add_child(foot)
		b.add_child(card)
	var pending_buy := {}
	for p in app.view.get("pending", []):
		if p["type"] == "BuyConstruction" and int(p.get("systemId", -1)) == sid:
			pending_buy = p
	var buy := Lcars.hbox(8)
	if not pending_buy.is_empty():
		buy.add_child(Lcars.para("Purchase ordered - completes at turn end.", Lcars.GOOD, 14))
		var pid := int(pending_buy["id"])
		buy.add_child(Lcars.pill("Cancel", "ghost", func(): app.cancel_orders([pid], "Purchase cancelled.")))
	else:
		var bl := Lcars.label("Buy current item  ¢%s" % Lcars.fmt(detail.get("rushCost", 0)), "", 14)
		bl.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		bl.tooltip_text = "Buying finishes the current item at turn end. Treasury %s." % Lcars.fmt(app.view["empire"].get("draftCredits", 0))
		bl.mouse_filter = Control.MOUSE_FILTER_PASS
		buy.add_child(bl)
		var reason := lock if lock != "" else (str(detail.get("buyBlocker", "")) if str(detail.get("buyBlocker", "")) != "" else ("Nothing to buy" if q.is_empty() else ""))
		var bb := Lcars.pill("Buy", "secondary", func(): app.issue({"type": "BuyConstruction", "systemId": sid}, [], "Purchase ordered."), reason)
		bb.name = "Buy"
		buy.add_child(bb)
	b.add_child(buy)
	var sm := Lcars.hbox(6)
	sm.add_child(Lcars.badge("%d ind/turn" % int(c["economy"]["industry"]), "primary"))
	if int(detail.get("overflow", 0)) > 0:
		sm.add_child(Lcars.badge("+%d carried" % int(detail["overflow"]), "good"))
	b.add_child(sm)
	return f

func _icon_btn(text: String, hint: String, reason: String, on: Callable) -> Button:
	var b := Button.new()
	b.text = text
	b.flat = true
	b.tooltip_text = ("Unavailable: " + reason) if reason != "" else hint
	b.disabled = reason != ""
	b.add_theme_color_override("font_color", Lcars.TEXT2)
	b.add_theme_color_override("font_hover_color", Lcars.accent())
	b.pressed.connect(on)
	return b

func _templates() -> Control:
	var v := app.view
	var temps: Array = v.get("templates", [])
	var f := Lcars.frame("Build templates", "%d saved" % temps.size(), "tertiary")
	f.name = "Templates"
	var b := Lcars.body(f)
	var sid := int(detail["colony"]["systemId"])
	var items := []
	for q in detail.get("queue", []):
		items.append({"product": q["product"], "referenceId": int(q["referenceId"])})
	var lock := _locked()
	if temps.is_empty():
		b.add_child(Lcars.para("Save this colony's queue as a template, then apply it anywhere. The default template is applied to every new colony with an empty queue.", Lcars.TEXT3, 13))
	for t in temps:
		var tid := int(t["id"])
		var tname: String = t["name"]
		var card := Lcars.card()
		var col := Lcars.vbox(4)
		card.add_child(col)
		var h := Lcars.hbox(6)
		h.add_child(Lcars.label(tname, "SubHeading", 17, Lcars.TEXT))
		if t.get("isDefault", false):
			h.add_child(Lcars.badge("default", "good"))
		col.add_child(h)
		col.add_child(Lcars.para(" · ".join(t["items"].map(func(i): return i["name"])), Lcars.TEXT3, 12))
		var blocker := lock
		if blocker == "":
			for dt in detail.get("templates", []):
				if int(dt["id"]) == tid:
					blocker = str(dt.get("blocker", ""))
		var acts := Lcars.hbox(4)
		acts.add_child(Lcars.pill("Apply", "secondary", func(): app.issue({"type": "ApplyBuildTemplate", "templateId": tid, "systemId": sid}, [], "“%s” applied." % tname), blocker, "Append these items to this colony's queue"))
		var is_def: bool = t.get("isDefault", false)
		acts.add_child(Lcars.pill("Default", "ghost", func(): app.issue({"type": "SetDefaultBuildTemplate", "templateId": 0 if is_def else tid}, [], "Default template updated."), lock))
		acts.add_child(Lcars.pill("Update", "ghost", func(): app.issue({"type": "DefineBuildTemplate", "templateId": tid, "templateName": tname, "templateItems": items}, [], "Template updated."), lock if lock != "" else ("The build queue is empty" if items.is_empty() else "")))
		acts.add_child(Lcars.pill("Delete", "alert", func(): app.confirm("Delete “%s”?" % tname, "Queues it was applied to are not affected.", "Delete", func(): app.issue({"type": "DeleteBuildTemplate", "templateId": tid}, [], "Template deleted.")), lock))
		col.add_child(acts)
		b.add_child(card)
	var nr := Lcars.hbox(6)
	var le := LineEdit.new()
	le.placeholder_text = "Template name"
	le.max_length = 48
	le.text = template_name
	le.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	le.name = "TemplateName"
	nr.add_child(le)
	var save := Lcars.pill("Save queue", "secondary", func():
		var n := le.text.strip_edges()
		if n == "":
			app.toast("Name the template", "warn")
			return
		app.issue({"type": "DefineBuildTemplate", "templateId": 0, "templateName": n, "templateItems": items}, [], "Template “%s” saved." % n), lock if lock != "" else ("The build queue is empty" if items.is_empty() else ""), "Save the %d queued items as a template" % items.size())
	nr.add_child(save)
	b.add_child(nr)
	if str(detail.get("queueExplanation", "")) != "":
		b.add_child(Lcars.para(str(detail["queueExplanation"]), Lcars.TEXT3, 12))
	return f

func _megaproject() -> Control:
	var m: Dictionary = detail.get("megaproject", {})
	if not m.get("enabled", false):
		return null
	var f := Lcars.frame(str(m["name"]), "scientific victory", "good")
	f.name = "Megaproject"
	var b := Lcars.body(f)
	var lock := _locked()
	var sid := int(detail["colony"]["systemId"])
	var elsewhere: bool = m.has("systemId") and not m.get("here", false)
	if m.get("here", false) or elsewhere:
		b.add_child(Lcars.para("%s: %s / %s industry, up to %d per turn." % ["Under construction here" if m.get("here", false) else "Under construction at %s" % m.get("system", ""), Lcars.fmt(m.get("work", 0)), Lcars.fmt(m["cost"]), int(m["workPerTurn"])]))
		b.add_child(Lcars.meter(m.get("work", 0), m["cost"], Lcars.GOOD, 7))
		var nm: String = m["name"]
		b.add_child(Lcars.pill("Abandon", "alert", func(): app.confirm("Abandon the %s?" % nm, "All work is lost and the credits are not refunded.", "Abandon", func(): app.issue({"type": "CancelMegaproject"}, [], "The %s was abandoned." % nm)), lock if lock != "" else str(m.get("cancelBlocker", ""))))
	else:
		b.add_child(Lcars.para("Completing the %s wins a scientific victory. It costs ¢%s up front, then absorbs up to %d industry per turn from this colony (%s in all)." % [m["name"], Lcars.fmt(m["credits"]), int(m["workPerTurn"]), Lcars.fmt(m["cost"])], Lcars.TEXT2, 13))
		var acts := Lcars.hbox(6)
		var nm2: String = m["name"]
		acts.add_child(Lcars.pill("Begin here", "good", func(): app.issue({"type": "StartMegaproject", "systemId": sid}, [], "The %s begins." % nm2), lock if lock != "" else str(m.get("startBlocker", ""))))
		acts.add_child(Lcars.pill("Victory progress", "ghost", func(): app.open_overlay("victory")))
		b.add_child(acts)
		if str(m.get("startBlocker", "")) != "":
			b.add_child(Lcars.para(str(m["startBlocker"]), Lcars.WARN, 12))
	return f
