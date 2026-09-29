extends Control
## Diplomacy: known powers and minors, incoming proposals and calls to arms, dossiers with
## relation memory, the treaty builder and statecraft, treaty ledger and galactic news.
## Port of screens/Diplomacy.tsx.

var app: App
var d: Dictionary = {}
var target := {}          # {minor: bool, id: int}
var pane := ""
var dossier_tab := "overview"
var tb := {"type": "", "duration": 25, "tone": "Cordial", "offer": 0, "demand": 0}
static var refused_calls := {}

const TREATY_EFFECTS := {
	"NonAggression": "Honourable AIs will not attack while it holds. Trust grows +1 every 4 turns (up to 60).",
	"Friendship": "+3 credits a turn to each party; trust +1 every 2 turns (up to 75).",
	"Affiliation": "+5 credits a turn each; shared star charts and sensors; trust +1 a turn (up to 85).",
	"Alliance": "+8 credits a turn each; shared charts, supply and mutual defence - you may be called to arms; trust +1 a turn.",
	"Peace": "Ends the war; binds honourable AIs for its term.",
	"Trade": "Enables trade routes between your empires (routes pay credits every turn).",
	"Membership": "The minor joins your empire as a member world.",
}

func _lock() -> String:
	return "Finish the battle in progress first" if app.view.get("session", {}).get("active", false) else ""

static func proposal_title(p: Dictionary) -> String:
	match str(p["kind"]):
		"tribute": return "Tribute demand: ¢%s" % Lcars.fmt(p["creditDemand"])
		"gift": return "Gift of ¢%s" % Lcars.fmt(p["creditOffer"])
		"war": return "Declaration of war"
	return "%s proposal" % p["typeName"]

static func consequences(p: Dictionary) -> Array:
	match str(p["kind"]):
		"tribute":
			return ["Pay ¢%s. Paying tribute costs 5 trust and leaves us a grievance against them." % Lcars.fmt(p["creditDemand"]), "They gain 2 grievances against us and trust falls by 3."]
		"gift":
			return ["Receive ¢%s; trust rises." % Lcars.fmt(p["creditOffer"]), "The gift is returned."]
	var effect: String = TREATY_EFFECTS.get(p["type"], "")
	var acc := "%s signed for %s (+5 trust). %s" % [p["typeName"], str(p["duration"]).to_lower(), effect]
	if int(p.get("creditOffer", 0)) > 0:
		acc += " They pay ¢%s." % Lcars.fmt(p["creditOffer"])
	if int(p.get("creditDemand", 0)) > 0:
		acc += " We pay ¢%s." % Lcars.fmt(p["creditDemand"])
	return [acc, "No treaty. Repeated refusals make them wait longer before proposing again."]

static func deadline(turn: int, due: int) -> Array:
	var left := due - turn
	return ["due now (turn %d)" % due if left <= 0 else "answer by turn %d · %d turn%s left" % [due, left, "" if left == 1 else "s"], "alert" if left <= 1 else ("warn" if left <= 2 else "dim")]

func refresh() -> void:
	for c in get_children():
		c.queue_free()
	var got: Variant = app.core.request("diplomacy")
	d = got if got is Dictionary else {}
	if d.is_empty():
		add_child(Lcars.empty_note("Diplomacy unavailable."))
		return
	if app.has_meta("diplomacy_focus"):
		target = app.get_meta("diplomacy_focus")
		app.remove_meta("diplomacy_focus")
		pane = "dossier"
	var incoming: Array = d["proposals"].filter(func(p): return p.get("incoming", false) and p["status"] == "Pending")
	var has_inbox: bool = incoming.size() > 0 or d["callsToArms"].size() > 0
	if pane == "":
		pane = "inbox" if has_inbox else "dossier"
	if pane == "inbox" and not has_inbox:
		pane = "dossier"
	if target.is_empty():
		for e in d["empires"]:
			if e.get("contact", false) and not e.get("eliminated", false):
				target = {"minor": false, "id": int(e["empire"])}
				break
		if target.is_empty() and d["minors"].size() > 0:
			target = {"minor": true, "id": int(d["minors"][0]["systemId"])}
	var m := MarginContainer.new()
	m.set_anchors_preset(Control.PRESET_FULL_RECT)
	for side in ["left", "right", "top", "bottom"]:
		m.add_theme_constant_override("margin_" + side, 16)
	add_child(m)
	var row := Lcars.hbox(14)
	m.add_child(row)
	row.add_child(_race_list())
	var main := Lcars.vbox(10)
	main.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(main)
	var tabs := Lcars.hbox(6)
	if has_inbox:
		var n: int = incoming.size() + d["callsToArms"].filter(func(c): return not c.get("honored", false)).size()
		tabs.add_child(_pane_tab("inbox", "Incoming  %d" % n))
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
	var contacted: Array = d["empires"].filter(func(e): return e.get("contact", false))
	var f := Lcars.frame("Known powers", "%d contacted" % (contacted.size() + d["minors"].size()), "primary")
	f.custom_minimum_size.x = 440
	var b := Lcars.body(f)
	var list := Lcars.vbox(4)
	for e in d["empires"]:
		var id := int(e["empire"])
		var active: bool = not target.get("minor", true) and int(target.get("id", -1)) == id
		var btn := _row_button(active)
		btn.name = "Race%d" % id
		btn.disabled = not e.get("contact", false)
		var h := _row_inner(btn)
		h.add_child(Lcars.emblem(id, 30, Lcars.owner_color(id) if e.get("contact", false) else Lcars.TEXT3))
		var t := Lcars.vbox(-2)
		t.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		t.add_child(Lcars.label(str(e["name"]), "", 15, Lcars.TEXT if e.get("contact", false) else Lcars.TEXT3))
		t.add_child(Lcars.label(_status(e), "Muted", 12))
		h.add_child(t)
		if e.get("contact", false) and not e.get("eliminated", false):
			h.add_child(Lcars.meter(e.get("trust", 0), 100, Lcars.ALERT if e.get("atWar", false) else Lcars.GOOD, 4, 70))
		if e.get("eliminated", false):
			h.add_child(Lcars.badge("OUT", "dim"))
		elif e.get("atWar", false):
			h.add_child(Lcars.badge("WAR", "alert"))
		elif e.get("allied", false):
			h.add_child(Lcars.badge("ALLY", "good"))
		_ignore_mouse(h)
		btn.pressed.connect(func(): target = {"minor": false, "id": id}; pane = "dossier"; refresh())
		list.add_child(btn)
	if d["minors"].size() > 0:
		list.add_child(Lcars.caption("Minor races"))
	for mi in d["minors"]:
		var sid := int(mi["systemId"])
		var btn2 := _row_button(target.get("minor", false) and int(target.get("id", -1)) == sid)
		var h2 := _row_inner(btn2)
		h2.add_child(Lcars.label("◇", "", 22, Color("#d8c3ff")))
		var t2 := Lcars.vbox(-2)
		t2.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		t2.add_child(Lcars.label(str(mi["name"]), "", 15, Lcars.TEXT))
		var st := ("Member of " + app.empire_name(int(mi["memberOf"]))) if int(mi.get("memberOf", 0)) != 0 else ("Friendship" if mi.get("friendship", false) else "Independent")
		st += " · " + str(mi["system"])
		if mi.get("race") is Dictionary:
			st += " · " + str(mi["race"]["temperament"])
		t2.add_child(Lcars.label(st, "Muted", 12))
		h2.add_child(t2)
		h2.add_child(Lcars.meter(mi["attitude"], 100, Lcars.GOOD, 4, 70))
		_ignore_mouse(h2)
		btn2.pressed.connect(func(): target = {"minor": true, "id": sid}; pane = "dossier"; refresh())
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
	var betr := int(d["standing"].get("betrayals", 0))
	rep.add_child(Lcars.badge(("%d treaty betrayal%s (last turn %d)" % [betr, "" if betr == 1 else "s", int(d["standing"]["lastBetrayalTurn"])]) if betr > 0 else "No betrayals", "alert" if betr > 0 else "good"))
	sv.add_child(rep)
	var vic: Variant = d.get("victory")
	if vic is Dictionary and d.get("modern", false):
		var br := Lcars.hbox(8)
		br.add_child(Lcars.caption("Bloc"))
		br.add_child(Lcars.meter(vic["bloc"], max(1, int(vic["galaxy"])), Lcars.GOOD if vic.get("achieved", false) else Lcars.accent(), 6))
		br.add_child(Lcars.label("%d%%" % int(round(100.0 * float(vic["bloc"]) / max(1, int(vic["galaxy"])))), "Mono"))
		br.tooltip_text = "Your bloc holds %d of %d population. A strict majority with at least one ally or member minor is a diplomatic victory." % [int(vic["bloc"]), int(vic["galaxy"])]
		br.mouse_filter = Control.MOUSE_FILTER_PASS
		sv.add_child(br)
	b.add_child(st2)
	return f

func _row_button(active: bool) -> Button:
	var btn := Button.new()
	btn.theme_type_variation = "GhostButton"
	btn.custom_minimum_size.y = 52
	if active:
		btn.add_theme_stylebox_override("normal", Lcars.box(Color(Lcars.accent(), 0.2), 12, Lcars.accent(), 1))
	return btn

func _row_inner(btn: Button) -> HBoxContainer:
	var h := Lcars.hbox(10)
	h.set_anchors_preset(Control.PRESET_FULL_RECT)
	h.offset_left = 10
	h.offset_right = -10
	btn.add_child(h)
	return h

func _ignore_mouse(n: Control) -> void:
	n.mouse_filter = Control.MOUSE_FILTER_IGNORE
	for c in n.find_children("*", "Control", true, false):
		c.mouse_filter = Control.MOUSE_FILTER_IGNORE

func _status(e: Dictionary) -> String:
	if not e.get("contact", false):
		return "No contact"
	if e.get("eliminated", false):
		return "Eliminated"
	var parts := ["At war" if e.get("atWar", false) else ("Full alliance" if e.get("allied", false) else ("Affiliated" if e.get("sensorSharing", false) else ("Treaty in force" if e.get("treatyForbidsAttack", false) else "Peace")))]
	if e.get("tradeTreaty", false):
		parts.append("trade pact")
	var bt := int(e.get("memory", {}).get("theirBetrayals", 0)) if e.get("memory") is Dictionary else 0
	if bt > 0:
		parts.append("%d betrayal%s" % [bt, "" if bt == 1 else "s"])
	return " · ".join(parts)

# ---------------------------------------------------------------------------------------------
func _inbox(incoming: Array) -> Control:
	var v := app.view
	var n: int = incoming.size() + d["callsToArms"].filter(func(c): return not c.get("honored", false)).size()
	var f := Lcars.frame("Incoming", "%d awaiting your answer" % n, "secondary")
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
		var drafted := {}
		for c in v.get("pending", []):
			if c["type"] == "RespondDiplomaticProposal" and int(c.get("proposalId", -1)) == pid:
				drafted = c
		if not drafted.is_empty():
			acts.add_child(Lcars.badge("Response ordered", "good"))
			var did := int(drafted["id"])
			acts.add_child(Lcars.pill("Undo", "ghost", func(): app.cancel_orders([did], "Response withdrawn."), lock))
		else:
			var a := Lcars.pill("Accept", "good", func(): app.issue({"type": "RespondDiplomaticProposal", "proposalId": pid, "proposalAccept": true}, [], "Acceptance ordered for turn end."), lock)
			a.name = "Accept%d" % pid
			acts.add_child(a)
			var r := Lcars.pill("Reject", "alert", func(): app.issue({"type": "RespondDiplomaticProposal", "proposalId": pid, "proposalAccept": false}, [], "Rejection ordered for turn end."), lock)
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
	var key := "%d:%d:%d" % [int(c["defender"]), int(c["aggressor"]), int(c["issuedTurn"])]
	var refused := refused_calls.has(key)
	var war := {}
	for p in v.get("pending", []):
		if p["type"] == "DeclareWar" and int(p.get("targetEmpire", -1)) == int(c["aggressor"]):
			war = p
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
			acts.add_child(Lcars.pill("Refuse instead", "ghost", func(): app.cancel_orders([wid]); refused_calls[key] = true; refresh(), lock))
		elif refused:
			acts.add_child(Lcars.pill("Reconsider", "ghost", func(): refused_calls.erase(key); refresh()))
		else:
			var body := "Honouring the %s's call to arms declares war on the %s at turn end.\n+10 trust with the %s.\n%s" % [c["defenderName"], c["aggressorName"], c["defenderName"],
				("We are bound by a treaty with the %s: declaring war is a betrayal (-15 trust with them, -10 with every other empire, and a permanent mark on our reputation)." % c["aggressorName"]) if c.get("boundByTreaty", false) else "Any trade routes with the %s end and our open proposals with them are cancelled." % c["aggressorName"]]
			var hb := Lcars.pill("Honour", "alert", func(): app.confirm("Declare war on the %s?" % c["aggressorName"], body, "Declare war", func():
				if app.issue({"type": "DeclareWar", "targetEmpire": aggressor}, [], "War declaration ordered: the alliance is honoured."):
					refused_calls.erase(key)), lock if lock != "" else str(c.get("honorBlocker", "")))
			hb.name = "Honor%d" % aggressor
			acts.add_child(hb)
			acts.add_child(Lcars.pill("Refuse", "ghost", func(): refused_calls[key] = true; refresh()))
	h.add_child(acts)
	return card

# ---------------------------------------------------------------------------------------------
func _event_tone(k: int) -> String:
	if k in [6, 8, 12]:
		return "alert"
	if k in [2, 3, 13, 15, 16, 17, 18]:
		return "warn"
	if k in [1, 4, 9, 10, 11, 14]:
		return "good"
	return "dim"

func _event_row(e: Dictionary, both: bool) -> Control:
	var h := Lcars.hbox(10)
	h.add_child(Lcars.label("T%d" % int(e["turn"]), "Mono", 13, Lcars.TEXT3))
	h.add_child(Lcars.badge(str(e["type"]), _event_tone(int(e["typeKey"]))))
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
	var is_minor: bool = target["minor"]
	var tid := int(target["id"])
	var rel := {}
	var minor := {}
	if is_minor:
		for mi in d["minors"]:
			if int(mi["systemId"]) == tid:
				minor = mi
	else:
		for e in d["empires"]:
			if int(e["empire"]) == tid:
				rel = e
	var name_: String = rel.get("name", minor.get("name", "Unknown"))
	var match_party := func(p: Dictionary) -> bool: return int(p["id"]) == tid and bool(p.get("minor", false)) == is_minor
	var treaties: Array = d["treaties"].filter(func(t): return match_party.call(t["counterparty"]))
	var ended: Array = d["treatyHistory"].filter(func(t): return match_party.call(t["counterparty"]))
	var events: Array = d["events"].filter(func(e): return match_party.call(e["a"]) or match_party.call(e["b"]))
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
			var dossiers: Array = d.get("dossiers", []).filter(func(x): return int(x["target"]) == tid) if not is_minor else []
			if dossiers.size() > 0:
				body.add_child(Lcars.para("Intelligence (turn %d): %d known colonies, about ¢%s in reserve." % [int(dossiers[0]["turn"]), int(dossiers[0]["knownColonies"]), Lcars.fmt(dossiers[0]["estimatedCredits"])]))
			elif not is_minor:
				body.add_child(Lcars.para("No intelligence dossier yet - run espionage from the Intel screen.", Lcars.TEXT3, 13))
			body.add_child(Lcars.caption("Proposals"))
			var props: Array = d["proposals"].filter(func(p): return match_party.call(p["proposer"] if p.get("incoming", false) else p["target"]))
			if props.is_empty():
				body.add_child(Lcars.label("No proposals in progress.", "Muted"))
			for p in props:
				body.add_child(Lcars.label("%s: %s · due turn %d · %s" % ["From them" if p.get("incoming", false) else "From you", proposal_title(p), int(p["dueTurn"]), p["status"]], "", 14))
			for o in d.get("majorOffers", []):
				if int(o["proposer"]) == tid or int(o["target"]) == tid:
					body.add_child(Lcars.label("%s %s offer is answered on turn %d." % ["Your" if int(o["proposer"]) == app.player() else "Their", "peace" if o["type"] == "OfferPeace" else "trade treaty", int(o["dueTurn"])], "", 14))
			for o in d.get("minorOffers", []):
				if int(o["systemId"]) == tid:
					body.add_child(Lcars.label("Your %s arrives on turn %d." % ["gift" if o["type"] == "SendGift" else ("membership offer" if o["type"] == "OfferMembership" else "friendship offer"), int(o["dueTurn"])], "", 14))
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
	var elig := {}
	for e in d.get("eligibility", []):
		if int(e["target"]) == tid and bool(e.get("minor", false)) == is_minor:
			elig = e
	if d.get("modern", false):
		var box := PanelContainer.new()
		box.theme_type_variation = "Inset"
		box.name = "TreatyBuilder"
		var bv := Lcars.vbox(8)
		box.add_child(bv)
		bv.add_child(Lcars.label("TREATY PROPOSAL", "SubHeading", 20, Lcars.accent2()))
		var treaties: Array = elig.get("treaties", [])
		if tb["type"] == "" or treaties.filter(func(t): return t["type"] == tb["type"]).is_empty():
			tb["type"] = "NonAggression"
			for t in treaties:
				if t.get("ok", false):
					tb["type"] = t["type"]
					break
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
		var proposal := {"type": "SubmitDiplomaticProposal", "proposalTreatyType": tb["type"], "proposalDuration": tb["duration"], "proposalTone": tb["tone"], "proposalCreditOffer": tb["offer"], "proposalCreditDemand": tb["demand"]}
		if is_minor:
			proposal["proposalTargetIsMinor"] = true
			proposal["targetSystemId"] = tid
		else:
			proposal["targetEmpire"] = tid
		var reason := lock
		if reason == "":
			for t in treaties:
				if t["type"] == tb["type"] and not t.get("ok", false):
					reason = str(t["reason"])
		if reason == "":
			reason = app.dry_run(proposal)
		var send := Lcars.hbox(8)
		var rl := Lcars.para(reason if reason != "" else "Terms are ready to send.", Lcars.ALERT if reason != "" else Lcars.GOOD, 14)
		rl.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		send.add_child(rl)
		var sb := Lcars.pill("Send proposal", "primary", func():
			var p2 := proposal.duplicate()
			p2["proposalCreditOffer"] = tb["offer"]
			p2["proposalCreditDemand"] = tb["demand"]
			app.issue(p2, [], "Proposal ordered for turn end."), reason)
		sb.name = "SendProposal"
		send.add_child(sb)
		bv.add_child(send)
		col.add_child(box)
	col.add_child(Lcars.label("ENVOYS" if is_minor else "STATECRAFT", "SubHeading", 20, Lcars.accent2()))
	var war_body := ("We are bound by a treaty with the %s. Declaring war now is a betrayal: they lose 15 more trust, every other empire loses 10, and our reputation carries the mark for good." % rel.get("name", "")) if rel.get("treatyForbidsAttack", false) else "War breaks treaties and allows attacks on their ships and colonies. Their allies may be called to arms against us."
	var classic := []
	if is_minor:
		classic = [[{"type": "OfferFriendship", "systemId": tid}, "Offer friendship", "primary", "", "Delivered at turn end; their attitude decides the answer."],
			[{"type": "SendGift", "systemId": tid}, "Send gift", "secondary", "", "Credits are paid at turn end; the gift improves relations."],
			[{"type": "OfferMembership", "systemId": tid}, "Offer membership", "primary", "", "Needs strong relations and a friendship signed in an earlier turn."]]
	else:
		classic = [[{"type": "DeclareWar", "targetEmpire": tid}, "Declare war", "alert", war_body, "Required before attacking."],
			[{"type": "OfferPeace", "targetEmpire": tid}, "Propose peace", "primary", "", "They answer after a delay."],
			[{"type": "ProposeTradeTreaty", "targetEmpire": tid}, "Propose trade pact", "secondary", "", "Enables trade routes; they answer after a delay."]]
	var row := Lcars.hbox(8)
	for c in classic:
		var cmd: Dictionary = c[0]
		var label: String = c[1]
		var confirm_body: String = c[3]
		var why := lock if lock != "" else app.dry_run(cmd)
		var p := Lcars.pill(label, c[2], func():
			if confirm_body != "":
				app.confirm("%s?" % label, confirm_body, label, func(): app.issue(cmd, [], "%s: ordered for turn end." % label))
			else:
				app.issue(cmd, [], "%s: ordered for turn end." % label), why, c[4])
		p.name = "Dip_" + str(cmd["type"])
		row.add_child(p)
	col.add_child(row)
	return col
