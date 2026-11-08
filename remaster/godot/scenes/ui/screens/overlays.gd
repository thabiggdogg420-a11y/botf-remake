class_name Overlays
extends RefCounted
## Modal overlays (presentation): turn briefing and chronicle, galactic event cards, victory panel, end screen,
## game menu, settings and keyboard help. Their data comes from OverlaysVM (scripts/viewmodels/overlays_vm.gd).

const ORDER := OverlaysVM.ORDER
const CONDITION_BLURB := OverlaysVM.CONDITION_BLURB
const CATEGORY_TONE := OverlaysVM.CATEGORY_TONE

static func _vm(app: App) -> OverlaysVM:
	return OverlaysVM.new(app.session)

static func open(app: App, kind: String) -> Control:
	match kind:
		"briefing": return briefing(app)
		"events": return events(app)
		"victory": return victory(app)
		"end": return end_screen(app)
		"menu": return menu(app)
		"settings": return settings(app)
		"help": return help(app)
	return null

# ---------------------------------------------------------------------------------------------
const CATEGORY_ICON := {"construction": "build", "research": "research", "exploration": "star", "diplomacy": "diplomacy", "intel": "intel", "combat": "combat",
	"colonization": "colony", "events": "events", "stations": "system", "victory": "victory", "economy": "credits", "fleets": "fleets"}

static func briefing(app: App, filter: String = "all") -> Control:
	var b: Variant = _vm(app).briefing()
	var root := Lcars.vbox(12)
	root.name = "Briefing"
	if not b is Dictionary:
		root.add_child(Lcars.empty_note("No briefing yet. End a turn to receive the chronicle."))
		return app.open_modal(root, "Turn briefing", Vector2(900, 0))
	var head := Lcars.hbox(24)
	var t := Lcars.vbox(0)
	t.add_child(Lcars.caption("Turn briefing · chronicle"))
	var outcome := str(b.get("outcome", "ongoing"))
	t.add_child(Lcars.heading("Turn %d %s" % [int(b["turn"]), "- Victory" if outcome == "victory" else ("- Defeat" if outcome == "defeat" else "report")], 34))
	head.add_child(t)
	head.add_child(Lcars.spacer())
	var credits := int(b.get("creditsAfter", 0)) - int(b.get("creditsBefore", 0))
	head.add_child(Lcars.kpi("credits", "Credits", Lcars.signed(credits), "", "alert" if credits < 0 else "good", {"title": "Credits this turn", "value": Lcars.signed(credits), "parts": [["Before", Lcars.fmt(b.get("creditsBefore", 0)), ""], ["After", Lcars.fmt(b.get("creditsAfter", 0)), ""]], "effect": "Income less upkeep and spending, resolved this turn."}))
	head.add_child(Lcars.kpi("save", "Treasury", Lcars.fmt(b.get("creditsAfter", 0)), "", "", {"title": "Treasury", "value": Lcars.fmt(b.get("creditsAfter", 0)), "effect": "Credits held after the turn resolved."}))
	head.add_child(Lcars.kpi("research", "Research", "+" + Lcars.fmt(b.get("researchGained", 0)), "", "info", {"title": "Research gained", "value": "+" + Lcars.fmt(b.get("researchGained", 0)), "effect": "Points added across the six fields by the allocation."}))
	head.add_child(Lcars.kpi("orders", "Orders", int(b.get("ordersApplied", 0)), "", "", {"title": "Orders applied", "value": str(int(b.get("ordersApplied", 0))), "effect": "Drafted orders that resolved this turn."}))
	var ack := Lcars.pill("Acknowledge", "primary", func(): app.close_modal(), "", "", "Enter")
	ack.name = "Acknowledge"
	ack.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	head.add_child(ack)
	root.add_child(head)
	var events: Array = b.get("events", [])
	var cats := OverlaysVM.categories(events)
	var tabs := Lcars.hbox(6)
	var modal_holder := {"m": null}
	var mk_tab := func(id: String, text: String):
		var tb := Lcars.toggle(text, filter == id, func(_on):
			app.close_modal(modal_holder["m"])
			briefing(app, id))
		tabs.add_child(tb)
	mk_tab.call("all", "All  %d" % events.size())
	for c in cats:
		mk_tab.call(c, "%s  %d" % [Lcars.CATEGORY_LABELS.get(c, c), events.filter(func(e): return e["category"] == c).size()])
	root.add_child(tabs)
	var list := Lcars.vbox(10)
	for c in cats:
		if filter != "all" and filter != c:
			continue
		var sec := Lcars.vbox(4)
		var h := Lcars.hbox(8)
		h.add_child(UiIcon.make(CATEGORY_ICON.get(c, "info"), 20, Lcars.tone_color(CATEGORY_TONE.get(c, "primary"))))
		h.add_child(Lcars.label(Lcars.CATEGORY_LABELS.get(c, c).to_upper(), "SubHeading"))
		sec.add_child(h)
		for e in events:
			if e["category"] != c:
				continue
			sec.add_child(_event_row(app, e, modal_holder))
		list.add_child(sec)
	if events.is_empty():
		list.add_child(Lcars.empty_note("A quiet turn. Production and research continue."))
	var sc := Lcars.scroll(list)
	sc.custom_minimum_size = Vector2(0, 520)
	root.add_child(sc)
	var foot := HFlowContainer.new()
	for col in b.get("colonies", []):
		var bd := Lcars.badge("%s%s" % [col["name"], (" ✓ " + str(col["completed"])) if str(col.get("completed", "")) != "" else ""], "alert" if int(col["economy"].get("foodDeficit", 0)) > 0 else "dim")
		bd.tooltip_text = "Food %s · industry %s · research %s · net %s" % [col["economy"]["food"], col["economy"]["industry"], col["economy"]["research"], Lcars.signed(col["economy"]["net"])]
		foot.add_child(bd)
	root.add_child(foot)
	var m := app.open_modal(root, "", Vector2(1100, 0))
	modal_holder["m"] = m
	ack.grab_focus.call_deferred()
	return m

static func _event_row(app: App, e: Dictionary, holder: Dictionary) -> Control:
	var sev := str(e.get("severity", "info"))
	var b := Button.new()
	b.theme_type_variation = "GhostButton"
	b.alignment = HORIZONTAL_ALIGNMENT_LEFT
	b.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	b.add_theme_font_override("font", Lcars.font("ui", 440))
	b.add_theme_font_size_override("font_size", 15)
	var sb := Lcars.box(Color(Lcars.PANEL2, 0.7), 10, Lcars.tone_color(OverlaysVM.severity_tone(sev)), 0, Vector4(16, 7, 16, 7))
	sb.border_width_left = 5
	b.add_theme_stylebox_override("normal", sb)
	var due := ""
	if e.has("dueTurn"):
		due = "   (due turn %d)" % int(e["dueTurn"])
	b.text = str(e["text"]) + due
	b.pressed.connect(func():
		app.close_modal(holder["m"])
		_open_event(app, e))
	return b

static func _open_event(app: App, e: Dictionary) -> void:
	var cat := str(e["category"])
	if cat == "combat" and e.has("battle"):
		app.goto("combat")
		return
	if e.has("eventId"):
		app.open_overlay("events")
		return
	if cat == "victory" and not e.has("x"):
		app.open_overlay("victory")
		return
	if e.has("systemId"):
		app.select_system(int(e["systemId"]))
		if cat == "construction":
			app.goto("system")
			return
	elif e.has("fleetId"):
		app.select_fleet(int(e["fleetId"]))
	if cat == "research":
		app.goto("research")
	elif cat == "diplomacy" and not e.has("x"):
		app.goto("diplomacy")
	elif cat == "intel" and not e.has("x"):
		app.goto("intel")
	elif e.has("x"):
		app.goto("galaxy")
		var s := app.current_screen()
		if s and s.has_method("focus_cell"):
			s.focus_cell(int(e["x"]), int(e["y"]))

# ---------------------------------------------------------------------------------------------
static func events(app: App) -> Control:
	var v := app.view
	var vm := _vm(app)
	var root := Lcars.vbox(12)
	root.name = "Events"
	var evs := vm.sorted_events()
	if evs.is_empty():
		# A compact note, not an empty 1120x600 card grid.
		root.add_child(Lcars.empty_note("No galactic events concern your empire right now. New events are drawn every few turns."))
		root.add_child(Lcars.pill("Close", "ghost", func(): app.close_modal(), "", "", "Esc"))
		return app.open_modal(root, "Galactic events", Vector2(560, 0), "secondary")
	var grid := Lcars.grid(2)
	grid.add_theme_constant_override("h_separation", 14)
	grid.add_theme_constant_override("v_separation", 14)
	var turn := int(v["meta"]["turn"])
	for e in evs:
		var c := Lcars.card()
		c.custom_minimum_size = Vector2(520, 0)
		var col := Lcars.vbox(6)
		c.add_child(col)
		var h := Lcars.hbox(10)
		var glyph := EventGlyph.new()
		glyph.kind = str(e["kind"])
		glyph.custom_minimum_size = Vector2(40, 40)
		h.add_child(glyph)
		var tt := Lcars.vbox(0)
		tt.add_child(Lcars.label(str(e["name"]).to_upper(), "SubHeading", 22, Lcars.TEXT))
		var where := ""
		if e.has("system"):
			where = str(e["system"]) + " · "
		elif e.has("x"):
			where = "Sector %d.%d · " % [int(e["x"]), int(e["y"])]
		tt.add_child(Lcars.caption("%ssince turn %d%s" % [where, int(e["startTurn"]), (" · effects until turn %d" % int(e["endTurn"])) if int(e["endTurn"]) > turn else ""]))
		tt.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		h.add_child(tt)
		var drafted := int(e.get("drafted", -1))
		if e["awaitingChoice"]:
			h.add_child(Lcars.badge("decided" if drafted >= 0 else "due T%d" % int(e["deadline"]), "good" if drafted >= 0 else ("alert" if int(e["deadline"]) <= turn else "warn")))
		else:
			h.add_child(Lcars.badge("resolved" if e.get("choiceApplied", false) else "active", "dim"))
		col.add_child(h)
		col.add_child(Lcars.para(str(e["description"]), Lcars.TEXT2, 14))
		var chosen := drafted if drafted >= 0 else int(e.get("choice", -1))
		for o in e.get("choices", []):
			var blocked := vm.choice_block(o)
			var ob := Button.new()
			ob.theme_type_variation = "GhostButton"
			ob.toggle_mode = true
			ob.button_pressed = chosen == int(o["index"])
			ob.alignment = HORIZONTAL_ALIGNMENT_LEFT
			ob.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
			ob.text = "%s%s\n%s" % [o["label"], "  (default)" if o.get("isDefault", false) else "", o["effect"]]
			ob.add_theme_font_override("font", Lcars.font("ui", 460))
			ob.add_theme_font_size_override("font_size", 14)
			ob.disabled = not e["awaitingChoice"] or blocked != ""
			ob.tooltip_text = ("Unavailable: " + blocked) if blocked != "" else str(o["effect"])
			var ev: Dictionary = e
			var opt: Dictionary = o
			ob.pressed.connect(func():
				if vm.resolve_event_choice(ev, opt):
					app.close_modal()
					events(app))
			col.add_child(ob)
		grid.add_child(c)
	var sc := Lcars.scroll(grid)
	sc.custom_minimum_size = Vector2(1080, 600)
	root.add_child(sc)
	var close := Lcars.pill("Close", "ghost", func(): app.close_modal(), "", "", "Esc")
	root.add_child(close)
	return app.open_modal(root, "Galactic events · decisions & developments", Vector2(1120, 0), "secondary")

# ---------------------------------------------------------------------------------------------
static func victory_rows(v: Dictionary, compact: bool = false) -> Control:
	var list := Lcars.vbox(10 if not compact else 6)
	for c in v.get("victory", {}).get("conditions", []):
		var outer := Lcars.hbox(14)
		var tone0 := "good" if c["achieved"] else ("warn" if int(c["percent"]) >= 75 else "primary")
		outer.add_child(RingGauge.make(64 if not compact else 44, int(c["percent"]) if c["enabled"] else 0, 100, Lcars.tone_color(tone0), ("%d" % int(c["percent"])) if c["enabled"] else "-", "",
			{"title": str(c["name"]), "value": ("%d%%" % int(c["percent"])) if c["enabled"] else "off", "effect": str(c["explanation"])}))
		var row := Lcars.vbox(3)
		row.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		outer.add_child(row)
		var h := Lcars.hbox(8)
		h.add_child(Lcars.label(str(c["name"]).to_upper(), "SubHeading", 20 if not compact else 16, Lcars.TEXT))
		if c["achieved"]:
			h.add_child(Lcars.badge("achieved", "good"))
		elif not c["enabled"]:
			h.add_child(Lcars.badge("off in this ruleset", "dim"))
		h.add_child(Lcars.spacer())
		h.add_child(Lcars.label("%d%%" % int(c["percent"]) if c["enabled"] else "-", "Mono", 16))
		row.add_child(h)
		var tone := "good" if c["achieved"] else ("warn" if int(c["percent"]) >= 75 else "primary")
		row.add_child(Lcars.meter(int(c["percent"]) if c["enabled"] else 0, 100, Lcars.tone_color(tone), 8 if not compact else 5))
		if not compact:
			row.add_child(Lcars.para(CONDITION_BLURB.get(c["condition"], ""), Lcars.TEXT3, 13))
		row.add_child(Lcars.para(str(c["explanation"]), Lcars.TEXT2, 13))
		outer.name = "Victory_" + str(c["condition"])
		list.add_child(outer)
	return list

static func victory(app: App) -> Control:
	var v := app.view
	var vic: Dictionary = v.get("victory", {})
	var root := Lcars.vbox(14)
	root.name = "Victory"
	root.add_child(Lcars.caption("Paths to victory · %s rules" % v["meta"].get("rules", {}).get("presetName", "")))
	root.add_child(victory_rows(v))
	if vic.get("alternative", false):
		var box := Lcars.frame(str(vic.get("megaprojectName", "Megaproject")), "scientific victory", "secondary")
		var b := Lcars.body(box)
		var mp: Variant = vic.get("megaproject")
		if mp is Dictionary:
			b.add_child(Lcars.para("Under construction at %s since turn %d." % [mp["system"], int(mp["startTurn"])]))
			b.add_child(Lcars.meter(mp["work"], vic["megaprojectCost"], Lcars.GOOD, 8))
			b.add_child(Lcars.label("%s / %s industry · up to %d/turn" % [Lcars.fmt(mp["work"]), Lcars.fmt(vic["megaprojectCost"]), int(vic["megaprojectWorkPerTurn"])], "Mono"))
		else:
			b.add_child(Lcars.para("Needs every research field at level %d, turn 50 or later and ¢%s; then %s industry (up to %d per turn) from one colony. Start it from the System screen." % [int(vic.get("threshold", 0)), Lcars.fmt(vic.get("megaprojectCredits", 0)), Lcars.fmt(vic.get("megaprojectCost", 0)), int(vic.get("megaprojectWorkPerTurn", 0))]))
		for r in vic.get("rivalProjects", []):
			var rr := Lcars.hbox(8)
			rr.add_child(Lcars.emblem(int(r["owner"]), 16))
			rr.add_child(Lcars.label("%s began theirs on turn %d: %s / %s" % [r["ownerName"], int(r["startTurn"]), Lcars.fmt(r["work"]), Lcars.fmt(vic["megaprojectCost"])], "", 14))
			rr.add_child(Lcars.meter(r["work"], vic["megaprojectCost"], Lcars.ALERT, 4, 160))
			b.add_child(rr)
		b.add_child(Lcars.pill("Open System screen", "secondary", func(): app.close_modal(); app.goto("system")))
		root.add_child(box)
	else:
		root.add_child(Lcars.para("Classic rules: conquest and domination only.", Lcars.TEXT3))
	root.add_child(Lcars.pill("Close", "ghost", func(): app.close_modal(), "", "", "Esc"))
	return app.open_modal(root, "Victory progress", Vector2(820, 0), "good")

# ---------------------------------------------------------------------------------------------
static func end_screen(app: App, preview: String = "") -> Control:
	var v := app.view
	var outcome := preview if preview != "" else str(v["meta"].get("outcome", "ongoing"))
	var sum := _vm(app).end_summary(outcome)
	var won: bool = sum["won"]
	var root := Lcars.vbox(14)
	root.name = "EndScreen"
	root.alignment = BoxContainer.ALIGNMENT_CENTER
	var winner: int = sum["winner"]
	var em := Lcars.emblem(app.player() if won or winner == 0 else winner, 84)
	em.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	root.add_child(em)
	var kick := Lcars.caption("Turn %d · Stardate %.1f" % [int(v["meta"]["turn"]), 41000 + int(v["meta"]["turn"]) * 25.7])
	kick.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	root.add_child(kick)
	var title := Lcars.label("VICTORY" if won else "DEFEAT", "", 96, Lcars.GOOD if won else Lcars.ALERT)
	title.add_theme_font_override("font", Lcars.font("display", 700))
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	root.add_child(title)
	var rl := Lcars.para(sum["reason"], Lcars.TEXT, 18)
	rl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	root.add_child(rl)
	var stats := Lcars.grid(4)
	stats.add_theme_constant_override("h_separation", 40)
	for pair in sum["stats"]:
		stats.add_child(Lcars.stat(pair[0], pair[1]))
	var sc := CenterContainer.new()
	sc.add_child(stats)
	root.add_child(sc)
	root.add_child(victory_rows(v, true))
	var acts := Lcars.hbox(10)
	acts.alignment = BoxContainer.ALIGNMENT_CENTER
	acts.add_child(Lcars.pill("Final briefing", "ghost", func(): app.close_modal(); app.open_overlay("briefing")))
	acts.add_child(Lcars.pill("View the galaxy", "ghost", func(): app.close_modal()))
	acts.add_child(Lcars.pill("Save", "secondary", func():
		if app.save_game("%s - %s turn %d" % [v["meta"]["playerName"], outcome, int(v["meta"]["turn"])]):
			app.toast("Campaign saved.", "good")))
	acts.add_child(Lcars.pill("New campaign", "primary", func(): app.close_modal(); app.view = {}; app.goto("new")))
	acts.add_child(Lcars.pill("Title", "tertiary", func(): app.close_modal(); app.view = {}; app.goto("title")))
	root.add_child(acts)
	return app.open_modal(root, "", Vector2(980, 0), "good" if won else "alert")

# ---------------------------------------------------------------------------------------------
static func menu(app: App) -> Control:
	var root := Lcars.vbox(10)
	root.name = "Menu"
	var name_edit := LineEdit.new()
	name_edit.text = app.save_name if app.save_name != "" else "Campaign"
	name_edit.custom_minimum_size.x = 380
	root.add_child(Lcars.caption("Save name"))
	var row := Lcars.hbox(8)
	row.add_child(name_edit)
	var do_save := func():
		if app.save_game(name_edit.text.strip_edges()):
			app.toast("Saved as %s." % name_edit.text, "good")
			app.close_modal()
	row.add_child(Lcars.pill("Save", "secondary", do_save, "", "", "Ctrl+S"))
	root.add_child(row)
	for item in [["Resume", "primary", func(): app.close_modal()],
			["Load campaign", "secondary", func(): app.close_modal(); app.goto("saves")],
			["Settings", "tertiary", func(): app.close_modal(); app.open_overlay("settings")],
			["Keyboard help", "tertiary", func(): app.close_modal(); app.open_overlay("help")],
			["Quit to title", "alert", func(): app.close_modal(); confirm_quit_to_title(app)],
			["Quit game", "ghost", func(): app.close_modal(); confirm_quit_game(app)]]:
		var b := Lcars.pill(item[0], item[1], item[2])
		b.name = str(item[0]).replace(" ", "")
		b.custom_minimum_size.y = 42
		root.add_child(b)
	return app.open_modal(root, "Menu", Vector2(480, 0))

## Quit to title from a campaign, after a confirm (unsaved progress since the last save is lost).
static func confirm_quit_to_title(app: App) -> Control:
	return app.confirm("Quit to title", "Leave this campaign and return to the title screen? Progress since your last save will be lost.",
		"Quit to title", func(): app.view = {}; app.goto("title", false))

## Quits the program, after a confirm. App.quit_hook (tests) replaces the real quit.
static func confirm_quit_game(app: App) -> Control:
	var body := "Close Birth of the Federation?"
	if not app.view.is_empty():
		body += " Progress since your last save will be lost."
	return app.confirm("Quit game", body, "Quit", func(): app.quit_game())

static func settings(app: App) -> Control:
	var s := app.settings
	var root := Lcars.vbox(14)
	root.name = "Settings"
	root.add_child(Lcars.caption("Display"))
	_display_rows(app, s, root)
	root.add_child(Lcars.caption("Interface and audio"))
	var scale_row := Lcars.hbox(12)
	scale_row.add_child(_setting_text("Interface scale", "Enlarges or shrinks text and controls (75-150%). The star chart keeps its own zoom."))
	var sl := HSlider.new()
	sl.min_value = 0.75
	sl.max_value = 1.5
	sl.step = 0.05
	sl.value = float(s.get("uiScale", 1.0))
	sl.custom_minimum_size.x = 220
	sl.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	var pct := Lcars.label("%d%%" % int(sl.value * 100), "Mono")
	sl.value_changed.connect(func(val): pct.text = "%d%%" % int(val * 100))
	sl.drag_ended.connect(func(_c): s["uiScale"] = sl.value; app.save_settings())
	scale_row.add_child(sl)
	scale_row.add_child(pct)
	root.add_child(scale_row)
	var era_row := Lcars.hbox(12)
	era_row.name = "Setting_uiEra"
	era_row.add_child(_setting_text("Interface look", "The LCARS of an era: shapes and colours of TOS, the movies, TNG and DS9, Voyager or the post-Nemesis years."))
	var eras := Lcars.hbox(4)
	for id in UiTokens.ERA_ORDER:
		var eid: String = id
		var t := Lcars.toggle(UiTokens.ERAS[id]["label"], str(s.get("uiEra", "tng")) == id, func(_on):
			s["uiEra"] = eid
			app.save_settings()
			app.close_modal()
			settings(app), UiTokens.ERAS[id]["name"])
		t.name = "Era_" + id
		eras.add_child(t)
	era_row.add_child(eras)
	root.add_child(era_row)
	for item in [["reducedMotion", "Reduced motion", "Stops camera glides, pulsing effects and battle animations."],
			["colorblind", "Colour-blind-safe owner colours", "Okabe-Ito palette for empires on the map and in the interface. Emblems stay the primary owner cue."],
			["autosave", "Autosave every turn", "Keeps one rolling autosave slot for the campaign in progress."],
			["confirmEndTurn", "Confirm end turn", "Ask before ending a turn with idle units."]]:
		var r := Lcars.hbox(12)
		r.add_child(_setting_text(item[1], item[2]))
		var key: String = item[0]
		var cb := CheckButton.new()
		cb.button_pressed = bool(s.get(key, false))
		cb.toggled.connect(func(on): s[key] = on; app.save_settings())
		cb.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		r.add_child(cb)
		root.add_child(r)
	BotfAudioMix.fill_defaults(s)
	for bus in BotfAudioCues.BUSES:
		root.add_child(_volume_row(app, s, bus))
	var acts := Lcars.hbox(8)
	acts.add_child(Lcars.pill("Reset to defaults", "ghost", func():
		app.settings = {"uiScale": 1.0, "colorblind": false, "reducedMotion": false, "autosave": true, "confirmEndTurn": false, "showGrid": true, "uiEra": "tng"}
		BotfAudioMix.fill_defaults(app.settings)
		DisplaySettings.fill_defaults(app.settings)
		DisplaySettings.apply(app.settings, app.get_tree())
		BotfAudioSystem.attach(app)
		app.save_settings()
		app.close_modal()
		settings(app)))
	acts.add_child(Lcars.spacer())
	acts.add_child(Lcars.pill("Close", "primary", func(): app.close_modal(), "", "", "Esc"))
	root.add_child(acts)
	root.add_child(Lcars.label("Core backend: %s" % app.core.backend, "Mono", 12, Lcars.TEXT3))
	# The display rows made the panel taller than a 1080 screen (Close fell off the bottom): scroll the rows, keep
	# the actions pinned below them.
	root.remove_child(acts)
	var scroll := ScrollContainer.new()
	scroll.name = "SettingsScroll"
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	scroll.custom_minimum_size = Vector2(700, minf(760.0, app.get_viewport_rect().size.y - 260.0))
	root.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.add_child(root)
	var outer := Lcars.vbox(10)
	outer.name = "SettingsPanel"
	outer.add_child(scroll)
	outer.add_child(acts)
	return app.open_modal(outer, "Settings", Vector2(720, 0))

## One audio slider (Master, Effects, Voice, Interface, Music): applied live, saved on release, with a sample sound.
static func _volume_row(app: App, s: Dictionary, bus: String) -> Control:
	var vr := Lcars.hbox(12)
	vr.name = "Volume_" + bus
	var lab: Array = BotfAudioMix.LABELS[bus]
	vr.add_child(_setting_text(lab[0], lab[1]))
	var vol := HSlider.new()
	vol.name = "Slider"
	vol.max_value = 1.0
	vol.step = 0.05
	vol.value = BotfAudioMix.volume_of(s, bus)
	vol.custom_minimum_size.x = 220
	vol.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	var pct := Lcars.label("%d%%" % int(vol.value * 100), "Mono")
	pct.custom_minimum_size.x = 52
	pct.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	vol.value_changed.connect(func(val):
		pct.text = "%d%%" % int(val * 100)
		s[BotfAudioMix.key_for(bus)] = val
		if BotfAudioSystem.instance != null:
			BotfAudioSystem.instance.set_volume(bus, val))
	vol.drag_ended.connect(func(_c):
		app.save_settings()
		if BotfAudioSystem.instance != null:
			BotfAudioSystem.instance.preview(bus))
	vol.focus_exited.connect(func(): app.save_settings())
	vr.add_child(vol)
	vr.add_child(pct)
	return vr

## Display rows: resolution, window mode, vsync, FPS cap, render scale, anti-aliasing, shadows. Each applies at once
## and is saved to user://; App._ready applies them again at the next start.
static func _display_rows(app: App, s: Dictionary, root: Control) -> void:
	DisplaySettings.fill_defaults(s)
	var changed := func(key: String, value) -> void:
		s[key] = value
		DisplaySettings.apply(s, app.get_tree())
		app.save_settings()
	var res_ids: Array = DisplaySettings.resolution_list()
	var res_labels: Array = res_ids.map(func(r): return str(r).replace("x", " x "))
	if not res_ids.has(str(s["display.resolution"])):
		res_ids.push_front(str(s["display.resolution"]))
		res_labels.push_front("Desktop size" if str(s["display.resolution"]) == "" else str(s["display.resolution"]))
	if DisplaySettings.offered("display.resolution"):
		root.add_child(_option_row("Resolution", "Window size in windowed mode. Borderless and fullscreen use the monitor's own size.",
			"display.resolution", res_ids, res_labels, s, changed))
	if DisplaySettings.offered("display.windowMode"):
		root.add_child(_option_row("Window mode", "Windowed, a borderless window covering the screen, or exclusive fullscreen.",
			"display.windowMode", DisplaySettings.WINDOW_MODES.keys(), DisplaySettings.WINDOW_MODES.values(), s, changed))
	var vr := Lcars.hbox(12)
	vr.name = "Setting_display_vsync"
	vr.add_child(_setting_text("Vertical sync", "Locks frames to the monitor refresh: no tearing, slightly more input delay."))
	var vcb := CheckButton.new()
	vcb.name = "Value"
	vcb.button_pressed = bool(s["display.vsync"])
	vcb.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	vcb.toggled.connect(func(on): changed.call("display.vsync", on))
	vr.add_child(vcb)
	if DisplaySettings.offered("display.vsync"):
		root.add_child(vr)
	else:
		vr.queue_free()
	if DisplaySettings.offered("display.fpsCap"):
		root.add_child(_option_row("Frame rate cap", "Upper limit on frames per second (with vsync off, or above the refresh rate).",
			"display.fpsCap", DisplaySettings.FPS_CAPS, DisplaySettings.FPS_CAPS.map(func(c): return DisplaySettings.fps_label(c)), s, changed))
	if DisplaySettings.offered("display.renderScale"):
		root.add_child(_option_row("Render scale", "3D resolution as a share of the window, upscaled with FSR. Lower is faster.",
			"display.renderScale", DisplaySettings.RENDER_SCALES, DisplaySettings.RENDER_SCALES.map(func(v): return "%d%%" % roundi(v * 100)), s, changed))
	root.add_child(_option_row("Anti-aliasing", "Smooths jagged edges on ships and planets. MSAA is sharper; FXAA is cheaper.",
		"display.antiAlias", DisplaySettings.ANTI_ALIAS.keys(), DisplaySettings.ANTI_ALIAS.values(), s, changed))
	root.add_child(_option_row("Shadow quality", "Shadow map size and softness in the battle and title scenes. Off draws no shadows.",
		"display.shadows", DisplaySettings.SHADOWS.keys(), DisplaySettings.SHADOWS.values(), s, changed))

static func _option_row(title: String, detail: String, key: String, ids: Array, labels: Array, s: Dictionary, changed: Callable) -> Control:
	var r := Lcars.hbox(12)
	r.name = "Setting_" + key.replace(".", "_")
	r.add_child(_setting_text(title, detail))
	var ob := OptionButton.new()
	ob.name = "Value"
	ob.custom_minimum_size.x = 220
	ob.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	var sel := 0
	for k in ids.size():
		ob.add_item(str(labels[k]), k)
		var cur = s.get(key)
		var num := (ids[k] is int or ids[k] is float) and (cur is int or cur is float)
		if (num and absf(float(ids[k]) - float(cur)) < 0.001) or (not num and str(ids[k]) == str(cur)):
			sel = k
	ob.select(sel)
	ob.item_selected.connect(func(k): changed.call(key, ids[k]))
	r.add_child(ob)
	return r

static func _setting_text(title: String, detail: String) -> Control:
	var v := Lcars.vbox(0)
	v.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	v.add_child(Lcars.label(title, "", 16, Lcars.TEXT))
	v.add_child(Lcars.para(detail, Lcars.TEXT3, 13))
	return v

static func help(app: App) -> Control:
	var root := Lcars.vbox(8)
	root.name = "Help"
	var g := Lcars.grid(2)
	g.add_theme_constant_override("h_separation", 26)
	for pair in [["F1 - F8", "Galaxy, System, Empire, Research, Fleets, Diplomacy, Intel, Database"], ["Enter", "End turn / acknowledge briefing"],
			["Esc", "Back, close panel, deselect unit, open menu"], ["Tab / Shift+Tab", "Cycle your ships on the star chart"],
			["Right-click", "Order menu for a sector (with a unit selected: move, attack, colonize...)"], ["Double-click", "Open your colony's System screen"],
			["Wheel / drag", "Zoom and pan the star chart"], ["F  /  H", "Fit the charted region  /  centre on the capital"],
			["Ctrl+K", "Command palette: any screen, colony, fleet or command"], ["F9  /  F10", "Saves  /  settings"],
			["E  /  V  /  C", "Galactic events  /  victory  /  chronicle"], ["Q E  or  [ ]", "Previous / next colony on the System screen"],
			["Alt+Left", "Back to the previous screen"], ["Ctrl+S", "Quick save"], ["?", "This help"]]:
		var k := Lcars.label(pair[0], "Mono", 16, Lcars.accent2())
		g.add_child(k)
		g.add_child(Lcars.label(pair[1], "", 15))
	root.add_child(g)
	root.add_child(Lcars.pill("Close", "ghost", func(): app.close_modal(), "", "", "Esc"))
	return app.open_modal(root, "Keyboard & mouse", Vector2(760, 0), "tertiary")
