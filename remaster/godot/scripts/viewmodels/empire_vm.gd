class_name EmpireVM
extends ViewModel
## Empire screen model: the overview totals, member-world bonuses, the tab state, and the data and orders of its
## managers: trade routes, governors, build templates, stations and minor worlds.

const TABS := [["colonies", "Colonies"], ["fleets", "Fleets"], ["trade", "Trade"], ["governors", "Governors"], ["templates", "Templates"], ["stations", "Stations"], ["members", "Member worlds"]]
const SECESSION_LINE := 30
const JOIN_LINE := 80
const LOYALTY := {
	"Warlike": "Loyalty rises while you are at war.",
	"Peaceful": "Loyalty falls while you are at war.",
	"Scientific": "Loyalty rises while your Computers level is at least its tech level.",
	"Mercantile": "Loyalty rises while your treasury holds ¢1000 or more.",
	"Spiritual": "Loyalty rises while its colony's morale is 70 or more.",
	"Xenophobic": "Loyalty falls steadily.",
}

var tab := "colonies"
## Manager state kept across rebuilds (template_source).
var tab_state := {}

# ---------------------------------------------------------------------------------------------
# Overview
# ---------------------------------------------------------------------------------------------
## Sums over the colonies: {food (balance), industry, research, pop, cap}.
func totals() -> Dictionary:
	var tot := {"food": 0, "industry": 0, "research": 0, "net": 0, "pop": 0, "cap": 0}
	for c in view()["colonies"]:
		tot["food"] += int(c["economy"]["food"]) - int(c["population"])
		tot["industry"] += int(c["economy"]["industry"])
		tot["research"] += int(c["economy"]["research"])
		tot["pop"] += int(c["population"])
		tot["cap"] += int(c["capacity"])
	return tot

func subtitle() -> String:
	var v := view()
	var contacts: int = v["empires"].filter(func(r): return r.get("contact", false)).size()
	return "Turn %d · %d colonies · %d ships · %d contacts" % [int(v["meta"]["turn"]), v["colonies"].size(), v["fleets"].size(), contacts]

func my_routes() -> Array:
	return view().get("tradeRoutes", []).filter(func(r): return int(r["owner"]) == player())

## Tab buttons with counts: [[id, label]].
func tab_labels() -> Array:
	var out := []
	for tb in TABS:
		var label: String = tb[1]
		match tb[0]:
			"colonies": label += "  %d" % view()["colonies"].size()
			"fleets": label += "  %d" % view()["fleets"].size()
			"trade": label += "  %d" % my_routes().size()
		out.append([tb[0], label])
	return out

func tab_title() -> String:
	for tb in TABS:
		if tb[0] == tab:
			return tb[1]
	return "Colonies"

## Member-world bonus line: {members: int, parts: [String], minors: [{name, bonus}]}.
func member_bonuses() -> Dictionary:
	var b: Dictionary = view()["empire"].get("memberBonuses", {}) if view()["empire"].get("memberBonuses") is Dictionary else {}
	var parts := []
	for pair in [["research", "% research"], ["industry", "% industry"], ["food", "% food"], ["credits", "% credit income"], ["morale", " morale"], ["groundDefense", "% ground defense"]]:
		if int(b.get(pair[0], 0)) != 0:
			parts.append("+%d%s" % [int(b[pair[0]]), pair[1]])
	return {"members": int(b.get("members", 0)), "parts": parts, "minors": b.get("minors", [])}

## Own stations: {starbases, outposts}.
func station_counts() -> Dictionary:
	var sb := 0
	var op := 0
	for s in view().get("stations", []):
		if s.get("mine", false):
			if s["type"] == "Starbase":
				sb += 1
			else:
				op += 1
	return {"starbases": sb, "outposts": op}

# ---------------------------------------------------------------------------------------------
# Trade routes (core query tradeRoutes)
# ---------------------------------------------------------------------------------------------
var trade: Dictionary = {}
var trade_source := 0
var trade_partner := 0

## Loads the routes; false when the core has none to give (error in session.core.last_error).
func load_trade() -> bool:
	var r: Variant = query("tradeRoutes")
	if not r is Dictionary:
		return false
	trade = r
	return true

## {active, total} over the routes.
func trade_summary() -> Dictionary:
	var active := 0
	var total := 0
	for rt in trade["routes"]:
		if rt.get("active", false):
			active += 1
			total += int(rt["value"])
	return {"active": active, "total": total}

## The empire a route cancel names: its partner, else the first contacted empire.
func cancel_target(partner_id: int) -> int:
	if partner_id != 0:
		return partner_id
	for e in view()["empires"]:
		if e.get("contact", false):
			return int(e["empire"])
	return 0

func cancel_route(from_id: int, partner_id: int) -> bool:
	return issue({"type": "CancelTradeRoute", "systemId": from_id, "targetEmpire": cancel_target(partner_id)}, [], "Trade route cancelled.")

## The establish panel: {sources: {id: name}, targets: [candidate], chosen: candidate}; {} when no candidates.
## Fixes trade_source and trade_partner to valid picks.
func route_choice() -> Dictionary:
	var cands: Array = trade.get("candidates", [])
	if cands.is_empty():
		return {}
	var sources := {}
	for c in cands:
		sources[int(c["from"])] = str(c["fromName"])
	if not sources.has(trade_source):
		trade_source = int(sources.keys()[0])
	var targets: Array = cands.filter(func(c): return int(c["from"]) == trade_source)
	targets.sort_custom(func(a, b2):
		var ab := str(a["blocker"]) != ""
		var bb := str(b2["blocker"]) != ""
		if ab != bb:
			return bb
		return int(a["value"]) > int(b2["value"]))
	var chosen := {}
	for t in targets:
		if int(t["to"]) == trade_partner:
			chosen = t
	if chosen.is_empty():
		chosen = targets[0]
		trade_partner = int(chosen["to"])
	return {"sources": sources, "targets": targets, "chosen": chosen}

func establish_command(chosen: Dictionary) -> Dictionary:
	return {"type": "EstablishTradeRoute", "systemId": trade_source, "targetSystemId": int(chosen["to"]), "targetEmpire": int(chosen["partner"])}

func establish_block(chosen: Dictionary) -> String:
	var r := lock_reason()
	return r if r != "" else dry_run(establish_command(chosen))

func establish(chosen: Dictionary) -> bool:
	if issue(establish_command(chosen), [], "Trade route %s to %s established." % [chosen["fromName"], chosen["toName"]]):
		trade_partner = 0
		return true
	return false

# ---------------------------------------------------------------------------------------------
# Governors
# ---------------------------------------------------------------------------------------------
func set_governor(sid: int, wf: String, qu: String) -> bool:
	return issue({"type": "SetColonyGovernor", "systemId": sid, "governorWorkforce": wf, "governorQueue": qu},
		session.pending_ids("SetColonyGovernor", func(p): return int(p.get("systemId", -1)) == sid))

## Sets one governor kind ("workforce" or "queue") to `key` on every colony; returns the colony count.
func set_all_governors(which: String, key: String) -> int:
	for c in view()["colonies"]:
		var g: Dictionary = c["governor"]
		set_governor(int(c["systemId"]), key if which == "workforce" else str(g["workforce"]), key if which == "queue" else str(g["queue"]))
	return view()["colonies"].size()

func resume_governor(sid: int) -> bool:
	return issue({"type": "ClearGovernorOverride", "systemId": sid}, [], "Governor resumed.")

## The note under a colony's governor row: [text, is_warning].
static func governor_note(g: Dictionary) -> Array:
	if str(g.get("warning", "")) != "":
		return [str(g["warning"]), true]
	if str(g["workforce"]) != "Manual" and str(g.get("workforceExplanation", "")) != "":
		return [str(g["workforceExplanation"]), false]
	return ["", false]

# ---------------------------------------------------------------------------------------------
# Build templates
# ---------------------------------------------------------------------------------------------
func idle_colonies() -> Array:
	return view()["colonies"].filter(func(c): return int(c.get("queueLength", 0)) == 0)

## The ApplyBuildTemplate commands the core accepts for idle colonies (dryRunMany).
func accepted_applies(tid: int) -> Array:
	var items := []
	for c in idle_colonies():
		items.append({"command": {"type": "ApplyBuildTemplate", "templateId": tid, "systemId": int(c["systemId"])}, "cancelIds": []})
	var accepted := []
	if items.size() > 0 and lock_reason() == "":
		var res: Variant = query("dryRunMany", {"items": items})
		if res is Array:
			for i in range(res.size()):
				if str(res[i]) == "":
					accepted.append(items[i]["command"])
	return accepted

func apply_block(accepted: Array) -> String:
	var reason := lock_reason()
	if reason == "":
		reason = "Every colony already has a queue" if idle_colonies().is_empty() else ("No idle colony can build this template" if accepted.is_empty() else "")
	return reason

## The queue of a colony as template items, [] when it is empty or unreadable.
func queue_items(sid: int) -> Array:
	var d := query_dict("colony", {"systemId": sid})
	var its := []
	for q in d.get("queue", []):
		its.append({"product": q["product"], "referenceId": int(q["referenceId"])})
	return its

# ---------------------------------------------------------------------------------------------
# Stations and minor worlds
# ---------------------------------------------------------------------------------------------
func my_stations() -> Array:
	return view().get("stations", []).filter(func(s): return s.get("mine", false))

## {members, candidates} from the diplomacy query, or {} (error in session.core.last_error).
func minor_worlds() -> Dictionary:
	var d: Variant = query("diplomacy")
	if not d is Dictionary:
		return {}
	var me := player()
	return {"members": d["minors"].filter(func(m): return int(m.get("memberOf", 0)) == me),
		"candidates": d["minors"].filter(func(m): return int(m.get("memberOf", 0)) == 0)}

## Standing of a minor against the secession (member) or join (candidate) line: {margin, bad, tip}.
static func minor_standing(m: Dictionary, is_member: bool) -> Dictionary:
	var att := int(m["attitude"])
	var margin := att - (SECESSION_LINE if is_member else JOIN_LINE)
	var tip := ""
	if is_member:
		tip = "Attitude %d. It secedes below %d: %d point%s of margin." % [att, SECESSION_LINE, margin, "" if abs(margin) == 1 else "s"]
	else:
		tip = "Attitude %d. Membership needs %d or more, a friendship signed in an earlier turn, and contact." % [att, JOIN_LINE]
	return {"margin": margin, "bad": (is_member and margin < 15) or (not is_member and margin < 0), "tip": tip}

func apply_template_to(commands: Array) -> void:
	for cmd in commands:
		issue(cmd)

func set_default_template(tid: int, is_default: bool) -> bool:
	return issue({"type": "SetDefaultBuildTemplate", "templateId": 0 if is_default else tid}, [], "Default template updated.")

func delete_template(tid: int) -> bool:
	return issue({"type": "DeleteBuildTemplate", "templateId": tid}, [], "Template deleted.")

func define_template(tid: int, name_: String, items: Array, success: String) -> bool:
	return issue({"type": "DefineBuildTemplate", "templateId": tid, "templateName": name_, "templateItems": items}, [], success)
