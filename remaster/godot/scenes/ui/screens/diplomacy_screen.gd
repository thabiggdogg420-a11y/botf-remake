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
	if not vm.load(focus):
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
	var f := Lcars.frame("Known powers", "%d contacted" % (contacted.size() + d["minors"].size()), "primary")
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
		t.add_child(Lcars.label(DiplomacyVM.status(e), "Muted", 12))
		h.add_child(t)
		if e.get("contact", false) and not e.get("eliminated", false):
			h.add_child(Lcars.meter(e.get("trust", 0), 100, Lcars.ALERT if e.get("atWar", false) else Lcars.GOOD, 4, 70))
		if e.get("eliminated", false):
			h.add_child(Lcars.badge("OUT", "dim"))
		elif e.get("atWar", false):
			h.add_child(Lcars.badge("WAR", "alert"))
		elif e.get("allied", false):
			h.add_child(Lcars.badge("ALLY", "good"))
		Lcars.ignore_mouse(h)
		btn.pressed.connect(func(): vm.select(false, id); refresh())
		list.add_child(btn)
	if d["minors"].size() > 0:
		list.add_child(Lcars.caption("Minor races"))
	for mi in d["minors"]:
		var sid := int(mi["systemId"])
		var btn2 := Lcars.row_button(vm.is_selected(true, sid))
		var h2 := Lcars.row_inner(btn2)
		h2.add_child(Lcars.label("◇", "", 22, Color("#d8c3ff")))
		var t2 := Lcars.vbox(-2)
		t2.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		t2.add_child(Lcars.label(str(mi["name"]), "", 15, Lcars.TEXT))
		t2.add_child(Lcars.label(vm.minor_status(mi), "Muted", 12))
		h2.add_child(t2)
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
	rep.add_child(Lcars.caption("Reputation"))
	var standing := vm.standing()
	var betr: int = standing["betrayals"]
	rep.add_child(Lcars.badge(("%d treaty betrayal%s (last turn %d)" % [betr, "" if betr == 1 else "s", int(standing["last"])]) if betr > 0 else "No betrayals", "alert" if betr > 0 else "good"))
	sv.add_child(rep)
	var vic: Dictionary = standing["bloc"]
	if not vic.is_empty():
		var br := Lcars.hbox(8)
		br.add_child(Lcars.caption("Bloc"))
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
	var f := Lcars.frame("Incoming", "%d awaiting your answer" % vm.inbox_count(), "secondary")
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
		col.add_child(Lcars.label(terms, "Muted", 13))
		var cons := consequences(p)
		col.add_child(Lcars.para("Accept: " + cons[0], Lcars.GOOD, 13))
		col.add_child(Lcars.para("Reject: " + cons[1], Lcars.ALERT, 13))
		h.add_child(col)
		var acts := Lcars.vbox(6)
		var drafted := vm.drafted_response(pid)
		if not drafted.is_empty():
			acts.add_child(Lcars.badge("Response ordered", "good"))
			var did := int(drafted["id"])
			acts.add_child(Lcars.pill("Undo", "ghost", func(): app.cancel_orders([did], "Response withdrawn."), lock))
		else:
			var a := Lcars.pill("Accept", "good", func(): vm.respond(pid, true), lock)
			a.name = "Accept%d" % pid
			acts.add_child(a)
			var r := Lcars.pill("Reject", "alert", func(): vm.respond(pid, false), lock)
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
	th.add_child(Lcars.label("%s · call to arms against %s" % [c["defenderName"], c["aggressorName"]], "SubHeading", 19, Lcars.TEXT))
	if c.get("honored", false):
		th.add_child(Lcars.badge("honoured - at war", "good"))
	else:
		th.add_child(Lcars.badge("declare war by turn %d · %d left" % [int(c["dueTurn"]), max(0, int(c["dueTurn"]) - int(d["turn"]))], deadline(int(d["turn"]), int(c["dueTurn"]))[1]))
	col.add_child(th)
	col.add_child(Lcars.para("Honour: we declare war on the %s; +10 trust with the %s.%s" % [c["aggressorName"], c["defenderName"], " Our treaty with the aggressor makes this a betrayal." if c.get("boundByTreaty", false) else ""], Lcars.GOOD, 13))
	col.add_child(Lcars.para("Refuse: at the deadline the %s lose 20 trust in us and gain 2 grievances; below 40 trust they dissolve the alliance." % c["defenderName"], Lcars.ALERT, 13))
	if refused and not c.get("honored", false):
		col.add_child(Lcars.para("You will let this call lapse. The consequences apply on turn %d." % int(c["dueTurn"]), Lcars.WARN, 13))
	h.add_child(col)
	var acts := Lcars.vbox(6)
	var aggressor := int(c["aggressor"])
	if not c.get("honored", false):
		if not war.is_empty():
			acts.add_child(Lcars.badge("War declaration ordered", "alert"))
			var wid := int(war["id"])
			acts.add_child(Lcars.pill("Refuse instead", "ghost", func(): vm.refuse_instead(c, wid); refresh(), lock))
		elif refused:
			acts.add_child(Lcars.pill("Reconsider", "ghost", func(): vm.set_refused(c, false); refresh()))
		else:
			var hb := Lcars.pill("Honour", "alert", func(): app.confirm("Declare war on the %s?" % c["aggressorName"], DiplomacyVM.honour_body(c), "Declare war", func(): vm.honour(c)), lock if lock != "" else str(c.get("honorBlocker", "")))
			hb.name = "Honor%d" % aggressor
			acts.add_child(hb)
			acts.add_child(Lcars.pill("Refuse", "ghost", func(): vm.set_refused(c, true); refresh()))
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
	var f := Lcars.frame("Treaty ledger", "in force and ended", "tertiary")
	var b := Lcars.body(f)
	b.add_child(Lcars.caption("In force"))
	if d["treaties"].is_empty():
		b.add_child(Lcars.label("No treaties in force.", "Muted"))
	for t in d["treaties"]:
		b.add_child(Lcars.para("%s with %s · since turn %d · %s" % [t["typeName"], t["counterparty"]["name"], int(t["startTurn"]), ("%d turns left" % int(t["turnsRemaining"])) if int(t["turnsRemaining"]) >= 0 else "indefinite"], Lcars.TEXT, 14))
	b.add_child(Lcars.caption("Ended"))
	if d["treatyHistory"].is_empty():
		b.add_child(Lcars.label("No ended treaties yet.", "Muted"))
	for t in d["treatyHistory"]:
		var h := Lcars.hbox(8)
		h.add_child(Lcars.label("%s with %s · from turn %d" % [t["typeName"], t["counterparty"]["name"], int(t["startTurn"])], "", 14))
		var s := str(t["status"])
		h.add_child(Lcars.badge("Broken by war" if s == "BrokenByWar" else s, "alert" if s in ["Dissolved", "BrokenByWar"] else "dim"))
		b.add_child(h)
	return f

func _news() -> Control:
	var f := Lcars.frame("Galactic news", "between the powers we know", "tertiary")
	var b := Lcars.body(f)
	b.add_child(Lcars.para("Every empire remembers betrayals, refused calls to arms and wars. These events shape who trusts whom.", Lcars.TEXT3, 13))
	var list := Lcars.vbox(4)
	for e in d["news"]:
		list.add_child(_event_row(e, true))
	if d["news"].is_empty():
		list.add_child(Lcars.label("No news yet.", "Muted"))
	b.add_child(Lcars.scroll(list))
	return f

# ---------------------------------------------------------------------------------------------
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
	var f := Lcars.frame(name_, "Minor race" if is_minor else ("Major power · %s temperament" % rel["personality"] if rel.has("personality") else "Major power"), "primary")
	f.name = "Dossier"
	var b := Lcars.body(f)
	var tabs := Lcars.hbox(6)
	for t in [["overview", "Overview"], ["propose", "Negotiate"], ["history", "History  %d" % (events.size() + ended.size())]]:
		var id: String = t[0]
		var tg := Lcars.toggle(t[1], dossier_tab == id, func(_on): dossier_tab = id; refresh())
		tg.name = "Dossier_" + id
		tabs.add_child(tg)
	b.add_child(tabs)
	var head := Lcars.hbox(16)
	if is_minor:
		head.add_child(Lcars.label("◇", "", 56, Color("#d8c3ff")))
	else:
		head.add_child(Lcars.emblem(tid, 64))
	var hm := Lcars.vbox(6)
	hm.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var badges := HFlowContainer.new()
	if not rel.is_empty():
		if rel.get("eliminated", false):
			badges.add_child(Lcars.badge("Eliminated - no colonies remain", "dim"))
		elif rel.get("atWar", false):
			badges.add_child(Lcars.badge("At war since turn %d" % int(rel.get("warStartTurn", 0)), "alert"))
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
		var tr := Lcars.hbox(8)
		tr.add_child(Lcars.caption("Relations" if is_minor else "Trust"))
		var val := int(rel.get("trust", minor.get("attitude", 0)))
		tr.add_child(Lcars.meter(val, 100, Lcars.ALERT if rel.get("atWar", false) else Lcars.GOOD, 8))
		tr.add_child(Lcars.label(str(val), "Mono", 16))
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
				var g := Lcars.grid(5)
				g.add_theme_constant_override("h_separation", 34)
				g.add_child(Lcars.stat("Their grievances", mem.get("theirGrievances", 0), "alert" if int(mem.get("theirGrievances", 0)) > 0 else "", "Grievances they hold against us. Each weighs -3 on our proposals and fuels war plans."))
				g.add_child(Lcars.stat("Our grievances", mem.get("ourGrievances", 0)))
				g.add_child(Lcars.stat("Their betrayals", mem.get("theirBetrayals", 0), "alert" if int(mem.get("theirBetrayals", 0)) > 0 else ""))
				g.add_child(Lcars.stat("Refused proposals", mem.get("ourRefusedProposals", 0)))
				var lw := int(mem.get("lastWarTurn", -1))
				var lp := int(mem.get("lastPeaceTurn", -1))
				g.add_child(Lcars.stat("Last war / peace", "%s / %s" % [("T%d" % lw) if lw >= 0 else "-", ("T%d" % lp) if lp >= 0 else "-"]))
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
				body.add_child(Lcars.para("No intelligence dossier yet - run espionage from the Intel screen.", Lcars.TEXT3, 13))
			body.add_child(Lcars.caption("Proposals"))
			var props: Array = ds["proposals"]
			if props.is_empty():
				body.add_child(Lcars.label("No proposals in progress.", "Muted"))
			for p in props:
				body.add_child(Lcars.label("%s: %s · due turn %d · %s" % ["From them" if p.get("incoming", false) else "From you", proposal_title(p), int(p["dueTurn"]), p["status"]], "", 14))
			for line in vm.offer_lines(tid):
				body.add_child(Lcars.label(line, "", 14))
			body.add_child(Lcars.caption("Treaties"))
			if treaties.is_empty():
				body.add_child(Lcars.label("No treaties in force.", "Muted"))
			for t in treaties:
				body.add_child(Lcars.para("%s since turn %d · %s - %s" % [t["typeName"], int(t["startTurn"]), ("%d turns remaining" % int(t["turnsRemaining"])) if int(t["turnsRemaining"]) >= 0 else "indefinite", TREATY_EFFECTS.get(t["type"], "")], Lcars.TEXT2, 13))
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
		col.add_child(Lcars.label("%s Attitude %d of %d needed" % ["✓" if att >= EmpireTabs.JOIN_LINE else "✗", att, EmpireTabs.JOIN_LINE], "", 14, Lcars.GOOD if att >= EmpireTabs.JOIN_LINE else Lcars.TEXT2))
		col.add_child(Lcars.label("%s Friendship signed in an earlier turn" % ("✓" if friend else "✗"), "", 14, Lcars.GOOD if friend else Lcars.TEXT2))
		col.add_child(Lcars.para("A gift raises attitude by 4 per 1,000 credits. A friendship class treaty adds +2 per review (every 5 turns); rivals with a friendship take -2.", Lcars.TEXT3, 12))
	return col
