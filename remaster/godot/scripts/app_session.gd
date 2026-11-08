class_name AppSession
extends RefCounted
## Game state of the strategic client, with no nodes: the core gateway, the current view, the selection and the
## order API. App (the shell) owns one and forwards its old fields to it; every view model reads it. Errors and
## success notes come out as signals so the shell decides how to show them (toasts).
## See docs/CLIENT-ARCHITECTURE.md.

signal view_changed
signal selection_changed
signal notified(text: String, tone: String)
signal failed(message: String)

const LOCK_REASON := "Finish the battle in progress first"

var core: CoreClient
var meta: Dictionary = {}
var view: Dictionary = {}
var encyclopedia: Dictionary = {}
var selection := {"cell": -1, "systemId": 0, "fleetId": 0, "taskForceId": 0}
var briefing: Variant = null
var encounter: Variant = null

## A session on the given core. `backend` is "fixture" or "auto" (BotfCore when the GDExtension is loaded).
static func create(backend: String = "auto") -> AppSession:
	var s := AppSession.new()
	s.connect_core(CoreClient.create(backend))
	return s

func connect_core(c: CoreClient) -> void:
	core = c
	var m: Variant = core.request("meta")
	meta = m if m is Dictionary else {}

# ---------------------------------------------------------------------------------------------
# Reads
# ---------------------------------------------------------------------------------------------
func in_game() -> bool:
	return not view.is_empty()

func player() -> int:
	return int(view.get("meta", {}).get("player", 0))

func width() -> int:
	return int(view.get("meta", {}).get("width", 1))

func battle_active() -> bool:
	return view.get("session", {}).get("active", false)

## The reason every order control is locked, or "" when orders may be drafted.
func lock_reason() -> String:
	return LOCK_REASON if battle_active() else ""

func get_encyclopedia() -> Dictionary:
	if encyclopedia.is_empty():
		var e: Variant = core.request("encyclopedia")
		encyclopedia = e if e is Dictionary else {}
	return encyclopedia

func find_by_id(list_key: String, id: int, key: String = "id") -> Dictionary:
	for item in view.get(list_key, []):
		if int(item.get(key, -1)) == id:
			return item
	return {}

func system_at(x: int, y: int) -> Dictionary:
	for s in view.get("systems", []):
		if int(s["x"]) == x and int(s["y"]) == y:
			return s
	return {}

func empire_name(id: int) -> String:
	if id == player():
		return str(view["meta"]["playerName"])
	for e in meta.get("empires", []):
		if int(e["id"]) == id:
			return str(e["name"])
	return "Independent"

## Ids of drafted orders of `type` (optionally filtered by `match_`, called with the order).
func pending_ids(type: String, match_: Callable = Callable()) -> Array:
	var out := []
	for p in view.get("pending", []):
		if p["type"] == type and (not match_.is_valid() or match_.call(p)):
			out.append(int(p["id"]))
	return out

func unit_order_ids(fleet_id: int, task_force_id: int, types: Array = []) -> Array:
	var out := []
	for p in view.get("pending", []):
		if not types.is_empty() and not types.has(p["type"]):
			continue
		if task_force_id != 0:
			if int(p.get("taskForceId", 0)) == task_force_id and p["type"] == "SetTaskForceOrder":
				out.append(int(p["id"]))
		elif int(p.get("fleetId", -1)) == fleet_id and p["type"] != "CreateTaskForce":
			out.append(int(p["id"]))
	return out

# ---------------------------------------------------------------------------------------------
# Selection
# ---------------------------------------------------------------------------------------------
func select_cell(cell: int) -> void:
	if cell < 0 or view.is_empty():
		selection = {"cell": -1, "systemId": 0, "fleetId": 0, "taskForceId": 0}
	else:
		var s := system_at(cell % width(), cell / width())
		selection = {"cell": cell, "systemId": int(s.get("id", 0)), "fleetId": 0, "taskForceId": 0}
	selection_changed.emit()

func select_system(system_id: int) -> void:
	var s := find_by_id("systems", system_id)
	if s.is_empty():
		return
	selection = {"cell": int(s["y"]) * width() + int(s["x"]), "systemId": system_id, "fleetId": 0, "taskForceId": 0}
	selection_changed.emit()

func select_fleet(fleet_id: int) -> void:
	var f := find_by_id("fleets", fleet_id)
	if f.is_empty():
		f = find_by_id("contacts", fleet_id)
	if f.is_empty():
		return
	var s := system_at(int(f["x"]), int(f["y"]))
	selection = {"cell": int(f["y"]) * width() + int(f["x"]), "systemId": int(s.get("id", 0)), "fleetId": fleet_id, "taskForceId": int(f.get("taskForceId", 0))}
	selection_changed.emit()

# ---------------------------------------------------------------------------------------------
# Writes
# ---------------------------------------------------------------------------------------------
## Re-reads the view from the core. Emits view_changed, or failed with the core's error.
func refresh() -> bool:
	var v: Variant = core.request("view")
	if v is Dictionary:
		view = v
		view_changed.emit()
		return true
	failed.emit(core.last_error)
	return false

## Submits an order through the core; on rejection emits failed with the core's reason.
func issue(command: Dictionary, cancel_ids: Array = [], success: String = "") -> bool:
	var r := core.invoke("submit", {"command": command, "cancelIds": cancel_ids})
	if not r["ok"]:
		failed.emit(r["error"])
		return false
	refresh()
	if success != "":
		notified.emit(success, "good")
	return true

func cancel_orders(ids: Array, message: String = "") -> void:
	if ids.is_empty():
		return
	var r := core.invoke("cancel", {"ids": ids})
	if not r["ok"]:
		failed.emit(r["error"])
		return
	refresh()
	if message != "":
		notified.emit(message, "info")

## The core's reason a command would be rejected, or "" when it is valid.
func dry_run(command: Dictionary, cancel_ids: Array = []) -> String:
	var r := core.invoke("dryRun", {"command": command, "cancelIds": cancel_ids})
	if not r["ok"]:
		return str(r["error"])
	var res: Variant = r["result"]
	if res is Dictionary:
		return "" if res.get("ok", true) else str(res.get("error", "Rejected"))
	return ""

# ---------------------------------------------------------------------------------------------
# Campaign lifecycle
# ---------------------------------------------------------------------------------------------
## New-campaign options as the core reads them. The core (WebBridge `newCampaign`) and the fixture backend read the
## player's power from "faction"; callers that pass "empire" (the audio tour and tests, M0 bug) were silently given
## the Federation. "empire" is accepted as an alias; "faction" wins when both are given.
static func campaign_options(options: Dictionary) -> Dictionary:
	var o := options.duplicate()
	if o.has("empire"):
		if not o.has("faction"):
			o["faction"] = int(o["empire"])
		o.erase("empire")
	if o.has("faction") and o.has("empires"):
		o["empires"] = max(int(o["empires"]), int(o["faction"]))
	return o

## Calls a core method that starts a game (newCampaign, a drill, load). Returns its view, or {} after failed.
func start(method: String, args: Dictionary) -> Dictionary:
	var r := core.invoke(method, args)
	if not r["ok"]:
		failed.emit(r["error"])
		return {}
	return r["result"]

## Adopts a freshly started view: clears the turn state and selects the first colony.
func enter(v: Dictionary) -> void:
	view = v
	briefing = null
	encounter = null
	var cols: Array = v.get("colonies", [])
	if cols.size() > 0:
		select_system(int(cols[0]["systemId"]))

## Ends the turn in the core. Returns the core's result ({status, briefing}) or {} after failed.
func end_turn() -> Dictionary:
	var r := core.invoke("endTurn", {})
	if not r["ok"]:
		failed.emit(r["error"])
		return {}
	return r["result"]

func fetch_encounter() -> Variant:
	encounter = core.request("tactical")
	return encounter

## The save blob from the core (data, scenario, turn, player ...), or {} after failed.
func save_blob() -> Dictionary:
	var r := core.invoke("save", {})
	if not r["ok"]:
		failed.emit(r["error"])
		return {}
	return r["result"]
