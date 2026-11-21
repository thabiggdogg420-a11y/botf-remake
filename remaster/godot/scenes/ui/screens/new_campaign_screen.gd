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
	if app.settings.get("uiEra", "tng") != vm.ui_era:
		app.settings["uiEra"] = vm.ui_era
		app.save_settings()
	app.start_campaign(vm.options())

func _unhandled_key_input(event: InputEvent) -> void:
	var k := event as InputEventKey
	if k and k.pressed and not k.echo and (k.keycode == KEY_ENTER or k.keycode == KEY_KP_ENTER) and not app.has_modal():
		get_viewport().set_input_as_handled()
		start()

var _preview: GalaxyPreview

func refresh() -> void:
	for c in get_children():
		c.queue_free()
	if app and not _era_read:
		_era_read = true
		vm.ui_era = str(app.settings.get("uiEra", "tng"))
	UiTokens.era = vm.ui_era
	Lcars.accent_empire = faction
	var _t := LoadTrace.start()
	app.theme = Lcars.build_theme(faction)
	_t = LoadTrace.done("new: theme", _t, 10)
	var margin := Lcars.screen_margin(self, Vector4(48, 48, 28, 28))
	var col := Lcars.vbox(16)
	margin.add_child(col)
	var head := Lcars.hbox(18)
	head.add_child(Lcars.back_pill(func(): app.goto("title", false)))
	head.add_child(Lcars.heading("New campaign", 46))
	var sub := Lcars.caption("Choose a power and shape the quadrant")
	sub.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	head.add_child(sub)
	col.add_child(head)
	var cards := Lcars.hbox(10)
	col.add_child(cards)
	for f in Lcars.FACTIONS:
		cards.add_child(_faction_card(f))
	_t = LoadTrace.done("new: faction cards", _t, 10)
	# Web (21 Nov): the first open builds over three frames (cards, then the galaxy and middle columns, then the rules), so no
	# frame carries the whole screen (Firefox: one 0.6-1.0 s frame). Later refreshes (a toggle) stay in one frame.
	var spread := OS.has_feature("web") and not _built_once and is_inside_tree()
	_built_once = true
	if spread:
		await get_tree().process_frame
		if not is_instance_valid(col) or col.is_queued_for_deletion():
			return
		_t = LoadTrace.start()
	# The option columns scroll: with the preset and Threats panels the middle column is taller than a 1080p window.
	var scroll := ScrollContainer.new()
	scroll.name = "OptionsScroll"
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	col.add_child(scroll)
	var opts := Lcars.hbox(16)
	opts.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	opts.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.add_child(opts)
	opts.add_child(_galaxy_panel())
	_t = LoadTrace.done("new: galaxy panel", _t, 10)
	# The middle column holds the preview and, under it, the preset picker (the Rules column keeps the height the launch card needs).
	var mid := Lcars.vbox(14)
	mid.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var pv := _preview_panel()
	_t = LoadTrace.done("new: preview", _t, 10)
	pv.size_flags_vertical = Control.SIZE_EXPAND_FILL
	mid.add_child(pv)
	if not vm.scenarios().is_empty():
		mid.add_child(_scenario_panel())
	mid.add_child(_preset_panel())
	_t = LoadTrace.done("new: presets", _t, 10)
	if spread:
		await get_tree().process_frame
		if not is_instance_valid(col) or col.is_queued_for_deletion():
			return
		_t = LoadTrace.start()
	mid.add_child(_threat_panel())
	opts.add_child(mid)
	_t = LoadTrace.done("new: threats", _t, 10)
	if spread:
		await get_tree().process_frame
		if not is_instance_valid(col) or col.is_queued_for_deletion():
			return
		_t = LoadTrace.start()
	opts.add_child(_rules_column())
	LoadTrace.done("new: rules", _t, 10)
	Motion.stagger(opts)

var _era_read := false
var _built_once := false

## A row of ghost toggles under a caption. `core_key` is kept for callers; every option on this screen is now read by the
## core (shape, minor races and difficulty joined with the schema-15 generator), so none carries a "generator pending" mark.
func _choice_row(cap: String, items: Array, current: String, on_pick: Callable, _core_key: String = "", min_w: int = 0) -> Control:
	var box := Lcars.vbox(5)
	var ch := Lcars.hbox(8)
	ch.add_child(Lcars.caption(cap))
	box.add_child(ch)
	var row := HFlowContainer.new()
	row.add_theme_constant_override("h_separation", 6)
	row.add_theme_constant_override("v_separation", 6)
	for it in items:
		var id: String = str(it[0])
		var t := Lcars.toggle(it[1], current == id, func(_on): on_pick.call(id), it[2] if it.size() > 2 else "")
		if min_w > 0:
			t.custom_minimum_size.x = min_w
		if it.size() > 3:
			t.name = it[3]
		row.add_child(t)
	box.add_child(row)
	return box

func _galaxy_panel() -> Control:
	var galaxy := Lcars.frame("Galaxy", "shape, size, races", "secondary", "galaxy")
	galaxy.custom_minimum_size.x = 430
	var gb := Lcars.body(galaxy)
	gb.add_theme_constant_override("separation", 14)
	if vm.scenario_active():
		# A scenario brings its own galaxy and powers; only the seed of an unseeded one is the player's.
		var sd := vm.scenario_def()
		var m: Dictionary = sd["map"]
		var fixed_note := Lcars.para("Fixed by the scenario %s: %dx%d sectors, %d stars%s%s. Powers: %s." % [str(sd["name"]), int(m["width"]), int(m["height"]), int(m["systems"]),
			", a fixed chart" if bool(m.get("fixed", false)) else "", (", " + str(m["shape"]) + " shape") if m.has("shape") else "",
			", ".join((sd.get("lineup", []) as Array).map(func(k): return str(k).capitalize()))], Lcars.TEXT2, 13)
		fixed_note.name = "ScenarioFixed"
		gb.add_child(fixed_note)
		if m.has("seed") or bool(m.get("fixed", false)):
			return galaxy
		gb.add_child(_seed_row())
		return galaxy
	gb.add_child(_choice_row("Shape", NewCampaignVM.SHAPES.map(func(s): return [s["id"], s["label"], s["blurb"]]), vm.shape, func(id): vm.shape = id; refresh(), "shape"))
	gb.add_child(_choice_row("Size", SIZES.map(func(s): return [s["id"], s["label"], "%dx%d sectors, %d star systems. %s" % [s["width"], s["height"], s["systems"], s["blurb"]]]), size_id, func(id): size_id = id; refresh(), "systems"))
	gb.add_child(_choice_row("Minor races", NewCampaignVM.DENSITIES.map(func(d): return [d["id"], d["label"], "%s minor races on this size (at most 29)." % _minors_for(d["id"])]), vm.minor_density, func(id): vm.minor_density = id; refresh(), "minorDensity"))
	gb.add_child(_choice_row("Planets per star", NewCampaignVM.PLANET_RANGES.map(func(r): return [r["id"], r["label"], r["blurb"]]), vm.planets_id, func(id): vm.planets_id = id; refresh(), "planetsPerStar"))
	gb.add_child(_choice_row("Start systems", NewCampaignVM.START_SYSTEM_CHOICES.map(func(n): return [str(n), str(n), "Each empire starts with its capital and %d more colon%s nearby." % [n, "y" if n == 1 else "ies"]]), str(vm.start_systems), func(id): vm.start_systems = int(id); refresh(), "startSystems"))
	gb.add_child(_powers_row())
	gb.add_child(_hotseat_row())
	gb.add_child(_seed_row())
	return galaxy

func _seed_row() -> Control:
	var box := Lcars.vbox(5)
	box.add_child(Lcars.caption("Seed"))
	var sr := Lcars.hbox(8)
	var seed_edit := SpinBox.new()
	seed_edit.max_value = 99999999
	seed_edit.value = seed_value
	seed_edit.custom_minimum_size.x = 180
	seed_edit.value_changed.connect(func(v): seed_value = int(v); _update_preview())
	seed_edit.name = "Seed"
	sr.add_child(seed_edit)
	sr.add_child(Lcars.pill("Randomise", "ghost", func(): vm.roll_seed(); refresh(), "", "Roll a new galaxy"))
	box.add_child(sr)
	return box

## "Powers in play": one toggle per playable power (two to eight; your own cannot be switched off).
func _powers_row() -> Control:
	var box := Lcars.vbox(5)
	box.name = "PowersInPlay"
	var cap := Lcars.hbox(8)
	cap.add_child(Lcars.caption("Powers in play  %d" % vm.powers()))
	box.add_child(cap)
	var row := HFlowContainer.new()
	row.add_theme_constant_override("h_separation", 6)
	row.add_theme_constant_override("v_separation", 6)
	var ids := vm.lineup_ids()
	for f in Lcars.FACTIONS:
		var id := int(f["id"])
		var blocked := vm.toggle_block(id)
		var t := Lcars.toggle(str(f["short"]), ids.has(id), func(_on): vm.toggle_power(id); refresh(), "%s: %s" % [f["short"], ", ".join(f["traits"])])
		t.name = "Power%d" % id
		t.disabled = blocked != "" and ids.has(id) and id == faction
		if blocked != "":
			t.tooltip_text = blocked
		row.add_child(t)
	box.add_child(row)
	return box

func _preview_panel() -> Control:
	var f := Lcars.frame("Preview", "%s · %d systems" % [vm.shape, int(vm.size_def()["systems"])], "primary", "star")
	f.name = "GalaxyPreviewPanel"
	f.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var b := Lcars.body(f)
	_preview = GalaxyPreview.new()
	_preview.name = "GalaxyPreview"
	_preview.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_preview.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_preview.custom_minimum_size = Vector2(420, 150)
	b.add_child(_preview)
	_update_preview()
	var stats := Lcars.hbox(10)
	var sz := vm.size_def()
	for st in [["empire", "Powers", str(vm.powers()), "Major powers in play."], ["star", "Systems", str(int(sz["systems"])), "Star systems the galaxy holds."], ["galaxy", "Sectors", "%dx%d" % [int(sz["width"]), int(sz["height"])], "The map's sector grid."], ["population", "Minor races", str(vm.minors_count()), "Estimated from the density: about %s of systems." % vm.minor_density]]:
		var t := Lcars.kpi(st[0], st[1], st[2], "", "", {"title": st[1], "value": st[2], "effect": st[3]})
		t.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		t.custom_minimum_size.x = 0
		stats.add_child(t)
	b.add_child(stats)
	var leg := Lcars.hbox(16)
	var legend := HFlowContainer.new()
	legend.add_theme_constant_override("h_separation", 14)
	legend.add_theme_constant_override("v_separation", 4)
	for e in vm.lineup_ids():
		var it := Lcars.hbox(6)
		it.add_child(Lcars.emblem(int(e), 16))
		it.add_child(Lcars.label(Lcars.FACTIONS[int(e) - 1]["short"], "Muted", 13))
		legend.add_child(it)
	var mi := Lcars.hbox(6)
	mi.add_child(UiIcon.make("minor", 16, Lcars.TEXT2))
	mi.add_child(Lcars.label("Minor race", "Muted", 13))
	legend.add_child(mi)
	leg.add_child(legend)
	b.add_child(leg)
	return f

## Minor races a density would give on the chosen size (for the button tooltips).
func _minors_for(density: String) -> int:
	var keep := vm.minor_density
	vm.minor_density = density
	var n := vm.minors_count()
	vm.minor_density = keep
	return n

func _update_preview() -> void:
	if _preview:
		var pv := vm.preview_galaxy()
		if pv.is_empty():
			_preview.configure(vm.shape, seed_value, int(vm.size_def()["systems"]), vm.powers(), vm.minors_count(), faction, vm.lineup_ids())
		else:
			_preview.configure_core(pv, faction)

## The scenario picker (docs/design/SCENARIOS.md): a free game, or a scripted start. It sits above the preset picker; picking one fills the
## preset, powers, galaxy and era, and the briefing, start year and victory conditions show under the buttons.
func _scenario_panel() -> Control:
	var frame := Lcars.frame("Scenario", "scripted starts", "primary", "alert")
	frame.name = "ScenarioPanel"
	var box := Lcars.body(frame)
	box.add_theme_constant_override("separation", 8)
	var items := [["", "Free game", vm.scenario_text(""), "ScenarioNone"]]
	for s in vm.scenarios():
		items.append([str(s["id"]), str(s["name"]).trim_prefix("The "), vm.scenario_text(str(s["id"])), "Scenario" + "".join(Array(str(s["id"]).split("-")).map(func(w): return str(w).capitalize()))])
	box.add_child(_choice_row("Pick a scenario", items, vm.scenario, func(id): vm.pick_scenario(id); refresh()))
	var note := Lcars.para(vm.scenario_text(vm.scenario), Lcars.TEXT2, 13)
	note.name = "ScenarioNote"
	box.add_child(note)
	return frame

## The preset picker (docs/design/PACKS-AND-PRESETS.md): one toggle per preset, then the chosen preset's description. Picking one
## fills the other options (era, size, shape, minor races, difficulty, events, rivals) from the preset's defaults.
func _preset_panel() -> Control:
	var frame := Lcars.frame("Preset", "rules, content, powers", "secondary", "settings")
	frame.name = "PresetPanel"
	var box := Lcars.body(frame)
	box.add_theme_constant_override("separation", 8)
	var items := vm.presets().map(func(p): return [str(p["id"]), str(p["name"]) + (" (yours)" if bool(p.get("user", false)) else ""), vm.preset_text(str(p["id"])), "Preset" + "".join(Array(str(p["id"]).split("-")).map(func(w): return str(w).capitalize()))])
	box.add_child(_choice_row("Pick a starting point", items, vm.preset, func(id): vm.pick_preset(id); refresh(), "preset"))
	var note := Lcars.para(vm.preset_text(vm.preset), Lcars.TEXT2, 13)
	note.name = "PresetNote"
	box.add_child(note)
	return frame

## One checkbox that writes a threat switch.
func _threat_check(text: String, key: String, tip: String) -> CheckBox:
	var c := CheckBox.new()
	c.text = text
	c.name = "Threat_" + key
	c.button_pressed = bool(vm.threats[key])
	c.tooltip_text = tip
	c.toggled.connect(func(on): vm.threats[key] = on; refresh())
	return c

## Threat actors (docs/design/THREAT-ACTORS.md): the Borg, Species 8472 and the vanilla monsters. Monsters and the Borg have
## their own switches (the Borg-only off switch), then a frequency, the replacements (sphere, 8472, DS9) and one toggle per actor.
func _threat_panel() -> Control:
	var frame := Lcars.frame("Threats", vm.threats_text(), "tertiary", "alert")
	frame.name = "ThreatPanel"
	var box := Lcars.body(frame)
	box.add_theme_constant_override("separation", 8)
	var sw := HFlowContainer.new()
	sw.add_theme_constant_override("h_separation", 16)
	sw.add_theme_constant_override("v_separation", 4)
	sw.add_child(_threat_check("Monsters", "monsters", "Husnock, Calamarain, Chodak, the Crystalline Entity, Echo Papa, the Edo Guardian, the Gomtuu and the Tarellians roam the galaxy."))
	sw.add_child(_threat_check("Borg invasion", "borg", "A Borg cube hunts your colonies and assimilates them. This switch turns the Borg off without touching the monsters. A seated Borg empire replaces it."))
	sw.add_child(_threat_check("Spare the AI", "sparesAi", "Threat actors hunt and fight only you; rival empires are left alone."))
	box.add_child(sw)
	box.add_child(_choice_row("Frequency", NewCampaignVM.THREAT_FREQUENCIES.map(func(f): return [f["id"], f["label"], f["blurb"]]), str(vm.threats["frequency"]), func(id): vm.threats["frequency"] = id; refresh()))
	var rp := HFlowContainer.new()
	rp.add_theme_constant_override("h_separation", 16)
	rp.add_theme_constant_override("v_separation", 4)
	rp.add_child(_threat_check("Sphere for cube", "borgSphere", "A Borg sphere (faster, weaker) replaces the cube."))
	rp.add_child(_threat_check("8472 for Husnock", "species8472", "Species 8472 replaces the Husnock."))
	rp.add_child(_threat_check("DS9 for Edo Guardian", "edoStation", "A peaceful Deep Space Nine replaces the Edo Guardian."))
	box.add_child(rp)
	box.add_child(Lcars.caption("Actors that may appear"))
	var row := HFlowContainer.new()
	row.add_theme_constant_override("h_separation", 6)
	row.add_theme_constant_override("v_separation", 6)
	for k in vm.threat_kinds():
		var key := str(k["key"])
		var t := Lcars.toggle(str(k["name"]), vm.threat_kind_on(key), func(_on): vm.toggle_threat_kind(key); refresh(), "Switch %s on or off for this game." % str(k["name"]))
		t.name = "Actor_" + key
		t.add_theme_font_size_override("font_size", 13)
		row.add_child(t)
	box.add_child(row)
	return frame

func _rules_column() -> Control:
	var colr := Lcars.vbox(14)
	colr.custom_minimum_size.x = 470
	var rules := Lcars.frame("Rules", "difficulty, era, look", "tertiary", "settings")
	rules.size_flags_vertical = Control.SIZE_EXPAND_FILL
	var rb := Lcars.body(rules)
	rb.add_theme_constant_override("separation", 12)
	rb.add_child(_choice_row("Difficulty", NewCampaignVM.DIFFICULTIES.map(func(d): return [d["id"], d["label"], d["blurb"]]), vm.difficulty, func(id): vm.difficulty = id; refresh(), "difficulty"))
	rb.add_child(_choice_row("Starting era", vm.eras().map(func(e): return [str(e["id"]), e["label"], e["tip"]]), str(era), func(id): era = int(id); refresh(), "era"))
	rb.add_child(_choice_row("Ruleset", [["reference", "Reference", "Reference catalogue ships and technologies, planets & terraforming, task forces, modern diplomacy and intelligence."], ["prototype", "Prototype", "The original compact prototype ruleset with generic hulls."]], ruleset, func(id): ruleset = id; refresh(), "ruleset"))
	var tg := Lcars.hbox(18)
	var ev := CheckBox.new()
	ev.text = "Galactic events" if vm.rules_base() == "remastered" else "Random events"
	ev.button_pressed = events
	ev.tooltip_text = "Events on or off: galactic events with choices (Remastered) or the original random events (Classic)."
	ev.toggled.connect(func(on): events = on)
	tg.add_child(ev)
	var ag := CheckBox.new()
	ag.text = "Aggressive rivals"
	ag.button_pressed = aggression
	ag.tooltip_text = "Rivals declare war and raid when they see weakness."
	ag.toggled.connect(func(on): aggression = on)
	tg.add_child(ag)
	var dm := CheckBox.new()
	dm.name = "Domination"
	dm.text = "Domination victory"
	dm.button_pressed = vm.domination
	dm.tooltip_text = "On: holding three fifths of the systems from turn 20 wins. Off: conquest (every colony) is the military victory."
	dm.toggled.connect(func(on): vm.domination = on)
	tg.add_child(dm)
	var stt := CheckBox.new()
	stt.text = "Race star types"
	stt.button_pressed = vm.star_types
	stt.tooltip_text = "Each empire's home star takes its race's colour (Federation yellow, Klingon red, Romulan green, Dominion blue, Borg violet)."
	stt.toggled.connect(func(on): vm.star_types = on)
	stt.name = "StarTypes"
	tg.add_child(stt)
	rb.add_child(tg)
	rb.add_child(_choice_row("Interface look", UiTokens.ERA_ORDER.map(func(id): return [id, UiTokens.ERAS[id]["label"], "%s interface: shapes and colours of that era's LCARS." % UiTokens.ERAS[id]["name"]]), vm.ui_era, func(id): vm.ui_era = id; UiTokens.era = id; refresh()))
	colr.add_child(rules)
	var summ := PanelContainer.new()
	var ss := Lcars.box_lcars(Color(Lcars.accent().darkened(0.8), 0.95), UiTokens.radius(), Lcars.accent(), 2, Vector4(18, 14, 18, 14))
	ss.shadow_color = Color(Lcars.accent(), 0.25)
	ss.shadow_size = 16
	summ.add_theme_stylebox_override("panel", ss)
	var sb := Lcars.vbox(8)
	summ.add_child(sb)
	var sh := Lcars.hbox(12)
	sh.add_child(Lcars.emblem(faction, 44))
	var nm := Lcars.vbox(0)
	nm.add_child(Lcars.label(vm.faction_name(faction, ""), "Heading", 28))
	nm.add_child(Lcars.caption("from " + Lcars.FACTIONS[faction - 1]["capital"]))
	sh.add_child(nm)
	sb.add_child(sh)
	for line in vm.summary():
		sb.add_child(Lcars.label(line, "", 13, Lcars.TEXT2))
	var launch := Lcars.pill("Launch campaign", "primary", func(): start(), "", "", "Enter")
	launch.name = "Launch"
	launch.custom_minimum_size.y = 60
	launch.add_theme_font_size_override("font_size", 28)
	sb.add_child(launch)
	colr.add_child(summ)
	return colr

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
	b.custom_minimum_size = Vector2(0, 186)
	b.disabled = vm.scenario_active() and not vm.lineup_ids().has(id)   # A scenario seats its own powers.
	b.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	b.clip_contents = true
	b.tooltip_text = "%s · capital %s\n“%s”\n%s" % [f["short"], f["capital"], f["motto"], ", ".join(f["traits"])]
	var bg := Lcars.box_lcars(Color(col, 0.16) if selected else Color(Lcars.PANEL2, 0.85), UiTokens.radius(), col if selected else Lcars.LINE2, 3 if selected else 1, Vector4(16, 14, 16, 14))
	if selected:
		bg.shadow_color = Color(col, 0.35)
		bg.shadow_size = 18
	for st in ["normal", "hover", "pressed", "hover_pressed", "focus"]:
		var s := bg.duplicate() as StyleBoxFlat
		if st == "hover":
			s.border_color = col
		b.add_theme_stylebox_override(st, s)
	b.pressed.connect(_pick_faction.bind(id))
	var h := Lcars.vbox(6)
	h.set_anchors_preset(Control.PRESET_FULL_RECT)
	h.offset_left = 14
	h.offset_top = 12
	h.offset_right = -12
	h.offset_bottom = -12
	b.add_child(h)
	# Eight powers share the row, so a card is a column: portrait over name, capital and traits (the motto is in the tooltip).
	var ps := PortraitSlot.make(id, Vector2(76, 62))
	ps.live = selected
	ps.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	h.add_child(ps)
	var v := Lcars.vbox(3)
	v.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	h.add_child(v)
	var nm := Lcars.label(str(f["short"]).to_upper(), "", 19, col)
	nm.add_theme_font_override("font", Lcars.font("display", 600))
	nm.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	v.add_child(nm)
	v.add_child(Lcars.caption(str(f["capital"])))
	for t in f["traits"]:
		var tl := Lcars.label(t, "Muted", 11, Color(pal["secondary"]))
		tl.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		var trow := Lcars.hbox(4)
		trow.add_child(UiIcon.make("delta_right", 8, Color(pal["secondary"])))
		tl.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		trow.add_child(tl)
		v.add_child(trow)
	for n in h.find_children("*", "Control", true, false):
		n.mouse_filter = Control.MOUSE_FILTER_IGNORE
	h.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return b


## Hotseat players (MULTIPLAYER.md): the other powers in play that a human at this PC will command. The chair passes between
## the humans each turn behind a hand-over card; the AI plays the rest.
func _hotseat_row() -> Control:
	var box := Lcars.vbox(5)
	box.name = "HotseatPlayers"
	var hs := vm.hotseat_ids()
	box.add_child(Lcars.caption("Hotseat players  %s" % ("off: one player" if hs.is_empty() else "%d humans at this PC" % (hs.size() + 1))))
	var row := HFlowContainer.new()
	row.add_theme_constant_override("h_separation", 6)
	row.add_theme_constant_override("v_separation", 6)
	var ids := vm.lineup_ids()
	for f in Lcars.FACTIONS:
		var id := int(f["id"])
		if not ids.has(id) or id == faction:
			continue
		var t := Lcars.toggle(str(f["short"]), hs.has(id), func(_on): vm.toggle_hotseat(id); refresh(), "A second human plays %s at this PC (hotseat)" % f["short"])
		t.name = "Hotseat%d" % id
		row.add_child(t)
	box.add_child(row)
	return box
