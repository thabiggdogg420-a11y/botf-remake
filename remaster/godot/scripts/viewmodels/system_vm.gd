class_name SystemVM
extends ViewModel
## System (colony) screen model: the selected colony's `colony` detail, the drafted workforce with the core's
## `workforcePreview`, the construction tab and template name, and every colony order (staffing, governor,
## shipyard, construction queue, purchase, templates, megaproject, scrapping).

const FIELDS := [
	{"key": "food", "label": "Food", "structure": "farms", "unit": "food"},
	{"key": "industry", "label": "Industry", "structure": "factories", "unit": "industry"},
	{"key": "research", "label": "Research", "structure": "laboratories", "unit": "research"},
]

var detail: Dictionary = {}
var draft := {"food": 0, "industry": 0, "research": 0}
var preview: Dictionary = {}
var tab := "structures"
var template_name := ""

## The selected system when it is one of ours, else our first colony, else 0.
func system_id() -> int:
	var sid := int(session.selection.get("systemId", 0))
	for c in view().get("colonies", []):
		if int(c["systemId"]) == sid:
			return sid
	var cols: Array = view().get("colonies", [])
	return int(cols[0]["systemId"]) if cols.size() > 0 else 0

## Selects the colony `d` places away (wrapping). False when there is nothing to cycle.
func cycle(d: int) -> bool:
	var cols: Array = view().get("colonies", [])
	if cols.size() < 2:
		return false
	var sid := system_id()
	var idx := 0
	for n in cols.size():
		if int(cols[n]["systemId"]) == sid:
			idx = n
	session.select_system(int(cols[(idx + d + cols.size()) % cols.size()]["systemId"]))
	return true

## Loads the colony detail and resets the draft to its staffing. False (error in last_error) when the core fails.
func fetch(sid: int) -> bool:
	var d: Variant = query("colony", {"systemId": sid})
	if not d is Dictionary:
		return false
	detail = d
	var c: Dictionary = detail["colony"]
	draft = {"food": int(c["workers"]["food"]), "industry": int(c["workers"]["industry"]), "research": int(c["workers"]["research"])}
	preview = {}
	return true

func colony() -> Dictionary:
	return detail["colony"]

func sid() -> int:
	return int(detail["colony"]["systemId"])

## [index, count] of this colony among ours.
func colony_position() -> Array:
	var cols: Array = view().get("colonies", [])
	var idx := 0
	for n in cols.size():
		if int(cols[n]["systemId"]) == sid():
			idx = n
	return [idx, cols.size()]

## Growth in the header: "fixed" when population is fixed, else "+n" or "+a to +b".
static func growth_text(g: Dictionary) -> String:
	if not g.get("enabled", false):
		return "fixed"
	return Fmt.signed(g["min"]) if int(g["min"]) == int(g["max"]) else "%s to %s" % [Fmt.signed(g["min"]), Fmt.signed(g["max"])]

## Growth in the staffing forecast: "+n" or "+a..+b".
static func forecast_growth_text(g: Dictionary) -> String:
	return Fmt.signed(g.get("min", 0)) if int(g.get("min", 0)) == int(g.get("max", 0)) else "%s..%s" % [Fmt.signed(g["min"]), Fmt.signed(g["max"])]

# ---------------------------------------------------------------------------------------------
# Workforce
# ---------------------------------------------------------------------------------------------
func used() -> int:
	return int(draft["food"]) + int(draft["industry"]) + int(draft["research"])

func unassigned() -> int:
	return int(colony()["population"]) - used()

## Sets a drafted field, clamped to its jobs and the free population, then previews it. Returns the value set.
func set_workers(key: String, value: int) -> int:
	var pop := int(colony()["population"])
	var others := used() - int(draft[key])
	var limit: int = min(int(detail["capacityLimits"][key]), pop - others)
	var v: int = clamp(value, 0, max(0, limit))
	draft[key] = v
	var p: Variant = query("workforcePreview", _preview_args(draft))
	preview = p if p is Dictionary else {"error": session.core.last_error}
	return v

func _preview_args(dr: Dictionary) -> Dictionary:
	return {"systemId": sid(), "food": dr["food"], "industry": dr["industry"], "research": dr["research"]}

func preview_ok() -> bool:
	return not preview.is_empty() and str(preview.get("error", "")) == ""

func preview_error() -> String:
	return str(preview["error"]) if not preview.is_empty() and str(preview.get("error", "")) != "" else ""

## The economy and growth the forecast shows: the preview's, else the colony's.
func forecast_economy() -> Dictionary:
	return preview["economy"] if preview_ok() and preview.has("economy") else colony()["economy"]

func forecast_growth() -> Dictionary:
	return preview["growth"] if preview_ok() and preview.has("growth") else colony()["growth"]

func draft_dirty() -> bool:
	var w: Dictionary = colony()["workers"]
	return int(draft["food"]) != int(w["food"]) or int(draft["industry"]) != int(w["industry"]) or int(draft["research"]) != int(w["research"])

## Orders the drafted staffing. Returns false and sets preview to the core's error when it would be refused.
func apply_workers() -> bool:
	if lock_reason() != "":
		return false
	var p: Variant = query("workforcePreview", _preview_args(draft))
	if not p is Dictionary:
		session.failed.emit(session.core.last_error)
		return false
	if str(p.get("error", "")) != "":
		preview = p
		return false
	return issue({"type": "AssignWorkers", "systemId": sid(), "foodWorkers": draft["food"], "industryWorkers": draft["industry"], "researchWorkers": draft["research"]}, p.get("cancelIds", []), "Staffing updated.")

## Feeds the colony (fewest farmers with no food deficit), then splits the rest; `industry_first` maximises industry.
func auto_balance(industry_first: bool) -> bool:
	var pop := int(colony()["population"])
	var lim: Dictionary = detail["capacityLimits"]
	var food := 0
	for fv in range(0, min(pop, int(lim["food"])) + 1):
		food = fv
		var p: Variant = query("workforcePreview", {"systemId": sid(), "food": fv, "industry": 0, "research": 0})
		if p is Dictionary and str(p.get("error", "")) == "" and p.has("economy") and int(p["economy"]["foodDeficit"]) == 0:
			break
	var rest := pop - food
	var research: int = max(0, min(int(lim["research"]), rest - int(lim["industry"]))) if industry_first else min(int(lim["research"]), rest / 2)
	var industry: int = min(int(lim["industry"]), rest - research)
	draft = {"food": food, "industry": industry, "research": min(int(lim["research"]), rest - industry)}
	return apply_workers()

# ---------------------------------------------------------------------------------------------
# Empire rules (schema 15, docs/design/EMPIRE-RULES.md): dilithium
# ---------------------------------------------------------------------------------------------
## The dilithium block of this colony's `empireRules` ({} when the rules are off).
func dilithium() -> Dictionary:
	var er: Variant = colony().get("empireRules", {})
	return er["dilithium"] if er is Dictionary and er.get("enabled", false) and er.has("dilithium") else {}

## Panel lines for the dilithium block: [{text, tone}] (tone "" / "good" / "warn" / "alert").
static func dilithium_lines(d: Dictionary, queue_blocker: String = "") -> Array:
	var out := []
	if d.is_empty():
		return out
	if bool(d["refinery"]):
		out.append({"text": "Refinery online - one ship slot for the empire." if bool(d["refineryPowered"]) else "Refinery has no power: it gives no ship slot. Build power plants.", "tone": "good" if bool(d["refineryPowered"]) else "alert"})
	elif bool(d["deposit"]):
		out.append({"text": "Dilithium deposit here. A refinery (build list) adds one ship slot for the empire.", "tone": "info"})
	else:
		out.append({"text": "No dilithium deposit in this system.", "tone": ""})
	out.append({"text": "Empire ship slots: %d of %d in use (%d refineries, %d powered)." % [int(d["slotsUsed"]), int(d["slots"]), int(d["empireRefineries"]), int(d["empirePowered"])], "tone": "warn" if int(d["slotsUsed"]) >= int(d["slots"]) else ""})
	if int(d.get("klingonBonusPercent", 0)) > 0:
		out.append({"text": "Klingon yard bonus: ships here build %d%% faster." % int(d["klingonBonusPercent"]), "tone": "good"})
	if queue_blocker.find("slots") >= 0 or queue_blocker.find("refinery") >= 0:
		out.append({"text": queue_blocker, "tone": "warn"})
	return out

# ---------------------------------------------------------------------------------------------
# Governor and shipyard
# ---------------------------------------------------------------------------------------------
func set_governor(which: String, key: String) -> bool:
	var g: Dictionary = colony()["governor"]
	var wf: String = key if which == "workforce" else g["workforce"]
	var qu: String = key if which == "queue" else g["queue"]
	return issue({"type": "SetColonyGovernor", "systemId": sid(), "governorWorkforce": wf, "governorQueue": qu}, [], "Governor orders updated.")

func resume_governor() -> bool:
	return issue({"type": "ClearGovernorOverride", "systemId": sid()}, [], "Governor resumed.")

func shipyard_block() -> String:
	var lock := lock_reason()
	return lock if lock != "" else str(detail.get("shipyardToggleBlocker", ""))

func toggle_shipyard() -> bool:
	return issue({"type": "ToggleShipyard", "systemId": sid()}, [], "Shipyard power change ordered for turn end.")

# ---------------------------------------------------------------------------------------------
# Construction
# ---------------------------------------------------------------------------------------------
func choices(ships: bool) -> Array:
	return detail.get("choices", []).filter(func(x): return x["isShip"] == ships)

func available_count(ships: bool) -> int:
	return choices(ships).filter(func(x): return str(x["blocker"]) == "").size()

func choice_block(ch: Dictionary) -> String:
	var lock := lock_reason()
	return lock if lock != "" else str(ch.get("blocker", ""))

static func choice_tip(ch: Dictionary) -> String:
	if ch["isShip"] and ch.has("hull"):
		return "%s %s\nHull %d · shields %d · attack %d\nSpeed %d · range %d · upkeep %d\n%s" % [ch.get("designClass", ""), ch.get("designName", ""), int(ch["hull"]), int(ch["shields"]), int(ch["attack"]), int(ch.get("speed", 0)), int(ch.get("range", 0)), int(ch.get("maintenance", 0)), ch.get("description", "")]
	return "%s\nIndustry %d%s · upkeep %d credits/turn" % [ch["name"], int(ch["cost"]), (" · energy %d" % int(ch["energyCost"])) if ch.has("energyCost") else "", int(ch["upkeep"])]

static func choice_meta(ch: Dictionary) -> String:
	var meta_ := ""
	if ch["isShip"] and ch.has("hull"):
		meta_ = "H%d · S%d · A%d · " % [int(ch["hull"]), int(ch["shields"]), int(ch["attack"])]
	meta_ += "%s ind · %s" % [Fmt.fmt(ch["cost"]), ("¢%d/t" % int(ch["upkeep"])) if int(ch["upkeep"]) > 0 else "no upkeep"]
	return meta_

func enqueue(ch: Dictionary) -> bool:
	return issue({"type": "EnqueueBuilding", "systemId": sid(), "building": ch["product"], "referenceId": int(ch["referenceId"])}, [], "%s added to the queue." % ch["name"])

func scrap_block(s: Dictionary) -> String:
	var lock := lock_reason()
	return lock if lock != "" else (str(s["blocker"]) if str(s["blocker"]) != "" else ("None installed" if int(s["count"]) == 0 else ""))

func scrap(building: String, name_: String) -> bool:
	return issue({"type": "ScrapStructure", "systemId": sid(), "building": building}, [], "%s will be scrapped." % name_)

func queue() -> Array:
	return detail.get("queue", [])

func set_pin(idx: int, pinned: bool) -> bool:
	return issue({"type": "SetQueuePin", "systemId": sid(), "queueIndex": idx, "queuePinned": pinned})

func move_item(idx: int, to: int) -> bool:
	return issue({"type": "MoveConstruction", "systemId": sid(), "queueIndex": idx, "queueDestination": to})

func remove_item(idx: int, name_: String) -> bool:
	return issue({"type": "RemoveConstruction", "systemId": sid(), "queueIndex": idx}, [], "%s removed." % name_)

## The drafted purchase for this colony, or {}.
func pending_buy() -> Dictionary:
	var out := {}
	for p in view().get("pending", []):
		if p["type"] == "BuyConstruction" and int(p.get("systemId", -1)) == sid():
			out = p
	return out

func buy_block() -> String:
	var lock := lock_reason()
	return lock if lock != "" else (str(detail.get("buyBlocker", "")) if str(detail.get("buyBlocker", "")) != "" else ("Nothing to buy" if queue().is_empty() else ""))

func buy() -> bool:
	return issue({"type": "BuyConstruction", "systemId": sid()}, [], "Purchase ordered.")

# ---------------------------------------------------------------------------------------------
# Templates and the megaproject
# ---------------------------------------------------------------------------------------------
## This colony's queue as template items.
func queue_items() -> Array:
	var items := []
	for q in queue():
		items.append({"product": q["product"], "referenceId": int(q["referenceId"])})
	return items

func template_block(tid: int) -> String:
	var blocker := lock_reason()
	if blocker == "":
		for dt in detail.get("templates", []):
			if int(dt["id"]) == tid:
				blocker = str(dt.get("blocker", ""))
	return blocker

func save_block() -> String:
	var lock := lock_reason()
	return lock if lock != "" else ("The build queue is empty" if queue_items().is_empty() else "")

func apply_template(tid: int, name_: String) -> bool:
	return issue({"type": "ApplyBuildTemplate", "templateId": tid, "systemId": sid()}, [], "“%s” applied." % name_)

func set_default_template(tid: int, is_default: bool) -> bool:
	return issue({"type": "SetDefaultBuildTemplate", "templateId": 0 if is_default else tid}, [], "Default template updated.")

func define_template(tid: int, name_: String, success: String) -> bool:
	return issue({"type": "DefineBuildTemplate", "templateId": tid, "templateName": name_, "templateItems": queue_items()}, [], success)

func delete_template(tid: int) -> bool:
	return issue({"type": "DeleteBuildTemplate", "templateId": tid}, [], "Template deleted.")

func megaproject() -> Dictionary:
	var m: Dictionary = detail.get("megaproject", {})
	return m if m.get("enabled", false) else {}

func start_megaproject(name_: String) -> bool:
	return issue({"type": "StartMegaproject", "systemId": sid()}, [], "The %s begins." % name_)

func cancel_megaproject(name_: String) -> bool:
	return issue({"type": "CancelMegaproject"}, [], "The %s was abandoned." % name_)

# ---------------------------------------------------------------------------------------------
# Key figures with explanations (docs/UI-DESIGN-SYSTEM.md §8)
# ---------------------------------------------------------------------------------------------
## The colony's KPI row: [{icon, cap, value, delta, tone, explain}].
func kpis() -> Array:
	var c := colony()
	var e: Dictionary = c["economy"]
	var s: Dictionary = c["structures"]
	var w: Dictionary = c["workers"]
	var g: Dictionary = c["growth"]
	var pop := int(c["population"])
	var food_bal := int(e["food"]) - pop
	var gtxt := growth_text(g)
	var fc := forecast_growth()
	var fe := forecast_economy()
	var eu := int(e["energyUsed"])
	var ea := int(e["energyAvailable"])
	return [
		{"icon": "population", "cap": "Population", "value": "%d/%d" % [pop, int(c["capacity"])], "delta": "", "tone": "",
			"explain": {"title": "Population", "value": "%d of %d" % [pop, int(c["capacity"])], "parts": [["Capacity now", str(int(c["capacity"])), ""], ["Capacity when every planet is terraformed", str(int(detail.get("maxCapacity", c["capacity"]))), "good"], ["Change per turn", gtxt, "alert" if int(g.get("max", 0)) < 0 else "good"]],
				"effect": "A food surplus grows the colony up to capacity; a shortfall shrinks it. Terraforming raises capacity.",
				"forecast": ("Next turn with the drafted staffing: %s." % forecast_growth_text(fc)) if not fc.is_empty() else ""}},
		{"icon": "growth", "cap": "Growth", "value": gtxt, "delta": "", "tone": "alert" if int(g.get("max", 0)) < 0 else "good",
			"explain": {"title": "Growth", "value": gtxt, "effect": "Population change per turn. It needs a food surplus and free capacity.", "forecast": forecast_growth_text(fc) if not fc.is_empty() else ""}},
		{"icon": "morale", "cap": "Morale", "value": str(int(c["morale"])), "delta": "", "tone": "warn" if int(c["morale"]) < 50 else "",
			"explain": {"title": "Morale", "value": str(int(c["morale"])), "effect": "Drifts toward 70 each turn. Events, war and conquest move it; under 50 output suffers and unrest can start."}},
		{"icon": "food", "cap": "Food", "value": str(int(e["food"])), "delta": Fmt.signed(food_bal), "tone": "alert" if int(e["foodDeficit"]) > 0 else "good",
			"explain": {"title": "Food", "value": "%d produced" % int(e["food"]), "parts": [["Farms", str(int(s["farms"])), ""], ["Farm workers", str(int(w["food"])), ""], ["Eaten (1 per population)", "-%d" % pop, ""], ["Balance", Fmt.signed(food_bal), "alert" if food_bal < 0 else "good"]],
				"effect": "Each farm worker adds food. A shortfall stops growth, then shrinks the colony.",
				"forecast": ("Drafted staffing: %s food." % Fmt.signed(int(fe.get("food", 0)) - pop)) if not fe.is_empty() else ""}},
		{"icon": "industry", "cap": "Industry", "value": str(int(e["industry"])), "delta": "", "tone": "",
			"explain": {"title": "Industry", "value": "%d per turn" % int(e["industry"]), "parts": [["Factories", str(int(s["factories"])), ""], ["Factory workers", str(int(w["industry"])), ""], ["Unpowered factories", str(int(e["inactiveFactories"])), "warn" if int(e["inactiveFactories"]) > 0 else ""]],
				"effect": "Builds the first item in the queue each turn. Unpowered factories produce nothing.",
				"forecast": ("Drafted staffing: %d industry." % int(fe.get("industry", 0))) if not fe.is_empty() else ""}},
		{"icon": "research", "cap": "Research", "value": str(int(e["research"])), "delta": "", "tone": "info",
			"explain": {"title": "Research", "value": "%d per turn" % int(e["research"]), "parts": [["Laboratories", str(int(s["laboratories"])), ""], ["Lab workers", str(int(w["research"])), ""], ["Unpowered labs", str(int(e["inactiveLabs"])), "warn" if int(e["inactiveLabs"]) > 0 else ""]],
				"effect": "Pooled across the empire and split by the research allocation.",
				"forecast": ("Drafted staffing: %d research." % int(fe.get("research", 0))) if not fe.is_empty() else ""}},
		{"icon": "energy", "cap": "Energy", "value": "%d/%d" % [eu, ea], "delta": "", "tone": "alert" if eu > ea else "",
			"explain": {"title": "Energy", "value": "%d used of %d" % [eu, ea], "parts": [["Power plants", str(int(s["powerPlants"])), ""], ["Fusion plants", str(int(s.get("fusionPlants", 0))), ""], ["Spare", str(ea - eu), "alert" if ea < eu else "good"]],
				"effect": "Structures draw power; when demand exceeds supply the last ones go idle. Build power plants to add capacity."}},
		{"icon": "credits", "cap": "Credits", "value": Fmt.signed(int(e["net"])), "delta": "", "tone": "alert" if int(e["net"]) < 0 else "secondary",
			"explain": {"title": "Colony credits", "value": Fmt.signed(int(e["net"])) + " per turn", "parts": [["Income", "+%d" % int(e["income"]), "good"], ["Structure upkeep", "-%d" % int(e["upkeep"]), "alert"]],
				"effect": "Net credits this colony adds to the treasury each turn. Every structure costs upkeep."}},
	]

## Planet explanation for the orrery.
static func planet_explain(p: Dictionary) -> Dictionary:
	var parts := [["Supports now", str(int(p["capacity"])), ""], ["When terraformed", str(int(p["maxCapacity"])), "good"]]
	if int(p.get("terraformNeeded", 0)) > 0:
		parts.append(["Terraforming", "%d/%d work" % [int(p["terraformWork"]), int(p["terraformNeeded"])], "info"])
	return {"title": str(p["name"]), "value": str(p.get("className", "")), "parts": parts, "effect": "State: %s. Terraforming raises the population it supports." % str(p.get("terraformState", ""))}
