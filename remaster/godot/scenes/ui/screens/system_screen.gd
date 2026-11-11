extends Control
## System (colony) presentation: planets, workforce with the core's forecast, governor, energy and shipyard,
## construction choices, build queue, build templates and the megaproject.
## Data and orders: SystemVM (scripts/viewmodels/system_vm.gd).

var app: App
var vm: SystemVM:
	get:
		if vm == null:
			vm = SystemVM.new(app.session)
		return vm
var detail: Dictionary:
	get: return vm.detail
var draft: Dictionary:
	get: return vm.draft
	set(v): vm.draft = v
var tab: String:
	get: return vm.tab
	set(v): vm.tab = v
var _sliders := {}
var _forecast: Control
var _wf_out := {}

const FIELDS := SystemVM.FIELDS

func _locked() -> String:
	return vm.lock_reason()

func cycle(d: int) -> void:
	if vm.cycle(d):
		refresh()

func _unhandled_key_input(event: InputEvent) -> void:
	var k := event as InputEventKey
	if k and k.pressed and not k.echo and not app.has_modal() and not (get_viewport().gui_get_focus_owner() is LineEdit):
		if k.keycode == KEY_Q or k.keycode == KEY_BRACKETLEFT:
			cycle(-1)
			get_viewport().set_input_as_handled()
		elif k.keycode == KEY_E or k.keycode == KEY_BRACKETRIGHT:
			cycle(1)
			get_viewport().set_input_as_handled()

func refresh() -> void:
	for c in get_children():
		c.queue_free()
	var sid := vm.system_id()
	if sid == 0:
		var f := Lcars.frame("System")
		f.set_anchors_preset(Control.PRESET_FULL_RECT)
		Lcars.body(f).add_child(Lcars.empty_note("You have no colonies."))
		add_child(f)
		return
	if not vm.fetch(sid):
		app.report_error(app.core.last_error)
		return
	var c := vm.colony()
	var root := Lcars.vbox(12)
	Lcars.screen_margin(self, Vector4(16, 16, 10, 16)).add_child(root)
	root.add_child(_header(c))
	root.add_child(_kpis())
	var cols := Lcars.hbox(14)
	cols.size_flags_vertical = Control.SIZE_EXPAND_FILL
	root.add_child(cols)
	var left := Lcars.vbox(12)
	left.custom_minimum_size.x = 560
	left.add_child(_workforce(c))
	left.add_child(_governor(c))
	left.add_child(_energy(c))
	var dil := _dilithium(c)
	if dil:
		left.add_child(dil)
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
	Motion.stagger(cols)

## The hero band: colony name and navigation, then the system's planets as lit spheres beside their star.
func _header(c: Dictionary) -> Control:
	var pos := vm.colony_position()
	var count: int = pos[1]
	var band := PanelContainer.new()
	band.name = "Orrery"
	var sb := Lcars.panel_box(Lcars.accent())
	sb.bg_color = Color(Lcars.BG2, 0.7)
	sb.content_margin_top = 8
	sb.content_margin_bottom = 8
	band.add_theme_stylebox_override("panel", sb)
	var h := Lcars.hbox(18)
	band.add_child(h)
	h.add_child(_nav("◂", -1, count, "Previous colony", "Q"))
	var tb := Lcars.vbox(0)
	tb.custom_minimum_size.x = 300
	tb.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	var nm := Lcars.label(str(c["name"]).to_upper(), "", 58, Lcars.accent())
	nm.add_theme_font_override("font", Lcars.font("display", 700))
	tb.add_child(nm)
	tb.add_child(Lcars.caption("Colony %d of %d · %s" % [int(pos[0]) + 1, count, _sector()]))
	var badges := Lcars.hbox(6)
	var gov := str(c["governor"]["workforce"])
	badges.add_child(Lcars.badge(TrekText.term(app.player(), "governor", "Governor") + " " + ("manual" if gov == "Manual" else gov), "secondary" if gov != "Manual" else "dim"))
	if int(c["economy"]["foodDeficit"]) > 0:
		badges.add_child(Lcars.badge("Food short", "alert"))
	if not (c.get("building") is Dictionary):
		badges.add_child(Lcars.badge("Queue idle", "warn"))
	tb.add_child(badges)
	h.add_child(tb)
	h.add_child(_orrery())
	h.add_child(_nav("▸", 1, count, "Next colony", "E"))
	return band

func _nav(glyph: String, d: int, count: int, hint: String, key: String) -> Button:
	var b := Lcars.pill(glyph, "ghost", func(): cycle(d), "" if count > 1 else "Only one colony", hint, key)
	b.custom_minimum_size = Vector2(44, 110)
	b.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	b.add_theme_font_size_override("font_size", 22)
	return b

## Star at the left, then each planet sized by class, its numeral, class and capacity bar; hover explains it.
func _orrery() -> Control:
	var row := Lcars.hbox(4)
	row.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.alignment = BoxContainer.ALIGNMENT_CENTER
	var star := StarGlow.new()
	star.custom_minimum_size = Vector2(110, 140)
	row.add_child(star)
	var n := 0
	for p in detail.get("planets", []):
		var cls := str(p["class"])
		var px := 112.0 if cls == "GasGiant" else (64.0 if cls == "K" or cls == "Y" else (96.0 if cls == "Asteroids" else 82.0))
		var col := Lcars.vbox(2)
		col.custom_minimum_size.x = 118
		var host := RichTip.Explained.new(SystemVM.planet_explain(p))
		var orb := PlanetOrb.make(cls, px, int(vm.sid()) * 13 + n)
		orb.custom_minimum_size = Vector2(118, 112)
		host.add_child(orb)
		col.add_child(host)
		var nm := str(p["name"])
		var sp := nm.rfind(" ")
		var lab := Lcars.hbox(6)
		lab.alignment = BoxContainer.ALIGNMENT_CENTER
		lab.add_child(Lcars.label(nm.substr(sp + 1) if sp > 0 else nm, "", 18, Lcars.TEXT))
		lab.get_child(0).add_theme_font_override("font", Lcars.font("display", 600))
		lab.add_child(Lcars.label("Gas giant" if cls == "GasGiant" else ("Belt" if cls == "Asteroids" else "Class " + cls), "Caption"))
		col.add_child(lab)
		if int(p["maxCapacity"]) > 0:
			var bar := Lcars.meter(p["capacity"], max(1, int(p["maxCapacity"])), Lcars.GOOD, 4)
			bar.custom_minimum_size.x = 80
			bar.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
			col.add_child(bar)
			var cap := Lcars.label("%d/%d" % [int(p["capacity"]), int(p["maxCapacity"])], "Mono", 12)
			cap.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
			col.add_child(cap)
		row.add_child(col)
		n += 1
	return row

func _sector() -> String:
	var sy := app.find_by_id("systems", vm.sid())
	return ("sector %s" % vm.place(int(sy["x"]), int(sy["y"]))) if not sy.is_empty() else "your space"

## The KPI row of the colony, each explained (SystemVM.kpis).
func _kpis() -> Control:
	var row := Lcars.hbox(10)
	row.name = "ColonyKpis"
	for k in vm.kpis():
		var t := Lcars.kpi(k["icon"], k["cap"], k["value"], k["delta"], k["tone"], k["explain"])
		t.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		t.custom_minimum_size.x = 0
		row.add_child(t)
	return row

## A sun with corona at the left of the orrery.
class StarGlow extends Control:
	func _ready() -> void:
		mouse_filter = Control.MOUSE_FILTER_IGNORE
	func _draw() -> void:
		var c := Vector2(size.x * 0.2, size.y / 2.0)
		for k in 12:
			draw_circle(c, 70.0 - k * 5.0, Color(1.0, 0.82, 0.5, 0.035 + k * 0.006))
		draw_circle(c, 18.0, Color(1.0, 0.93, 0.75))
		draw_circle(c, 12.0, Color(1, 1, 1))

func _workforce(c: Dictionary) -> Control:
	var f := Lcars.frame("Workforce", "%d unassigned" % vm.unassigned(), "primary", "population")
	f.name = "Workforce"
	var b := Lcars.body(f)
	var w: Dictionary = c["workers"]
	var top := Lcars.hbox(16)
	var ring := RingGauge.make(108, 0, 1, Lcars.accent(), str(int(c["population"])), "WORKERS",
		{"title": "Workforce", "value": "%d citizens" % int(c["population"]), "parts": [["Food", str(int(w["food"])), "good"], ["Industry", str(int(w["industry"])), "secondary"], ["Research", str(int(w["research"])), "info"], ["Unassigned", str(int(w.get("unassigned", 0))), "warn"]],
			"effect": "Drag a slider to move workers; the core forecasts the result before you apply it."})
	ring.segments = [[int(w["food"]), Lcars.GOOD], [int(w["industry"]), Lcars.accent2()], [int(w["research"]), Lcars.INFO], [int(w.get("unassigned", 0)), Lcars.WARN]]
	top.add_child(ring)
	var right := Lcars.vbox(8)
	right.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	top.add_child(right)
	b.add_child(top)
	var acts := Lcars.hbox(8)
	acts.add_child(Lcars.pill("Balance", "ghost", func(): if not vm.auto_balance(false): _update_forecast(), _locked(), "Feed the colony, then split industry and research"))
	acts.add_child(Lcars.pill("Industry", "ghost", func(): if not vm.auto_balance(true): _update_forecast(), _locked(), "Feed the colony, then maximise industry"))
	right.add_child(acts)
	var legend := Lcars.hbox(14)
	for lg in [["Food", Lcars.GOOD], ["Industry", Lcars.accent2()], ["Research", Lcars.INFO], ["Idle", Lcars.WARN]]:
		var it := Lcars.hbox(5)
		var dot := ColorRect.new()
		dot.color = lg[1]
		dot.custom_minimum_size = Vector2(10, 10)
		dot.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		it.add_child(dot)
		it.add_child(Lcars.label(lg[0], "Muted", 13))
		legend.add_child(it)
	right.add_child(legend)
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
	var v := vm.set_workers(key, value)
	if int(_sliders[key].value) != v:
		_sliders[key].set_value_no_signal(v)
	cnt.text = "%d/%d" % [v, int(detail["capacityLimits"][key])]
	_update_forecast()

## Shows the core's forecast for the drafted staffing.
func _update_forecast() -> void:
	if _forecast == null or not is_instance_valid(_forecast):
		return
	for ch in _forecast.get_children():
		ch.queue_free()
	var c := vm.colony()
	var pop := int(c["population"])
	var eco := vm.forecast_economy()
	var growth := vm.forecast_growth()
	for fd in FIELDS:
		var key: String = fd["key"]
		var now := int(c["economy"].get(key, 0))
		var nxt := int(eco.get(key, 0))
		var l: Label = _wf_out.get(key)
		if l:
			l.text = "%d%s%s" % [nxt, (" (%s)" % Lcars.signed(nxt - now)) if nxt != now else "", (" / %d needed" % pop) if key == "food" else ""]
			l.add_theme_color_override("font_color", Lcars.GOOD if nxt > now else (Lcars.ALERT if nxt < now else Lcars.TEXT2))
	if vm.preview_error() != "":
		_forecast.add_child(Lcars.label(vm.preview_error(), "", 14, Lcars.ALERT))
		return
	_forecast.add_child(Lcars.stat("Growth", SystemVM.forecast_growth_text(growth), "alert" if int(growth.get("max", 0)) < 0 else "good", "Population change next turn with this staffing (core population forecast)."))
	_forecast.add_child(Lcars.stat("Food", Lcars.signed(int(eco["food"]) - pop), "alert" if int(eco["food"]) < pop else "good"))
	_forecast.add_child(Lcars.stat("Credits", Lcars.signed(eco["net"])))
	var idle := vm.unassigned()
	_forecast.add_child(Lcars.stat("Idle", idle, "warn" if idle > 0 else ""))
	if vm.draft_dirty():
		var ap := Lcars.pill("Apply", "primary", func(): apply_workers(), _locked())
		ap.name = "ApplyWorkers"
		ap.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		_forecast.add_child(ap)

func apply_workers() -> void:
	if not vm.apply_workers():
		_update_forecast()

func _governor(c: Dictionary) -> Control:
	var g: Dictionary = c["governor"]
	var f := Lcars.frame(TrekText.term(app.player(), "governor", "Governor"), "automation", "secondary", "settings")
	var b := Lcars.body(f)
	for which in ["workforce", "queue"]:
		var row := Lcars.hbox(4)
		var l := Lcars.caption("Workforce" if which == "workforce" else "Queue")
		l.custom_minimum_size.x = 84
		Lcars.tip(l, "Automatically assigns citizens each turn." if which == "workforce" else "Queues construction whenever the build queue runs empty.")
		row.add_child(l)
		for o in app.meta.get("governors", []):
			var key: String = o["key"]
			var w: String = which
			var t := Lcars.toggle(o["name"], g[which] == key, func(_on): vm.set_governor(w, key))
			t.add_theme_font_size_override("font_size", 14)
			t.disabled = _locked() != ""
			row.add_child(t)
		b.add_child(row)
	if g.get("override", false) and g["workforce"] != "Manual":
		var r := Lcars.hbox(8)
		r.add_child(Lcars.para("Manual staffing is overriding the governor.", Lcars.WARN, 13))
		r.add_child(Lcars.pill("Resume governor", "primary", func(): vm.resume_governor()))
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
	var f := Lcars.frame("Energy & shipyard", "", "tertiary", "energy")
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
		sy.add_child(Lcars.pill("Switch %s" % ("off" if c.get("shipyardEnabled", false) else "on"), "ghost", func(): vm.toggle_shipyard(), vm.shipyard_block()))
	b.add_child(sy)
	return f

## Dilithium refinery and the empire's ship construction slots (schema 15). Null when the rules are off.
func _dilithium(c: Dictionary) -> Control:
	var lines := SystemVM.dilithium_lines(vm.dilithium(), str(detail.get("queueBlocker", "")))
	if lines.is_empty():
		return null
	var f := Lcars.frame("Dilithium", "ship slots", "secondary", "energy")
	f.name = "DilithiumPanel"
	var b := Lcars.body(f)
	var d := vm.dilithium()
	var r := Lcars.hbox(8)
	r.add_child(Lcars.label("Slots", "Muted", 13))
	r.add_child(Lcars.meter(int(d["slotsUsed"]), max(1, int(d["slots"])), Lcars.WARN if int(d["slotsUsed"]) >= int(d["slots"]) else Lcars.accent2(), 8))
	r.add_child(Lcars.label("%d/%d" % [int(d["slotsUsed"]), int(d["slots"])], "Mono", 13))
	b.add_child(r)
	for ln in lines:
		b.add_child(Lcars.para(str(ln["text"]), Lcars.tone_color(str(ln["tone"])) if str(ln["tone"]) != "" else Lcars.TEXT2, 13))
	return f

func _construction() -> Control:
	var f := Lcars.frame("Construction", "", "primary", "build")
	f.name = "Construction"
	var b := Lcars.body(f)
	var tabs := Lcars.hbox(6)
	for t in [["structures", "Structures  %d" % vm.available_count(false)], ["ships", "Starships  %d" % vm.available_count(true)], ["installed", "Installed"]]:
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
			r.add_child(Lcars.pill("Scrap", "alert", func():
				app.confirm("Scrap a %s?" % nm.to_lower(), "The structure is demolished at turn end. Cancel the order to undo it before then.", "Scrap",
					func(): vm.scrap(bld, nm)), vm.scrap_block(s)))
			list.add_child(r)
	else:
		var src := vm.choices(tab == "ships")
		if src.is_empty():
			list.add_child(Lcars.empty_note("No starships available - build and power a shipyard." if tab == "ships" else "Nothing can be built here right now."))
		for ch in src:
			list.add_child(_choice(ch))
	b.add_child(Lcars.scroll(list))
	return f

## The class name ("Oberth") of a ship choice: the encyclopedia entry of its reference ship, else the front of its name.
func _ship_class(ch: Dictionary) -> String:
	for sh in app.get_encyclopedia().get("ships", []):
		if int(sh.get("id", -1)) == int(ch.get("referenceId", -2)) and str(sh.get("designClass", "")) != "":
			return str(sh["designClass"])
	return ShipGlyph.class_of({"name": str(ch.get("name", ""))})

func _choice(ch: Dictionary) -> Control:
	var reason := vm.choice_block(ch)
	var btn := Button.new()
	btn.name = "Choice_%s_%d" % [ch["product"], int(ch["referenceId"])]
	btn.theme_type_variation = "GhostButton"
	btn.custom_minimum_size.y = 58 if reason == "" else 82 # the reason is a third text line: room for it, so it never crowds the next row
	btn.disabled = reason != ""
	var sb := Lcars.box(Color(Lcars.PANEL2, 0.8), 12, Lcars.LINE2, 1, Vector4(12, 6, 12, 6))
	btn.add_theme_stylebox_override("disabled", Lcars.box(Color(Lcars.PANEL, 0.6), 12, Lcars.LINE, 1, Vector4(12, 6, 12, 6)))
	btn.add_theme_stylebox_override("normal", sb)
	btn.tooltip_text = ("Unavailable: " + reason) if reason != "" else SystemVM.choice_tip(ch)
	var row := Lcars.row_inner(btn, 12)
	var icon := ChoiceIcon.new()
	icon.product = str(ch["product"])
	icon.ship = ch["isShip"]
	if icon.ship:
		icon.owner_id = app.player()
		icon.klass = _ship_class(ch)
	icon.custom_minimum_size = Vector2(34, 34)
	icon.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	row.add_child(icon)
	var main := Lcars.vbox(0)
	main.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	main.alignment = BoxContainer.ALIGNMENT_CENTER
	var nm := Lcars.label(str(ch["name"]), "", 15, Lcars.TEXT if reason == "" else Lcars.TEXT3)
	nm.add_theme_font_override("font", Lcars.font("ui", 560))
	main.add_child(nm)
	main.add_child(Lcars.label(SystemVM.choice_meta(ch), "Muted", 12))
	if reason != "":
		main.add_child(Lcars.label(reason, "", 12, Lcars.WARN))
	row.add_child(main)
	row.add_child(Lcars.label(("%dT" % int(ch["turns"])) if int(ch["turns"]) > 0 else "∞", "Mono", 15, Lcars.accent2()))
	row.add_child(Lcars.label("+", "", 26, Lcars.accent()))
	Lcars.ignore_mouse(row)
	btn.pressed.connect(func(): vm.enqueue(ch))
	return btn

func _queue(c: Dictionary) -> Control:
	var q := vm.queue()
	var f := Lcars.frame("Build queue", "%d item%s" % [q.size(), "" if q.size() == 1 else "s"], "secondary", "industry")
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
		var pl := str(it.get("product", "")).to_lower()
		if bool(it.get("isShip", false)) or pl.begins_with("ship") or pl.begins_with("starship"):
			h.add_child(ShipPicture.make(app.player(), _ship_class({"name": str(it["name"]), "referenceId": int(it.get("referenceId", -1))}), 30, "top", Lcars.accent()))
		var n := Lcars.label(str(it["name"]), "", 15, Lcars.TEXT)
		n.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		h.add_child(n)
		var eta := Lcars.label(("%dT" % int(it["eta"])) if int(it["eta"]) > 0 else "-", "Mono", 15, Lcars.accent2())
		Lcars.tip(eta, "Estimated completion in %d turns at %d industry/turn." % [int(it["eta"]), int(c["economy"]["industry"])] if int(it["eta"]) > 0 else "No industry: this item makes no progress.")
		h.add_child(eta)
		col.add_child(h)
		col.add_child(Lcars.meter(it["work"], it["cost"], Lcars.accent() if i == 0 else Lcars.accent2(), 4))
		var foot := Lcars.hbox(4)
		var w := Lcars.label("%s / %s" % [Lcars.fmt(it["work"]), Lcars.fmt(it["cost"])], "Mono", 12)
		w.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		foot.add_child(w)
		var idx := i
		var pinned: bool = it.get("pinned", false)
		foot.add_child(Lcars.icon_button("◆" if pinned else "◇", "Unpin" if pinned else "Pin (protect from governor changes)", lock, func(): vm.set_pin(idx, not pinned)))
		foot.add_child(Lcars.icon_button("▲", "Move up", lock if lock != "" else ("Already first" if i == 0 else ""), func(): vm.move_item(idx, idx - 1)))
		foot.add_child(Lcars.icon_button("▼", "Move down", lock if lock != "" else ("Already last" if i == q.size() - 1 else ""), func(): vm.move_item(idx, idx + 1)))
		var nm: String = it["name"]
		foot.add_child(Lcars.icon_button("✕", "Remove (invested work is lost)", lock, func(): vm.remove_item(idx, nm)))
		col.add_child(foot)
		b.add_child(card)
	var pending_buy := vm.pending_buy()
	var buy := Lcars.hbox(8)
	if not pending_buy.is_empty():
		buy.add_child(Lcars.para("Purchase ordered - completes at turn end.", Lcars.GOOD, 14))
		var pid := int(pending_buy["id"])
		buy.add_child(Lcars.pill("Cancel", "ghost", func(): vm.cancel([pid], "Purchase cancelled.")))
	else:
		var bl := Lcars.label("Buy current item  ¢%s" % Lcars.fmt(detail.get("rushCost", 0)), "", 14)
		bl.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		Lcars.tip(bl, "Buying finishes the current item at turn end. Treasury %s." % Lcars.fmt(app.view["empire"].get("draftCredits", 0)))
		buy.add_child(bl)
		var bb := Lcars.pill("Buy", "secondary", func(): vm.buy(), vm.buy_block())
		bb.name = "Buy"
		buy.add_child(bb)
	b.add_child(buy)
	var sm := Lcars.hbox(6)
	sm.add_child(Lcars.badge("%d ind/turn" % int(c["economy"]["industry"]), "primary"))
	if int(detail.get("overflow", 0)) > 0:
		sm.add_child(Lcars.badge("+%d carried" % int(detail["overflow"]), "good"))
	b.add_child(sm)
	return f

func _templates() -> Control:
	var temps: Array = app.view.get("templates", [])
	var f := Lcars.frame("Build templates", "%d saved" % temps.size(), "tertiary", "chronicle")
	f.name = "Templates"
	var b := Lcars.body(f)
	var items := vm.queue_items()
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
		var acts := Lcars.hbox(4)
		acts.add_child(Lcars.pill("Apply", "secondary", func(): vm.apply_template(tid, tname), vm.template_block(tid), "Append these items to this colony's queue"))
		var is_def: bool = t.get("isDefault", false)
		acts.add_child(Lcars.pill("Default", "ghost", func(): vm.set_default_template(tid, is_def), lock))
		acts.add_child(Lcars.pill("Update", "ghost", func(): vm.define_template(tid, tname, "Template updated."), vm.save_block()))
		acts.add_child(Lcars.pill("Delete", "alert", func(): app.confirm("Delete “%s”?" % tname, "Queues it was applied to are not affected.", "Delete", func(): vm.delete_template(tid)), lock))
		col.add_child(acts)
		b.add_child(card)
	var nr := Lcars.hbox(6)
	var le := LineEdit.new()
	le.placeholder_text = "Template name"
	le.max_length = 48
	le.text = vm.template_name
	le.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	le.name = "TemplateName"
	nr.add_child(le)
	var save := Lcars.pill("Save queue", "secondary", func():
		var n := le.text.strip_edges()
		if n == "":
			app.toast("Name the template", "warn")
			return
		vm.define_template(0, n, "Template “%s” saved." % n), vm.save_block(), "Save the %d queued items as a template" % items.size())
	nr.add_child(save)
	b.add_child(nr)
	if str(detail.get("queueExplanation", "")) != "":
		b.add_child(Lcars.para(str(detail["queueExplanation"]), Lcars.TEXT3, 12))
	return f

func _megaproject() -> Control:
	var m := vm.megaproject()
	if m.is_empty():
		return null
	var f := Lcars.frame(str(m["name"]), "scientific victory", "good", "victory")
	f.name = "Megaproject"
	var b := Lcars.body(f)
	var lock := _locked()
	var elsewhere: bool = m.has("systemId") and not m.get("here", false)
	var nm: String = m["name"]
	if m.get("here", false) or elsewhere:
		b.add_child(Lcars.para("%s: %s / %s industry, up to %d per turn." % ["Under construction here" if m.get("here", false) else "Under construction at %s" % m.get("system", ""), Lcars.fmt(m.get("work", 0)), Lcars.fmt(m["cost"]), int(m["workPerTurn"])]))
		b.add_child(Lcars.meter(m.get("work", 0), m["cost"], Lcars.GOOD, 7))
		b.add_child(Lcars.pill("Abandon", "alert", func(): app.confirm("Abandon the %s?" % nm, "All work is lost and the credits are not refunded.", "Abandon", func(): vm.cancel_megaproject(nm)), lock if lock != "" else str(m.get("cancelBlocker", ""))))
	else:
		b.add_child(Lcars.para("Completing the %s wins a scientific victory. It costs ¢%s up front, then absorbs up to %d industry per turn from this colony (%s in all)." % [m["name"], Lcars.fmt(m["credits"]), int(m["workPerTurn"]), Lcars.fmt(m["cost"])], Lcars.TEXT2, 13))
		var acts := Lcars.hbox(6)
		acts.add_child(Lcars.pill("Begin here", "good", func(): vm.start_megaproject(nm), lock if lock != "" else str(m.get("startBlocker", ""))))
		acts.add_child(Lcars.pill("Victory progress", "ghost", func(): app.open_overlay("victory")))
		b.add_child(acts)
		if str(m.get("startBlocker", "")) != "":
			b.add_child(Lcars.para(str(m["startBlocker"]), Lcars.WARN, 12))
	return f
