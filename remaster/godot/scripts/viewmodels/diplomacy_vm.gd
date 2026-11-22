class_name DiplomacyVM
extends ViewModel
## Diplomacy screen model: the core's `diplomacy` query, the selected race (major or minor), the pane and dossier
## tab, the treaty builder's draft, the calls to arms the player chose to let lapse, and every diplomatic order.

const TREATY_EFFECTS := {
	"NonAggression": "Honourable AIs will not attack while it holds. Trust grows +1 every 4 turns (up to 60).",
	"Friendship": "+3 credits a turn to each party; trust +1 every 2 turns (up to 75).",
	"Affiliation": "+5 credits a turn each; shared star charts and sensors; trust +1 a turn (up to 85).",
	"Alliance": "+8 credits a turn each; shared charts, supply and mutual defence - you may be called to arms; trust +1 a turn.",
	"Peace": "Ends the war; binds honourable AIs for its term.",
	"Trade": "Enables trade routes between your empires (routes pay credits every turn).",
	"Membership": "The minor joins your empire as a member world.",
}

## Major powers that can be hailed without a diplomacy row (the lineup may not include them): the card shows the race name, never "Unknown".
const MAJOR_NAMES := {1: "Federation", 2: "Klingon Empire", 3: "Romulan Star Empire", 4: "Cardassian Union", 5: "Ferengi Alliance", 6: "Dominion", 7: "Borg Collective", 8: "Breen Confederacy"}

## Calls to arms the player refuses, by "defender:aggressor:turn"; kept for the session (static, as before).
static var refused_calls := {}

var d: Dictionary = {}
var target := {}          # {minor: bool, id: int}
var pane := ""
var dossier_tab := "overview"
var tb := {"type": "", "duration": 25, "tone": "Cordial", "offer": 0, "demand": 0, "cede": [], "ask": []}   # E3: cede = our worlds handed over in a peace deal, ask = theirs we ask for

## E3: the most worlds a peace deal can move each way (the core's PeaceMaxWorlds).
const PEACE_MAX_WORLDS := 3

## Names of worlds from a proposal's [{systemId, name}] list, comma separated.
static func world_names(list: Array) -> String:
	var out := []
	for w in list:
		out.append(str(w.get("name", "a world")))
	return ", ".join(out)

static func proposal_title(p: Dictionary) -> String:
	match str(p["kind"]):
		"tribute": return "Tribute demand: ¢%s" % Fmt.fmt(p["creditDemand"])
		"gift": return "Gift of ¢%s" % Fmt.fmt(p["creditOffer"])
		"war": return "Declaration of war"
	return "%s proposal" % p["typeName"]

## [accept text, reject text] for an incoming proposal.
static func consequences(p: Dictionary) -> Array:
	match str(p["kind"]):
		"tribute":
			return ["Pay ¢%s. Paying tribute costs 5 trust and leaves us a grievance against them." % Fmt.fmt(p["creditDemand"]), "They gain 2 grievances against us and trust falls by 3."]
		"gift":
			return ["Receive ¢%s; trust rises." % Fmt.fmt(p["creditOffer"]), "The gift is returned."]
	var effect: String = TREATY_EFFECTS.get(p["type"], "")
	var acc := "%s signed for %s (+5 trust). %s" % [p["typeName"], str(p["duration"]).to_lower(), effect]
	if int(p.get("creditOffer", 0)) > 0:
		acc += " They pay ¢%s." % Fmt.fmt(p["creditOffer"])
	if int(p.get("creditDemand", 0)) > 0:
		acc += " We pay ¢%s." % Fmt.fmt(p["creditDemand"])
	if p.get("cedeSystems", []).size() > 0:   # E3: the proposer hands us worlds
		acc += " We receive %s." % world_names(p["cedeSystems"])
	if p.get("demandSystems", []).size() > 0:   # E3: the proposer asks us for worlds
		acc += " We hand over %s." % world_names(p["demandSystems"])
	return [acc, "No treaty. Repeated refusals make them wait longer before proposing again."]

## [text, tone] for an answer deadline.
static func deadline(turn: int, due: int) -> Array:
	var left := due - turn
	return ["due now (turn %d)" % due if left <= 0 else "answer by turn %d · %d turn%s left" % [due, left, "" if left == 1 else "s"], "alert" if left <= 1 else ("warn" if left <= 2 else "dim")]

static func event_tone(k: int) -> String:
	if k in [6, 8, 12]:
		return "alert"
	if k in [2, 3, 13, 15, 16, 17, 18]:
		return "warn"
	if k in [1, 4, 9, 10, 11, 14]:
		return "good"
	return "dim"

## Relation line under a major power's name.
static func status(e: Dictionary) -> String:
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

## Reloads diplomacy; `focus` (from another screen) selects a race and opens its dossier. False when unavailable.
func fetch(focus: Variant = null) -> bool:
	d = query_dict("diplomacy")
	if d.is_empty():
		return false
	if focus is Dictionary:
		target = focus
		pane = "dossier"
	var has_inbox := has_inbox()
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
	return true

func incoming() -> Array:
	return d["proposals"].filter(func(p): return p.get("incoming", false) and p["status"] == "Pending")

func has_inbox() -> bool:
	return incoming().size() > 0 or d["callsToArms"].size() > 0

func inbox_count() -> int:
	return incoming().size() + d["callsToArms"].filter(func(c): return not c.get("honored", false)).size()

func select(minor: bool, id: int) -> void:
	target = {"minor": minor, "id": id}
	pane = "dossier"

func is_selected(minor: bool, id: int) -> bool:
	return bool(target.get("minor", not minor)) == minor and int(target.get("id", -1)) == id

func contacted() -> Array:
	return d["empires"].filter(func(e): return e.get("contact", false))

func minor_status(mi: Dictionary) -> String:
	var st := ("Member of " + session.empire_name(int(mi["memberOf"]))) if int(mi.get("memberOf", 0)) != 0 else ("Friendship" if mi.get("friendship", false) else "Independent")
	st += " · " + str(mi["system"])
	if mi.get("race") is Dictionary:
		st += " · " + str(mi["race"]["temperament"])
	return st

## The player's reputation and bloc share: {betrayals, last, bloc: {} or {bloc, galaxy, achieved}}.
func standing() -> Dictionary:
	var vic: Variant = d.get("victory")
	return {"betrayals": int(d["standing"].get("betrayals", 0)), "last": int(d["standing"].get("lastBetrayalTurn", 0)),
		"bloc": vic if vic is Dictionary and d.get("modern", false) else {}}

## The drafted answer to a proposal, or {}.
func drafted_response(pid: int) -> Dictionary:
	var out := {}
	for c in view().get("pending", []):
		if c["type"] == "RespondDiplomaticProposal" and int(c.get("proposalId", -1)) == pid:
			out = c
	return out

func respond(pid: int, accept: bool) -> bool:
	return issue({"type": "RespondDiplomaticProposal", "proposalId": pid, "proposalAccept": accept}, [], "Acceptance ordered for turn end." if accept else "Rejection ordered for turn end.")

# ---------------------------------------------------------------------------------------------
# Calls to arms
# ---------------------------------------------------------------------------------------------
static func call_key(c: Dictionary) -> String:
	return "%d:%d:%d" % [int(c["defender"]), int(c["aggressor"]), int(c["issuedTurn"])]

func refused(c: Dictionary) -> bool:
	return refused_calls.has(call_key(c))

func set_refused(c: Dictionary, on: bool) -> void:
	if on:
		refused_calls[call_key(c)] = true
	else:
		refused_calls.erase(call_key(c))

## The drafted war declaration on the call's aggressor, or {}.
func war_order(c: Dictionary) -> Dictionary:
	var war := {}
	for p in view().get("pending", []):
		if p["type"] == "DeclareWar" and int(p.get("targetEmpire", -1)) == int(c["aggressor"]):
			war = p
	return war

static func honour_body(c: Dictionary) -> String:
	return "Honouring the %s's call to arms declares war on the %s at turn end.\n+10 trust with the %s.\n%s" % [c["defenderName"], c["aggressorName"], c["defenderName"],
		("We are bound by a treaty with the %s: declaring war is a betrayal (-15 trust with them, -10 with every other empire, and a permanent mark on our reputation)." % c["aggressorName"]) if c.get("boundByTreaty", false) else "Any trade routes with the %s end and our open proposals with them are cancelled." % c["aggressorName"]]

func honour(c: Dictionary) -> bool:
	if issue({"type": "DeclareWar", "targetEmpire": int(c["aggressor"])}, [], "War declaration ordered: the alliance is honoured."):
		set_refused(c, false)
		return true
	return false

func refuse_instead(c: Dictionary, war_id: int) -> void:
	cancel([war_id])
	set_refused(c, true)

# ---------------------------------------------------------------------------------------------
# Dossier
# ---------------------------------------------------------------------------------------------
## Everything about the selected race: {is_minor, id, rel, minor, name, treaties, ended, events, proposals}.
func dossier() -> Dictionary:
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
	var match_party := func(p: Dictionary) -> bool: return int(p["id"]) == tid and bool(p.get("minor", false)) == is_minor
	return {
		"is_minor": is_minor, "id": tid, "rel": rel, "minor": minor,
		"name": rel.get("name", minor.get("name", MAJOR_NAMES.get(tid, "Unknown") if not is_minor else "Unknown")),
		"treaties": d["treaties"].filter(func(t): return match_party.call(t["counterparty"])),
		"ended": d["treatyHistory"].filter(func(t): return match_party.call(t["counterparty"])),
		"events": d["events"].filter(func(e): return match_party.call(e["a"]) or match_party.call(e["b"])),
		"proposals": d["proposals"].filter(func(p): return match_party.call(p["proposer"] if p.get("incoming", false) else p["target"])),
	}

## The latest espionage dossier on a major power, or {}.
func intel_dossier(tid: int) -> Dictionary:
	var ds: Array = d.get("dossiers", []).filter(func(x): return int(x["target"]) == tid)
	return ds[0] if ds.size() > 0 else {}

## Pending offers the selected race is party to, as lines.
func offer_lines(tid: int) -> Array:
	var out := []
	for o in d.get("majorOffers", []):
		if int(o["proposer"]) == tid or int(o["target"]) == tid:
			out.append("%s %s offer is answered on turn %d." % ["Your" if int(o["proposer"]) == player() else "Their", "peace" if o["type"] == "OfferPeace" else "trade treaty", int(o["dueTurn"])])
	for o in d.get("minorOffers", []):
		if int(o["systemId"]) == tid:
			out.append("Your %s arrives on turn %d." % ["gift" if o["type"] == "SendGift" else ("membership offer" if o["type"] == "OfferMembership" else "friendship offer"), int(o["dueTurn"])])
	return out

# ---------------------------------------------------------------------------------------------
# Negotiation
# ---------------------------------------------------------------------------------------------
func modern() -> bool:
	return d.get("modern", false)

## The core's treaty eligibility for a target: [{type, name, ok, reason}]. Fixes tb.type to an eligible one.
func treaty_options(is_minor: bool, tid: int) -> Array:
	var elig := {}
	for e in d.get("eligibility", []):
		if int(e["target"]) == tid and bool(e.get("minor", false)) == is_minor:
			elig = e
	var treaties: Array = elig.get("treaties", [])
	if tb["type"] == "" or treaties.filter(func(t): return t["type"] == tb["type"]).is_empty():
		tb["type"] = "NonAggression"
		for t in treaties:
			if t.get("ok", false):
				tb["type"] = t["type"]
				break
	return treaties

func proposal_command(is_minor: bool, tid: int) -> Dictionary:
	var proposal := {"type": "SubmitDiplomaticProposal", "proposalTreatyType": tb["type"], "proposalDuration": tb["duration"], "proposalTone": tb["tone"], "proposalCreditOffer": tb["offer"], "proposalCreditDemand": tb["demand"]}
	if is_minor:
		proposal["proposalTargetIsMinor"] = true
		proposal["targetSystemId"] = tid
	else:
		proposal["targetEmpire"] = tid
		if str(tb["type"]) == "Peace":   # E3: worlds in the peace deal
			if tb["cede"].size() > 0:
				proposal["proposalCedeSystems"] = tb["cede"].duplicate()
			if tb["ask"].size() > 0:
				proposal["proposalDemandSystems"] = tb["ask"].duplicate()
	return proposal

## E3: how the war with this power stands for us, -100 (losing) to +100 (winning); 0 when there is no war.
static func war_score(rel: Dictionary) -> int:
	return int(rel.get("warScore", 0)) if rel.get("atWar", false) else 0

static func war_score_text(rel: Dictionary) -> String:
	var ws := war_score(rel)
	var word := "we are winning" if ws >= 25 else ("we are losing" if ws <= -25 else "evenly matched")
	return "War score %+d: %s. A side that is losing may hand over border worlds to end it." % [ws, word]

## E3: the worlds that could change hands: key "ourCedable" (we hand over) or "theirCedable" (we ask for): [{systemId, name, population}].
static func world_choices(rel: Dictionary, key: String) -> Array:
	return rel.get(key, [])

## E3: toggles a world in the peace deal (list "cede" or "ask"), at most PEACE_MAX_WORLDS each; false when the limit stops it.
func toggle_world(list_key: String, system_id: int) -> bool:
	var list: Array = tb[list_key]
	if list.has(system_id):
		list.erase(system_id)
		return true
	if list.size() >= PEACE_MAX_WORLDS:
		return false
	list.append(system_id)
	return true

## E3: drops chosen worlds that are no longer on offer (after a turn, or a change of target).
func prune_worlds(rel: Dictionary) -> void:
	var ours := world_choices(rel, "ourCedable").map(func(w): return int(w["systemId"]))
	var theirs := world_choices(rel, "theirCedable").map(func(w): return int(w["systemId"]))
	tb["cede"] = tb["cede"].filter(func(id): return ours.has(int(id)))
	tb["ask"] = tb["ask"].filter(func(id): return theirs.has(int(id)))

func proposal_block(treaties: Array, proposal: Dictionary) -> String:
	var reason := lock_reason()
	if reason == "":
		for t in treaties:
			if t["type"] == tb["type"] and not t.get("ok", false):
				reason = str(t["reason"])
	if reason == "":
		reason = dry_run(proposal)
	return reason

## Sends the drafted proposal, taking the credit terms as they are now.
func send_proposal(proposal: Dictionary) -> bool:
	var p2 := proposal.duplicate()
	p2["proposalCreditOffer"] = tb["offer"]
	p2["proposalCreditDemand"] = tb["demand"]
	var done := issue(p2, [], "Proposal ordered for turn end.")
	if done:
		tb["cede"] = []
		tb["ask"] = []
	return done

## Classic orders for the target: [{command, label, tone, confirm, hint, block}].
func classic_actions(is_minor: bool, tid: int, rel: Dictionary) -> Array:
	var war_body := ("We are bound by a treaty with the %s. Declaring war now is a betrayal: they lose 15 more trust, every other empire loses 10, and our reputation carries the mark for good." % rel.get("name", "")) if rel.get("treatyForbidsAttack", false) else "War breaks treaties and allows attacks on their ships and colonies. Their allies may be called to arms against us."
	var rows := []
	if is_minor:
		rows = [[{"type": "OfferFriendship", "systemId": tid}, "Offer friendship", "primary", "", "Delivered at turn end; their attitude decides the answer."],
			[{"type": "SendGift", "systemId": tid}, "Send gift", "secondary", "", "Credits are paid at turn end; the gift improves relations."],
			[{"type": "OfferMembership", "systemId": tid}, "Offer membership", "primary", "", "Needs strong relations and a friendship signed in an earlier turn."]]
	else:
		rows = [[{"type": "DeclareWar", "targetEmpire": tid}, "Declare war", "alert", war_body, "Required before attacking."],
			[{"type": "OfferPeace", "targetEmpire": tid}, "Propose peace", "primary", "", "They answer after a delay."],
			[{"type": "ProposeTradeTreaty", "targetEmpire": tid}, "Propose trade pact", "secondary", "", "Enables trade routes; they answer after a delay."]]
	var lock := lock_reason()
	var out := []
	for r in rows:
		out.append({"command": r[0], "label": r[1], "tone": r[2], "confirm": r[3], "hint": r[4], "block": lock if lock != "" else dry_run(r[0])})
	return out

func order_classic(a: Dictionary) -> bool:
	return issue(a["command"], [], "%s: ordered for turn end." % a["label"])
