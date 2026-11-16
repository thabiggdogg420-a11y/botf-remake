class_name GalaxyHud
extends MarginContainer
## The galaxy screen's heads-up display in the LCARS design system (docs/UI-DESIGN-SYSTEM.md): an alerts frame and a status
## strip top left, the layer toggles with the map legend bottom left, and the drafted-orders frame bottom right of the chart
## (the inspector owns the right-hand 440 px). Fed by GalaxyVM; it draws and forwards clicks, it reads no game state.
## Layout is containers only, so it follows the window size in both renderers.

## The chart should centre this sector (an alert was clicked).
signal focus_requested(x: int, y: int)
## A map layer was switched.
signal layer_changed(key: String, on: bool)

const INSPECTOR_WIDTH := 440
const PANEL_WIDTH := 420

var app: App
var _vm: GalaxyVM
var vm: GalaxyVM:
	get:
		if _vm == null:
			_vm = GalaxyVM.new(app.session)
		return _vm
var alerts_open := true
var orders_open := false

var _built_once := false

func _init() -> void:
	name = "GalaxyHud"
	set_anchors_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_theme_constant_override("margin_left", 16)
	add_theme_constant_override("margin_top", 14)
	add_theme_constant_override("margin_right", INSPECTOR_WIDTH + 16)
	add_theme_constant_override("margin_bottom", 16)

## Rebuilds every panel from the view model.
func refresh() -> void:
	for c in get_children():
		remove_child(c)
		c.queue_free()
	var col := VBoxContainer.new()
	col.mouse_filter = Control.MOUSE_FILTER_IGNORE
	col.add_theme_constant_override("separation", 10)
	add_child(col)
	var top := _row()
	col.add_child(top)
	var gap := Control.new()
	gap.mouse_filter = Control.MOUSE_FILTER_IGNORE
	gap.size_flags_vertical = Control.SIZE_EXPAND_FILL
	col.add_child(gap)
	var bottom := _row()
	col.add_child(bottom)

	var left := Lcars.vbox(8)
	left.mouse_filter = Control.MOUSE_FILTER_IGNORE
	left.custom_minimum_size.x = PANEL_WIDTH
	left.size_flags_vertical = Control.SIZE_SHRINK_BEGIN
	left.add_child(_alerts())
	var evs := _events()
	if evs != null:
		left.add_child(evs)
	top.add_child(left)
	top.add_child(_status())

	bottom.add_child(_layers())
	var sp := Control.new()
	sp.mouse_filter = Control.MOUSE_FILTER_IGNORE
	sp.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	bottom.add_child(sp)
	bottom.add_child(_orders())
	if not _built_once:
		_built_once = true
		Motion.stagger(left)

func _row() -> HBoxContainer:
	var r := HBoxContainer.new()
	r.mouse_filter = Control.MOUSE_FILTER_IGNORE
	r.add_theme_constant_override("separation", 14)
	return r

# ---------------------------------------------------------------------------------------------
# Panels
# ---------------------------------------------------------------------------------------------
func _alerts() -> Control:
	var a := vm.alerts()
	var tone: String = a["tone"]
	var f := Lcars.frame("Alerts", ("%d" % a["count"]) if int(a["count"]) > 0 else "", tone, "alert" if tone == "alert" else ("warn" if tone == "warn" else "good"))
	f.name = "Alerts"
	var body := Lcars.body(f)
	var head: HBoxContainer = f.get_meta("head")
	var toggle := Lcars.icon_button("▾" if alerts_open else "▸", "Show or hide the alert list", "", func():
		alerts_open = not alerts_open
		refresh())
	toggle.name = "AlertsToggle"
	head.add_child(toggle)
	var status := Lcars.label(str(a["title"]), "", 15, Lcars.tone_color(tone))
	status.name = "AlertsTitle"
	body.add_child(status)
	if alerts_open:
		for item in a["items"]:
			body.add_child(_alert_row(item))
		if int(a["more"]) > 0:
			body.add_child(Lcars.label("+ %d more" % int(a["more"]), "Muted", 13))
	return f

func _alert_row(item: Dictionary) -> Control:
	var b := Lcars.row_button(false, 34, 8)
	b.name = "AlertRow"
	b.tooltip_text = str(item["text"])
	b.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var row := Lcars.row_inner(b, 8, 8)
	var tc := Lcars.tone_color(item["tone"])
	row.add_child(Lcars.icon(item["icon"], 18, tc))
	var l := Lcars.label(str(item["text"]), "", 14, Lcars.TEXT2)
	l.clip_text = true
	l.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	l.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	l.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	row.add_child(l)
	Lcars.ignore_mouse(row)
	var target: Dictionary = item["target"]
	b.pressed.connect(func(): _activate(target))
	return b

func _activate(t: Dictionary) -> void:
	match str(t.get("do", "")):
		"focus":
			if t.has("system"):
				app.select_system(int(t["system"]))
			elif t.has("fleet"):
				app.select_fleet(int(t["fleet"]))
			if t.get("found", false):
				focus_requested.emit(int(t["x"]), int(t["y"]))
		"screen":
			app.goto(str(t["id"]))
		"overlay":
			app.open_overlay(str(t["id"]))

func _events() -> Control:
	var e := vm.events()
	if e.is_empty():
		return null
	var b := Lcars.pill(str(e["text"]), e["tone"], func(): app.open_overlay("events"))
	b.name = "EventsHud"
	return b

func _status() -> Control:
	var p := PanelContainer.new()
	p.name = "ChartStatus"
	p.add_theme_stylebox_override("panel", Lcars.box_lcars(Color(Lcars.PANEL_SOLID, 0.78), UiTokens.radius(), Lcars.LINE, 1, Vector4(10, 6, 10, 6)))
	p.size_flags_vertical = Control.SIZE_SHRINK_BEGIN
	var row := Lcars.hbox(8)
	p.add_child(row)
	for s in vm.status():
		row.add_child(Lcars.kpi(s["icon"], s["cap"], s["value"], "", s["tone"], s["tip"], true))
	return p

func _layers() -> Control:
	var p := PanelContainer.new()
	p.name = "LayerBar"
	p.add_theme_stylebox_override("panel", Lcars.box_lcars(Color(Lcars.PANEL_SOLID, 0.86), UiTokens.radius(), Lcars.LINE2, 1, Vector4(12, 8, 12, 8)))
	p.size_flags_vertical = Control.SIZE_SHRINK_END
	var col := Lcars.vbox(6)
	p.add_child(col)
	var row := Lcars.hbox(4)
	row.add_child(Lcars.icon("labels", 18, Lcars.TEXT3))
	for l in vm.layers(app.map_layers):
		var key: String = l["key"]
		var t := Pic.icon_toggle(str(l["icon"]), l["on"], func(on): layer_changed.emit(key, on), "%s: %s" % [l["label"], l["hint"]], 20, Vector2(44, 36))
		t.name = "Layer_" + key
		row.add_child(t)
	col.add_child(row)
	var leg := Lcars.hbox(12)
	leg.name = "Legend"
	for item in vm.legend():
		# Every legend entry is the same mark the chart draws: hulls, contacts and starbases, the anomaly swirl, the hostile delta.
		# The mark is the legend; hover names it.
		leg.add_child(Pic.tipped(UiIcon.make(str(item.get("icon", "contact")), 20, _legend_color(item["color"])), str(item["label"])))
	col.add_child(leg)
	return p

func _legend_color(token: String) -> Color:
	match token:
		"owner": return Lcars.owner_color(app.player())
		"alert": return Lcars.ALERT
		"secondary": return Lcars.accent2()
		"anomaly": return Color(0.45, 0.85, 1.0)
	return Color("#1a2238")

func _orders() -> Control:
	var o := vm.orders()
	var f := Lcars.frame("Orders this turn", "", "secondary", "orders")
	f.name = "OrdersCard"
	f.custom_minimum_size.x = 480 # the 22 px title, cap, icon, badge and toggle crowded the 380 px header (the rule shrank to a dash)
	f.size_flags_vertical = Control.SIZE_SHRINK_END
	var head: HBoxContainer = f.get_meta("head")
	head.add_theme_constant_override("separation", 14)
	for c in head.get_children():
		if c is ColorRect:
			c.custom_minimum_size.x = 48 # the header rule keeps a visible length between the title and the badge
	head.add_child(Lcars.badge(str(o["count"]), "secondary" if int(o["count"]) > 0 else "dim"))
	var toggle := Lcars.icon_button("▾" if orders_open else "▴", "Show or hide the drafted orders", "", func():
		orders_open = not orders_open
		refresh())
	toggle.name = "OrdersToggle"
	toggle.custom_minimum_size = Vector2(34, 34)
	head.add_child(toggle)
	var body := Lcars.body(f)
	if not orders_open:
		body.visible = false
		return f
	if int(o["count"]) == 0:
		body.add_child(Lcars.empty_note("No orders drafted yet. Select a ship and right-click a sector."))
	for p in o["items"]:
		var r := Lcars.hbox(6)
		var l := Lcars.label(str(p["summary"]), "", 13, Lcars.TEXT2)
		l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		l.custom_minimum_size.x = 380
		l.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		r.add_child(l)
		var pid := int(p["id"])
		var x := Lcars.icon_button("✕", "Cancel this order", str(o["lock"]), func(): vm.cancel_order(pid))
		x.name = "CancelOrder"
		r.add_child(x)
		body.add_child(r)
	return f
