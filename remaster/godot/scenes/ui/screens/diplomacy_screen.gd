extends Control
## Diplomacy presentation: known powers and minors, incoming proposals and calls to arms, dossiers with relation
## memory, the treaty builder and statecraft, the treaty ledger and galactic news.
## Data and orders: DiplomacyVM (scripts/viewmodels/diplomacy_vm.gd).

var app: App
var vm: DiplomacyVM:
	get:
		if vm == null:
			vm = DiplomacyVM.new(app.session)
		return vm
var d: Dictionary:
	get: return vm.d
	set(v): vm.d = v
var target: Dictionary:
	get: return vm.target
	set(v): vm.target = v
var pane: String:
	get: return vm.pane
	set(v): vm.pane = v
var dossier_tab: String:
	get: return vm.dossier_tab
	set(v): vm.dossier_tab = v
var tb: Dictionary:
	get: return vm.tb
	set(v): vm.tb = v

const TREATY_EFFECTS := DiplomacyVM.TREATY_EFFECTS

func _lock() -> String:
	return vm.lock_reason()

static func proposal_title(p: Dictionary) -> String:
	return DiplomacyVM.proposal_title(p)

static func consequences(p: Dictionary) -> Array:
	return DiplomacyVM.consequences(p)

static func deadline(turn: int, due: int) -> Array:
	return DiplomacyVM.deadline(turn, due)

func refresh() -> void:
	for c in get_children():
		c.queue_free()
	var focus: Variant = null
	if app.has_meta("diplomacy_focus"):
		focus = app.get_meta("diplomacy_focus")
		app.remove_meta("diplomacy_focus")
	if not vm.fetch(focus):
		add_child(Lcars.empty_note("Diplomacy unavailable."))
		return
	var incoming := vm.incoming()
	var has_inbox := vm.has_inbox()
	var row := Lcars.hbox(14)
	Lcars.screen_margin(self).add_child(row)
	row.add_child(_race_list())
	var main := Lcars.vbox(10)
	main.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(main)
	var tabs := Lcars.hbox(6)
	if has_inbox:
		tabs.add_child(_pane_tab("inbox", "Incoming  %d" % vm.inbox_count()))
	tabs.add_child(_pane_tab("dossier", "Dossier"))
	tabs.add_child(_pane_tab("ledger", "Treaty ledger  %d" % (d["treaties"].size() + d["treatyHistory"].size())))
	tabs.add_child(_pane_tab("news", "Galactic news  %d" % d["news"].size()))
	main.add_child(tabs)
	var content: Control
	match pane:
		"inbox": content = _inbox(incoming)
		"ledger": content = _ledger()
		"news": content = _news()
		_: content = _dossier() if not target.is_empty() else Lcars.empty_note("Select a contacted race.")
	content.size_flags_vertical = Control.SIZE_EXPAND_FILL
	main.add_child(content)

func _pane_tab(id: String, text: String) -> Button:
	var t := Lcars.toggle(text, pane == id, func(_on): pane = id; refresh())
	t.name = "Pane_" + id
	return t

func _race_list() -> Control:
	var contacted := vm.contacted()
	var f := Lcars.frame("Known powers", "%d" % (contacted.size() + d["minors"].size()), "primary", "diplomacy")
	f.custom_minimum_size.x = 440
	var b := Lcars.body(f)
	var list := Lcars.vbox(4)
	for e in d["empires"]:
		var id := int(e["empire"])
		var btn := Lcars.row_button(vm.is_selected(false, id))
		btn.name = "Race%d" % id
		btn.disabled = not e.get("contact", false)
		var h := Lcars.row_inner(btn)
		h.add_child(Lcars.emblem(id, 30, Lcars.owner_color(id) if e.get("contact", false) else Lcars.TEXT3))
		var t := Lcars.vbox(-2)
		t.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		t.add_child(Lcars.label(str(e["name"]), "", 15, Lcars.TEXT if e.get("contact", false) else Lcars.TEXT3))
		h.add_child(t)
		btn.tooltip_text = DiplomacyVM.status(e)
		if e.get("contact", false) and not e.get("eliminated", false):
			h.add_child(Lcars.meter(e.get("trust", 0), 100, Lcars.ALERT if e.get("atWar", false) else Lcars.GOOD, 4, 70))
		if e.get("eliminated", false):
			h.add_child(Lcars.icon("cross", 20, Lcars.TEXT3))
		elif e.get("atWar", false):
			h.add_child(Lcars.icon("war", 22, Lcars.ALERT))
		elif e.get("allied", false):
			h.add_child(Lcars.icon("treaty", 22, Lcars.GOOD))
		elif e.get("contact", false):
			h.add_child(Lcars.icon("peace", 22, Lcars.GOOD))
		else:
			h.add_child(Lcars.icon("unexplored", 20, Lcars.TEXT3))
		Lcars.ignore_mouse(h)
		btn.pressed.connect(func(): vm.select(false, id); refresh())
		list.add_child(btn)
	if d["minors"].size() > 0:
		list.add_child(Pic.tipped(Lcars.icon("minor", 22, Color("#d8c3ff")), "Minor races"))
	for mi in d["minors"]:
		var sid := int(mi["systemId"])
		var btn2 := Lcars.row_button(vm.is_selected(true, sid))
		var h2 := Lcars.row_inner(btn2)
		h2.add_child(UiIcon.make(UiIcon.minor_name(str((mi.get("race", {}) as Dictionary).get("temperament", "")) if mi.get("race") is Dictionary else ""), 22, Color("#d8c3ff")))
		var t2 := Lcars.vbox(-2)
		t2.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		t2.add_child(Lcars.label(str(mi["name"]), "", 15, Lcars.TEXT))
		h2.add_child(t2)
		btn2.tooltip_text = vm.minor_status(mi)
		h2.add_child(Lcars.meter(mi["attitude"], 100, Lcars.GOOD, 4, 70))
		Lcars.ignore_mouse(h2)
		btn2.pressed.connect(func(): vm.select(true, sid); refresh())
		list.add_child(btn2)
	if contacted.is_empty() and d["minors"].is_empty():
		list.add_child(Lcars.empty_note("No contact yet. Explore to meet other powers."))
	b.add_child(Lcars.scroll(list))
	# Standing
	var st2 := PanelContainer.new()
	st2.theme_type_variation = "Inset"
	var sv := Lcars.vbox(6)
	st2.add_child(sv)
	var rep := Lcars.hbox(8)
	var standing := vm.standing()
	var betr: int = standing["betrayals"]
	rep.add_child(Pic.num("treaty", "%d" % betr, ("%d treaty betrayal%s (last turn %d)" % [betr, "" if betr == 1 else "s", int(standing["last"])]) if betr > 0 else "Reputation: no betrayals", Lcars.ALERT if betr > 0 else Lcars.GOOD, 22, 16, Lcars.ALERT if betr > 0 else Lcars.GOOD))
	sv.add_child(rep)
	var vic: Dictionary = standing["bloc"]
	if not vic.is_empty():
		var br := Lcars.hbox(8)
		br.add_child(Pic.tipped(Lcars.icon("diplomacy", 20, Lcars.accent()), "Bloc"))
		br.add_child(Lcars.meter(vic["bloc"], max(1, int(vic["galaxy"])), Lcars.GOOD if vic.get("achieved", false) else Lcars.accent(), 6))
		br.add_child(Lcars.label("%d%%" % int(round(100.0 * float(vic["bloc"]) / max(1, int(vic["galaxy"])))), "Mono"))
		br.tooltip_text = "Your bloc holds %d of %d population. A strict majority with at least one ally or member minor is a diplomatic victory." % [int(vic["bloc"]), int(vic["galaxy"])]
		br.mouse_filter = Control.MOUSE_FILTER_PASS
		sv.add_child(br)
	b.add_child(st2)
	return f

# ---------------------------------------------------------------------------------------------
func _inbox(incoming: Array) -> Control:
	var v := app.view
	var f := Lcars.frame("Incoming", "%d" % vm.inbox_count(), "secondary")
	f.name = "Inbox"
	var list := Lcars.vbox(10)
	var lock := _lock()
	for p in incoming:
		var pid := int(p["id"])
		var card := Lcars.card()
		card.name = "Proposal%d" % pid
		var h := Lcars.hbox(12)
		card.add_child(h)
		h.add_child(Lcars.emblem(0 if p["proposer"].get("minor", false) else int(p["proposer"]["id"]), 34))
		var col := Lcars.vbox(4)
		col.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		var th := Lcars.hbox(8)
		th.add_child(Lcars.label("%s · %s" % [p["proposer"]["name"], proposal_title(p)], "SubHeading", 19, Lcars.TEXT))
		var dl := deadline(int(d["turn"]), int(p["dueTurn"]))
		th.add_child(Lcars.badge(dl[0], dl[1]))
		col.add_child(th)
		var terms := ("%s · %s tone" % [p["duration"], str(p["tone"]).to_lower()]) if p["kind"] == "treaty" else "%s tone" % str(p["tone"]).to_lower()
		th.tooltip_text = terms
		th.mouse_filter = Control.MOUSE_FILTER_PASS
		var cons := consequences(p)
		h.add_child(col)
		var acts := Lcars.vbox(6)
		var drafted := vm.drafted_response(pid)
		if not drafted.is_empty():
			acts.add_child(Pic.tipped(Lcars.icon("tick", 24, Lcars.GOOD), "Response ordered"))
			var did := int(drafted["id"])
			acts.add_child(Lcars.pill("Undo", "ghost", func(): app.cancel_orders([did], "Response withdrawn."), lock))
		else:
			var a := Lcars.pill("Accept", "good", func(): vm.respond(pid, true), lock, str(cons[0]))
			a.name = "Accept%d" % pid
			acts.add_child(a)
			var r := Lcars.pill("Reject", "alert", func(): vm.respond(pid, false), lock, str(cons[1]))
			r.name = "Reject%d" % pid
			acts.add_child(r)
		h.add_child(acts)
		list.add_child(card)
	for c in d["callsToArms"]:
		list.add_child(_call_card(c))
	Lcars.body(f).add_child(Lcars.scroll(list))
	return f

func _call_card(c: Dictionary) -> Control:
	var v := app.view
	var refused := vm.refused(c)
	var war := vm.war_order(c)
	var lock := _lock()
	var card := Lcars.card()
	card.name = "Call%d" % int(c["aggressor"])
	var h := Lcars.hbox(12)
	card.add_child(h)
	h.add_child(Lcars.emblem(int(c["defender"]), 34))
	var col := Lcars.vbox(4)
	col.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var th := Lcars.hbox(8)
	th.add_child(Lcars.label("%s · vs %s" % [c["defenderName"], c["aggressorName"]], "SubHeading", 19, Lcars.TEXT))
	th.add_child(Pic.tipped(Lcars.icon("war", 22, Lcars.ALERT), "Call to arms against %s" % c["aggressorName"]))
	if c.get("honored", false):
		th.add_child(Pic.tipped(Lcars.icon("tick", 22, Lcars.GOOD), "Honoured - at war"))
	else:
		th.add_child(Pic.num("clock", "%d" % max(0, int(c["dueTurn"]) - int(d["turn"])), "Declare war by turn %d" % int(c["dueTurn"]), Lcars.WARN, 18, 14))
	col.add_child(th)
	var honour_tip := "Honour: we declare war on the %s; +10 trust with the %s.%s" % [c["aggressorName"], c["defenderName"], " Our treaty with the aggressor makes this a betrayal." if c.get("boundByTreaty", false) else ""]
	var refuse_tip := "Refuse: at the deadline the %s lose 20 trust in us and gain 2 grievances; below 40 trust they dissolve the alliance." % c["defenderName"]
	if refused and not c.get("honored", false):
		col.add_child(Pic.num("clock", "%d" % int(c["dueTurn"]), "You will let this call lapse. The consequences apply on turn %d." % int(c["dueTurn"]), Lcars.WARN, 18, 14))
	h.add_child(col)
	var acts := Lcars.vbox(6)
	var aggressor := int(c["aggressor"])
	if not c.get("honored", false):
		if not war.is_empty():
			acts.add_child(Pic.tipped(Lcars.icon("war", 24, Lcars.ALERT), "War declaration ordered"))
			var wid := int(war["id"])
			acts.add_child(Lcars.pill("Refuse instead", "ghost", func(): vm.refuse_instead(c, wid); refresh(), lock))
		elif refused:
			acts.add_child(Lcars.pill("Reconsider", "ghost", func(): vm.set_refused(c, false); refresh()))
		else:
			var hb := Lcars.pill("Honour", "alert", func(): app.confirm("Declare war on the %s?" % c["aggressorName"], DiplomacyVM.honour_body(c), "Declare war", func(): vm.honour(c)), lock if lock != "" else str(c.get("honorBlocker", "")), honour_tip)
			hb.name = "Honor%d" % aggressor
			acts.add_child(hb)
			acts.add_child(Lcars.pill("Refuse", "ghost", func(): vm.set_refused(c, true); refresh(), "", refuse_tip))
	h.add_child(acts)
	return card

# ---------------------------------------------------------------------------------------------
func _event_row(e: Dictionary, both: bool) -> Control:
	var h := Lcars.hbox(10)
	h.add_child(Lcars.label("T%d" % int(e["turn"]), "Mono", 13, Lcars.TEXT3))
	h.add_child(Lcars.badge(str(e["type"]), DiplomacyVM.event_tone(int(e["typeKey"]))))
	var txt := str(e["text"])
	if both:
		txt = "%s → %s · %s" % [e["a"]["name"], e["b"]["name"], txt]
	var l := Lcars.para(txt, Lcars.TEXT2, 13)
	l.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	h.add_child(l)
	return h

func _ledger() -> Control:
	var f := Lcars.frame("Treaty ledger", "", "tertiary", "treaty")
	var b := Lcars.body(f)
	b.add_child(Pic.tipped(Lcars.icon("tick", 22, Lcars.GOOD), "In force"))
	if d["treaties"].is_empty():
		b.add_child(Pic.num("cross", "", "No treaties in force.", Lcars.TEXT3, 20))
	for t in d["treaties"]:
		var tr := Lcars.hbox(10)
		tr.add_child(Lcars.icon("treaty", 20, Lcars.GOOD))
		tr.add_child(Lcars.label("%s · %s" % [t["typeName"], t["counterparty"]["name"]], "", 14))
		tr.add_child(Pic.num("clock", ("%d" % int(t["turnsRemaining"])) if int(t["turnsRemaining"]) >= 0 else "-", "Turns left (- is indefinite). Since turn %d." % int(t["startTurn"]), Lcars.accent2(), 16, 13))
		b.add_child(tr)
	b.add_child(Pic.tipped(Lcars.icon("cross", 22, Lcars.TEXT3), "Ended"))
	if d["treatyHistory"].is_empty():
		b.add_child(Pic.num("cross", "", "No ended treaties yet.", Lcars.TEXT3, 20))
	for t in d["treatyHistory"]:
		var h := Lcars.hbox(8)
		h.add_child(Lcars.label("%s with %s · from turn %d" % [t["typeName"], t["counterparty"]["name"], int(t["startTurn"])], "", 14))
		var s := str(t["status"])
		h.add_child(Lcars.badge("Broken by war" if s == "BrokenByWar" else s, "alert" if s in ["Dissolved", "BrokenByWar"] else "dim"))
		b.add_child(h)
	return f

func _news() -> Control:
	var f := Lcars.frame("Galactic news", "", "tertiary", "events")
	var b := Lcars.body(f)
	f.tooltip_text = "Every empire remembers betrayals, refused calls to arms and wars. These events shape who trusts whom."
	var list := Lcars.vbox(4)
	for e in d["news"]:
		list.add_child(_event_row(e, true))
	if d["news"].is_empty():
		list.add_child(Pic.num("cross", "", "No news yet.", Lcars.TEXT3, 20))
	b.add_child(Lcars.scroll(list))
	return f

# ---------------------------------------------------------------------------------------------
## The hail panel (docs/UI-DESIGN-SYSTEM.md §5): a viewscreen with the race's portrait slot (lane L7's art when
## present, else the emblem card), the name strip and the standing. Hail opens Negotiate.
func _hail(name_: String, tid: int, is_minor: bool, rel: Dictionary, minor: Dictionary) -> Control:
	var p := PanelContainer.new()
	p.name = "HailPanel"
	var sb := Lcars.box(Color(0, 0, 0, 0.6), UiTokens.radius(), Color(Lcars.accent(), 0.5), 2, Vector4(8, 8, 8, 8))
	p.add_theme_stylebox_override("panel", sb)
	var v := Lcars.vbox(6)
	p.add_child(v)
	var key := PortraitSlot.minor_key(name_) if is_minor else ""
	var ps := PortraitSlot.make(0 if is_minor else tid, Vector2(300, 170), key, name_)
	ps.name = "Portrait"
	v.add_child(ps)
	var row := Lcars.hbox(6)
	var st := "Independent"
	var tone := "dim"
	if not rel.is_empty():
		st = "At war" if rel.get("atWar", false) else ("Allied" if rel.get("allied", false) else "At peace")
		tone = "alert" if rel.get("atWar", false) else "good"
	elif not minor.is_empty() and int(minor.get("memberOf", 0)) != 0:
		st = "Member world"
		tone = "primary"
	row.add_child(Lcars.badge(st, tone))
	row.add_child(Lcars.spacer())
	var hail := Lcars.pill("Hail", "secondary", func(): dossier_tab = "propose"; refresh(), "" if not rel.get("eliminated", false) else "No one answers", "Open a channel and negotiate")
	hail.name = "Hail"
	row.add_child(hail)
	v.add_child(row)
	return p

func _dossier() -> Control:
	var v := app.view
	var ds := vm.dossier()
	var is_minor: bool = ds["is_minor"]
	var tid: int = ds["id"]
	var rel: Dictionary = ds["rel"]
	var minor: Dictionary = ds["minor"]
	var name_: String = ds["name"]
	var treaties: Array = ds["treaties"]
	var ended: Array = ds["ended"]
	var events: Array = ds["events"]
	var f := Lcars.frame(name_, "", "primary")
	f.tooltip_text = "Minor race" if is_minor else ("Major power · %s temperament" % rel["personality"] if rel.has("personality") else "Major power")
	f.name = "Dossier"
	var b := Lcars.body(f)
	var tabs := Lcars.hbox(6)
	for t in [["overview", "Overview"], ["propose", "Negotiate"], ["history", "History  %d" % (events.size() + ended.size())]]:
		var id: String = t[0]
		var tg := Lcars.toggle(t[1], dossier_tab == id, func(_on): dossier_tab = id; refresh())
		tg.name = "Dossier_" + id
		tabs.add_child(tg)
	b.add_child(tabs)
	var head := Lcars.hbox(18)
	head.add_child(_hail(name_, tid, is_minor, rel, minor))
	var hm := Lcars.vbox(6)
	hm.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var badges := HFlowContainer.new()
	if not rel.is_empty():
		if rel.get("eliminated", false):
			badges.add_child(Lcars.badge("Eliminated", "dim"))
		elif rel.get("atWar", false):
			badges.add_child(Lcars.badge("At war", "alert"))
		else:
			badges.add_child(Lcars.badge("At peace", "good"))
		if rel.get("allied", false):
			badges.add_child(Lcars.badge("Mutual defence", "good"))
		elif rel.get("sensorSharing", false):
			badges.add_child(Lcars.badge("Shared sensors", "primary"))
		if rel.get("treatyForbidsAttack", false):
			badges.add_child(Lcars.badge("Treaty forbids attack", "secondary"))
		if rel.get("tradeTreaty", false):
			badges.add_child(Lcars.badge("Trade pact", "primary"))
	if not minor.is_empty():
		badges.add_child(Lcars.badge(("Member of " + app.empire_name(int(minor["memberOf"]))) if int(minor.get("memberOf", 0)) != 0 else "Independent", "primary" if int(minor.get("memberOf", 0)) != 0 else "dim"))
		if minor.get("friendship", false):
			badges.add_child(Lcars.badge("Friendship", "good"))
	for t in treaties:
		badges.add_child(Lcars.badge("%s%s" % [t["typeName"], (" · %dT left" % int(t["turnsRemaining"])) if int(t["turnsRemaining"]) >= 0 else ""], "secondary"))
	hm.add_child(badges)
	if not rel.get("eliminated", false):
		var tr := Lcars.hbox(14)
		var val := int(rel.get("trust", minor.get("attitude", 0)))
		var mem: Dictionary = rel.get("memory", {})
		tr.add_child(RingGauge.make(96, val, 100, Lcars.ALERT if rel.get("atWar", false) else (Lcars.GOOD if val >= 50 else Lcars.WARN), str(val), "",
			{"title": "Relations" if is_minor else "Trust", "value": "%d of 100" % val, "parts": [["Their grievances", str(int(mem.get("theirGrievances", 0))), "alert"], ["Our grievances", str(int(mem.get("ourGrievances", 0))), ""], ["Their betrayals", str(int(mem.get("theirBetrayals", 0))), "alert"]] if not is_minor else [],
				"effect": "Gifts, kept treaties and shared enemies raise it; demands, broken treaties and border incursions lower it. Proposals need enough of it to be accepted."}))
		var tl := Lcars.vbox(2)
		tl.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		tl.tooltip_text = "Relations" if is_minor else "Trust"
		tl.mouse_filter = Control.MOUSE_FILTER_PASS
		tl.add_child(Lcars.label("Warm" if val >= 70 else ("Cordial" if val >= 50 else ("Cool" if val >= 30 else "Hostile")), "", 26, Lcars.TEXT))
		tr.add_child(tl)
		hm.add_child(tr)
	head.add_child(hm)
	b.add_child(head)
	var body := Lcars.vbox(8)
	match dossier_tab:
		"propose":
			if rel.get("eliminated", false):
				body.add_child(Lcars.empty_note("The %s has been eliminated and can no longer negotiate." % name_))
			else:
				body.add_child(_negotiate(is_minor, tid, rel))
		"history":
			for t in ended:
				body.add_child(Lcars.label("%s from turn %d · %s" % [t["typeName"], int(t["startTurn"]), t["status"]], "", 14))
			for e in events:
				body.add_child(_event_row(e, false))
			if events.is_empty() and ended.is_empty():
				body.add_child(Lcars.label("No recorded history.", "Muted"))
		_:
			if rel.get("memory") is Dictionary:
				var mem: Dictionary = rel["memory"]
				var g := Lcars.hbox(24)
				var tg1 := int(mem.get("theirGrievances", 0))
				var tb1 := int(mem.get("theirBetrayals", 0))
				g.add_child(Pic.num("threat", "%d" % tg1, "Their grievances: held against us. Each weighs -3 on our proposals and fuels war plans.", Lcars.ALERT if tg1 > 0 else Lcars.TEXT2, 24, 18, Lcars.ALERT if tg1 > 0 else Lcars.TEXT3))
				g.add_child(Pic.num("alert", "%d" % int(mem.get("ourGrievances", 0)), "Our grievances against them", Lcars.TEXT2, 24, 18, Lcars.WARN))
				g.add_child(Pic.num("treaty", "%d" % tb1, "Their betrayals of treaties", Lcars.ALERT if tb1 > 0 else Lcars.TEXT2, 24, 18, Lcars.ALERT if tb1 > 0 else Lcars.TEXT3))
				g.add_child(Pic.num("cross", "%d" % int(mem.get("ourRefusedProposals", 0)), "Proposals of ours they refused", Lcars.TEXT2, 24, 18, Lcars.TEXT3))
				var lw := int(mem.get("lastWarTurn", -1))
				var lp := int(mem.get("lastPeaceTurn", -1))
				g.add_child(Pic.num("war", ("T%d" % lw) if lw >= 0 else "-", "Last war", Lcars.TEXT2, 22, 16, Lcars.ALERT))
				g.add_child(Pic.num("peace", ("T%d" % lp) if lp >= 0 else "-", "Last peace", Lcars.TEXT2, 22, 16, Lcars.GOOD))
				body.add_child(g)
			if minor.get("race") is Dictionary:
				var race: Dictionary = minor["race"]
				body.add_child(Lcars.para(str(race["description"]), Lcars.TEXT2, 14))
				var g2 := Lcars.grid(4)
				g2.add_theme_constant_override("h_separation", 34)
				g2.add_child(Lcars.stat("Homeworld", minor["system"]))
				g2.add_child(Lcars.stat("Temperament", race["temperament"]))
				g2.add_child(Lcars.stat("Join bonus", race["bonus"], "good"))
				if minor.has("techLevel"):
					g2.add_child(Lcars.stat("Tech level", minor["techLevel"]))
				body.add_child(g2)
			if is_minor and not minor.is_empty():
				body.add_child(_minor_standing(minor))
			elif not is_minor and rel.get("tradeTreaty", false):
				var tp := Lcars.pill("Manage trade routes", "secondary", func():
					app.goto("empire")
					var es := app.current_screen()
					if es:
						es.tab = "trade"
						es.refresh(), "", "Link your colonies to theirs; each route pays 5 + a tenth of the smaller population")
				tp.name = "ManageTrade"
				body.add_child(tp)
			var dz := vm.intel_dossier(tid) if not is_minor else {}
			if not dz.is_empty():
				body.add_child(Lcars.para("Intelligence (turn %d): %d known colonies, about ¢%s in reserve." % [int(dz["turn"]), int(dz["knownColonies"]), Lcars.fmt(dz["estimatedCredits"])]))
			elif not is_minor:
				body.add_child(Pic.num("intel", "-", "No intelligence dossier yet - run espionage from the Intel screen.", Lcars.TEXT3, 22, 16, Lcars.TEXT3))
			var props: Array = ds["proposals"]
			for p in props:
				body.add_child(Lcars.label("%s: %s · due turn %d · %s" % ["From them" if p.get("incoming", false) else "From you", proposal_title(p), int(p["dueTurn"]), p["status"]], "", 14))
			for line in vm.offer_lines(tid):
				body.add_child(Lcars.label(line, "", 14))
			for t in treaties:
				var trw := Lcars.hbox(10)
				trw.add_child(Lcars.icon("treaty", 22, Lcars.GOOD))
				trw.add_child(Lcars.label(str(t["typeName"]), "", 14))
				trw.add_child(Pic.num("clock", ("%d" % int(t["turnsRemaining"])) if int(t["turnsRemaining"]) >= 0 else "-", "Turns remaining (- is indefinite). Since turn %d." % int(t["startTurn"]), Lcars.accent2(), 16, 13))
				trw.tooltip_text = str(TREATY_EFFECTS.get(t["type"], ""))
				trw.mouse_filter = Control.MOUSE_FILTER_PASS
				body.add_child(trw)
	b.add_child(Lcars.scroll(body))
	return f

func _negotiate(is_minor: bool, tid: int, rel: Dictionary) -> Control:
	var v := app.view
	var col := Lcars.vbox(10)
	var lock := _lock()
	if vm.modern():
		var box := PanelContainer.new()
		box.theme_type_variation = "Inset"
		box.name = "TreatyBuilder"
		var bv := Lcars.vbox(8)
		box.add_child(bv)
		bv.add_child(Lcars.label("TREATY PROPOSAL", "SubHeading", 20, Lcars.accent2()))
		var treaties := vm.treaty_options(is_minor, tid)
		var types := HFlowContainer.new()
		for t in treaties:
			var ty: String = t["type"]
			var tg := Lcars.toggle(str(t["name"]), tb["type"] == ty, func(_on): tb["type"] = ty; refresh(), ("Eligible" if t.get("ok", false) else "Not eligible: " + str(t["reason"])) + "\n" + TREATY_EFFECTS.get(ty, ""))
			if not t.get("ok", false):
				tg.add_theme_color_override("font_color", Lcars.TEXT3)
			types.add_child(tg)
		bv.add_child(types)
		var dr := Lcars.hbox(4)
		dr.add_child(Lcars.caption("Duration"))
		for x in app.meta.get("durations", []):
			var val := int(x["value"])
			dr.add_child(Lcars.toggle(str(x["name"]), int(tb["duration"]) == val, func(_on): tb["duration"] = val; refresh()))
		bv.add_child(dr)
		var tr := Lcars.hbox(4)
		tr.add_child(Lcars.caption("Tone"))
		for x in app.meta.get("tones", []):
			var k: String = x["key"]
			tr.add_child(Lcars.toggle(str(x["name"]), tb["tone"] == k, func(_on): tb["tone"] = k; refresh()))
		bv.add_child(tr)
		var cr := Lcars.hbox(8)
		for pair in [["offer", "Offer ¢"], ["demand", "Demand ¢"]]:
			cr.add_child(Lcars.caption(pair[1]))
			var sp := SpinBox.new()
			sp.max_value = 100000
			sp.step = 50
			sp.value = int(tb[pair[0]])
			var key: String = pair[0]
			sp.value_changed.connect(func(val): tb[key] = int(val))
			cr.add_child(sp)
		cr.add_child(Lcars.label("Treasury ¢%s" % Lcars.fmt(v["empire"].get("draftCredits", 0)), "Muted"))
		bv.add_child(cr)
		var proposal := vm.proposal_command(is_minor, tid)
		var reason := vm.proposal_block(treaties, proposal)
		var send := Lcars.hbox(8)
		var rl := Lcars.para(reason if reason != "" else "Terms are ready to send.", Lcars.ALERT if reason != "" else Lcars.GOOD, 14)
		rl.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		send.add_child(rl)
		var sb := Lcars.pill("Send proposal", "primary", func(): vm.send_proposal(proposal), reason)
		sb.name = "SendProposal"
		send.add_child(sb)
		bv.add_child(send)
		col.add_child(box)
	col.add_child(Lcars.label("ENVOYS" if is_minor else "STATECRAFT", "SubHeading", 20, Lcars.accent2()))
	var row := Lcars.hbox(8)
	for a in vm.classic_actions(is_minor, tid, rel):
		var act: Dictionary = a
		var label: String = act["label"]
		var p := Lcars.pill(label, act["tone"], func():
			if act["confirm"] != "":
				app.confirm("%s?" % label, act["confirm"], label, func(): vm.order_classic(act))
			else:
				vm.order_classic(act), act["block"], act["hint"])
		p.name = "Dip_" + str(act["command"]["type"])
		row.add_child(p)
	col.add_child(row)
	return col


## Membership and secession in numbers: the rules from MINOR-RACES.md / DIPLOMACY.md section 6, with this minor's values.
func _minor_standing(minor: Dictionary) -> Control:
	var att := int(minor.get("attitude", 0))
	var col := Lcars.vbox(4)
	col.name = "MinorStanding"
	col.add_child(Lcars.caption("Membership"))
	if int(minor.get("memberOf", 0)) == app.player():
		var margin := att - EmpireTabs.SECESSION_LINE
		col.add_child(Lcars.label("Member of your empire. It secedes if its attitude falls below %d (you must keep another colony): now %d, %d point%s of margin." % [EmpireTabs.SECESSION_LINE, att, margin, "" if abs(margin) == 1 else "s"], "", 14, Lcars.GOOD if margin >= 15 else Lcars.ALERT))
		col.add_child(Lcars.para("Each 5 turns: -2 for rising expectations, +2 if its morale is 70+, -2 per extra war you fight. Gifts raise attitude by 4 per 1,000 credits.", Lcars.TEXT3, 12))
	elif int(minor.get("memberOf", 0)) != 0:
		col.add_child(Lcars.label("Member of %s. A gift to it lowers its attitude toward its owner by half the gain." % app.empire_name(int(minor["memberOf"])), "", 14, Lcars.TEXT2))
	else:
		var friend := bool(minor.get("friendship", false))
		var att_ok: bool = att >= EmpireTabs.JOIN_LINE
		col.add_child(Lcars.marked("tick" if att_ok else "cross", "Attitude %d of %d needed" % [att, EmpireTabs.JOIN_LINE], "", 14, Lcars.GOOD if att_ok else Lcars.TEXT2, 14))
		col.add_child(Lcars.marked("tick" if friend else "cross", "Friendship signed in an earlier turn", "", 14, Lcars.GOOD if friend else Lcars.TEXT2, 14))
		col.add_child(Lcars.para("A gift raises attitude by 4 per 1,000 credits. A friendship class treaty adds +2 per review (every 5 turns); rivals with a friendship take -2.", Lcars.TEXT3, 12))
	return col
