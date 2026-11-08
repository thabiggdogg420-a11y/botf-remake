class_name IntelVM
extends ViewModel
## Intelligence screen model: the core's `intel` query, the drafted budget (security gets whatever operations do
## not use), per-rival sabotage targets, the tab and the highlighted rival, and the budget and mission orders.

var intel: Dictionary = {}
var budget := {"security": 100, "rivals": {}}
var original := {}
var tab := "operations"
var highlight := 0
var targets := {}

func load() -> void:
	intel = query_dict("intel")
	budget = _from_intel()
	original = budget.duplicate(true)

func available() -> bool:
	return not intel.is_empty()

func _from_intel() -> Dictionary:
	var b := {"security": int(intel.get("budget", {}).get("security", 100)), "rivals": {}}
	for r in intel.get("rivals", []):
		var a := {"espionage": 0, "sabotage": 0}
		for x in intel["budget"].get("rivals", []):
			if int(x["id"]) == int(r["id"]):
				a = {"espionage": int(x["espionage"]), "sabotage": int(x["sabotage"])}
		b["rivals"][int(r["id"])] = a
	return b

## Security strength as shown: the core's milli-strength, else the stockpile.
func security_text() -> String:
	return ("%.1f" % (float(intel["defenseStrengthMilli"]) / 1000.0)) if intel.has("defenseStrengthMilli") else Lcars.fmt(intel.get("securityStockpile", 0))

func reports_count() -> int:
	return intel["reports"].size() + intel["operations"].size()

static func rival_status(r: Dictionary) -> String:
	return "Eliminated - no longer a target" if r.get("eliminated", false) else ("No contact" if not r.get("contact", false) else ("At war - sabotage permitted" if r.get("atWar", false) else "At peace"))

## Sets a rival's espionage or sabotage share, clamped so operations never exceed 100%. Returns the value set.
func set_share(id: int, kind: String, value: int) -> int:
	var others := 0
	for rid in budget["rivals"]:
		var a: Dictionary = budget["rivals"][rid]
		if int(rid) == id:
			others += int(a["sabotage"] if kind == "espionage" else a["espionage"])
		else:
			others += int(a["espionage"]) + int(a["sabotage"])
	var clamped: int = clamp(value, 0, 100 - others)
	budget["rivals"][id][kind] = clamped
	budget["security"] = 100 - others - clamped
	return clamped

func budget_command() -> Dictionary:
	var ids := []
	var esp := []
	var sab := []
	for r in intel["rivals"]:
		var id := int(r["id"])
		ids.append(id)
		esp.append(int(budget["rivals"][id]["espionage"]))
		sab.append(int(budget["rivals"][id]["sabotage"]))
	return {"type": "SetIntelBudget", "intelInternalSecurity": budget["security"], "intelRivalIds": ids, "intelEspionagePcts": esp, "intelSabotagePcts": sab}

func cancel_ids() -> Array:
	return session.pending_ids("SetIntelBudget")

func dirty() -> bool:
	return JSON.stringify(budget) != JSON.stringify(original)

func apply_block() -> String:
	var reason := lock_reason()
	if reason == "":
		reason = dry_run(budget_command(), cancel_ids()) if dirty() else "No changes"
	return reason

func apply() -> bool:
	return issue(budget_command(), cancel_ids(), "Intelligence budget ordered.")

func set_focus(id: int, key: String, name_: String) -> bool:
	return issue({"type": "SetIntelFocus", "targetEmpire": id, "intelDepartmentFocus": key}, [], "Focus on %s targets." % name_.to_lower())

## The sabotage target colony for a rival, defaulting to its first visible colony.
func target_of(r: Dictionary) -> int:
	var id := int(r["id"])
	var cols: Array = r.get("visibleColonies", [])
	if not targets.has(id):
		targets[id] = int(cols[0]["systemId"]) if cols.size() > 0 else 0
	return int(targets[id])

func gather_command(id: int) -> Dictionary:
	return {"type": "GatherIntelligence", "targetEmpire": id}

func gather_block(id: int) -> String:
	var lock := lock_reason()
	return lock if lock != "" else dry_run(gather_command(id))

func sabotage_command(r: Dictionary) -> Dictionary:
	return {"type": "Sabotage", "targetEmpire": int(r["id"]), "targetSystemId": target_of(r)}

func sabotage_block(r: Dictionary) -> String:
	var reason := lock_reason()
	if reason == "":
		reason = "No colony of theirs is within sensor range" if target_of(r) == 0 else dry_run(sabotage_command(r))
	return reason

## Completed operations as display rows: [{turn, tone, text, badge}].
func operation_rows() -> Array:
	var out := []
	for o in intel.get("operations", []):
		var tone := ("good" if o.get("success", false) else "warn") if o.get("mine", false) else "alert"
		var txt := "%s · %s · %s" % [("%s vs %s" % [o["area"], o["defenderName"]]) if o.get("mine", false) else "Intrusion by %s" % o["attackerName"], o["department"], o["outcome"]]
		if int(o.get("creditsStolen", 0)) > 0:
			txt += " · ¢%s taken" % Lcars.fmt(o["creditsStolen"])
		if int(o.get("researchStolen", 0)) > 0:
			txt += " · %s research taken" % Lcars.fmt(o["researchStolen"])
		if str(o.get("details", "")) != "":
			txt += "\n" + str(o["details"])
		var badge := ""
		if o.get("detected", false):
			badge = "exposed" if o.get("mine", false) else "detected"
		out.append({"turn": int(o["turn"]), "tone": tone, "text": txt, "badge": badge})
	return out

## Espionage reports as display rows: [{turn, ok, text}].
func report_rows() -> Array:
	var out := []
	for r in intel.get("reports", []):
		out.append({"turn": int(r["turn"]), "ok": r.get("success", false), "text": "%s · %s%s" % [r["targetName"], ("%d colonies, about ¢%s" % [int(r["knownColonies"]), Lcars.fmt(r["estimatedCredits"])]) if r.get("success", false) else "operation failed", (" · " + str(r["system"])) if str(r.get("system", "")) != "" else ""]})
	return out
