class_name FleetsVM
extends ViewModel
## Fleets screen model: units (single ships and task forces), grouped by sector, the marks for a new task force,
## the selected unit's totals, the send-to destination with the core's route ETA, and the unit's orders.

var marks: Array = []
var tf_name := ""
var dest := 0

## Units: [{lead, members, tf}], one per ungrouped ship and one per task force (led by its first member).
func units() -> Array:
	var v := view()
	var out := []
	var seen := {}
	for f in v.get("fleets", []):
		var tf := int(f.get("taskForceId", 0))
		if tf != 0:
			if seen.has(tf):
				continue
			seen[tf] = true
			var members: Array = v["fleets"].filter(func(m): return int(m.get("taskForceId", 0)) == tf)
			out.append({"lead": f, "members": members, "tf": session.find_by_id("taskForces", tf)})
		else:
			out.append({"lead": f, "members": [f], "tf": {}})
	return out

func sector_name(x: int, y: int) -> String:
	return place(x, y, "Sector %d.%d")

## The unit holding the selected fleet, else the first.
func current(list: Array) -> Dictionary:
	var cur := {}
	for u in list:
		for m in u["members"]:
			if int(m["id"]) == int(session.selection.get("fleetId", 0)):
				cur = u
	if cur.is_empty() and list.size() > 0:
		cur = list[0]
	return cur

## Units grouped by sector in first-seen order: [[sector name, [unit], ship count]].
func by_sector(list: Array) -> Array:
	var sectors := {}
	var order := []
	for u in list:
		var key := sector_name(int(u["lead"]["x"]), int(u["lead"]["y"]))
		if not sectors.has(key):
			sectors[key] = []
			order.append(key)
		sectors[key].append(u)
	var out := []
	for key in order:
		var n := 0
		for u in sectors[key]:
			n += u["members"].size()
		out.append([key, sectors[key], n])
	return out

## The one-line state of a unit: idle, ordered, terraforming or "→ place · nT".
func status(lead: Dictionary) -> String:
	if lead.get("route", []).size() > 0:
		return "→ %s · %dT" % [sector_name(int(lead["targetX"]), int(lead["targetY"])), int(lead["eta"])]
	if lead.get("hasOrders", false):
		return "ordered"
	if lead.get("terraforming", false):
		return "terraforming"
	return "idle"

## Summed member stats: {hull, max_hull, shields, max_shields, upkeep, range}.
static func totals(members: Array) -> Dictionary:
	var t := {"hull": 0, "max_hull": 0, "shields": 0, "max_shields": 0, "upkeep": 0, "range": 999}
	for m in members:
		t["hull"] += int(m["hull"])
		t["max_hull"] += int(m["maxHull"])
		t["shields"] += int(m["shields"])
		t["max_shields"] += int(m["maxShields"])
		t["upkeep"] += int(m.get("maintenance", 0))
		t["range"] = min(int(t["range"]), int(m.get("range", 0)))
	return t

func toggle_mark(fid: int, on: bool) -> void:
	if on:
		marks.append(fid)
	else:
		marks.erase(fid)

func form_command(name_: String) -> Dictionary:
	return {"type": "CreateTaskForce", "memberFleetIds": marks, "taskForceName": name_.strip_edges()}

func form_block() -> String:
	return "Tick at least two ungrouped ships in the same sector" if marks.size() < 2 else dry_run(form_command(tf_name))

func form(name_: String) -> bool:
	if issue(form_command(name_), [], "Task force formed from %d ships." % marks.size()):
		marks = []
		tf_name = ""
		return true
	return false

## Known systems other than the unit's own sector.
func destinations(lead: Dictionary) -> Array:
	return view()["systems"].filter(func(s): return not (int(s["x"]) == int(lead["x"]) and int(s["y"]) == int(lead["y"])))

## {target, route, eta_text, block}: the chosen destination and the core's route to it.
func send_plan(lead: Dictionary) -> Dictionary:
	var route := {}
	var target := session.find_by_id("systems", dest) if dest != 0 else {}
	if not target.is_empty():
		route = query_dict("route", {"fleetId": int(lead["id"]), "x": int(target["x"]), "y": int(target["y"])})
	var eta := ""
	if not route.is_empty():
		eta = "out of range" if int(route["steps"]) < 0 else "%d turn%s" % [int(route["eta"]), "" if int(route["eta"]) == 1 else "s"]
	var block := lock_reason()
	if block == "":
		block = "Choose a destination" if route.is_empty() else ("Outside supply range" if int(route["steps"]) < 0 else "")
	return {"target": target, "route": route, "eta_text": eta, "block": block}

## The unit's tactical stance and the order that sets it.
func stance(u: Dictionary) -> String:
	var tf: Dictionary = u["tf"]
	return tf.get("stance", u["lead"].get("stance", "Balanced"))

func set_stance(u: Dictionary, st: String, cancel_ids: Array) -> bool:
	var tf: Dictionary = u["tf"]
	if not tf.is_empty():
		return issue({"type": "SetTaskForceOrder", "taskForceId": int(tf["id"]), "targetX": int(tf["targetX"]), "targetY": int(tf["targetY"]), "stance": st, "targetFleetId": int(tf.get("attackTarget", 0))}, cancel_ids, "Stance updated.")
	return issue({"type": "SetTacticalStance", "fleetId": int(u["lead"]["id"]), "stance": st}, cancel_ids, "Stance updated.")

## Drafted orders among `ids`.
func drafted(ids: Array) -> Array:
	return view()["pending"].filter(func(p): return ids.has(int(p["id"])))

func split(tf: Dictionary) -> bool:
	return issue({"type": "DisbandTaskForce", "taskForceId": int(tf["id"])}, [], "%s split into individual ships." % tf["name"])
