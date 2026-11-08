extends SceneTree
## Native trade-route test (M0 L6). The old test could only dry-run a made-up target system (999), because no drill showed a
## foreign colony. The diplomacy drill (seed 2024) does, after one scouting turn: its scout moves to Aldebaran (9,5), the
## Romulan colony at Romulus comes into sensor range, and tradeRoutes lists it as a candidate. The test then drives the full
## path through the real core: blocked by the treaty rule, trade treaty, Establish, listed active, Cancel.
## Run: godot --headless --audio-driver Dummy --path remaster/godot -s res://tests/test_trade_route.gd

var core: Object
var failed := 0
var checks := 0

func check(cond: bool, what: String) -> bool:
	checks += 1
	if not cond:
		failed += 1
		printerr("  FAIL trade route: %s" % what)
	return cond

func req(method: String, args := {}) -> Variant:
	var r: Dictionary = core.request(method, args)
	if not check(r.get("ok", false), "%s answered: %s" % [method, r.get("error", "")]):
		return null
	return r.get("result")

func _initialize() -> void:
	core = Engine.get_singleton("BotfCore") if Engine.has_singleton("BotfCore") else null
	if core == null:
		printerr("FAIL: BotfCore singleton missing")
		quit(1)
		return
	req("newDiplomacyDrill")
	var v: Dictionary = req("view")
	var routes: Dictionary = req("tradeRoutes")
	check(routes.candidates.size() == 0, "turn 1: no foreign colony is visible yet")
	# One scouting turn: the scout goes to (9,5), within sensor range of the Romulan colony.
	req("submit", {"command": {"type": "MoveFleet", "fleetId": int(v.fleets[0].id), "targetX": 9, "targetY": 5}})
	req("endTurn")
	routes = req("tradeRoutes")
	var cand: Dictionary = {}
	for c in routes.candidates:
		if str(c.toName) == "Romulus":
			cand = c
	if not check(not cand.is_empty(), "the Romulan colony Romulus is a visible candidate after one scouting turn"):
		quit(1)
		return
	check(int(cand.partner) == 3 and int(cand.value) > 0, "candidate names the Romulans with a positive value (%s)" % str(cand.value))
	check(str(cand.blocker).contains("trade treaty"), "advanced rules block it until a trade treaty exists (%s)" % cand.blocker)
	var est := {"type": "EstablishTradeRoute", "systemId": int(cand.from), "targetSystemId": int(cand.to), "targetEmpire": int(cand.partner)}
	var dry: Dictionary = req("dryRun", {"command": est})
	check(dry.ok == false and str(dry.error).contains("trade treaty"), "dryRun refuses without the treaty")
	# Treaty: propose, resolve two turns.
	req("submit", {"command": {"type": "ProposeTradeTreaty", "targetEmpire": 3}})
	var treaty := false
	for i in 3:
		req("endTurn")
		for e in req("view").empires:
			if int(e.empire) == 3:
				treaty = bool(e.tradeTreaty)
		if treaty:
			break
	if not check(treaty, "the Romulans accept the trade treaty within 3 turns"):
		quit(1)
		return
	routes = req("tradeRoutes")
	cand = {}
	for c in routes.candidates:
		if str(c.toName) == "Romulus":
			cand = c
	check(not cand.is_empty() and str(cand.blocker) == "", "Romulus is now an unblocked candidate")
	dry = req("dryRun", {"command": est})
	check(dry.ok == true, "dryRun accepts the route: %s" % str(dry.error))
	req("submit", {"command": est})
	routes = req("tradeRoutes")
	check(routes.routes.size() == 1 and bool(routes.routes[0].active) and int(routes.routes[0].to) == int(cand.to), "the established route is listed and active")
	req("submit", {"command": {"type": "CancelTradeRoute", "systemId": int(cand.from), "targetEmpire": int(cand.partner)}})
	req("endTurn")
	check(req("tradeRoutes").routes.size() == 0, "the route is gone after the cancel resolves")
	print("trade route: %d checks, %d failures" % [checks, failed])
	quit(1 if failed > 0 else 0)
