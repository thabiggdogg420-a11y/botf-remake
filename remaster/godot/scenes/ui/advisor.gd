class_name Advisor
extends RefCounted
## F3 advisor and first-run tutorial. One piece of guidance at a time. The tutorial (ONBOARDING-AND-MOBILE-PLAN.md 1.2) is five
## hands-on beats over the first three turns: each dims the screen but one control, pulses it, says one short line (12 words
## at most) and moves on only when the player has DONE the thing. Research, diplomacy, first contact and the first battle are
## taught when they first happen (EVENT_BEATS), never up front. After the tutorial the advisor reads the empire each turn
## (EndTurnVM's idle checks, exploration, colony ships, battles, open reports) and offers the next thing worth doing, ending
## with "End the turn" when every station reports ready.
## Presentation only: every action is an order the player could give from the screens, issued through the same calls (no rule
## changes, nothing the core does not already allow; each command is dry-run first).
##
## guidance() -> {id, title, text, tone, act, act_label, step, steps, tutorial: bool, beat: bool, dismiss: bool}
##   act: "next" | "goto:<screen>" | "build" | "balance" | "explore" | "colonize" | "fix" | "end_turn" | "close" |
##        "auto_battle" | "continue_battle" | "dismiss"
## A beat carries no button: its `act` is only what perform() does when a test (or a future "show me") drives it.
## targets(g) -> the controls it points at: node names (a trailing * matches the first enabled button) or screen Rect2s.

## The five beats. `text` is the one line, `min_turn` the first turn it may appear on.
const STEPS := [
	{"id": "scout", "text": "Send your scout to explore. Tap it, then a star.", "act": "explore", "min_turn": 0},
	{"id": "build", "text": "Your home system. Pick something to build.", "act": "build", "min_turn": 0},
	{"id": "end_turn", "text": "Orders set. End the turn.", "act": "end_turn", "min_turn": 0},
	{"id": "log", "text": "What happened this turn. Close it when done.", "act": "close", "min_turn": 0},
	{"id": "colony", "text": "A free world in reach. Send the colony ship.", "act": "colonize", "min_turn": 1},
]
## The colony beat gives up after this turn (no free world in reach, or the player went their own way).
const COLONY_BEAT_LAST_TURN := 5

## Taught when it first happens, one short line each. `%s` is the diplomacy tab's name for the player's power.
const EVENT_BEATS := {
	"contact": {"text": "First contact. Open the %s tab.", "act": "goto:diplomacy"},
	"battle": {"text": "Your first battle. Command it, or auto-resolve.", "act": "auto_battle"},
}

## The screens each tutorial step is shown on (R8, Gemini audit): a step not listed shows on every screen.
const STEP_SCREENS := {
	"scout": ["galaxy"],
	"build": ["galaxy", "system"],
	"colony": ["galaxy"],
	"contact": ["galaxy", "system", "empire", "research", "fleets", "intel", "database"],
}

## The advisor's own words (the wordiness pass, 1.4): every string is 20 words or fewer with the longest name filled in, one
## idea per line. test_wordiness.gd checks them all. [title, text]; %s is a colony or system name, %d a count.
const TEXTS := {
	"battle": ["Battle joined", "Your ships are engaged. Command them round by round, or let your captains fight it out."],
	"battle_done": ["Battle over", "The battle is decided. Continue the turn to see its outcome in your log."],
	"aftermath": ["A world is taken", "Choose what happens to the conquered world. I will take the first choice offered."],
	"report": ["Captain's log", "Read the log, then close it to carry on."],
	"queue": ["Idle shipyard", "%s has an empty queue. Its industry goes to waste."],
	"food": ["Feed your people", "%s is short of food. Its people will shrink."],
	"workers": ["Idle citizens", "%s has idle citizens producing nothing."],
	"power": ["Power shortage", "%s has unpowered structures. Build a power plant."],
	"colony_ship": ["Colony ship waiting", "A colony ship has no orders. %s"],
	"colony_here": ["", "It can settle %s now."],
	"colony_free": ["", "%s is a free world in reach."],
	"idle_fleet": ["Ships standing by", "Ships have no orders. Send one to chart the nearest unknown star."],
	"credits": ["Treasury", "Credits are running low. Check upkeep on the empire screen."],
	"ready": ["All stations ready", "Every colony is busy and every ship has orders. End the turn."],
}

var app: App
var _set_aside := {}     ## situation id ("battle", "report"...) -> true while "Not now" holds; cleared when the situation ends
var _dismissed := {}     ## hint id -> turn it was set aside
var _system_vm: SystemVM
var _taught := {}        ## event beat id -> true once it has been taught (or the player skipped the tutorial)
var _skipped := false    ## "Skip tutorial" was pressed in this game: no beats, no event beats
var _base := {"idx": -1, "turn": 0, "builds": 0}   ## where the current beat started, to tell when the player has done it

func _init(app_: App) -> void:
	app = app_

## A new campaign: nothing taught yet, nothing skipped.
func new_game() -> void:
	_taught.clear()
	_skipped = false
	_set_aside.clear()
	_dismissed.clear()
	_base = {"idx": -1, "turn": 0, "builds": 0}

# ---------------------------------------------------------------------------------------------
# Tutorial state (settings: "tutorial" = beat index, -1 when finished or skipped)
# ---------------------------------------------------------------------------------------------
## Settings saved before the beats (the 8-card tutorial: welcome, system, build, research, explore, end turn, log, handover)
## hold an index into that list. It maps to the beat that teaches the same thing next; the card list is gone.
const OLD_STEP_TO_BEAT := [0, 1, 1, 1, 0, 2, 3, -1]

func tutorial_step() -> int:
	var s := int(app.settings.get("tutorial", 0))
	if s >= 0 and int(app.settings.get("tutorialBeats", 0)) < 1:
		s = OLD_STEP_TO_BEAT[s] if s < OLD_STEP_TO_BEAT.size() else -1
		app.settings["tutorial"] = s
		app.settings["tutorialBeats"] = 1
		AppSaves.store_settings(app.settings)
	return s

func tutorial_active() -> bool:
	if "tutorial_held" in app and app.tutorial_held:
		return false
	var s := tutorial_step()
	return s >= 0 and s < STEPS.size()

func _advance() -> void:
	var s := tutorial_step() + 1
	app.settings["tutorial"] = s if s < STEPS.size() else -1
	AppSaves.store_settings(app.settings)

## Whether this guidance may be drawn on `screen`: a tutorial step only on the screens it teaches from.
func shown_on(g: Dictionary, screen: String) -> bool:
	if not g.get("tutorial", false):
		return true
	return not STEP_SCREENS.has(str(g.get("id", ""))) or screen in STEP_SCREENS[str(g["id"])]

func skip_tutorial() -> void:
	_skipped = true
	app.settings["tutorial"] = -1
	app.settings["tutorialBeats"] = 1
	AppSaves.store_settings(app.settings)

func restart_tutorial() -> void:
	if "tutorial_held" in app:
		app.tutorial_held = false
	_skipped = false
	_taught.clear()
	_base["idx"] = -1
	app.settings["tutorial"] = 0
	app.settings["tutorialBeats"] = 1
	AppSaves.store_settings(app.settings)

func turn() -> int:
	return int(app.view.get("meta", {}).get("turn", 0))

## Event beats run while the tutorial does, and through the whole of a Quick Start game unless the player skipped it.
func events_on() -> bool:
	if _skipped or ("tutorial_held" in app and app.tutorial_held):
		return false
	return tutorial_active() or ("tutorial_game" in app and app.tutorial_game)

# ---------------------------------------------------------------------------------------------
# What to say now
# ---------------------------------------------------------------------------------------------
func guidance() -> Dictionary:
	if app.view.is_empty():
		return {}
	var urgent := _situational()   # a battle or an open report outranks the lesson
	_prune_set_aside(urgent)
	if urgent.get("hide", false):
		return {}
	if not urgent.is_empty() and _set_aside.has(str(urgent.get("id", ""))):
		return {}   # "Not now" on a battle, log or aftermath card: it stays away until that situation ends
	if not urgent.is_empty():
		return urgent
	for k in STEPS.size() + 1:
		if not tutorial_active():
			break
		var b := _beat()
		if b.get("again", false):
			continue          # that beat was done or had nothing to teach: look at the next
		if b.is_empty():
			break             # the next beat is not due yet
		return b              # a tutorial beat that is due comes before an event line
	var ev := _event_beat()
	if not ev.is_empty():
		return ev
	return _hint()

## The current tutorial beat as guidance; {"again": true} after it moved the tutorial on (ask again); {} while it waits for its turn.
func _beat() -> Dictionary:
	var i := tutorial_step()
	var st: Dictionary = STEPS[i].duplicate()
	var id := str(st["id"])
	if int(_base["idx"]) != i:
		_base = {"idx": i, "turn": turn(), "builds": _builds()}
	if int(st["min_turn"]) > turn():
		return {}
	if id == "colony" and turn() > COLONY_BEAT_LAST_TURN:
		_advance()
		return {"again": true}
	if _beat_done(id) or _beat_unneeded(id):
		_advance()
		return {"again": true}
	st["title"] = ""
	st["act_label"] = ""
	st["step"] = i + 1
	st["steps"] = STEPS.size()
	st["tutorial"] = true
	st["beat"] = true
	st["tone"] = "primary"
	st["dismiss"] = false
	if id == "colony":
		var cs := _idle_colony_ship()
		st["fleet"] = int(cs.get("id", 0))
	return st

## Whether the player has done what the beat asks. The check reads the empire, never a button press.
func _beat_done(id: String) -> bool:
	match id:
		"scout":
			for f in app.view.get("fleets", []):
				if int(f.get("owner", 0)) == app.player() and not f.get("colonyShip", false) and not EndTurnVM.is_idle(app.session, f):
					return true
			return false
		"build":
			return _builds() > int(_base["builds"])
		"end_turn":
			return turn() > int(_base["turn"])
		"log":
			return not app.has_modal()   # the log was open (or never opened on a battle turn) and is now closed
		"colony":
			return _idle_colony_ship().is_empty() and _own_colony_ship_count() > 0
	return false

## A beat with nothing to teach in this game (no scout to send, no home to build in, no colony ship).
func _beat_unneeded(id: String) -> bool:
	match id:
		"scout":
			return _explore_target().is_empty()
		"build":
			return app.view.get("colonies", []).is_empty()
		"colony":
			return _own_colony_ship_count() == 0
	return false

## The captain's log opens short while the tutorial's log beat is up: the headlines and Close, not the whole chronicle.
func log_short() -> bool:
	if not tutorial_active():
		return false
	var id := str(STEPS[tutorial_step()]["id"])
	# the poll that moves the tutorial on from End turn to the log comes after the log opens, so End turn done counts too
	return id == "log" or (id == "end_turn" and int(_base["idx"]) == tutorial_step() and turn() > int(_base["turn"]))

func _builds() -> int:
	var n := 0
	for o in app.view.get("pending", []):
		if str(o.get("type", "")).begins_with("Enqueue"):
			n += 1
	return n

func _own_colony_ship_count() -> int:
	var n := 0
	for f in app.view.get("fleets", []):
		if int(f.get("owner", 0)) == app.player() and f.get("colonyShip", false):
			n += 1
	return n

## An own colony ship with no orders and a world it could be sent to, else {}.
func _idle_colony_ship() -> Dictionary:
	for f in app.view.get("fleets", []):
		if int(f.get("owner", 0)) == app.player() and f.get("colonyShip", false) and EndTurnVM.is_idle(app.session, f):
			if not _colony_target(f).is_empty():
				return f
	return {}

## First contact and the first battle: one short line, once, with the control that opens it spotlit.
func _event_beat() -> Dictionary:
	if not events_on():
		return {}
	if not _taught.has("contact") and _contact_made():
		if not _taught.has("contact_turn"):
			_taught["contact_turn"] = turn()   # the turn the line first shows
		if app.screen == "diplomacy" or turn() >= int(_taught["contact_turn"]) + 3:
			_taught["contact"] = true   # opened, or left alone for three turns
			return {}
		var tab := TrekText.rail_label(app.player(), "diplomacy", "Diplomacy")
		return _event_g("contact", EVENT_BEATS["contact"]["text"] % tab, EVENT_BEATS["contact"]["act"], "")
	return {}

func _event_g(id: String, text: String, act: String, label_: String) -> Dictionary:
	return {"id": id, "title": "", "text": text, "tone": "primary", "act": act, "act_label": label_, "step": 0, "steps": 0,
		"tutorial": true, "beat": true, "event": true, "dismiss": false}

## A major power or a minor race has been met (the same test lights the Diplomats and Fleet Intel tabs).
func _contact_made() -> bool:
	return TabReveal.contact_made(app.view)

## Battles and modal reports: what must be answered before anything else.
func _situational() -> Dictionary:
	if app.screen == "combat":
		var s: Dictionary = app.view.get("session", {})
		if s.get("battleFinished", false) or not s.get("active", false):
			_taught["battle"] = true
			return _sit(_t("battle_done", "good", "continue_battle", "Continue turn"))
		if events_on() and not _taught.has("battle"):
			var bg := _event_g("battle", EVENT_BEATS["battle"]["text"], EVENT_BEATS["battle"]["act"], "Auto-resolve")
			bg["situational"] = true
			return bg
		return _sit(_t("battle", "alert", "auto_battle", "Auto-resolve"))
	if app.has_modal():
		var top: Control = app._modals.back()
		if top.find_child("Aftermath", true, false) != null:
			return _sit(_t("aftermath", "warn", "aftermath", "Decide"))
		var report := top.find_child("Briefing", true, false) != null or top.find_child("Events", true, false) != null
		if report and log_short():
			return {}   # the tutorial's log beat speaks for the log (the poll that advances to it has not run yet)
		if report:
			return _sit(_t("report", "info", "close", "Close log"))
		return {"hide": true}   # a menu, a confirmation or a hail: the player is busy with it
	return {}

## After the tutorial: the next useful thing, from the end-turn check first.
func _hint() -> Dictionary:
	var check := EndTurnVM.build(app.session)
	for w in check["warnings"]:
		var id := "%s-%d-%d" % [w["kind"], int(w["system"]), int(w["fleet"])]
		if _dismissed.get(id, -1) == turn():
			continue
		var cname := str(app.find_by_id("colonies", int(w["system"]), "systemId").get("name", "A colony"))
		match str(w["kind"]):
			"queue":
				var g := _t("queue", "warn", "build", "Queue a build", id, [cname])
				g["system"] = int(w["system"])
				if w.has("fix") and str(w["fix"].get("reason", "x")) == "":
					g["act"] = "fix"
					g["act_label"] = str(w["fix"]["label"])
					g["command"] = w["fix"]["command"]
				return g
			"food", "workers":
				var g2 := _t(str(w["kind"]), str(w["tone"]), "balance", "Balance workforce", id, [cname])
				g2["system"] = int(w["system"])
				return g2
			"power":
				var g3 := _t("power", "warn", "build", "Queue power plant", id, [cname])
				g3["system"] = int(w["system"])
				g3["prefer"] = "power"
				return g3
			"colony-ship":
				var c := _colony_target(app.find_by_id("fleets", int(w["fleet"])))
				if not c.is_empty():
					var g4 := _t("colony_ship", "good", "colonize", str(c["label"]), id, [str(c["why"])])
					g4["fleet"] = int(w["fleet"])
					return g4
			"idle-fleet":
				if not _explore_target().is_empty():
					return _t("idle_fleet", "info", "explore", "Set course", id)
			"credits":
				return _t("credits", str(w["tone"]), "dismiss", "Understood", id)
	return _t("ready", "good", "end_turn", "End turn")

## Guidance from a TEXTS entry: its title and text (with `args` filled in), the tone, the action and its label.
func _t(key: String, tone: String, act: String, label_: String, id: String = "", args: Array = []) -> Dictionary:
	var e: Array = TEXTS[key]
	var text := str(e[1]) % args if args.size() > 0 else str(e[1])
	return _g(id if id != "" else key, str(e[0]), text, tone, act, label_)

## The controls a piece of guidance points at: node names (a trailing * matches the first enabled button), or Rect2s of the
## screen for things drawn on the star chart. The first one found is highlighted, and the card is placed clear of all of them
## (the lead's F3 review: the card sat over the Workforce rows it was teaching about).
func targets(g: Dictionary) -> Array:
	var out: Array = []
	if g.get("beat", false):
		return _beat_targets(str(g.get("id", "")))
	var act := str(g.get("act", ""))
	match act:
		"build":
			out.assign(["Construction", "Workforce"])
		"balance":
			out.assign(["WorkforceBalance", "Workforce"])
		"end_turn":
			out.assign(["EndTurn"])
		_:
			if act.begins_with("goto:"):
				out.assign(["Rail_" + act.substr(5)])
	return out

func _beat_targets(id: String) -> Array:
	match id:
		"scout":
			var scout := _scout()
			if scout.is_empty():
				return []
			if int(app.selection.get("fleetId", 0)) != int(scout["id"]):
				return ["Unit_%d" % int(scout["id"]), _cell_rect(int(scout["x"]), int(scout["y"]))]
			var t := _explore_target()
			return [_cell_rect(int(t["x"]), int(t["y"]))] if not t.is_empty() else []
		"build":
			return ["Rail_system"] if app.screen != "system" else ["Choice_*"]
		"end_turn":
			return ["EndTurn"]
		"log":
			return ["Acknowledge"]
		"colony":
			var ship := _idle_colony_ship()
			if ship.is_empty():
				return []
			if int(app.selection.get("fleetId", 0)) != int(ship["id"]):
				return ["Unit_%d" % int(ship["id"]), _cell_rect(int(ship["x"]), int(ship["y"]))]
			var c := _colony_target(ship)
			return [_cell_rect(int(c["x"]), int(c["y"]))] if c.has("x") else []
		"contact":
			return ["Rail_diplomacy"]
		"battle":
			return ["AutoResolve"]
	return []

## The idle scout: the first own ship that is not a colony ship and has no orders.
func _scout() -> Dictionary:
	for f in app.view.get("fleets", []):
		if int(f.get("owner", 0)) == app.player() and not f.get("colonyShip", false) and EndTurnVM.is_idle(app.session, f):
			return f
	return {}

## A sector on the star chart as a screen rect (the chart is 3D; the rect is where the sector's star is drawn now).
func _cell_rect(x: int, y: int) -> Rect2:
	var gs := app.current_screen()
	if gs == null or not ("map" in gs) or gs.map == null:
		return Rect2()
	var m: Control = gs.map
	var p: Vector2 = m.get_global_transform_with_canvas() * m.cell_screen_pos(x, y)
	return Rect2(p - Vector2(46, 46), Vector2(92, 92))

## The tutorial's tap-a-star: while the scout (or colony) beat is up and that ship is selected, a left click on a star sends it
## there. Returns true when the click was used. No other game gets this; the right-click menu is unchanged.
func beat_click(cell: int) -> bool:
	if not tutorial_active():
		return false
	var id := str(STEPS[tutorial_step()]["id"])
	if id != "scout" and id != "colony":
		return false
	var f := GalaxyOrders.selected_unit(app)
	if f.is_empty() or int(f.get("owner", 0)) != app.player() or bool(f.get("colonyShip", false)) != (id == "colony"):
		return false
	var x := cell % app.width()
	var y := cell / app.width()
	if (x == int(f["x"]) and y == int(f["y"])) or (app.system_at(x, y).is_empty() and app.session.uncharted_at(x, y).is_empty()):
		return false   # not a star: the click is an ordinary select
	if app.dry_run(GalaxyOrders.move_command(f, x, y), GalaxyOrders.replace_ids(app, f)) != "":
		return false   # out of reach: the click is an ordinary select, and the hover card says why
	return GalaxyOrders.plot_course(app, f, x, y)

func _sit(g: Dictionary) -> Dictionary:
	g["situational"] = true
	return g

func _g(id: String, title: String, text: String, tone: String, act: String, label_: String) -> Dictionary:
	return {"id": id, "title": title, "text": text, "tone": tone, "act": act, "act_label": label_, "tutorial": false, "dismiss": act != "end_turn" and act != "dismiss", "step": 0, "steps": 0}

func dismiss(g: Dictionary) -> void:
	if g.get("tutorial", false):
		skip_tutorial()
	else:
		_dismissed[str(g["id"])] = turn()
		if g.get("situational", false):
			_set_aside[str(g["id"])] = true

## Situational cards (battle, battle over, report, aftermath) are not hints, so the per-turn _dismissed list never applied
## to them and "Not now" re-showed the card on the next poll (round 9 browser drive). A set-aside lasts while the same
## situation lasts and is forgotten once something else is on screen.
func _prune_set_aside(urgent: Dictionary) -> void:
	for k in _set_aside.keys():
		if str(urgent.get("id", "")) != k:
			_set_aside.erase(k)

# ---------------------------------------------------------------------------------------------
# Doing it
# ---------------------------------------------------------------------------------------------
## Performs the guidance's action. Returns "" or the reason it could not. A beat moves on by itself once the empire shows
## the thing done, so performing one (a test, a future "show me") needs no step counter.
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
		"aftermath":
			var top: Control = app._modals.back() if app.has_modal() else null
			var picks: Array = (top.find_children("Aftermath_*", "Button", true, false) if top else []).filter(func(b): return not (b as Button).disabled)
			if picks.is_empty():
				why = "No decision on screen"
			else:
				(picks[0] as Button).pressed.emit()
		_:
			if act.begins_with("goto:"):
				app.goto(act.substr(5))
	if why == "" and not g.get("tutorial", false) and act not in ["end_turn", "close", "auto_battle", "continue_battle", "aftermath"]:
		_dismissed[str(g["id"])] = turn()   # one offer per hint per turn: done, or it could not be done
	if why == "" and str(g.get("id", "")) == "battle" and g.get("event", false):
		_taught["battle"] = true
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
		return {"label": "Colonize", "why": TEXTS["colony_here"][1] % str(here.get("name", "this system")), "here": true, "x": int(f["x"]), "y": int(f["y"])}
	var best := {}
	var best_d := 1e9
	for s in app.view.get("systems", []):
		if not AppSession.is_charted(s) or int(s.get("owner", 0)) != 0 or int(s.get("maxCapacity", 0)) <= 0:
			continue
		var d := Vector2(int(s["x"]) - int(f["x"]), int(s["y"]) - int(f["y"])).length()
		if d < best_d and app.dry_run(GalaxyOrders.move_command(f, int(s["x"]), int(s["y"])), GalaxyOrders.replace_ids(app, f)) == "":
			best_d = d
			best = {"label": "Send to %s" % s["name"], "why": TEXTS["colony_free"][1] % str(s["name"]), "x": int(s["x"]), "y": int(s["y"])}
	return best

func _colonize(fleet_id: int) -> String:
	var f := app.find_by_id("fleets", fleet_id)
	var t := _colony_target(f)
	if t.is_empty():
		return "No world to settle"
	if t.get("here", false):
		return "" if app.issue({"type": "Colonize", "fleetId": fleet_id}, GalaxyOrders.replace_ids(app, f), "Colony ship ordered to settle.") else "Refused"
	return "" if GalaxyOrders.plot_course(app, f, int(t["x"]), int(t["y"])) else "Refused"
