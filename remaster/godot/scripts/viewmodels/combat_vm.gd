class_name CombatVM
extends ViewModel
## Tactical combat model: the live encounter (session.encounter), the selected ship and target, the order scope
## (all / group / ship), whether the encounter choice is still to be shown, the archive selection, and the tactical
## orders (tacticalOrder, tacticalRound, tacticalAuto) with the round log.

const ATTACK := "Attack order: a cloaked ship decloaks and fires a +25% ambush volley."
const MANEUVERS := [
	{"key": "Engage", "name": "Engage", "hint": "Advance to optimal weapon range and fire.\n" + ATTACK},
	{"key": "Assault", "name": "Assault", "hint": "Disciplined advance to the medium beam/torpedo envelope.\n" + ATTACK},
	{"key": "Charge", "name": "Charge", "hint": "Close to point-blank range: +20% beam damage, but +15% incoming.\n" + ATTACK},
	{"key": "Strafe", "name": "Strafe", "hint": "Fast close pass: +25% beam, -20% torpedo, +10% incoming.\n" + ATTACK},
	{"key": "Flyby", "name": "Flyby", "hint": "Fast pass at medium range: -15% incoming.\n" + ATTACK},
	{"key": "Harry", "name": "Harry", "hint": "Close evasive circling: -10% beam, -20% incoming.\n" + ATTACK},
	{"key": "Circle", "name": "Circle", "hint": "Stand-off orbit: -15% beam, -25% incoming. Keeps a cloak."},
	{"key": "Evade", "name": "Evade", "hint": "Long-range evasion: -30% fire, -35% incoming. Keeps a cloak."},
	{"key": "Hold", "name": "Hold", "hint": "Brace in place: -20% incoming damage. Shields do not recharge. Keeps a cloak."},
	{"key": "Ram", "name": "Ram", "hint": "Collision: the target takes the rammer's hull as damage and the rammer takes the target's hull + shields as recoil."},
	{"key": "Hail", "name": "Hail", "hint": "Offer a truce. If every active ship hails and the enemy accepts, the battle ends before any fire."},
	{"key": "Retreat", "name": "Retreat", "hint": "Disengage: one round at -25% incoming with weapons offline, then escape if still retreating."},
]
const IMMOBILE_BLOCKED := ["Retreat", "Charge", "Circle", "Flyby", "Strafe", "Evade", "Ram", "Harry", "Assault"]
const SCOPES := [["all", "All ships", "Every active ship"], ["group", "Group", "The selected ship's like-class tactical group"], ["ship", "Ship", "Only the selected ship"]]

var ship := 0
var target := 0
var scope := "all"
var choice := true
var archive_sel := {}
var subsystem := "none" ## Rules 4 aim for ship and all-ships orders: none, weapons, engines, shields, sensors or warpCore.
var maneuver := "Engage" ## The last maneuver ordered, used for the plan-phase forecast.

const FACINGS := ["fore", "aft", "port", "starboard", "dorsal", "ventral"]
const SUBSYSTEMS := [["weapons", "WPN", "Weapons"], ["engines", "ENG", "Engines"], ["shields", "SHD", "Shield emitters"], ["sensors", "SNS", "Sensors"], ["warpCore", "CORE", "Warp core"]]
const CREW_TONES := {"Green": "warn", "Regular": "primary", "Veteran": "good", "Elite": "good", "Legendary": "good"}

static func is_active(s: Dictionary) -> bool:
	return s.get("alive", false) and s.get("active", false) and not s.get("retreated", false)

static func short_name(s: Dictionary) -> String:
	if s.is_empty():
		return "nearest enemy"
	return "Cloaked contact" if s.get("hidden", false) else str(s["name"]).split(" — ")[0]

## The rules 4 block of a ship ({facings, maxFacings, subsystems, crew, canWarp, ...}), or {} under rules 1-3.
static func wego_of(s: Dictionary) -> Dictionary:
	var w: Variant = s.get("wego")
	return w if w is Dictionary else {}

## Retreat needs a warp-capable ship under rules 4 (core and engines up, a safe sector): hidden otherwise.
static func retreat_hidden(s: Dictionary) -> bool:
	var w := wego_of(s)
	return not w.is_empty() and not w.get("canWarp", false)

func enc() -> Dictionary:
	return session.encounter if session.encounter is Dictionary else {}

## A battle is in progress or pending (the live layout), else the archive.
func live() -> bool:
	var s: Dictionary = view().get("session", {})
	return s.get("active", false) or s.get("battlePending", false)

## Fetches the encounter when a live battle has none yet.
func ensure_encounter() -> void:
	if live() and enc().is_empty():
		session.fetch_encounter()

## Splits the encounter and fixes the selection to active ships: {ships, mine, foes, foe_name, last}.
func sides() -> Dictionary:
	var e := enc()
	var ships: Array = e.get("ships", [])
	var mine: Array = ships.filter(func(s): return s.get("mine", false))
	var foes: Array = ships.filter(func(s): return not s.get("mine", false))
	if ship == 0 or mine.filter(func(s): return int(s["id"]) == ship and is_active(s)).is_empty():
		var a: Array = mine.filter(func(s): return is_active(s))
		ship = int(a[0]["id"]) if a.size() > 0 else 0
	if target == 0 or foes.filter(func(s): return int(s["id"]) == target and is_active(s) and not s.get("hidden", false)).is_empty():
		var b: Array = foes.filter(func(s): return is_active(s) and not s.get("hidden", false))
		target = int(b[0]["id"]) if b.size() > 0 else 0
	var rounds: Array = e.get("rounds", [])
	return {"ships": ships, "mine": mine, "foes": foes, "foe_name": foes[0]["ownerName"] if foes.size() > 0 else "enemy", "last": rounds.back() if rounds.size() > 0 else {}}

## {hull, max, alive} over a force (hidden ships do not count toward the hull bar).
static func force(ships: Array) -> Dictionary:
	var hull := 0
	var mx := 0
	var alive := 0
	for s in ships:
		if not s.get("hidden", false):
			hull += max(0, int(s["hull"]))
			mx += int(s["maxHull"])
		if s.get("alive", false) and not s.get("retreated", false):
			alive += 1
	return {"hull": hull, "max": mx, "alive": alive}

static func title(e: Dictionary, last: Dictionary) -> String:
	return "Truce agreed" if e.get("finished", false) and last.get("truce", false) else ("Engagement concluded" if e.get("finished", false) else "Red alert")

static func name_of(all: Array, id: int) -> String:
	for s in all:
		if int(s["id"]) == id:
			return "%s #%d" % [short_name(s), id]
	return "#%d" % id

## The changing lines of a ship card: {status, line, ordered, flags}.
static func card_state(s: Dictionary, all: Array, hostile: bool) -> Dictionary:
	var status := "DESTROYED" if not s.get("alive", true) else ("WITHDRAWN" if s.get("retreated", false) else "")
	var nx: Variant = s.get("next")
	var line := status
	if line == "":
		if nx is Dictionary:
			line = "%s%s%s  (%s)" % ["will " if hostile else "next: ", nx["maneuver"], (" → " + name_of(all, int(nx["target"]))) if int(nx.get("target", 0)) != 0 else "", {"ai": "AI", "standing": "standing", "order": "ordered"}.get(nx.get("source", ""), "")]
		else:
			line = "beam %d · torp %d" % [int(s["beam"]), int(s["torpedo"])]
	var flags := []
	if s.get("cloaked", false) and s.get("alive", false):
		flags.append("Cloaked")
	if s.get("station", false):
		flags.append("%s · immobile" % s.get("stationType", "Station"))
	if s.get("disengaging", false):
		flags.append("Disengaging")
	if str(s.get("group", "")) != "":
		flags.append(str(s["group"]))
	if not hostile:
		flags.append("B%d · T%d" % [int(s["beam"]), int(s["torpedo"])])
	return {"status": status, "line": line, "ordered": nx is Dictionary and nx.get("source", "") == "order", "flags": "  ·  ".join(flags)}

## Picks a ship: ours becomes the selected ship (narrowing scope from all to ship), theirs the target.
func pick(id: int) -> void:
	for s in enc().get("ships", []):
		if int(s["id"]) == id and not s.get("hidden", false):
			if s.get("mine", false):
				ship = id
				if scope == "all":
					scope = "ship"
			else:
				target = id

func selected() -> Dictionary:
	for s in enc().get("ships", []):
		if int(s["id"]) == ship:
			return s
	return {}

func scope_label() -> String:
	var sel := selected()
	return "all active ships" if scope == "all" else (("%s group" % sel.get("group", "")) if scope == "group" else short_name(sel))

func scope_disabled(k: String) -> bool:
	var sel := selected()
	return k != "all" and (sel.is_empty() or not is_active(sel))

## Why a maneuver cannot be ordered for the current scope, or "".
func maneuver_block(key: String) -> String:
	var sel := selected()
	if enc().get("finished", false):
		return "The battle is over"
	if scope == "ship" and sel.get("station", false) and IMMOBILE_BLOCKED.has(key):
		return "Stations are immobile: they hold position and fire"
	if key == "Retreat" and scope == "ship" and not sel.is_empty() and not sel.get("canRetreat", false):
		return "No safe supplied withdrawal sector"
	if scope != "all" and (sel.is_empty() or not is_active(sel)):
		return "Select one of your active ships"
	return ""

func maneuver_hint(mv: Dictionary) -> String:
	var hint: String = mv["hint"]
	if target != 0 and not mv["key"] in ["Retreat", "Hail"]:
		hint += "\nTarget: #%d" % target
	return hint

static func maneuver_tone(key: String) -> String:
	return "alert" if key in ["Retreat", "Ram"] else ("ghost" if key == "Hail" else "secondary")

## [text, tone]: whether the enemy would accept a truce.
static func hail_outlook(e: Dictionary, foe: String) -> Array:
	var h: Dictionary = e.get("hail", {})
	var why := ("formally at war · trust %d · their strength %d vs ours %d" % [int(h.get("trust", 0)), int(h.get("theirStrength", 0)), int(h.get("ourStrength", 0)) ]) if h.get("atWar", false) else "not formally at war"
	if h.get("accepts", false):
		return ["If every one of our ships hails, the %s would accept a truce (%s)." % [foe, why], "good"]
	return ["The %s would refuse a truce unless they hail too (%s)." % [foe, why], "alert"]

## Orders a maneuver for the current scope. Returns the toast text, or "" after failed.
func order(maneuver_key: String) -> String:
	var sel := selected()
	if not maneuver_key in ["Retreat", "Hail"]:
		maneuver = maneuver_key
	var t := 0 if maneuver_key in ["Retreat", "Hail"] else target
	var args := {"maneuver": maneuver_key, "targetFleetId": t}
	if subsystem != "none" and not wego_of(sel).is_empty() and scope != "group":
		args["subsystem"] = subsystem
	if scope == "all":
		args["all"] = true
	elif scope == "group":
		args["groupId"] = int(sel.get("groupId", 0))
	else:
		args["fleetId"] = ship
	var r := session.core.invoke("tacticalOrder", args)
	if not r["ok"]:
		session.failed.emit(r["error"])
		return ""
	session.fetch_encounter()
	return "%s ordered for %s ship%s%s." % [maneuver_key, Fmt.num(r["result"]), "" if int(r["result"]) == 1 else "s", (", target #%d" % t) if t != 0 else ""]

## The plan-phase forecast for the selected ship against the target (bridge "tacticalForecast"), or {} when the core
## cannot give one (rules 1-3, no target, the fixture backend).
func forecast() -> Dictionary:
	var sel := selected()
	if sel.is_empty() or wego_of(sel).is_empty() or target == 0 or not is_active(sel) or enc().get("finished", false):
		return {}
	var args := {"fleetId": ship, "targetFleetId": target, "maneuver": maneuver}
	if subsystem != "none":
		args["subsystem"] = subsystem
	var r := session.core.invoke("tacticalForecast", args)
	return r["result"] if r["ok"] and r["result"] is Dictionary else {}

## The aim picked on the plan overlay (subsystem pips) or the selector; clicking the active aim clears it.
func set_subsystem(key: String) -> void:
	subsystem = key if key in ["weapons", "engines", "shields", "sensors", "warpCore"] else "none"

## Resolves one round (or the whole battle when `auto`). False after failed.
func resolve(auto: bool) -> bool:
	var r := session.core.invoke("tacticalAuto" if auto else "tacticalRound", {})
	if not r["ok"]:
		session.failed.emit(r["error"])
		return false
	session.encounter = r["result"]
	choice = false
	return true

func hail_all() -> void:
	session.core.invoke("tacticalOrder", {"all": true, "maneuver": "Hail"})

## Human-readable log lines for a round: [[tone, text], ...].
static func round_log(e: Dictionary, r: Dictionary) -> Array:
	var out := []
	if r.is_empty():
		return out
	var names := {}
	for s in e.get("ships", []):
		names[int(s["id"])] = short_name(s)
	for st in r.get("ships", []):
		if st.get("decloaked", false):
			out.append(["warn", "%s decloaks." % names.get(int(st["id"]), "?")])
	for sv in r.get("salvos", []):
		var dmg := int(sv.get("beam", 0)) + int(sv.get("torpedo", 0))
		var txt := "%s → %s: %d damage%s%s%s" % [names.get(int(sv["from"]), "?"), names.get(int(sv["to"]), "?"), dmg, " (ram)" if sv.get("ram", false) else "", " (ambush)" if sv.get("ambush", false) else "", " (absorbed by shields)" if sv.get("absorbed", false) else ""]
		out.append(["alert" if sv.get("ram", false) else "info", txt])
	for id in r.get("lost", []):
		out.append(["alert", "%s destroyed." % names.get(int(id), "?")])
	for id in r.get("retreatedIds", []):
		out.append(["warn", "%s withdrew." % names.get(int(id), "?")])
	if r.get("truce", false):
		out.append(["good", "Truce agreed."])
	if r.get("ended", false):
		out.append(["good", "The engagement is over."])
	return out

# ---------------------------------------------------------------------------------------------
# Archive
# ---------------------------------------------------------------------------------------------
func replays() -> Array:
	var reps: Variant = query("replays")
	return reps if reps is Array else []

func battles() -> Array:
	return view().get("battles", [])

## The selected replay encounter or battle report, or null (error in last_error for a report).
func archive_entry() -> Variant:
	match archive_sel.get("kind", ""):
		"replay": return query("replay", {"index": int(archive_sel["index"])})
		"report": return query("battleReport", {"index": int(archive_sel["index"])})
	return null
