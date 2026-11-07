class_name GalaxyOrders
extends RefCounted
## Contextual actions for a sector, built from the view and validated by dry-running the exact
## command through the core, so every disabled reason is the core's own message.
## Port of remaster/web/src/state/orders.ts. An action is a Dictionary:
##   {id, label, detail, tone, disabled (reason or ""), group: orders|units|system|view, run: Callable}

static func selected_unit(app: App) -> Dictionary:
	var fid := int(app.selection.get("fleetId", 0))
	if fid == 0:
		return {}
	return app.find_by_id("fleets", fid)

static func unit_name(app: App, fleet: Dictionary) -> String:
	var tf := int(fleet.get("taskForceId", 0))
	if tf != 0:
		var t := app.find_by_id("taskForces", tf)
		if not t.is_empty():
			return "%s (%d ships)" % [t["name"], t["fleetIds"].size()]
	return "%s #%d" % [fleet["name"], int(fleet["id"])]

static func move_command(fleet: Dictionary, x: int, y: int, target_fleet: int = 0) -> Dictionary:
	if int(fleet.get("taskForceId", 0)) != 0:
		return {"type": "SetTaskForceOrder", "taskForceId": int(fleet["taskForceId"]), "targetX": x, "targetY": y, "stance": fleet.get("stance", "Balanced"), "targetFleetId": target_fleet}
	if target_fleet != 0:
		return {"type": "AttackFleet", "fleetId": int(fleet["id"]), "targetFleetId": target_fleet}
	return {"type": "MoveFleet", "fleetId": int(fleet["id"]), "targetX": x, "targetY": y}

static func replace_ids(app: App, fleet: Dictionary) -> Array:
	return app.unit_order_ids(int(fleet["id"]), int(fleet.get("taskForceId", 0)))

static func plot_course(app: App, fleet: Dictionary, x: int, y: int) -> bool:
	var ok := app.issue(move_command(fleet, x, y), replace_ids(app, fleet))
	if ok:
		var s := app.system_at(x, y)
		var upd := app.find_by_id("fleets", int(fleet["id"]))
		var eta := int(upd.get("eta", 0))
		app.toast("%s: course set for %s%s." % [unit_name(app, fleet), s["name"] if not s.is_empty() else "sector %d.%d" % [x, y], (" - %d turn%s" % [eta, "" if eta == 1 else "s"]) if eta > 0 else ""], "good")
	return ok

static func sector_label(app: App, cell: int) -> String:
	var w := app.width()
	var x := cell % w
	var y := cell / w
	var s := app.system_at(x, y)
	if not s.is_empty():
		return str(s["name"])
	for st in app.view.get("stations", []):
		if int(st["x"]) == x and int(st["y"]) == y:
			return str(st["name"])
	return ("Uncharted sector %d.%d" if str(app.view["fog"])[cell] == "0" else "Sector %d.%d") % [x, y]

static func _act(id: String, label: String, group: String, tone: String, run: Callable, detail: String = "", disabled: String = "") -> Dictionary:
	return {"id": id, "label": label, "group": group, "tone": tone, "run": run, "detail": detail, "disabled": disabled}

static func upgrade_action(app: App, station: Dictionary, battle_lock: String) -> Dictionary:
	if not station.get("mine", false) or station.get("type", "") != "Outpost":
		return {}
	var cost: Dictionary = app.view.get("stationUpgradeCost", {})
	if station.has("upgrade"):
		var u: Dictionary = station["upgrade"]
		return _act("upgrade", "Starbase upgrade %d/%d" % [int(u["work"]), int(u["cost"])], "orders", "good", func(): pass, "Funded by %s" % u["funding"], "The upgrade is already under way")
	var reason := battle_lock
	var funder := {}
	var detail: Variant = app.core.request("station", {"stationId": int(station["id"])})
	if detail is Dictionary:
		var opts: Array = detail.get("funding", []).duplicate()
		opts.sort_custom(func(a, b): return int(a["industry"]) > int(b["industry"]))
		for o in opts:
			if str(o.get("blocker", "")) == "":
				funder = o
				break
		if funder.is_empty() and reason == "":
			reason = str(opts[0]["blocker"]) if opts.size() > 0 else "No colony within 6 sectors can fund the upgrade"
	elif reason == "":
		reason = app.core.last_error
	var sid := int(station["id"])
	return _act("upgrade", "Upgrade to starbase", "orders", "good", func():
		if not funder.is_empty():
			app.issue({"type": "UpgradeStation", "stationId": sid, "systemId": int(funder["systemId"])}, [], "%s: starbase upgrade funded by %s." % [station["name"], funder["name"]]),
		"¢%s now, then %s industry from %s (its queue pauses)" % [str(cost.get("credits", "?")), str(cost.get("industry", "?")), funder.get("name", "a colony within 6 sectors")], reason)

## Why this colony ship cannot terraform a planet, or "" when the core's canTerraform says it can.
## The core answers with a bool only, so the reason is rebuilt from the same conditions, in the same order.
static func terraform_blocker(app: App, fleet: Dictionary, system: Dictionary, planet: Dictionary) -> String:
	var ok: Variant = app.core.request("canTerraform", {"fleetId": int(fleet["id"]), "systemId": int(system["id"]), "planetIndex": int(planet["index"])})
	if ok == true:
		return ""
	var cls := str(planet.get("class", ""))
	var state := str(planet.get("terraformState", ""))
	if not app.view["meta"].get("campaignExpansion", false):
		return "Terraforming needs the campaign expansion rules"
	if not fleet.get("colonyShip", false):
		return "Only colony ships terraform"
	if int(fleet.get("hull", 1)) <= 0:
		return "The ship is destroyed"
	if fleet.get("retreat", false):
		return "The ship is retreating - cancel the retreat first"
	if int(fleet.get("attackTarget", 0)) != 0:
		return "The ship is pursuing a target - cancel that order first"
	if fleet.get("colonize", false):
		return "The ship is about to colonize - cancel that order first"
	var col := app.find_by_id("colonies", int(system["id"]), "systemId")
	if col.is_empty() and int(system.get("owner", 0)) not in [0, app.player()]:
		return "Another empire holds this system"
	if int(fleet["x"]) != int(system["x"]) or int(fleet["y"]) != int(system["y"]):
		return "The ship must be in this system"
	if cls == "GasGiant":
		return "Gas giants cannot be terraformed"
	if cls == "Asteroids":
		return "Asteroid belts cannot be terraformed"
	if state == "Terraformed":
		return "Already fully terraformed"
	if state == "Uninhabitable":
		return "Uninhabitable - this world cannot be terraformed"
	return "The core refuses this terraforming order"

## What a terraforming order buys: capacity now against capacity when finished, and the work needed.
static func terraform_detail(p: Dictionary) -> String:
	var base := int(p.get("baseCapacity", 0))
	var top := int(p.get("maxCapacity", 0))
	var txt := "%s: supports %d now, %d when terraformed (%s)" % [p.get("className", ""), base, top, Lcars.signed(top - base)]
	if int(p.get("terraformNeeded", 0)) > 0:
		txt += " - %d/%d work done" % [int(p.get("terraformWork", 0)), int(p["terraformNeeded"])]
	return txt

## Where RetreatFleet sends a ship, mirroring the core: the nearest own colony, except that a damaged ship
## under advanced rules prefers a colony with a powered shipyard (repairs). Returns {} with no colony.
static func retreat_target(app: App, fleet: Dictionary) -> Dictionary:
	var damaged: bool = app.view["meta"].get("advancedRules", false) and int(fleet["hull"]) < int(fleet["maxHull"])
	var best := {}
	var best_d := 1 << 30
	var best_yard := false
	for c in app.view.get("colonies", []):
		var s := app.find_by_id("systems", int(c["systemId"]))
		if s.is_empty():
			continue
		var d: int = max(abs(int(fleet["x"]) - int(s["x"])), abs(int(fleet["y"]) - int(s["y"])))
		var yard: bool = app.view["meta"].get("advancedRules", false) and c["economy"].get("shipyardPowered", false)
		if damaged:
			if (yard and not best_yard) or (yard == best_yard and d < best_d):
				best = {"name": s["name"], "x": int(s["x"]), "y": int(s["y"]), "distance": d, "yard": yard}
				best_d = d
				best_yard = yard
		elif d < best_d:
			best = {"name": s["name"], "x": int(s["x"]), "y": int(s["y"]), "distance": d, "yard": yard}
			best_d = d
	return best

## The retreat order for a unit: one RetreatFleet per ship (the core detaches each ship from its task force).
static func retreat_action(app: App, fleet: Dictionary, battle_lock: String) -> Dictionary:
	var tf := int(fleet.get("taskForceId", 0))
	var ships := [fleet]
	if tf != 0:
		ships = app.view["fleets"].filter(func(m): return int(m.get("taskForceId", 0)) == tf)
	var target := retreat_target(app, fleet)
	var uname := unit_name(app, fleet)
	var detail := "No colony to fall back to"
	if not target.is_empty():
		detail = "%s falls back to %s (%d sector%s)%s - it breaks off at turn end" % [uname, target["name"], int(target["distance"]), "" if int(target["distance"]) == 1 else "s", " for repairs at its powered shipyard" if target["yard"] and int(fleet["hull"]) < int(fleet["maxHull"]) else ""]
		if tf != 0:
			detail += "; the task force disbands"
	var reason := battle_lock
	if reason == "" and target.is_empty():
		reason = "You have no colony to retreat to"
	if reason == "" and not target.is_empty() and int(target["distance"]) == 0 and not (int(fleet["hull"]) < int(fleet["maxHull"]) and not target["yard"]):
		reason = "Already at %s, your nearest colony" % target["name"]
	if reason == "" and fleet.get("retreat", false) and int(fleet["targetX"]) == int(target.get("x", -1)) and int(fleet["targetY"]) == int(target.get("y", -1)):
		reason = "Already retreating to %s" % target["name"]
	if reason == "":
		for m in ships:
			reason = app.dry_run({"type": "RetreatFleet", "fleetId": int(m["id"])}, unit_order_ids_for(app, m))
			if reason != "":
				break
	var run := func():
		var done := 0
		for m in ships:
			if app.issue({"type": "RetreatFleet", "fleetId": int(m["id"])}, unit_order_ids_for(app, m)):
				done += 1
		if done > 0:
			app.toast("%s retreating to %s." % [uname, target.get("name", "safety")], "good")
	var act := _act("retreat", "Retreat to " + str(target.get("name", "?")), "orders", "alert", run, detail, reason)
	return act

static func unit_order_ids_for(app: App, f: Dictionary) -> Array:
	return app.unit_order_ids(int(f["id"]), 0)

static func cell_actions(app: App, cell: int) -> Array:
	var v := app.view
	if v.is_empty():
		return []
	var w := app.width()
	var x := cell % w
	var y := cell / w
	var system := app.system_at(x, y)
	var fleet := selected_unit(app)
	var actions := []
	var battle_lock := "Finish the battle in progress first" if v.get("session", {}).get("active", false) else ""
	if not fleet.is_empty():
		var here := int(fleet["x"]) == x and int(fleet["y"]) == y
		var cancel_ids := replace_ids(app, fleet)
		var uname := unit_name(app, fleet)
		if not here:
			var label := "Set course here"
			var reason := battle_lock
			var r := app.core.invoke("route", {"fleetId": int(fleet["id"]), "x": x, "y": y})
			if r["ok"]:
				var route: Dictionary = r["result"]
				if int(route["steps"]) < 0:
					if reason == "":
						reason = "No supplied path to this sector" if route.get("supplied", false) else "Outside this unit's supply range"
				else:
					label = "Set course here - %d turn%s" % [int(route["eta"]), "" if int(route["eta"]) == 1 else "s"]
			elif reason == "":
				reason = str(r["error"])
			if reason == "":
				reason = app.dry_run(move_command(fleet, x, y), cancel_ids)
			actions.append(_act("move", label, "orders", "primary", func(): plot_course(app, fleet, x, y), "%s moves when the turn ends" % uname, reason))
		actions.append(retreat_action(app, fleet, battle_lock))
		var n := 0
		for c in v.get("contacts", []):
			if int(c["x"]) != x or int(c["y"]) != y or n >= 3:
				continue
			n += 1
			var cmd := move_command(fleet, x, y, int(c["id"]))
			var reason2 := battle_lock if battle_lock != "" else app.dry_run(cmd, cancel_ids)
			var cname: String = c["name"]
			actions.append(_act("attack-%d" % int(c["id"]), "Attack " + cname, "orders", "alert", func(): app.issue(cmd, cancel_ids, "%s will engage %s." % [uname, cname]),
				"%s #%d - the battle is fought at turn end" % [c["ownerName"], int(c["id"])], reason2))
		if here:
			if fleet.get("colonyShip", false):
				var cc := {"type": "Colonize", "fleetId": int(fleet["id"])}
				actions.append(_act("colonize", ("Colonize " + str(system["name"])) if not system.is_empty() else "Colonize", "orders", "good",
					func(): app.issue(cc, cancel_ids, "Colony ship ordered to settle %s." % system.get("name", "here")),
					"The colony ship is consumed to found a colony", battle_lock if battle_lock != "" else app.dry_run(cc, cancel_ids)))
				if not system.is_empty() and v["meta"].get("campaignExpansion", false):
					for p in system.get("planets", []):
						var t := {"type": "TerraformPlanet", "fleetId": int(fleet["id"]), "systemId": int(system["id"]), "planetIndex": int(p["index"])}
						var pname: String = p["name"]
						var why := battle_lock if battle_lock != "" else terraform_blocker(app, fleet, system, p)
						if why == "":
							why = app.dry_run(t, cancel_ids)
						actions.append(_act("terraform-%d" % int(p["index"]), "Terraform " + pname, "orders", "secondary",
							func(): app.issue(t, cancel_ids, "Terraforming %s begins at turn end." % pname),
							terraform_detail(p), why))
			if fleet.get("shipClass", "") == "Transport" and v["meta"].get("rules", {}).get("stations", false):
				var oc := {"type": "BuildOutpost", "fleetId": int(fleet["id"])}
				var turns := int(v.get("outpostTurns", 3))
				var reason3 := battle_lock
				if reason3 == "" and fleet.get("buildingOutpost", false):
					reason3 = "Assembly under way - any new order for this transport abandons it"
				if reason3 == "":
					reason3 = app.dry_run(oc, cancel_ids)
				actions.append(_act("outpost", ("Assembling outpost (%d/%d)" % [int(fleet.get("outpostProgress", 0)), turns]) if fleet.get("buildingOutpost", false) else "Build outpost - %d turns" % turns,
					"orders", "good", func(): app.issue(oc, cancel_ids, "Outpost assembly begins - ready in %d turns." % turns),
					"The transport is consumed and becomes an outpost: supply range 3, sensors 2.", reason3))
			if fleet.get("shipClass", "") == "Transport" and not system.is_empty():
				var ic := {"type": "Invade", "fleetId": int(fleet["id"]), "systemId": int(system["id"])}
				actions.append(_act("invade", "Invade " + str(system["name"]), "orders", "alert", func(): app.issue(ic, cancel_ids, "Invasion of %s ordered." % system["name"]),
					"Ground assault resolves at turn end", battle_lock if battle_lock != "" else app.dry_run(ic, cancel_ids)))
			if int(fleet["targetX"]) != int(fleet["x"]) or int(fleet["targetY"]) != int(fleet["y"]) or cancel_ids.size() > 0:
				actions.append(_act("hold", "Hold position", "orders", "tertiary", func():
					if cancel_ids.size() > 0:
						app.cancel_orders(cancel_ids, "Orders cancelled.")
					var ref := app.find_by_id("fleets", int(fleet["id"]))
					if not ref.is_empty() and (int(ref["targetX"]) != int(ref["x"]) or int(ref["targetY"]) != int(ref["y"])):
						app.issue(move_command(ref, int(ref["x"]), int(ref["y"])), replace_ids(app, ref), "Holding position."),
					"Cancel this unit's orders for the turn", battle_lock))
	for st in v.get("stations", []):
		if int(st["x"]) == x and int(st["y"]) == y:
			var up := upgrade_action(app, st, battle_lock)
			if not up.is_empty():
				actions.append(up)
			if fleet.is_empty():
				actions.append(_act("station", "Inspect " + str(st["name"]), "units", "tertiary", func(): app.select_cell(cell), "%s %s - hull %d/%d" % [st["ownerName"], str(st["typeName"]).to_lower(), int(st["hull"]), int(st["maxHull"])]))
	var seen := {}
	var unit_count := 0
	for f in v.get("fleets", []):
		if int(f["x"]) != x or int(f["y"]) != y:
			continue
		var tf := int(f.get("taskForceId", 0))
		if tf != 0:
			if seen.has(tf):
				continue
			seen[tf] = true
		if not fleet.is_empty() and (int(f["id"]) == int(fleet["id"]) or (tf != 0 and tf == int(fleet.get("taskForceId", 0)))):
			continue
		if unit_count >= 5:
			break
		unit_count += 1
		var fid := int(f["id"])
		actions.append(_act("select-%d" % fid, "Select " + unit_name(app, f), "units", "secondary", func(): app.select_fleet(fid)))
	if fleet.is_empty():
		for c in v.get("contacts", []):
			if int(c["x"]) == x and int(c["y"]) == y:
				var cid := int(c["id"])
				actions.append(_act("inspect-%d" % cid, "Inspect " + str(c["name"]), "units", "tertiary", func(): app.select_fleet(cid), "%s #%d" % [c["ownerName"], cid]))
	if not system.is_empty():
		var sid := int(system["id"])
		if int(system.get("owner", 0)) == app.player():
			actions.append(_act("open", "Manage " + str(system["name"]), "system", "primary", func(): app.select_system(sid); app.goto("system"), "Open the system screen (double-click)"))
		var minor: Dictionary = system.get("minor", {})
		if not minor.is_empty() and not minor.get("unknown", false):
			actions.append(_act("diplomacy", "Diplomacy: " + str(minor["name"]), "system", "secondary", func(): app.set_meta("diplomacy_focus", {"minor": true, "id": sid}); app.goto("diplomacy")))
		elif int(system.get("owner", 0)) != 0 and int(system["owner"]) != app.player():
			var owner := int(system["owner"])
			var rel := app.find_by_id("empires", owner, "empire")
			if rel.get("contact", false):
				actions.append(_act("diplomacy", "Diplomacy: " + str(rel["name"]), "system", "secondary", func(): app.set_meta("diplomacy_focus", {"minor": false, "id": owner}); app.goto("diplomacy")))
				if system.get("visible", false):
					actions.append(_act("intel", "Intelligence on " + str(rel["name"]), "system", "tertiary", func(): app.set_meta("intel_focus", owner); app.goto("intel")))
	actions.append(_act("centre", "Centre view here", "view", "ghost", func():
		var s := app.current_screen()
		if s and s.has_method("focus_cell"):
			s.focus_cell(x, y)))
	if not fleet.is_empty():
		actions.append(_act("deselect", "Deselect unit", "view", "ghost", func(): app.select_cell(int(app.selection["cell"]))))
	return actions

static func order_text(app: App, fleet: Dictionary) -> String:
	var drafted := []
	for p in app.view.get("pending", []):
		if int(p.get("fleetId", -1)) == int(fleet["id"]) or (int(fleet.get("taskForceId", 0)) != 0 and int(p.get("taskForceId", -1)) == int(fleet["taskForceId"]) and p["type"] == "SetTaskForceOrder"):
			var s: String = p["summary"]
			var colon := s.find(": ")
			drafted.append(s.substr(colon + 2) if colon >= 0 else s)
	if drafted.size() > 0:
		return "; ".join(drafted)
	if fleet.get("colonize", false):
		return "Colonizing at turn end"
	if fleet.get("buildingOutpost", false):
		return "Assembling an outpost (%d/%d turns)" % [int(fleet.get("outpostProgress", 0)), int(app.view.get("outpostTurns", 3))]
	if fleet.get("terraforming", false):
		return "Terraforming"
	if fleet.get("retreat", false):
		return "Retreating to %s" % str(retreat_target(app, fleet).get("name", "safety"))
	if int(fleet.get("attackTarget", 0)) != 0:
		return "Pursuing contact #%d" % int(fleet["attackTarget"])
	if fleet.get("route", []).size() > 0:
		var s2 := app.system_at(int(fleet["targetX"]), int(fleet["targetY"]))
		return "En route to %s" % (s2["name"] if not s2.is_empty() else "%d.%d" % [int(fleet["targetX"]), int(fleet["targetY"])])
	return "Awaiting orders"
