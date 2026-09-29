extends Control
## New Campaign: faction cards, galaxy size, powers, seed, era, reference/prototype, Classic/Remastered.
## Port of screens/NewCampaign.tsx.

var app: App

const SIZES := [
	{"id": "small", "label": "Small", "width": 12, "height": 9, "systems": 16, "blurb": "Quick, crowded: contact comes early."},
	{"id": "medium", "label": "Medium", "width": 16, "height": 12, "systems": 28, "blurb": "The classic reference galaxy."},
	{"id": "large", "label": "Large", "width": 22, "height": 16, "systems": 48, "blurb": "Room to expand before borders meet."},
	{"id": "huge", "label": "Huge", "width": 30, "height": 20, "systems": 80, "blurb": "A long campaign across a wide quadrant."},
]
const ERA_BLURBS := {
	1: "Warp-capable newcomers. Build everything from the ground up.",
	2: "Established powers with developed technology.",
	3: "Expanded empires, advanced shipyards.",
	4: "Advanced civilisations on the brink of dominance.",
	5: "Supreme technology from turn one.",
}

var faction := 1
var empires := 5
var size_id := "medium"
var era := 1
var ruleset := "reference"
var preset := "remastered"
var seed_value := 0
var events := true
var aggression := true

func _ready() -> void:
	seed_value = randi_range(100000, 999999)

func options() -> Dictionary:
	var sz: Dictionary = SIZES.filter(func(s): return s["id"] == size_id)[0]
	return {"faction": faction, "empires": max(empires, faction), "seed": seed_value, "era": era, "ruleset": ruleset,
		"width": sz["width"], "height": sz["height"], "systems": sz["systems"], "events": events, "aiAggression": aggression, "preset": preset}

func start() -> void:
	app.start_campaign(options())

func _unhandled_key_input(event: InputEvent) -> void:
	var k := event as InputEventKey
	if k and k.pressed and not k.echo and (k.keycode == KEY_ENTER or k.keycode == KEY_KP_ENTER) and not app.has_modal():
		get_viewport().set_input_as_handled()
		start()

func refresh() -> void:
	for c in get_children():
		c.queue_free()
	Lcars.accent_empire = faction
	app.theme = Lcars.build_theme(faction)
	var margin := MarginContainer.new()
	margin.set_anchors_preset(Control.PRESET_FULL_RECT)
	for side in ["left", "right"]:
		margin.add_theme_constant_override("margin_" + side, 64)
	margin.add_theme_constant_override("margin_top", 40)
	margin.add_theme_constant_override("margin_bottom", 40)
	add_child(margin)
	var col := Lcars.vbox(22)
	margin.add_child(col)
	var head := Lcars.hbox(18)
	head.add_child(Lcars.pill("Back", "ghost", func(): app.goto("title", false), "", "", "Esc"))
	head.add_child(Lcars.heading("New campaign", 46))
	var sub := Lcars.caption("Choose a power and shape the quadrant")
	sub.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	head.add_child(sub)
	col.add_child(head)
	# Faction cards
	var cards := Lcars.hbox(16)
	col.add_child(cards)
	for f in Lcars.FACTIONS:
		cards.add_child(_faction_card(f))
	# Options
	var opts := Lcars.hbox(20)
	col.add_child(opts)
	var galaxy := Lcars.frame("Galaxy", "size, powers, seed", "secondary")
	galaxy.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	opts.add_child(galaxy)
	var gb := Lcars.body(galaxy)
	var sizes := Lcars.hbox(8)
	for s in SIZES:
		var sid: String = s["id"]
		sizes.add_child(Lcars.toggle(s["label"], size_id == sid, func(_on): size_id = sid; refresh(), "%dx%d sectors, %d star systems. %s" % [s["width"], s["height"], s["systems"], s["blurb"]]))
	gb.add_child(sizes)
	gb.add_child(Lcars.caption("Powers in play"))
	var pw := Lcars.hbox(8)
	for n in [2, 3, 4, 5]:
		var t := Lcars.toggle(str(n), max(empires, faction) == n, func(_on): empires = n; refresh(), "%d major powers compete." % n)
		t.custom_minimum_size.x = 56
		if n < faction:
			t.disabled = true
			t.tooltip_text = "Powers join in canonical order (Federation, Klingon, Romulan, Cardassian, Ferengi)."
		pw.add_child(t)
	gb.add_child(pw)
	gb.add_child(Lcars.caption("Seed"))
	var sr := Lcars.hbox(8)
	var seed_edit := SpinBox.new()
	seed_edit.max_value = 99999999
	seed_edit.value = seed_value
	seed_edit.custom_minimum_size.x = 180
	seed_edit.value_changed.connect(func(v): seed_value = int(v))
	seed_edit.name = "Seed"
	sr.add_child(seed_edit)
	sr.add_child(Lcars.pill("Randomise", "ghost", func(): seed_value = randi_range(100000, 999999); refresh(), "", "Roll a new galaxy"))
	gb.add_child(sr)
	var rules := Lcars.frame("Era & rules", "technology and gameplay", "tertiary")
	rules.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	opts.add_child(rules)
	var rb := Lcars.body(rules)
	var eras := Lcars.hbox(6)
	for e in app.meta.get("eras", []):
		var eid := int(e["id"])
		var lvl := int(e["techLevel"] if ruleset == "reference" else e["prototypeTechLevel"])
		var t2 := Lcars.toggle("%s  T%d" % [e["name"], lvl], era == eid, func(_on): era = eid; refresh(), "%s\nStarting technology level %d in every field." % [ERA_BLURBS.get(eid, ""), lvl])
		eras.add_child(t2)
	rb.add_child(eras)
	var rs := Lcars.hbox(8)
	rs.add_child(Lcars.toggle("Reference rules", ruleset == "reference", func(_on): ruleset = "reference"; refresh(), "Reference catalogue ships and technologies, planets & terraforming, task forces, modern diplomacy and intelligence."))
	rs.add_child(Lcars.toggle("Prototype rules", ruleset == "prototype", func(_on): ruleset = "prototype"; refresh(), "The original compact prototype ruleset with generic hulls."))
	rb.add_child(rs)
	rb.add_child(Lcars.caption("Gameplay"))
	var ps := Lcars.hbox(8)
	var rem := Lcars.toggle("Remastered", preset == "remastered", func(_on): preset = "remastered"; refresh(), "Outposts and starbases, nebulae, wormholes, black holes, neutron stars, ion storms, curated galactic events and four paths to victory.")
	rem.name = "PresetRemastered"
	ps.add_child(rem)
	var cls := Lcars.toggle("Classic", preset == "classic", func(_on): preset = "classic"; refresh(), "Original-like: stations, the original random events, conquest or domination victories only. No anomalies.")
	cls.name = "PresetClassic"
	ps.add_child(cls)
	rb.add_child(ps)
	rb.add_child(Lcars.para("Anomalies, galactic events with choices, stations and four paths to victory." if preset == "remastered" else "Close to the 1999 original: stations, simple random events, conquest or domination.", Lcars.TEXT3, 14))
	var tg := Lcars.hbox(18)
	var ev := CheckBox.new()
	ev.text = "Galactic events" if preset == "remastered" else "Random events"
	ev.button_pressed = events
	ev.toggled.connect(func(on): events = on)
	tg.add_child(ev)
	var ag := CheckBox.new()
	ag.text = "Aggressive rivals"
	ag.button_pressed = aggression
	ag.toggled.connect(func(on): aggression = on)
	tg.add_child(ag)
	rb.add_child(tg)
	# Summary + launch
	var summ := PanelContainer.new()
	summ.add_theme_stylebox_override("panel", Lcars.box(Color(Lcars.PANEL_SOLID, 0.95), 20, Lcars.accent(), 2, Vector4(20, 18, 20, 18)))
	summ.custom_minimum_size.x = 420
	opts.add_child(summ)
	var sb := Lcars.vbox(10)
	summ.add_child(sb)
	var sh := Lcars.hbox(12)
	sh.add_child(Lcars.emblem(faction, 48))
	var nm := Lcars.vbox(0)
	nm.add_child(Lcars.label(app.empire_name(faction).to_upper() if not app.meta.is_empty() else "", "Heading", 30))
	nm.add_child(Lcars.caption("from " + Lcars.FACTIONS[faction - 1]["capital"]))
	sh.add_child(nm)
	sb.add_child(sh)
	var o := options()
	for line in ["%d major powers · %d systems · %dx%d sectors" % [o["empires"], o["systems"], o["width"], o["height"]],
			"%s era · %s rules · %s gameplay" % [_era_name(), ruleset, preset.capitalize()],
			"Seed %d%s%s" % [seed_value, " · events" if events else "", " · aggressive AI" if aggression else ""]]:
		sb.add_child(Lcars.para("•  " + line, Lcars.TEXT2, 15))
	var sp := Control.new()
	sp.size_flags_vertical = Control.SIZE_EXPAND_FILL
	sb.add_child(sp)
	var launch := Lcars.pill("Launch campaign", "primary", func(): start(), "", "", "Enter")
	launch.name = "Launch"
	launch.custom_minimum_size.y = 64
	launch.add_theme_font_size_override("font_size", 28)
	sb.add_child(launch)

func _era_name() -> String:
	for e in app.meta.get("eras", []):
		if int(e["id"]) == era:
			return str(e["name"])
	return ""

func _pick_faction(id: int) -> void:
	faction = id
	empires = max(empires, id)
	refresh()

func _faction_card(f: Dictionary) -> Control:
	var id := int(f["id"])
	var pal := Lcars.palette(id)
	var col := Color(pal["primary"])
	var selected := id == faction
	var b := Button.new()
	b.name = "Faction%d" % id
	b.toggle_mode = true
	b.button_pressed = selected
	b.custom_minimum_size = Vector2(0, 250)
	b.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var bg := Lcars.box(Color(col, 0.16) if selected else Color(Lcars.PANEL2, 0.85), 18, col if selected else Lcars.LINE2, 3 if selected else 1, Vector4(16, 14, 16, 14))
	if selected:
		bg.shadow_color = Color(col, 0.35)
		bg.shadow_size = 18
	for st in ["normal", "hover", "pressed", "hover_pressed", "focus"]:
		var s := bg.duplicate() as StyleBoxFlat
		if st == "hover":
			s.border_color = col
		b.add_theme_stylebox_override(st, s)
	b.pressed.connect(_pick_faction.bind(id))
	var v := Lcars.vbox(6)
	v.set_anchors_preset(Control.PRESET_FULL_RECT)
	v.offset_left = 18
	v.offset_top = 14
	v.offset_right = -14
	v.offset_bottom = -12
	v.mouse_filter = Control.MOUSE_FILTER_IGNORE
	b.add_child(v)
	var band := ColorRect.new()
	band.color = col
	band.custom_minimum_size = Vector2(0, 6)
	v.add_child(band)
	var top := Lcars.hbox(10)
	top.add_child(Lcars.emblem(id, 46, col))
	var nm := Lcars.label(app.empire_name(id).to_upper() if not app.meta.is_empty() else f["short"], "", 24, col)
	nm.add_theme_font_override("font", Lcars.font("display", 600))
	nm.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	nm.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	top.add_child(nm)
	v.add_child(top)
	v.add_child(Lcars.caption("Capital · " + f["capital"]))
	v.add_child(Lcars.para("“%s”" % f["motto"], Lcars.TEXT, 15))
	for t in f["traits"]:
		v.add_child(Lcars.label("▸ " + t, "Muted", 14, Color(pal["secondary"])))
	for n in v.find_children("*", "Control", true, false):
		n.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return b
