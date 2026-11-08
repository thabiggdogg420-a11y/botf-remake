class_name EndTurnVM
extends RefCounted
## End-turn check model: what the turn will do (the core's forecast of the turn with this turn's drafted orders)
## and what has been left idle, each warning with the screen that fixes it and, where one order cures it, a fix
## the core has already dry-run (dryRunMany). Presentation: scenes/ui/end_turn_check.gd.
## Core calls: forecast, describeTurn, dryRunMany.

## {warnings: [{kind, tone, text, screen, system, fleet, fix: {label, command, reason}}], forecast: Dictionary,
##  last_turn: Dictionary, alerts: int}
static func build(session: AppSession) -> Dictionary:
	var v := session.view
	var warnings: Array = []
	var fc: Variant = session.core.request("forecast")
	var forecast: Dictionary = fc if fc is Dictionary else {}
	if str(forecast.get("error", "")) != "":
		forecast = {"error": str(forecast["error"])}
	var last := _last_turn(session)
	var default_template := 0
	for t in v.get("templates", []):
		if t.get("isDefault", false):
			default_template = int(t["id"])

	for c in v.get("colonies", []):
		var sid := int(c["systemId"])
		var gov: Dictionary = c.get("governor", {})
		var eco: Dictionary = c["economy"]
		if int(eco.get("foodDeficit", 0)) > 0:
			warnings.append(_w("food", "alert", "%s is short of food by %d: its population will shrink." % [c["name"], int(eco["foodDeficit"])], "system", sid))
		if str(gov.get("queue", "Manual")) == "Manual" and int(c.get("queueLength", 0)) == 0 and not _builds_megaproject(c):
			var w := _w("queue", "warn", "%s has an empty build queue: its industry (%d) will be wasted." % [c["name"], int(eco["industry"])], "system", sid)
			if default_template != 0:
				w["fix"] = {"label": "Apply default template", "command": {"type": "ApplyBuildTemplate", "templateId": default_template, "systemId": sid}, "reason": ""}
			warnings.append(w)
		var unassigned := int(c.get("workers", {}).get("unassigned", 0))
		if unassigned > 0 and str(gov.get("workforce", "Manual")) == "Manual":
			warnings.append(_w("workers", "warn", "%s has %d unassigned citizen%s producing nothing." % [c["name"], unassigned, "" if unassigned == 1 else "s"], "system", sid))
		var dead := int(eco.get("inactiveFactories", 0)) + int(eco.get("inactiveLabs", 0))
		if dead > 0:
			warnings.append(_w("power", "warn", "%s: %d structures are unpowered (%d/%d energy)." % [c["name"], dead, int(eco["energyUsed"]), int(eco["energyAvailable"])], "system", sid))

	var idle_colony_ships := 0
	var idle_ships := 0
	var first_idle := 0
	var seen_tf := {}
	for f in v.get("fleets", []):
		var tf := int(f.get("taskForceId", 0))
		if tf != 0:
			if seen_tf.has(tf):
				continue
			seen_tf[tf] = true
		if not is_idle(session, f):
			continue
		if first_idle == 0:
			first_idle = int(f["id"])
		if f.get("colonyShip", false):
			idle_colony_ships += 1
			warnings.append(_w("colony-ship", "warn", "%s #%d (colony ship) has no orders." % [f["name"], int(f["id"])], "fleets", 0, int(f["id"])))
		else:
			idle_ships += 1
	if idle_ships > 0:
		warnings.append(_w("idle-fleet", "info", "%d ship%s or task force%s with no orders." % [idle_ships, "" if idle_ships == 1 else "s", "" if idle_ships == 1 else "s"], "fleets", 0, first_idle))

	if forecast.has("credits"):
		if int(forecast["creditsAfter"]) < 0:
			warnings.append(_w("credits", "alert", "The treasury goes negative: %s after the turn (%s per turn)." % [Fmt.fmt(forecast["creditsAfter"]), Fmt.signed(forecast["credits"])], "empire"))
		elif int(forecast["credits"]) < 0:
			warnings.append(_w("credits", "warn", "Income is negative: %s per turn leaves %s." % [Fmt.signed(forecast["credits"]), Fmt.fmt(forecast["creditsAfter"])], "empire"))

	# dryRunMany: ask the core, in one call, which fixes it would accept.
	var items := []
	var owners := []
	for w in warnings:
		if w.has("fix"):
			items.append({"command": w["fix"]["command"], "cancelIds": []})
			owners.append(w)
	if items.size() > 0:
		var res: Variant = session.core.request("dryRunMany", {"items": items})
		if res is Array:
			for i in range(min(res.size(), owners.size())):
				owners[i]["fix"]["reason"] = str(res[i])
	var alerts := warnings.filter(func(w): return w["tone"] == "alert").size()
	return {"warnings": warnings, "forecast": forecast, "last_turn": last, "alerts": alerts}

static func _w(kind: String, tone: String, text: String, screen: String, system: int = 0, fleet: int = 0) -> Dictionary:
	return {"kind": kind, "tone": tone, "text": text, "screen": screen, "system": system, "fleet": fleet}

static func _builds_megaproject(c: Dictionary) -> bool:
	return c.get("buildsMegaproject", false)

## A ship with nothing to do this turn: no course, no drafted order, no standing job.
static func is_idle(session: AppSession, f: Dictionary) -> bool:
	if f.get("hasOrders", false) or f.get("route", []).size() > 0 or f.get("terraforming", false) or f.get("buildingOutpost", false) or f.get("colonize", false):
		return false
	if int(f.get("attackTarget", 0)) != 0 or int(f.get("invadeSystemId", 0)) != 0 or f.get("retreat", false):
		return false
	return session.unit_order_ids(int(f["id"]), int(f.get("taskForceId", 0))).is_empty()

## describeTurn returns the last resolved turn's briefing as a JSON string (null before the first turn ends).
static func _last_turn(session: AppSession) -> Dictionary:
	var r: Variant = session.core.request("describeTurn")
	if r is String and r != "":
		var parsed: Variant = JSON.parse_string(r)
		if parsed is Dictionary:
			return parsed
	return {}

