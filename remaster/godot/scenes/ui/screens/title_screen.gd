extends Control
## Title screen: continue, new campaign, load, drills, settings. Port of screens/Title.tsx.

var app: App

func refresh() -> void:
	for c in get_children():
		c.queue_free()
	var planet := TitlePlanet.new()
	planet.set_anchors_preset(Control.PRESET_FULL_RECT)
	add_child(planet)
	var root := Lcars.vbox(14)
	root.position = Vector2(size.x * 0.09, size.y * 0.16)
	root.custom_minimum_size = Vector2(620, 0)
	add_child(root)
	var emblems := Lcars.hbox(14)
	for id in range(1, 6):
		emblems.add_child(Lcars.emblem(id, 30))
	root.add_child(emblems)
	root.add_child(Lcars.label("BIRTH OF THE FEDERATION", "Caption", 22, Lcars.accent2()))
	var big := Lcars.hbox(14)
	var logo := Lcars.label("BOTF", "", 132, Lcars.TEXT)
	logo.add_theme_font_override("font", Lcars.font("display", 700))
	big.add_child(logo)
	var rem := Lcars.label("REMASTERED", "", 44, Lcars.accent())
	rem.add_theme_font_override("font", Lcars.font("display", 400))
	rem.size_flags_vertical = Control.SIZE_SHRINK_END
	big.add_child(rem)
	root.add_child(big)
	var tag := Lcars.para("Five powers. One quadrant. A modern reconstruction of the 1999 grand strategy classic, running on its deterministic simulation core.", Lcars.TEXT2, 17)
	tag.custom_minimum_size.x = 560
	root.add_child(tag)
	root.add_child(Control.new())
	var menu := Lcars.vbox(10)
	menu.custom_minimum_size.x = 420
	root.add_child(menu)
	var saves := app.list_saves()
	if saves.size() > 0:
		var s: Dictionary = saves[0]
		var b := Lcars.pill("Continue  ·  %s turn %d" % [s.get("playerName", ""), int(s.get("turn", 0))], "primary", func(): app.load_game(s), "", str(s.get("name", "")))
		b.name = "Continue"
		menu.add_child(b)
	var nb := Lcars.pill("New campaign", "secondary" if saves.size() > 0 else "primary", func(): app.goto("new"))
	nb.name = "NewCampaign"
	menu.add_child(nb)
	menu.add_child(Lcars.pill("Load campaign", "secondary", func(): app.goto("saves"), "", "Saved campaigns"))
	var row := Lcars.hbox(10)
	row.add_child(_wide(Lcars.pill("Tactical drill", "tertiary", func(): app.start_drill("newBattleDrill", "Tactical drill"), "", "A ready-made skirmish at Sol to practise tactical command")))
	row.add_child(_wide(Lcars.pill("Diplomacy drill", "tertiary", func(): app.start_drill("newDiplomacyDrill", "Diplomacy drill"), "", "Incoming proposals, a tribute demand and an ally's call to arms")))
	menu.add_child(row)
	var row2 := Lcars.hbox(10)
	row2.add_child(_wide(Lcars.pill("Frontier drill", "tertiary", func(): app.start_drill("newFrontierDrill", "Frontier drill"), "", "Outposts, starbases, anomalies and galactic events")))
	row2.add_child(_wide(Lcars.pill("Settings", "ghost", func(): app.open_overlay("settings"))))
	menu.add_child(row2)
	for b2 in menu.get_children():
		if b2 is Button:
			b2.custom_minimum_size.y = 46
			b2.add_theme_font_size_override("font_size", 21)
	var foot := Lcars.hbox(40)
	foot.position = Vector2(size.x * 0.09, size.y - 48)
	foot.add_child(Lcars.label("Simulation core %s · save schema %s · backend: %s" % [app.meta.get("contentVersion", "…"), str(app.meta.get("saveVersion", "…")), app.core.backend], "Mono", 13, Lcars.TEXT3))
	foot.add_child(Lcars.label("Unofficial fan reconstruction · not affiliated with any rights holder", "Muted"))
	add_child(foot)
	var first := menu.get_child(0) as Button
	if first and is_inside_tree():
		first.grab_focus.call_deferred()

func _wide(b: Button) -> Button:
	b.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	return b

class TitlePlanet extends Control:
	func _ready() -> void:
		mouse_filter = Control.MOUSE_FILTER_IGNORE
	func _process(_d: float) -> void:
		queue_redraw()
	func _draw() -> void:
		var c := Vector2(size.x * 0.78, size.y * 0.62)
		var r := size.y * 0.42
		var a := Lcars.accent()
		# Atmosphere glow
		for k in range(14, 0, -1):
			draw_circle(c, r + k * 6, Color(a, 0.012 * (15 - k)))
		# Planet body with terminator shading
		draw_circle(c, r, Color("#0b1326"))
		var t := Time.get_ticks_msec() / 1000.0
		for k in range(24):
			var f := float(k) / 24.0
			draw_circle(c + Vector2(r * 0.35 * f, -r * 0.2 * f), r * (1.0 - f * 0.9), Color(a.lerp(Color("#1a2a55"), 0.6), 0.035))
		# bands
		for k in range(6):
			var y := c.y - r * 0.8 + k * r * 0.3 + sin(t * 0.1 + k) * 4
			var hw := sqrt(max(0.0, r * r - pow(y - c.y, 2)))
			draw_line(Vector2(c.x - hw, y), Vector2(c.x + hw, y), Color(a, 0.05), 10)
		draw_arc(c, r, -PI * 0.95, -PI * 0.2, 96, Color(a.lightened(0.3), 0.8), 2.5, true)
		# ring
		var ring := PackedVector2Array()
		for k in range(97):
			var ang := TAU * k / 96.0
			ring.append(c + Vector2(cos(ang) * r * 1.7, sin(ang) * r * 0.22).rotated(-0.25))
		draw_polyline(ring, Color(Lcars.accent2(), 0.25), 3, true)
