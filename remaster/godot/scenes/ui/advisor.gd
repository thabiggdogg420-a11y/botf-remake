class_name Advisor
extends RefCounted
## F3 advisor and first-run tutorial. One piece of guidance at a time, each with a single action the player can take
## from the advisor panel (AdvisorPanel). The tutorial teaches the turn loop on a new player's first campaign:
## chart -> home system -> build -> research -> send a ship -> end turn -> read the captain's log. After it, the advisor
## reads the empire each turn (EndTurnVM's idle checks, exploration, colony ships, battles, open reports) and offers the
## next thing worth doing, ending with "End the turn" when every station reports ready.
## Presentation only: every action is an order the player could give from the screens, issued through the same
## calls (no rule changes, nothing the core does not already allow; each command is dry-run first).
##
## guidance() -> {id, title, text, tone, act, act_label, step, steps, tutorial: bool, dismiss: bool}
##   act: "next" | "goto:<screen>" | "build" | "balance" | "explore" | "colonize" | "fix" | "end_turn" | "close" |
##        "auto_battle" | "continue_battle" | "dismiss"

const STEPS := [
	{"id": "welcome", "title": "Welcome, Captain",
		"text": "This is the star chart. Each turn you give orders, then end the turn and the galaxy moves. Your home system glows in your colours.",
		"act": "next", "act_label": "Begin"},
	{"id": "system", "title": "Your home system",
		"text": "Open your home system to see its planets, its people and what it is building.",
		"act": "goto:system", "act_label": "Open system"},
	{"id": "build", "title": "Build something",
		"text": "Industry builds the first item in the queue each turn. An empty queue wastes it. Add a structure.",
		"act": "build", "act_label": "Queue a build"},
	{"id": "research", "title": "Research",
		"text": "Labs pool research across the empire. This screen shows what you are studying and what comes next.",
		"act": "goto:research", "act_label": "Open research"},
	{"id": "explore", "title": "Send a ship out",
		"text": "Uncharted stars hide colonies, races and danger. Send a ship to the nearest one.",
		"act": "explore", "act_label": "Set course"},
	{"id": "end_turn", "title": "End the turn",
		"text": "Orders are set. End the turn and the galaxy moves: ships fly, colonies grow, research advances.",
		"act": "end_turn", "act_label": "End turn"},
	{"id": "log", "title": "The captain's log",
		"text": "Each turn ends with your log: what happened and what needs you. Read it, then close it.",
		"act": "close", "act_label": "Close log"},
	{"id": "handover", "title": "Advisor on station",
		"text": "That is the turn loop. From now on I will point out the next thing worth doing. Follow me, or play your own way.",
		"act": "next", "act_label": "Understood"},
]

var app: App
var _dismissed := {}     ## hint id -> turn it was set aside
var _system_vm: SystemVM

func _init(app_: App) -> void:
	app = app_

# ---------------------------------------------------------------------------------------------
# Tutorial state (settings: "tutorial" = step index, -1 when finished or skipped)
# ---------------------------------------------------------------------------------------------
func tutorial_step() -> int:
	return int(app.settings.get("tutorial", 0))

func tutorial_active() -> bool:
	var s := tutorial_step()
	return s >= 0 and s < STEPS.size()

func _advance() -> void:
	var s := tutorial_step() + 1
	app.settings["tutorial"] = s if s < STEPS.size() else -1
	AppSaves.store_settings(app.settings)

func skip_tutorial() -> void:
	app.settings["tutorial"] = -1
	AppSaves.store_settings(app.settings)

func restart_tutorial() -> void:
	app.settings["tutorial"] = 0
	AppSaves.store_settings(app.settings)

func turn() -> int:
	return int(app.view.get("meta", {}).get("turn", 0))

# ---------------------------------------------------------------------------------------------
# What to say now
# ---------------------------------------------------------------------------------------------
func guidance() -> Dictionary:
	if app.view.is_empty():
		return {}
	var urgent := _situational()   # a battle or an open report outranks the lesson
	if not urgent.is_empty():
		return urgent
	if tutorial_active():
		var i := tutorial_step()
		var st: Dictionary = STEPS[i].duplicate()
		st["step"] = i + 1
		st["steps"] = STEPS.size()
		st["tutorial"] = true
		st["tone"] = "primary"
		st["dismiss"] = false
		if st["id"] == "log" and not app.has_modal():
			_advance()          # no log open (a battle turn): nothing to close
			return guidance()
		if st["id"] == "explore" and _explore_target().is_empty():
			_advance()
			return guidance()
		return st
	return _hint()

## Battles and modal reports: what must be answered before anything else.
func _situational() -> Dictionary:
	if app.screen == "combat":
		var s: Dictionary = app.view.get("session", {})
		if s.get("battleFinished", false) or not s.get("active", false):
			return _g("battle_done", "Battle over", "The battle is decided. Continue the turn to see its outcome in your log.", "good", "continue_battle", "Continue turn")
		return _g("battle", "Battle joined", "Your ships are engaged. Command them round by round, or let your captains fight it out.", "alert", "auto_battle", "Auto-resolve")
	if app.has_modal():
		var log_step := tutorial_active() and str(STEPS[tutorial_step()]["id"]) == "log"
		if log_step:
			return {}
		return _g("report", "Report waiting", "Read the report on screen, then close it to carry on.", "info", "close", "Close")
	return {}

## After the tutorial: the next useful thing, from the end-turn check first.
func _hint() -> Dictionary:
	var check := EndTurnVM.build(app.session)
	for w in check["warnings"]:
		var id := "%s-%d-%d" % [w["kind"], int(w["system"]), int(w["fleet"])]
		if _dismissed.get(id, -1) == turn():
			continue
		match str(w["kind"]):
			"queue":
				var g := _g(id, "Idle shipyard", str(w["text"]), "warn", "build", "Queue a build")
				g["system"] = int(w["system"])
				if w.has("fix") and str(w["fix"].get("reason", "x")) == "":
					g["act"] = "fix"
					g["act_label"] = str(w["fix"]["label"])
					g["command"] = w["fix"]["command"]
				return g
			"food", "workers":
				var g2 := _g(id, "Feed your people" if w["kind"] == "food" else "Idle citizens", str(w["text"]), str(w["tone"]), "balance", "Balance workforce")
				g2["system"] = int(w["system"])
				return g2
			"power":
				var g3 := _g(id, "Power shortage", str(w["text"]) + " Build a power plant.", "warn", "build", "Queue power plant")
				g3["system"] = int(w["system"])
				g3["prefer"] = "power"
				return g3
			"colony-ship":
				var c := _colony_target(app.find_by_id("fleets", int(w["fleet"])))
				if not c.is_empty():
					var g4 := _g(id, "Colony ship waiting", str(w["text"]) + " " + str(c["why"]), "good", "colonize", str(c["label"]))
					g4["fleet"] = int(w["fleet"])
					return g4
			"idle-fleet":
				if not _explore_target().is_empty():
					return _g(id, "Ships standing by", str(w["text"]) + " Send one to chart the nearest unknown star.", "info", "explore", "Set course")
			"credits":
				return _g(id, "Treasury", str(w["text"]) + " Check the empire screen for upkeep.", str(w["tone"]), "dismiss", "Understood")
	return _g("ready", "All stations ready", "Every colony is busy and every ship has orders. End the turn.", "good", "end_turn", "End turn")

func _g(id: String, title: String, text: String, tone: String, act: String, label_: String) -> Dictionary:
	return {"id": id, "title": title, "text": text, "tone": tone, "act": act, "act_label": label_, "tutorial": false, "dismiss": act != "end_turn" and act != "dismiss", "step": 0, "steps": 0}

func dismiss(g: Dictionary) -> void:
	if g.get("tutorial", false):
		skip_tutorial()
	else:
		_dismissed[str(g["id"])] = turn()

# ---------------------------------------------------------------------------------------------
# Doing it
# ---------------------------------------------------------------------------------------------
## Performs the guidance's action. Returns "" or the reason it could not.
func perform(g: Dictionary) -> String:
	var act := str(g.get("act", ""))
	var why := ""
	match act:
		"next":
			pass
		"dismiss":
			_dismissed[str(g["id"])] = turn()
		"close":
			app.close_all_modals()
		"build":
			why = _build(int(g.get("system", 0)), str(g.get("prefer", "")))
		"fix":
			if not app.issue(g["command"], [], "Default build template applied."):
				why = "The template was refused"
		"balance":
			why = _balance(int(g.get("system", 0)))
		"explore":
			if app.screen != "galaxy":
				app.goto("galaxy")
			why = _explore()
		"colonize":
			why = _colonize(int(g.get("fleet", 0)))
		"end_turn":
			app.close_all_modals()
			if app.screen != "galaxy" and app.screen != "combat":
				app.goto("galaxy")
			BotfAudioSystem.cue_now("end_turn")
			if app.end_turn() == "error":
				why = "The turn did not end"
		"auto_battle":
			var cs := app.current_screen()
			if cs and cs.has_method("_auto"):
				cs._auto()
			else:
				why = "No battle screen"
		"continue_battle":
			app.end_turn()
		_:
			if act.begins_with("goto:"):
				app.goto(act.substr(5))
	if why == "" and g.get("tutorial", false):
		_advance()
	elif not g.get("tutorial", false) and act not in ["end_turn", "close", "auto_battle", "continue_battle"]:
		_dismissed[str(g["id"])] = turn()   # one offer per hint per turn: done, or it could not be done
	return why

func _home_sid() -> int:
	var cols: Array = app.view.get("colonies", [])
	return int(cols[0]["systemId"]) if cols.size() > 0 else 0

func _sysvm(sid: int) -> SystemVM:
	if _system_vm == null:
		_system_vm = SystemVM.new(app.session)
	if sid == 0:
		sid = _home_sid()
	app.select_system(sid)
	_system_vm.fetch(sid)
	return _system_vm

## Queues the first structure the core will accept (a power plant when `prefer` is "power", farms when food is short).
func _build(sid: int, prefer: String) -> String:
	var vm := _sysvm(sid)
	if vm.detail.is_empty():
		return "No colony"
	var opts: Array = vm.choices(false).filter(func(ch): return str(ch.get("blocker", "")) == "")
	if opts.is_empty():
		opts = vm.choices(true).filter(func(ch): return str(ch.get("blocker", "")) == "")
	if opts.is_empty():
		return "Nothing can be built here yet"
	var food_short := int(vm.colony()["economy"].get("foodDeficit", 0)) > 0
	var want := "power" if prefer == "power" else ("farm" if food_short else "")
	var pick: Dictionary = opts[0]
	if want != "":
		for ch in opts:
			if str(ch.get("product", "")).to_lower().contains(want) or str(ch.get("name", "")).to_lower().contains(want):
				pick = ch
				break
	return "" if vm.enqueue(pick) else "The build was refused"

func _balance(sid: int) -> String:
	var vm := _sysvm(sid)
	if vm.detail.is_empty():
		return "No colony"
	vm.auto_balance(false)
	return ""

## Nearest uncharted star a ship can reach that no other ship is already heading for: {fleet, x, y}.
func _explore_target(skip: Dictionary = {}) -> Dictionary:
	var v := app.view
	var heading := skip.duplicate()
	for f in v.get("fleets", []):
		if int(f.get("owner", 0)) == app.player() and (f.get("route", []) as Array).size() > 0:
			heading["%d,%d" % [int(f.get("targetX", -1)), int(f.get("targetY", -1))]] = true
	var best := {}
	var best_d := 1e9
	for f in v.get("fleets", []):
		if int(f.get("owner", 0)) != app.player() or f.get("colonyShip", false) or not EndTurnVM.is_idle(app.session, f):
			continue
		var fx := int(f["x"])
		var fy := int(f["y"])
		for s in v.get("systems", []):
			if AppSession.is_charted(s) or heading.has("%d,%d" % [int(s["x"]), int(s["y"])]):
				continue
			var d := Vector2(int(s["x"]) - fx, int(s["y"]) - fy).length()
			if d < best_d:
				best_d = d
				best = {"fleet": f, "x": int(s["x"]), "y": int(s["y"])}
	return best

func _explore() -> String:
	var skip := {}
	for attempt in 8:
		var t := _explore_target(skip)
		if t.is_empty():
			return "No uncharted star in reach"
		var f: Dictionary = t["fleet"]
		var cmd := GalaxyOrders.move_command(f, int(t["x"]), int(t["y"]))
		if app.dry_run(cmd, GalaxyOrders.replace_ids(app, f)) == "":
			return "" if GalaxyOrders.plot_course(app, f, int(t["x"]), int(t["y"])) else "The course was refused"
		skip["%d,%d" % [int(t["x"]), int(t["y"])]] = true   # out of range: try the next nearest
	return "No reachable star"

## Where a colony ship should go: settle here when the core allows it, else the nearest charted free world.
func _colony_target(f: Dictionary) -> Dictionary:
	if f.is_empty():
		return {}
	var cc := {"type": "Colonize", "fleetId": int(f["id"])}
	if app.dry_run(cc, GalaxyOrders.replace_ids(app, f)) == "":
		var here := app.system_at(int(f["x"]), int(f["y"]))
		return {"label": "Colonize", "why": "It can settle %s now." % here.get("name", "this system"), "here": true}
	var best := {}
	var best_d := 1e9
	for s in app.view.get("systems", []):
		if not AppSession.is_charted(s) or int(s.get("owner", 0)) != 0 or int(s.get("maxCapacity", 0)) <= 0:
			continue
		var d := Vector2(int(s["x"]) - int(f["x"]), int(s["y"]) - int(f["y"])).length()
		if d < best_d and app.dry_run(GalaxyOrders.move_command(f, int(s["x"]), int(s["y"])), GalaxyOrders.replace_ids(app, f)) == "":
			best_d = d
			best = {"label": "Send to %s" % s["name"], "why": "%s is a free world in reach." % s["name"], "x": int(s["x"]), "y": int(s["y"])}
	return best

func _colonize(fleet_id: int) -> String:
	var f := app.find_by_id("fleets", fleet_id)
	var t := _colony_target(f)
	if t.is_empty():
		return "No world to settle"
	if t.get("here", false):
		return "" if app.issue({"type": "Colonize", "fleetId": fleet_id}, GalaxyOrders.replace_ids(app, f), "Colony ship ordered to settle.") else "Refused"
	return "" if GalaxyOrders.plot_course(app, f, int(t["x"]), int(t["y"])) else "Refused"
