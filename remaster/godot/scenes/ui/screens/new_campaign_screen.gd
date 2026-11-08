extends Control
## New Campaign presentation: faction cards, galaxy, era and rules panels, summary and launch.
## Choices live in NewCampaignVM (scripts/viewmodels/new_campaign_vm.gd); the fields below forward to it.

var app: App
var vm: NewCampaignVM:
	get:
		if vm == null:
			vm = NewCampaignVM.new(app.session if app else null)
			vm.roll_seed()
		return vm
var faction: int:
	get: return vm.faction
	set(v): vm.faction = v
var empires: int:
	get: return vm.empires
	set(v): vm.empires = v
var size_id: String:
	get: return vm.size_id
	set(v): vm.size_id = v
var era: int:
	get: return vm.era
	set(v): vm.era = v
var ruleset: String:
	get: return vm.ruleset
	set(v): vm.ruleset = v
var preset: String:
	get: return vm.preset
	set(v): vm.preset = v
var seed_value: int:
	get: return vm.seed_value
	set(v): vm.seed_value = v
var events: bool:
	get: return vm.events
	set(v): vm.events = v
var aggression: bool:
	get: return vm.aggression
	set(v): vm.aggression = v

const SIZES := NewCampaignVM.SIZES

func options() -> Dictionary:
	return vm.options()

func start() -> void:
	app.start_campaign(vm.options())

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
	var margin := Lcars.screen_margin(self, Vector4(64, 64, 40, 40))
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
		var t := Lcars.toggle(str(n), vm.powers() == n, func(_on): empires = n; refresh(), "%d major powers compete." % n)
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
	sr.add_child(Lcars.pill("Randomise", "ghost", func(): vm.roll_seed(); refresh(), "", "Roll a new galaxy"))
	gb.add_child(sr)
	var rules := Lcars.frame("Era & rules", "technology and gameplay", "tertiary")
	rules.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	opts.add_child(rules)
	var rb := Lcars.body(rules)
	var eras := Lcars.hbox(6)
	for e in vm.eras():
		var eid := int(e["id"])
		var lvl := int(e["level"])
		var t2 := Lcars.toggle("%s  T%d" % [e["name"], lvl], era == eid, func(_on): era = eid; refresh(), "%s\nStarting technology level %d in every field." % [e["blurb"], lvl])
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
	nm.add_child(Lcars.label(vm.faction_name(faction, ""), "Heading", 30))
	nm.add_child(Lcars.caption("from " + Lcars.FACTIONS[faction - 1]["capital"]))
	sh.add_child(nm)
	sb.add_child(sh)
	for line in vm.summary():
		sb.add_child(Lcars.para("•  " + line, Lcars.TEXT2, 15))
	var sp := Control.new()
	sp.size_flags_vertical = Control.SIZE_EXPAND_FILL
	sb.add_child(sp)
	var launch := Lcars.pill("Launch campaign", "primary", func(): start(), "", "", "Enter")
	launch.name = "Launch"
	launch.custom_minimum_size.y = 64
	launch.add_theme_font_size_override("font_size", 28)
	sb.add_child(launch)

func _pick_faction(id: int) -> void:
	vm.pick_faction(id)
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
	var nm := Lcars.label(vm.faction_name(id, f["short"]), "", 24, col)
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
