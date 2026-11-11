class_name ResearchVM
extends ViewModel
## Research screen model: the six fields from the core's `research` query, the turn forecast, the drafted
## allocation (kept at 100% by rebalance), per-field locks and the focused field for the ladder.

const EVEN := [17, 17, 17, 17, 16, 16]

var research: Dictionary = {}
var forecast: Dictionary = {}
var alloc: Array = []
var locks := [false, false, false, false, false, false]
var focus := 0

## Sets `field` to `value` and spreads the remainder over the unlocked fields so the total stays 100.
static func rebalance(a: Array, field: int, value: int, locked: Array) -> Array:
	var nxt := a.duplicate()
	var locked_total := 0
	for i in nxt.size():
		if i != field and locked[i]:
			locked_total += int(nxt[i])
	nxt[field] = clamp(value, 0, 100 - locked_total)
	var others := []
	for i in nxt.size():
		if i != field and not locked[i]:
			others.append(i)
	var remaining := 100 - locked_total - int(nxt[field])
	if others.is_empty():
		nxt[field] = 100 - locked_total
		return nxt
	var current := 0
	for i in others:
		current += int(nxt[i])
	for i in others:
		nxt[i] = int(floor(float(nxt[i]) / current * remaining)) if current > 0 else remaining / others.size()
	var diff := remaining
	for i in others:
		diff -= int(nxt[i])
	var k := 0
	while diff > 0:
		nxt[others[k % others.size()]] += 1
		diff -= 1
		k += 1
	return nxt

## 50% to `field`, 10% to every other.
static func focus_alloc(field: int) -> Array:
	var a := []
	for k in 6:
		a.append(50 if k == field else 10)
	return a

## The highest research level of this campaign: 10, or up to 14 under the eras rule (schema 22).
func cap() -> int:
	return int(research.get("techCap", 10))

## Reloads research, forecast and the allocation in force.
func fetch() -> void:
	research = query_dict("research")
	forecast = query_dict("forecast")
	alloc = current()

func available() -> bool:
	return not research.is_empty()

## The allocation the empire runs now.
func current() -> Array:
	return view()["empire"]["techs"].map(func(t): return int(t["allocation"]))

func fields() -> Array:
	return research.get("fields", [])

func total() -> int:
	var sum := 0
	for a in alloc:
		sum += int(a)
	return sum

func dirty() -> bool:
	return alloc != current()

func slide(field: int, value: int) -> void:
	alloc = rebalance(alloc, field, value, locks)

## "" when Apply is live, else why not.
func apply_block() -> String:
	if session.battle_active():
		return "Finish the battle first"
	return "No changes" if not dirty() else ""

func forecast_ok() -> bool:
	return str(forecast.get("error", "x")) == ""

## Progress of a field toward its next level: {next, cost, gained, turns}.
func progress(i: int) -> Dictionary:
	var fd: Dictionary = fields()[i]
	var levels: Array = fd["levels"]
	var lvl := int(fd["level"])
	var nxt: Dictionary = levels[lvl + 1] if lvl + 1 < levels.size() else {}
	var cost := int(nxt.get("cost", 0))
	var gained := 0
	for x in forecast.get("fields", []):
		if int(x["field"]) == i:
			gained = int(x["gained"])
	var turns := int(ceil(float(cost - int(fd["progress"])) / gained)) if cost > 0 and gained > 0 else 0
	return {"next": nxt, "cost": cost, "gained": gained, "turns": turns}

## Technologies still locked in this ruleset: [{name, blocker}].
func blocked() -> Array:
	return research.get("blockers", []).filter(func(x): return str(x.get("blocker", "")) != "")

## Orders the drafted allocation, replacing any drafted earlier this turn.
func apply() -> bool:
	return issue({"type": "AllocateResearch", "researchAllocation": alloc}, session.pending_ids("AllocateResearch"), "Research allocation ordered for this turn.")
