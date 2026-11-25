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
	if PlatformMode.is_mobile():
		cols.custom_minimum_size.y = 440   # a phone window leaves these columns no height at all (their lists collapse to nothing): the screen pans instead
	root.add_child(cols)
	var left := Lcars.vbox(12)
	left.custom_minimum_size.x = 560
	left.add_child(_workforce(c))
	left.add_child(_governor(c))
	left.add_child(_energy(c))
	var dil := _dilithium(c)
	if dil:
		left.add_child(dil)
	var prog := _program(c)
	if prog:
		left.add_child(prog)
	var eco := _economy_rules()
	if eco:
		left.add_child(eco)
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
	# the 3D system fills the whole band; the title block and the colony arrows ride over it (F3 review: the view sat in a
	# strip right of the title)
	var stack := Control.new()
	stack.custom_minimum_size = Vector2(0, 300)
	stack.mouse_filter = Control.MOUSE_FILTER_IGNORE
	band.add_child(stack)
	var view := _system_view()
	view.set_anchors_preset(Control.PRESET_FULL_RECT)
	view.left_inset = 44 + 18 + 300 + 18
	stack.add_child(view)
	var h := Lcars.hbox(18)
	h.set_anchors_preset(Control.PRESET_FULL_RECT)
	h.mouse_filter = Control.MOUSE_FILTER_IGNORE
	stack.add_child(h)
	h.add_child(_nav("delta_left", -1, count, "Previous colony", "Q"))
	var tb := Lcars.vbox(0)
	tb.custom_minimum_size.x = 300
	tb.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	var nm := Lcars.label(str(c["name"]).to_upper(), "", 58, Lcars.accent())
	nm.add_theme_font_override("font", Lcars.font("display", 700))
	tb.add_child(nm)
	tb.add_child(Lcars.caption("%d/%d · %s" % [int(pos[0]) + 1, count, _sector()]))
	var badges := Lcars.hbox(6)
	var gov := str(c["governor"]["workforce"])
	badges.add_child(Lcars.badge(TrekText.term(app.player(), "governor", "Governor") + " " + ("manual" if gov == "Manual" else gov), "secondary" if gov != "Manual" else "dim"))
	if int(c["economy"]["foodDeficit"]) > 0:
		badges.add_child(Lcars.badge("Food short", "alert"))
	if not (c.get("building") is Dictionary):
		badges.add_child(Lcars.badge("Queue idle", "warn"))
	tb.add_child(badges)
	h.add_child(tb)
	var gap := Lcars.spacer()
	gap.mouse_filter = Control.MOUSE_FILTER_IGNORE   # clicks reach the planets under it
	h.add_child(gap)
	h.add_child(_nav("delta_right", 1, count, "Next colony", "E"))
	return band

func _nav(glyph: String, d: int, count: int, hint: String, key: String) -> Button:
	var b := Lcars.pill("", "ghost", func(): cycle(d), "" if count > 1 else "Only one colony", hint, key)
	b.custom_minimum_size = Vector2(44, 110)
	b.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	var ic := UiIcon.make(glyph, 22, Lcars.TEXT2 if count > 1 else Lcars.TEXT3)
	ic.position = Vector2(11, 44)
	ic.size = Vector2(22, 22)
	b.add_child(ic)
	return b

## F3: the system in 3D (SystemView3D): its star, the planets on their orbits with the colony's structures, the LCARS
## labels over them. The flat orrery below stays for tools that want it.
func _system_view() -> SystemView3D:
	var sid := int(vm.sid())
	var star := Color(1.0, 0.86, 0.58)
	var seed_v := int(app.view.get("meta", {}).get("seed", 0))
	for s in app.view.get("systems", []):
		if int(s["id"]) == sid:
			var ti := int(s["starType"]) if s.has("starType") else GalaxyLayout.star_type(int(s["x"]), int(s["y"]), seed_v)
			star = GalaxyLayout.STAR_TYPES[clampi(ti, 0, GalaxyLayout.STAR_TYPES.size() - 1)]["color"]
			break
	var c := vm.colony()
	return SystemView3D.make(detail.get("planets", []), c.get("structures", {}), star, sid, Vector2(640, 300))

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
		lab.add_child(Lcars.label("GG" if cls == "GasGiant" else ("Belt" if cls == "Asteroids" else cls), "Caption"))
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
	return vm.place(int(sy["x"]), int(sy["y"])) if not sy.is_empty() else "-"

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

## Workforce as people (docs: playtest 16 Nov, "easier to navigate because of pictures"): a row of figures per sector, a figure is
## `unit` workers; the number beside it is what the sector makes. The core's forecast sits beside the ring.
func _workforce(c: Dictionary) -> Control:
	var f := Lcars.frame("Workforce", "", "primary", "population")
	f.name = "Workforce"
	var b := Lcars.body(f)
	var w: Dictionary = c["workers"]
	var top := Lcars.hbox(16)
	var ring := RingGauge.make(108, 0, 1, Lcars.accent(), str(int(c["population"])), "",
		{"title": "Workforce", "value": "%d citizens" % int(c["population"]), "parts": [["Food", str(int(w["food"])), "good"], ["Industry", str(int(w["industry"])), "secondary"], ["Research", str(int(w["research"])), "info"], ["Unassigned", str(int(w.get("unassigned", 0))), "warn"]],
			"effect": "Click the people in a row to move workers; the core forecasts the result before you apply it."})
	ring.segments = [[int(w["food"]), Lcars.GOOD], [int(w["industry"]), Lcars.accent2()], [int(w["research"]), Lcars.INFO], [int(w.get("unassigned", 0)), Lcars.WARN]]
	top.add_child(ring)
	var right := Lcars.vbox(10)
	right.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	right.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	top.add_child(right)
	b.add_child(top)
	var acts := Lcars.hbox(8)
	var bal := Lcars.pill("Balance", "ghost", func(): if not vm.auto_balance(false): _update_forecast(), _locked(), "Feed the colony, then split industry and research")
	bal.name = "WorkforceBalance"   # the advisor's Balance hint points here
	acts.add_child(bal)
	acts.add_child(Lcars.pill("Industry", "ghost", func(): if not vm.auto_balance(true): _update_forecast(), _locked(), "Feed the colony, then maximise industry"))
	right.add_child(acts)
	_forecast = Lcars.hbox(16)
	_forecast.name = "Forecast"
	right.add_child(_forecast)
	var limits: Dictionary = detail["capacityLimits"]
	_sliders.clear()
	_wf_out.clear()
	for fd in FIELDS:
		var key: String = fd["key"]
		var col := Pic.tint(key)
		var row := Lcars.hbox(10)
		var lead := Lcars.hbox(8)
		lead.custom_minimum_size.x = 112
		lead.add_child(UiIcon.make(key, 30, col))
		var lt := Lcars.vbox(0)
		lt.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		lt.add_child(Lcars.label(str(fd["label"]), "SubHeading", 16, Lcars.TEXT))
		lt.add_child(Lcars.label("x%d" % int(c["structures"][fd["structure"]]), "Mono", 12, Lcars.TEXT3))
		lead.add_child(lt)
		Pic.tipped(lead, "%d %s" % [int(c["structures"][fd["structure"]]), fd["structure"]])
		row.add_child(lead)
		var ppl := PeopleRow.make(int(draft[key]), int(limits[key]), col, _locked() == "", "%s workers (up to %d jobs)" % [fd["label"], int(limits[key])])
		ppl.name = "wf_" + key
		var cnt := Lcars.label("%d/%d" % [int(draft[key]), int(limits[key])], "Mono", 12, Lcars.TEXT3)
		cnt.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		ppl.changed.connect(func(val): _on_people(key, val, cnt))
		ppl.released.connect(func(): apply_workers())
		_sliders[key] = ppl
		row.add_child(ppl)
		var res := Lcars.vbox(0)
		res.custom_minimum_size.x = 92
		res.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		var out := Lcars.label("", "Mono", 17)
		out.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		_wf_out[key] = out
		res.add_child(out)
		res.add_child(cnt)
		row.add_child(res)
		b.add_child(row)
	_update_forecast()
	return f

func _on_people(key: String, value: int, cnt: Label) -> void:
	var v := vm.set_workers(key, value)
	if int(_sliders[key].value) != v:
		_sliders[key].set_value_no_signal(v)
	cnt.text = "%d/%d" % [v, int(detail["capacityLimits"][key])]
	_update_forecast()

## Shows the core's forecast for the drafted staffing: icons and numbers, the words in the tooltips.
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
			l.text = "%d%s%s" % [nxt, (" (%s)" % Lcars.signed(nxt - now)) if nxt != now else "", (" / %d" % pop) if key == "food" else ""]
			l.add_theme_color_override("font_color", Lcars.GOOD if nxt > now else (Lcars.ALERT if nxt < now else Lcars.TEXT))
			l.tooltip_text = ("%s per turn. Food: %d made for %d people." % [fd["label"], nxt, pop]) if key == "food" else "%s per turn." % fd["label"]
			l.mouse_filter = Control.MOUSE_FILTER_PASS
	if vm.preview_error() != "":
		_forecast.add_child(Lcars.label(vm.preview_error(), "", 14, Lcars.ALERT))
		return
	var g_ok := int(growth.get("max", 0)) >= 0
	_forecast.add_child(Pic.num("growth", SystemVM.forecast_growth_text(growth), "Population change next turn with this staffing (core population forecast).", Lcars.GOOD if g_ok else Lcars.ALERT, 20, 17))
	_forecast.add_child(Pic.num("food", Lcars.signed(int(eco["food"]) - pop), "Food surplus (+) or shortfall (-) next turn.", Lcars.GOOD if int(eco["food"]) >= pop else Lcars.ALERT, 20, 17))
	_forecast.add_child(Pic.num("credits", Lcars.signed(eco["net"]), "Credits per turn after upkeep.", null, 20, 17))
	var idle := vm.unassigned()
	_forecast.add_child(Pic.num("population", str(idle), "Idle citizens with no job.", Lcars.WARN if idle > 0 else Lcars.TEXT3, 20, 17, Lcars.WARN if idle > 0 else Lcars.TEXT3))
	if not app.view["meta"].get("populationEnabled", true):
		_forecast.add_child(Pic.num("lock", "", "Population is fixed in this ruleset.", null, 18, 13, Lcars.TEXT3))
	if vm.draft_dirty():
		var ap := Lcars.pill("Apply", "primary", func(): apply_workers(), _locked())
		ap.name = "ApplyWorkers"
		ap.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		_forecast.add_child(ap)

func apply_workers() -> void:
	if not vm.apply_workers():
		_update_forecast()

const GOV_ICONS := {"Manual": "settings", "Balanced": "colony", "Growth": "growth", "Research": "research", "Manufacturing": "industry"}

## The colonial administrator: a row of mode icons for the workforce and one for the queue; the chosen mode's name beside them and
## the governor's reasoning in the tooltip.
func _governor(c: Dictionary) -> Control:
	var g: Dictionary = c["governor"]
	var f := Lcars.frame(TrekText.term(app.player(), "governor", "Governor"), "", "secondary", "settings")
	var b := Lcars.body(f)
	for which in ["workforce", "queue"]:
		var row := Lcars.hbox(6)
		var why := str(g.get("workforceExplanation" if which == "workforce" else "queueExplanation", ""))
		var lead := UiIcon.make("population" if which == "workforce" else "build", 26, Lcars.accent2())
		Pic.tipped(lead, ("Automatically assigns citizens each turn." if which == "workforce" else "Queues construction whenever the build queue runs empty.") + ("\n" + why if why != "" and g[which] != "Manual" else ""))
		lead.custom_minimum_size.x = 38
		row.add_child(lead)
		for o in app.meta.get("governors", []):
			var key: String = o["key"]
			var w: String = which
			var gart := Pic.governor_art(key)
			var t: Button = Pic.picture_toggle(gart, g[which] == key, func(_on): vm.set_governor(w, key), str(o["name"])) if gart != null else Pic.icon_toggle(GOV_ICONS.get(key, "settings"), g[which] == key, func(_on): vm.set_governor(w, key), str(o["name"]), 22.0, Vector2(52, 52))
			t.disabled = _locked() != ""
			row.add_child(t)
		row.add_child(Lcars.label(str(g[which]), "Muted", 14))
		b.add_child(row)
	if g.get("override", false) and g["workforce"] != "Manual":
		b.add_child(Lcars.pill("Resume governor", "primary", func(): vm.resume_governor(), "", "Manual staffing is overriding the governor."))
	if str(g.get("warning", "")) != "":
		b.add_child(Pic.tipped(Lcars.marked("warn", "Governor warning", "", 13, Lcars.WARN, 16), str(g["warning"])))
	return f

## Power as a meter and the plant pictures with a count, not a sentence.
func _energy(c: Dictionary) -> Control:
	var e: Dictionary = c["economy"]
	var s: Dictionary = c["structures"]
	var f := Lcars.frame("Energy", "", "tertiary", "energy")
	var b := Lcars.body(f)
	var r := Lcars.hbox(8)
	r.add_child(UiIcon.make("energy", 22, Pic.tint("energy")))
	r.add_child(Lcars.meter(e["energyUsed"], max(1, int(e["energyAvailable"])), Lcars.ALERT if int(e["energyUsed"]) > int(e["energyAvailable"]) else Lcars.accent2(), 8))
	r.add_child(Lcars.label("%d/%d" % [int(e["energyUsed"]), int(e["energyAvailable"])], "Mono", 14))
	Pic.tipped(r, "Power in use / produced")
	b.add_child(r)
	var sy := Lcars.hbox(14)
	for st in [["Power plant", "powerPlants", "Power plants"], ["Fusion plant", "fusionPlants", "Fusion plants"], ["Shipyard", "shipyards", "Shipyard"], ["Defense battery", "defenseBatteries", "Defense batteries"]]:
		var n := int(s.get(st[1], 0))
		if n <= 0:
			continue
		var h := Lcars.hbox(4)
		h.add_child(Pic.structure(st[0], 34))
		h.add_child(Lcars.label("x%d" % n, "Mono", 15))
		var tip: String = "%d %s" % [n, st[2]]
		if st[1] == "shipyards":
			var up: bool = e.get("shipyardPowered", false)
			h.add_child(UiIcon.make("tick" if up else "cross", 16, Lcars.GOOD if up else Lcars.ALERT))
			tip = "Shipyard powered - builds ships and extends range" if up else "Shipyard offline"
		Pic.tipped(h, tip)
		sy.add_child(h)
	var dead := int(e["inactiveFactories"]) + int(e["inactiveLabs"])
	if dead > 0:
		sy.add_child(Pic.num("warn", "%d" % dead, "%d factories and %d labs stand idle without power." % [int(e["inactiveFactories"]), int(e["inactiveLabs"])], Lcars.WARN, 20, 16, Lcars.WARN))
	else:
		sy.add_child(Pic.tipped(UiIcon.make("tick", 18, Lcars.GOOD), "All structures powered"))
	sy.add_child(Lcars.spacer())
	if int(s["shipyards"]) > 0:
		sy.add_child(Lcars.pill("Switch %s" % ("off" if c.get("shipyardEnabled", false) else "on"), "ghost", func(): vm.toggle_shipyard(), vm.shipyard_block()))
	b.add_child(sy)
	return f

## Dilithium refinery and the empire's ship construction slots (schema 15). Null when the rules are off.
func _dilithium(c: Dictionary) -> Control:
	var lines := SystemVM.dilithium_lines(vm.dilithium(), str(detail.get("queueBlocker", "")))
	if lines.is_empty():
		return null
	var f := Lcars.frame("Dilithium", "", "secondary", "energy")
	f.name = "DilithiumPanel"
	var b := Lcars.body(f)
	var d := vm.dilithium()
	var r := Lcars.hbox(8)
	r.add_child(Pic.tipped(Pic.structure("Dilithium refinery", 34), "Ship slots in use"))
	r.add_child(Lcars.meter(int(d["slotsUsed"]), max(1, int(d["slots"])), Lcars.WARN if int(d["slotsUsed"]) >= int(d["slots"]) else Lcars.accent2(), 8))
	r.add_child(Lcars.label("%d/%d" % [int(d["slotsUsed"]), int(d["slots"])], "Mono", 13))
	b.add_child(r)
	var tips := []
	for ln in lines:
		tips.append(str(ln["text"]))
	r.tooltip_text = "\n".join(tips)
	r.mouse_filter = Control.MOUSE_FILTER_PASS
	var mk := Lcars.hbox(10)
	var ok: bool = bool(d["refinery"]) and bool(d["refineryPowered"])
	mk.add_child(Pic.tipped(Lcars.icon("tick" if ok else ("info" if bool(d["deposit"]) else "cross"), 22, Lcars.GOOD if ok else (Lcars.INFO if bool(d["deposit"]) else Lcars.TEXT3)), str(lines[0]["text"])))
	mk.add_child(Pic.num("energy", "%d" % int(d["empirePowered"]), "Powered refineries of %d in the empire" % int(d["empireRefineries"]), null, 20, 15))
	if int(d.get("klingonBonusPercent", 0)) > 0:
		mk.add_child(Pic.num("speed", "+%d%%" % int(d["klingonBonusPercent"]), "Klingon yard bonus: ships here build faster.", Lcars.GOOD, 20, 15))
	if str(detail.get("queueBlocker", "")).find("slots") >= 0 or str(detail.get("queueBlocker", "")).find("refinery") >= 0:
		mk.add_child(Pic.tipped(Lcars.icon("warn", 20, Lcars.WARN), str(detail["queueBlocker"])))
	b.add_child(mk)
	return f

## Schema 23: structure upgrade tiers, the Protectorate and shared morale at this colony. Null when none of those rules is on.
func _economy_rules() -> Control:
	var er: Variant = vm.colony().get("economyRules", {})
	if not er is Dictionary or er.is_empty():
		return null
	var ups: Array = er.get("upgrades", [])
	var rules: Dictionary = vm.view().get("meta", {}).get("rules", {})
	var show_up: bool = rules.get("structureUpgrades", false) or rules.get("autoUpgrades", false) or rules.get("noUpgrades", false)
	var show_pro: bool = bool(er.get("subjugated", false)) or int(er.get("sharedMorale", 0)) > 0
	if not show_up and not show_pro:
		return null
	var f := Lcars.frame("Upgrades", "", "secondary", "build")
	f.name = "EconomyPanel"
	var b := Lcars.body(f)
	if show_up:
		for u in ups:
			if int(u["units"]) <= 0:
				continue
			var row := Lcars.hbox(8)
			var uk: String = {"farms": "Farm", "factories": "Factory", "laboratories": "Laboratory"}.get(str(u["kind"]), str(u["kind"]))
			row.add_child(Pic.structure(uk, 30))
			row.add_child(Lcars.meter(int(u["steps"]), max(1, int(u["units"]) * 3), Lcars.accent2(), 8))
			row.add_child(Lcars.label("%d/%d" % [int(u["tier"]), int(u["available"])], "Mono", 14))
			row.add_child(Pic.num("growth", "+%d%%" % int(u["percent"]), "Tier %d of %d: +%d%% output. A tier needs the matching science at level 2, 4 and 6." % [int(u["tier"]), int(u["available"]), int(u["percent"])], Lcars.GOOD, 16, 13))
			b.add_child(row)
		if rules.get("noUpgrades", false):
			b.add_child(Pic.num("lock", "", "This game plays without structure upgrades.", Lcars.TEXT3, 20))
	if bool(er.get("subjugated", false)):
		var hq: bool = bool(er.get("hasHq", false))
		var sj := Lcars.hbox(12)
		sj.add_child(Lcars.icon("war", 22, Lcars.GOOD if hq else Lcars.WARN))
		sj.add_child(Pic.num("morale", "%+d" % int(er["moraleBonus"]), "Morale from the Protectorate", Lcars.GOOD if hq else Lcars.WARN, 20, 15))
		if hq:
			sj.add_child(Pic.num("credits", "%d" % int(er["tribute"]), "Tribute per turn", Lcars.GOOD, 20, 15))
		sj.tooltip_text = ("Subjugated world. A Protectorate HQ holds order here (+%d morale) and %d credits a turn come in as tribute." % [int(er["moraleBonus"]), int(er["tribute"])]) if hq else ("Subjugated world. Without a Protectorate Headquarters in a subjugated world it is restless (%d morale)." % int(er["moraleBonus"]))
		sj.mouse_filter = Control.MOUSE_FILTER_PASS
		b.add_child(sj)
	if int(er.get("sharedMorale", 0)) > 0:
		b.add_child(Pic.num("morale", "+%d" % int(er["sharedMorale"]), "Shared morale from the empire's Unity Monument.", Lcars.GOOD, 22, 16))
	return f

## The race's emergency program for this colony (schema 15). Null when the rules are off.
func _program(c: Dictionary) -> Control:
	var p := vm.program()
	if p.is_empty():
		return null
	var f := Lcars.frame("Emergency program", "", "tertiary", "morale")
	f.name = "ProgramPanel"
	var b := Lcars.body(f)
	var active := int(p["activeId"]) != 0
	var row := Lcars.hbox(10)
	row.tooltip_text = "%s\n%s" % [SystemVM.program_status(p), str(p["effect"])]
	row.mouse_filter = Control.MOUSE_FILTER_PASS
	row.add_child(Pic.num("morale", "%d" % int(c["morale"]), "Morale", null, 22, 17))
	row.add_child(Pic.num("alert" if active else "info", str(p["name"]), SystemVM.program_status(p), Lcars.WARN if active else Lcars.TEXT2, 20, 14, Lcars.WARN if active else Lcars.TEXT2))
	row.add_child(Lcars.spacer())
	if active:
		row.add_child(Lcars.pill("Lift %s" % p["activeName"], "ghost", func(): vm.cancel_program(), _locked(), "Ends the program at turn end"))
	else:
		row.add_child(Lcars.pill("Declare", "primary", func(): vm.start_program(), vm.program_block(), "Declare %s. %s" % [p["name"], str(p["effect"])]))
	b.add_child(row)
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
	var list: Control = Lcars.vbox(6)
	if tab == "installed":
		# MOD-3: race structures the pack adds (the Dominion ladder); they have no scrap order yet, so they show as a read-only row.
		for rs in detail.get("raceStructures", []):
			var rr := Lcars.hbox(10)
			rr.name = "RaceStructure_%d" % int(rs["id"])
			rr.add_child(Pic.structure(str(rs["name"]), 36))
			var rn := Lcars.label(str(rs["name"]), "", 15)
			rn.size_flags_horizontal = Control.SIZE_EXPAND_FILL
			rr.add_child(rn)
			rr.add_child(Pic.num("credits", "%d" % int(rs["upkeep"]), "Upkeep per turn"))
			Pic.tipped(rr, "%s\n%s" % [str(rs.get("description", "")), str(rs.get("effect", ""))])
			list.add_child(rr)
		for s in detail.get("scrap", []):
			var r := Lcars.hbox(10)
			r.add_child(Pic.structure(str(s["name"]), 36))
			var n := Lcars.label(str(s["name"]), "", 15)
			n.size_flags_horizontal = Control.SIZE_EXPAND_FILL
			r.add_child(n)
			r.add_child(Lcars.label("x%d" % int(s["count"]), "Mono", 16))
			r.add_child(Pic.num("energy", "%d" % int(s["energyCost"]), "Energy it uses"))
			r.add_child(Pic.num("credits", "%d" % int(s["upkeep"]), "Upkeep per turn"))
			var bld: String = s["building"]
			var nm: String = s["name"]
			r.add_child(Lcars.pill("Scrap", "alert", func():
				app.confirm("Scrap a %s?" % nm.to_lower(), "The structure is demolished at turn end. Cancel the order to undo it before then.", "Scrap",
					func(): vm.scrap(bld, nm)), vm.scrap_block(s)))
			list.add_child(r)
	else:
		var src := vm.choices(tab == "ships")
		if src.is_empty():
			list.add_child(Pic.num("lock", "", "No starships available - build and power a shipyard." if tab == "ships" else "Nothing can be built here right now.", Lcars.TEXT3, 28))
		else:
			var grid := Lcars.grid(2 if tab == "ships" else 3)
			grid.add_theme_constant_override("h_separation", 8)
			grid.add_theme_constant_override("v_separation", 8)
			for ch in src:
				var tile := _choice(ch)
				tile.size_flags_horizontal = Control.SIZE_EXPAND_FILL
				grid.add_child(tile)
			list = grid
	b.add_child(Lcars.scroll(list))
	return f

## The class name ("Oberth") of a ship choice: the encyclopedia entry of its reference ship, else the front of its name.
func _ship_class(ch: Dictionary) -> String:
	for sh in app.get_encyclopedia().get("ships", []):
		if int(sh.get("id", -1)) == int(ch.get("referenceId", -2)) and str(sh.get("designClass", "")) != "":
			return str(sh["designClass"])
	return ShipGlyph.class_of({"name": str(ch.get("name", ""))})

## One choice as a picture tile: the structure (or the ship) large, its name, and its numbers beside icons. What it does, and
## why it cannot be built yet, are in the tooltip; a locked tile shows the tech field and level it waits for.
func _choice(ch: Dictionary) -> Control:
	var reason := vm.choice_block(ch)
	var is_ship: bool = ch["isShip"]
	var btn := Button.new()
	btn.name = "Choice_%s_%d" % [ch["product"], int(ch["referenceId"])]
	btn.theme_type_variation = "GhostButton"
	btn.custom_minimum_size = Vector2(150, 222 if is_ship else ((178 if ChoiceIcon.art(str(ch["name"])) != null else 150) + (22 if reason != "" else 0)))
	btn.disabled = reason != ""
	btn.add_theme_stylebox_override("normal", Pic.tile_box())
	btn.add_theme_stylebox_override("hover", Pic.tile_box(true))
	btn.add_theme_stylebox_override("pressed", Pic.tile_box(true))
	btn.add_theme_stylebox_override("disabled", Lcars.box_tab(Color(Lcars.PANEL, 0.6), 14, 4, Lcars.LINE, 1, Vector4(8, 8, 8, 8)))
	btn.tooltip_text = ("Unavailable: " + reason + "\n" if reason != "" else "") + SystemVM.choice_tip(ch)
	var col := Lcars.vbox(4)
	col.set_anchors_preset(Control.PRESET_FULL_RECT)
	col.offset_left = 6
	col.offset_right = -6
	col.offset_top = 6
	col.offset_bottom = -6
	col.alignment = BoxContainer.ALIGNMENT_CENTER
	btn.add_child(col)
	var nm := str(ch["name"])
	var title := nm
	var sub := ""
	if is_ship:
		var cls := _ship_class(ch)
		var pic := ShipPicture.make(app.player(), cls, 118, "iso", Lcars.accent())
		pic.plate = true
		pic.custom_minimum_size = Vector2(0, 112)
		pic.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		col.add_child(pic)
		var sp := nm.find(" — ")
		if sp > 0:
			title = nm.substr(0, sp)
			sub = nm.substr(sp + 3)
	else:
		var ic := Pic.structure(nm, 62)
		if ChoiceIcon.art(nm) != null:
			ic.custom_minimum_size = Vector2(0, 100) # the illustration fills the top of the tile
			ic.size_flags_horizontal = Control.SIZE_EXPAND_FILL
			ic.clip_contents = true
		else:
			ic.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
		col.add_child(ic)
	var tl := Lcars.label(title, "", 15, Lcars.TEXT if reason == "" else Lcars.TEXT3)
	tl.add_theme_font_override("font", Lcars.font("ui", 560))
	tl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	tl.clip_text = true
	tl.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	col.add_child(tl)
	if sub != "":
		var sl := Lcars.label(sub, "Muted", 12)
		sl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		sl.clip_text = true
		sl.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
		col.add_child(sl)
	if is_ship and ch.has("hull"):
		var st := Lcars.hbox(10)
		st.alignment = BoxContainer.ALIGNMENT_CENTER
		st.add_child(Pic.num("attack", "%d" % int(ch["attack"]), "Attack", null, 16, 13))
		st.add_child(Pic.num("shield", "%d" % int(ch["shields"]), "Shields", null, 16, 13))
		st.add_child(Pic.num("hull", "%d" % int(ch["hull"]), "Hull", null, 16, 13))
		col.add_child(st)
	var nums := Lcars.hbox(10)
	nums.alignment = BoxContainer.ALIGNMENT_CENTER
	nums.add_child(Pic.num("industry", Lcars.fmt(ch["cost"]), "Industry cost", null, 16, 13))
	if int(ch["upkeep"]) > 0:
		nums.add_child(Pic.num("credits", "%d" % int(ch["upkeep"]), "Upkeep per turn", null, 16, 13))
	nums.add_child(Pic.num("clock", ("%d" % int(ch["turns"])) if int(ch["turns"]) > 0 else "-", "Turns to build at the colony's industry", Lcars.accent2(), 16, 13))
	col.add_child(nums)
	if reason != "":
		var rq := Pic.requirement(reason)
		var lk := Lcars.hbox(4)
		lk.alignment = BoxContainer.ALIGNMENT_CENTER
		lk.add_child(UiIcon.make("lock", 15, Lcars.WARN))
		if rq[0] != "lock":
			lk.add_child(UiIcon.make(rq[0], 15, Lcars.WARN))
			lk.add_child(Lcars.label(rq[1], "Mono", 13, Lcars.WARN))
		col.add_child(lk)
	Lcars.ignore_mouse(col)
	btn.pressed.connect(func(): vm.enqueue(ch))
	return btn

## The build queue: a picture of each item, a bar and the turns, with the order buttons as icons.
func _queue(c: Dictionary) -> Control:
	var q := vm.queue()
	var f := Lcars.frame("Build queue", "", "secondary", "industry")
	f.name = "BuildQueue"
	var b := Lcars.body(f)
	if str(detail.get("queueBlocker", "")) != "":
		b.add_child(Pic.tipped(Lcars.marked("warn", "Blocked", "", 13, Lcars.WARN, 16), str(detail["queueBlocker"])))
	if q.is_empty():
		var es := Lcars.hbox(8)
		for k in 3:
			es.add_child(Pic.empty_slot(48, "The queue is empty. Choose a structure or starship to build."))
		b.add_child(es)
	var lock := _locked()
	for i in q.size():
		var it: Dictionary = q[i]
		var card := Lcars.card()
		var col := Lcars.vbox(4)
		card.add_child(col)
		var h := Lcars.hbox(10)
		h.add_child(Lcars.label(str(i + 1), "Mono", 16, Lcars.accent() if i == 0 else Lcars.TEXT3))
		var pl := str(it.get("product", "")).to_lower()
		if bool(it.get("isShip", false)) or pl.begins_with("ship") or pl.begins_with("starship"):
			var sp := ShipPicture.make(app.player(), _ship_class({"name": str(it["name"]), "referenceId": int(it.get("referenceId", -1))}), 56, "iso", Lcars.accent())
			sp.plate = true
			h.add_child(sp)
		else:
			h.add_child(Pic.structure(str(it["name"]), 48))
		var mid := Lcars.vbox(3)
		mid.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		mid.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		var n := Lcars.label(str(it["name"]), "", 15, Lcars.TEXT)
		n.clip_text = true
		n.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
		mid.add_child(n)
		mid.add_child(Lcars.meter(it["work"], it["cost"], Lcars.accent() if i == 0 else Lcars.accent2(), 6))
		mid.add_child(Pic.num("industry", "%s/%s" % [Lcars.fmt(it["work"]), Lcars.fmt(it["cost"])], "Industry invested / needed", Lcars.TEXT3, 14, 12))
		h.add_child(mid)
		var eta := Pic.num("clock", ("%d" % int(it["eta"])) if int(it["eta"]) > 0 else "-", "Estimated completion in %d turns at %d industry/turn." % [int(it["eta"]), int(c["economy"]["industry"])] if int(it["eta"]) > 0 else "No industry: this item makes no progress.", Lcars.accent2(), 20, 17)
		h.add_child(eta)
		col.add_child(h)
		var foot := Lcars.hbox(2)
		foot.alignment = BoxContainer.ALIGNMENT_END
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
		buy.add_child(Pic.num("tick", "Ordered", "Purchase ordered - completes at turn end.", Lcars.GOOD, 20, 14, Lcars.GOOD))
		buy.add_child(Lcars.spacer())
		var pid := int(pending_buy["id"])
		buy.add_child(Lcars.pill("Cancel", "ghost", func(): vm.cancel([pid], "Purchase cancelled.")))
	else:
		var bl := Pic.num("credits", Lcars.fmt(detail.get("rushCost", 0)), "Buying finishes the current item at turn end. Treasury %s." % Lcars.fmt(app.view["empire"].get("draftCredits", 0)), null, 22, 17)
		bl.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		buy.add_child(bl)
		var bb := Lcars.pill("Buy", "secondary", func(): vm.buy(), vm.buy_block())
		bb.name = "Buy"
		buy.add_child(bb)
	b.add_child(buy)
	var sm := Lcars.hbox(14)
	sm.add_child(Pic.num("industry", "%d" % int(c["economy"]["industry"]), "Industry per turn goes into the first item.", null, 20, 16))
	if int(detail.get("overflow", 0)) > 0:
		sm.add_child(Pic.num("plus", "%d" % int(detail["overflow"]), "Industry carried over from the last item.", Lcars.GOOD, 18, 15, Lcars.GOOD))
	b.add_child(sm)
	return f

## Templates: a strip of the queued pictures per template. The sentence that explains them is the name box's tooltip.
func _templates() -> Control:
	var temps: Array = app.view.get("templates", [])
	var f := Lcars.frame("Build templates", "", "tertiary", "chronicle")
	f.name = "Templates"
	var b := Lcars.body(f)
	var items := vm.queue_items()
	var lock := _locked()
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
		var strip := Lcars.hbox(4)
		for ti in t["items"]:
			strip.add_child(Pic.tipped(Pic.structure(str(ti["name"]), 30), str(ti["name"])))
		col.add_child(strip)
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
	le.tooltip_text = "Save this colony's queue as a template, then apply it anywhere. The default template is applied to every new colony with an empty queue."
	nr.add_child(le)
	var save := Lcars.pill("Save queue", "secondary", func():
		var n := le.text.strip_edges()
		if n == "":
			app.toast("Name the template", "warn")
			return
		vm.define_template(0, n, "Template “%s” saved." % n), vm.save_block(), "Save the %d queued items as a template" % items.size())
	nr.add_child(save)
	b.add_child(nr)
	return f

## The scientific-victory project: its cost in icons; the rules sentence is the tooltip.
func _megaproject() -> Control:
	var m := vm.megaproject()
	if m.is_empty():
		return null
	var f := Lcars.frame(str(m["name"]), "", "good", "victory")
	f.name = "Megaproject"
	var b := Lcars.body(f)
	var lock := _locked()
	var elsewhere: bool = m.has("systemId") and not m.get("here", false)
	var nm: String = m["name"]
	var rules := "Completing the %s wins a scientific victory. It costs ¢%s up front, then absorbs up to %d industry per turn from this colony (%s in all)." % [m["name"], Lcars.fmt(m["credits"]), int(m["workPerTurn"]), Lcars.fmt(m["cost"])]
	if m.get("here", false) or elsewhere:
		var hd := Lcars.hbox(10)
		hd.add_child(Pic.num("industry", "%s/%s" % [Lcars.fmt(m.get("work", 0)), Lcars.fmt(m["cost"])], "Industry invested / needed. Up to %d per turn." % int(m["workPerTurn"]), null, 22, 16))
		if elsewhere:
			hd.add_child(Pic.num("system", str(m.get("system", "")), "Under construction at %s" % m.get("system", ""), null, 20, 14))
		b.add_child(hd)
		b.add_child(Lcars.meter(m.get("work", 0), m["cost"], Lcars.GOOD, 7))
		b.add_child(Lcars.pill("Abandon", "alert", func(): app.confirm("Abandon the %s?" % nm, "All work is lost and the credits are not refunded.", "Abandon", func(): vm.cancel_megaproject(nm)), lock if lock != "" else str(m.get("cancelBlocker", ""))))
	else:
		var cost := Lcars.hbox(16)
		cost.add_child(Pic.num("credits", Lcars.fmt(m["credits"]), "Credits up front", null, 22, 17))
		cost.add_child(Pic.num("industry", "%d/t" % int(m["workPerTurn"]), "Industry taken per turn from this colony", null, 22, 17))
		cost.add_child(Pic.num("victory", Lcars.fmt(m["cost"]), "Industry in all", null, 22, 17))
		Pic.tipped(cost, rules)
		b.add_child(cost)
		var acts := Lcars.hbox(6)
		acts.add_child(Lcars.pill("Begin here", "good", func(): vm.start_megaproject(nm), lock if lock != "" else str(m.get("startBlocker", ""))))
		acts.add_child(Lcars.pill("Victory progress", "ghost", func(): app.open_overlay("victory")))
		if str(m.get("startBlocker", "")) != "":
			acts.add_child(Pic.num("lock", "", str(m["startBlocker"]), null, 18, 13, Lcars.WARN))
		b.add_child(acts)
	return f
