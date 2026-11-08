class_name CoreClient
extends RefCounted
## Thin interface between the UI and the simulation core. Same methods and JSON shapes as the
## web bridge (`botf_call(method, argsJson)` in remaster/web/bridge/WebBridge.cpp).
##
## Backends:
##  - "native":  the BotfCore GDExtension singleton (`BotfCore.request(method, args) -> Dictionary`).
##  - "fixture": JSON recorded from the wasm core by scenes/ui/fixtures/record_fixtures.mjs.
##    It is a small stateful stub: orders become pending entries, endTurn advances the turn.
##
## Every call returns {ok: bool, result: Variant, error: String}.

const FIXTURE_DIRS := ["res://scenes/ui/fixtures", "res://fixtures"]

var backend := "fixture"
var last_error := ""
var _native: Object = null
# Fixture state
var _set := "campaign"
var _view: Dictionary = {}
var _next_id := 1000
var _cache := {}
var _battle_round := 0

static func create(prefer: String = "auto") -> CoreClient:
	var c := CoreClient.new()
	if prefer != "fixture" and Engine.has_singleton("BotfCore"):
		c.backend = "native"
		c._native = Engine.get_singleton("BotfCore")
	return c

func is_native() -> bool:
	return backend == "native"

## Raw call: {ok, result, error}.
func invoke(method: String, args: Dictionary = {}) -> Dictionary:
	var reply: Dictionary
	if backend == "native":
		reply = _native_call(method, args)
	else:
		reply = _fixture_call(method, args)
	last_error = "" if reply.get("ok", false) else str(reply.get("error", "Unknown error"))
	return reply

## Convenience: the result, or null when the core rejected the call (see last_error).
func request(method: String, args: Dictionary = {}) -> Variant:
	var r := invoke(method, args)
	return r.get("result") if r.get("ok", false) else null

func _native_call(method: String, args: Dictionary) -> Dictionary:
	var out: Variant
	if _native.has_method("botf_call"):
		out = _native.callv("botf_call", [method, args])
	else:
		out = _native.callv("request", [method, args])
	if out is String:
		out = JSON.parse_string(out)
	if out is Dictionary and out.has("ok"):
		return out
	return {"ok": out != null, "result": out, "error": "" if out != null else "No reply from BotfCore"}

# ---------------------------------------------------------------------------------------------
# Fixture backend
# ---------------------------------------------------------------------------------------------
func _load(set_name: String, name: String) -> Variant:
	var key := set_name + "/" + name
	if _cache.has(key):
		return _cache[key].duplicate(true) if _cache[key] is Dictionary or _cache[key] is Array else _cache[key]
	for dir in FIXTURE_DIRS:
		var path := "%s/%s/%s.json" % [dir, set_name, name]
		var data: Variant = AppLoader.json(path)
		if data != null:
			_cache[key] = data
			return data.duplicate(true) if data is Dictionary or data is Array else data
	return null

func _ok(result: Variant) -> Dictionary:
	return {"ok": true, "result": result, "error": ""}

func _fail(error: String) -> Dictionary:
	return {"ok": false, "result": null, "error": error}

func _fixture(name: String, fallback_set: String = "campaign") -> Dictionary:
	var v: Variant = _load(_set, name)
	if v == null:
		v = _load(fallback_set, name)
	if v == null:
		return _fail("Fixture backend has no recording for '%s'" % name)
	return _ok(v)

func _start(set_name: String, args: Dictionary = {}) -> Dictionary:
	_set = set_name
	_view = _load(set_name, "view")
	if _view == null:
		return _fail("Missing fixture view for " + set_name)
	if args.has("faction"):
		var f := int(args["faction"])
		_view["meta"]["player"] = f
		for e in _load("campaign", "meta")["empires"]:
			if int(e["id"]) == f:
				_view["meta"]["playerName"] = e["name"]
	if args.has("preset"):
		_view["meta"]["rules"]["preset"] = args["preset"]
	_battle_round = 0
	return _ok(_view.duplicate(true))

func _summary(command: Dictionary) -> String:
	var t := str(command.get("type", "Order"))
	var bits := []
	for k in command.keys():
		if k != "type":
			bits.append("%s %s" % [k, str(command[k])])
	return "%s: %s" % [t, ", ".join(bits)]

func _fixture_call(method: String, args: Dictionary) -> Dictionary:
	match method:
		"meta", "encyclopedia":
			return _fixture(method)
		"newCampaign":
			return _start("campaign", args)
		"newFrontierDrill":
			return _start("frontier")
		"newDiplomacyDrill":
			return _start("diplomacy")
		"newBattleDrill":
			var r := _start("campaign")
			_set = "battle"
			_view["session"] = {"active": false, "battlePending": true, "battleFinished": false, "replays": 0}
			return r
		"load":
			return _start("campaign")
		"view":
			if _view.is_empty():
				return _fail("No campaign in progress")
			return _ok(_view.duplicate(true))
		"submit":
			if _view.is_empty():
				return _fail("No campaign in progress")
			var command: Dictionary = args.get("command", {})
			if not command.has("type"):
				return _fail("Command has no type")
			_cancel(args.get("cancelIds", []))
			_next_id += 1
			var p := {"id": _next_id, "type": command["type"], "summary": _summary(command)}
			for k in ["systemId", "fleetId", "taskForceId", "targetFleetId", "targetEmpire", "targetX", "targetY", "building", "referenceId", "proposalId"]:
				if command.has(k):
					p[k] = command[k]
			_view["pending"].append(p)
			_apply_locally(command)
			_view["meta"]["version"] = int(_view["meta"].get("version", 0)) + 1
			return _ok({"id": _next_id})
		"cancel":
			_cancel(args.get("ids", []))
			return _ok(true)
		"dryRun":
			var c: Dictionary = args.get("command", {})
			if c.get("type", "") == "AllocateResearch":
				var total := 0
				for n in c.get("researchAllocation", []):
					total += int(n)
				if total != 100:
					return _ok({"ok": false, "error": "Research allocation must total 100 percent"})
			return _ok({"ok": true, "error": ""})
		"dryRunMany":
			var out := []
			for i in args.get("items", []):
				out.append("")
			return _ok(out)
		"route":
			var fleet := _find_fleet(int(args.get("fleetId", 0)))
			if fleet.is_empty():
				return _fail("Unknown fleet")
			var w := int(_view["meta"]["width"])
			var tx := int(args.get("x", 0))
			var ty := int(args.get("y", 0))
			var cells := []
			var p := Vector2i(int(fleet["x"]), int(fleet["y"]))
			cells.append(p.y * w + p.x)
			while p != Vector2i(tx, ty):
				p += Vector2i(signi(tx - p.x), signi(ty - p.y))
				cells.append(p.y * w + p.x)
			var steps := cells.size() - 1
			var speed: int = max(1, int(fleet.get("speed", 1)))
			return _ok({"cells": cells, "steps": steps, "speed": speed, "eta": int(ceil(float(steps) / speed)), "supplied": true})
		"supply":
			var s: Variant = _load(_set, "supply_%d" % int(args.get("fleetId", 0)))
			if s == null:
				s = _load("campaign", "supply_%d" % int(args.get("fleetId", 0)))
			if s == null:
				var w2 := int(_view["meta"]["width"])
				var h2 := int(_view["meta"]["height"])
				s = "1".repeat(w2 * h2)
			return _ok(s)
		"colony":
			var c2: Variant = _load(_set, "colony_%d" % int(args.get("systemId", 0)))
			if c2 == null:
				c2 = _load("campaign", "colony_%d" % int(args.get("systemId", 0)))
			if c2 == null:
				return _fail("Not your colony")
			return _ok(c2)
		"station":
			var st: Variant = _load(_set, "station_%d" % int(args.get("stationId", 0)))
			if st == null:
				return _fail("No station detail recorded")
			return _ok(st)
		"workforcePreview", "research", "forecast", "intel", "replays", "save", "fingerprint", "headlessFingerprint", "briefing":
			return _fixture(method)
		"diplomacy":
			return _fixture("diplomacy")
		"describeTurn":
			var bf := _fixture("briefing")
			return _ok(JSON.stringify(bf["result"]) if bf["ok"] else "")
		"canTerraform":
			return _ok(_can_terraform(int(args.get("fleetId", 0)), int(args.get("systemId", 0)), int(args.get("planetIndex", 0))))
		"tradeRoutes":
			return _ok(_trade_routes())
		"endTurn":
			if _view.is_empty():
				return _fail("No campaign in progress")
			if _set == "battle" and _battle_round >= 0 and not _view["session"].get("battleFinished", false):
				_view["session"]["active"] = true
				return _ok({"status": "battle", "turn": _view["meta"]["turn"]})
			var r2 := _fixture("endTurn", "campaign")
			_view["meta"]["turn"] = int(_view["meta"]["turn"]) + 1
			_view["pending"] = []
			if _set == "battle":
				_set = "campaign"
				_view["session"] = {"active": false, "battlePending": false, "battleFinished": false, "replays": 1}
			if r2["ok"]:
				r2["result"]["turn"] = _view["meta"]["turn"]
				if r2["result"].get("briefing") is Dictionary:
					r2["result"]["briefing"]["turn"] = _view["meta"]["turn"]
			return r2
		"tactical":
			if _set != "battle":
				return _ok(null)
			return _ok(_load("battle", "tactical" if _battle_round == 0 else "tacticalRound"))
		"tacticalOrder":
			return _ok(1)
		"tacticalRound":
			_battle_round += 1
			return _fixture("tacticalRound", "battle")
		"tacticalAuto":
			var e: Variant = _load("battle", "tacticalRound")
			e["finished"] = true
			_battle_round = -1
			_view["session"]["battleFinished"] = true
			return _ok(e)
		"replay":
			return _ok(_load("battle", "tacticalRound"))
		"battleReport":
			return _fail("No battle report recorded")
	return _fail("Fixture backend does not implement '%s'" % method)

func _cancel(ids: Array) -> void:
	if ids.is_empty() or _view.is_empty():
		return
	var keep := []
	for p in _view["pending"]:
		if not ids.has(int(p["id"])) and not ids.has(float(p["id"])):
			keep.append(p)
	_view["pending"] = keep

## Stand-in for the core's canTerraformPlanet: same conditions, from the recorded view.
func _can_terraform(fleet_id: int, system_id: int, planet_index: int) -> bool:
	if _view.is_empty() or not _view["meta"].get("campaignExpansion", false):
		return false
	var f := _find_fleet(fleet_id)
	if f.is_empty() or not f.get("colonyShip", false) or f.get("retreat", false) or int(f.get("attackTarget", 0)) != 0 or f.get("colonize", false):
		return false
	for s in _view["systems"]:
		if int(s["id"]) != system_id:
			continue
		if int(s["x"]) != int(f["x"]) or int(s["y"]) != int(f["y"]):
			return false
		for p in s.get("planets", []):
			if int(p["index"]) == planet_index:
				return not (str(p["class"]) in ["GasGiant", "Asteroids"] or str(p["terraformState"]) in ["Terraformed", "Uninhabitable"])
	return false

## Stand-in for the bridge's tradeRoutes query (fixtures carry no foreign populations: 60 is assumed).
func _trade_routes() -> Dictionary:
	var me := int(_view["meta"]["player"])
	var advanced: bool = _view["meta"].get("advancedRules", false)
	var rel := func(owner: int) -> Dictionary:
		for e in _view["empires"]:
			if int(e["empire"]) == owner:
				return e
		return {}
	var blocker := func(owner: int) -> String:
		var r: Dictionary = rel.call(owner)
		if r.is_empty() or not r.get("contact", false):
			return "No contact with that empire"
		if r.get("atWar", false):
			return "War blocks trade"
		if advanced and not r.get("tradeTreaty", false):
			return "An accepted trade treaty is required"
		return ""
	var sys_name := func(id: int) -> String:
		for s in _view["systems"]:
			if int(s["id"]) == id:
				return str(s["name"])
		return "System %d" % id
	var pop_of := func(id: int) -> int:
		for c in _view["colonies"]:
			if int(c["systemId"]) == id:
				return int(c["population"])
		return 60
	var routes := []
	var used := {}
	for t in _view.get("tradeRoutes", []):
		if int(t["owner"]) != me:
			continue
		used[int(t["from"])] = true
		var dst_owner := 0
		for s in _view["systems"]:
			if int(s["id"]) == int(t["to"]):
				dst_owner = int(s["owner"])
		var b: String = blocker.call(dst_owner)
		routes.append({"from": t["from"], "to": t["to"], "fromName": sys_name.call(int(t["from"])), "toName": sys_name.call(int(t["to"])), "partner": dst_owner, "partnerName": empire_label(dst_owner), "value": 5 + mini(pop_of.call(int(t["from"])), 60) / 10, "blocker": b, "active": b == ""})
	var cands := []
	for c in _view["colonies"]:
		if used.has(int(c["systemId"])):
			continue
		for s in _view["systems"]:
			var o := int(s["owner"])
			if o == 0 or o == me or not s.get("visible", false) or rel.call(o).is_empty():
				continue
			cands.append({"from": c["systemId"], "to": s["id"], "fromName": c["name"], "toName": s["name"], "partner": o, "partnerName": empire_label(o), "value": 5 + mini(int(c["population"]), 60) / 10, "blocker": blocker.call(o)})
	return {"advancedRules": advanced, "routes": routes, "candidates": cands}

func empire_label(id: int) -> String:
	for e in _load("campaign", "meta").get("empires", []):
		if int(e["id"]) == id:
			return str(e["name"])
	return "Empire %d" % id

func _find_fleet(id: int) -> Dictionary:
	for f in _view.get("fleets", []):
		if int(f["id"]) == id:
			return f
	return {}

## Mirrors the visible effect of common orders so the fixture UI reacts.
func _apply_locally(c: Dictionary) -> void:
	match str(c["type"]):
		"MoveFleet":
			var f := _find_fleet(int(c["fleetId"]))
			if not f.is_empty():
				f["targetX"] = c["targetX"]
				f["targetY"] = c["targetY"]
				f["hasOrders"] = true
				var r := _fixture_call("route", {"fleetId": f["id"], "x": c["targetX"], "y": c["targetY"]})
				f["route"] = r["result"]["cells"]
				f["eta"] = r["result"]["eta"]
		"AllocateResearch":
			var alloc: Array = c.get("researchAllocation", [])
			for i in range(min(alloc.size(), _view["empire"]["techs"].size())):
				_view["empire"]["techs"][i]["allocation"] = alloc[i]
		"RetreatFleet":
			var f3 := _find_fleet(int(c["fleetId"]))
			if not f3.is_empty():
				f3["retreat"] = true
				f3["hasOrders"] = true
		"EstablishTradeRoute":
			_view["tradeRoutes"].append({"owner": int(_view["meta"]["player"]), "from": int(c["systemId"]), "to": int(c["targetSystemId"])})
		"CancelTradeRoute":
			_view["tradeRoutes"] = _view["tradeRoutes"].filter(func(t): return not (int(t["owner"]) == int(_view["meta"]["player"]) and int(t["from"]) == int(c["systemId"])))
		"SetTacticalStance":
			var f2 := _find_fleet(int(c["fleetId"]))
			if not f2.is_empty():
				f2["stance"] = c["stance"]
