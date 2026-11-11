class_name DatabaseVM
extends ViewModel
## Database (encyclopedia) model: the tab, search and ship facet filters (faction, role, era, source), the research
## field filter, the selected entry, and the list rows and entry lookups over the core's `encyclopedia`.

const ROLE_NAMES := {0: "Scout", 1: "Destroyer", 2: "Cruiser", 3: "Strike cruiser", 4: "Command ship", 5: "Colony ship", 6: "Outpost", 7: "Starbase", 8: "Station", 9: "Troop transport"}
const RACE_TO_EMPIRE := {1: 1, 3: 2, 4: 3, 0: 4, 2: 5, 35: 6, 36: 7, 37: 8}
const RACE_TO_FACTION := {1: "Federation", 3: "Klingon", 4: "Romulan", 0: "Cardassian", 2: "Ferengi", 35: "Dominion", 36: "Borg", 37: "Breen"}
const MAJOR_FACTIONS := ["Federation", "Klingon", "Romulan", "Cardassian", "Ferengi", "Dominion", "Borg", "Breen"]
const TABS := [["ships", "Ships"], ["technologies", "Tech"], ["structures", "Structures"], ["races", "Races"], ["minors", "Minor races"]]
const SOURCES := ["Legacy", "Canon", "ModDerived", "Kelvin"]

var tab := "ships"
var query_text := ""
var faction := "__init"
var role := ""
var era := ""
var source := ""
var field := -1
var selected := -1

func data() -> Dictionary:
	return session.get_encyclopedia()

func fields() -> Array:
	return meta().get("researchFields", [])

## First use: the faction filter starts on the player's own power.
func init_faction() -> void:
	if faction != "__init":
		return
	faction = ""
	if session.in_game():
		for e in meta().get("empires", []):
			if int(e["id"]) == player():
				faction = RACE_TO_FACTION.get(int(e["race"]), "")

func race_name(id: int) -> String:
	for r in data().get("races", []):
		if int(r["id"]) == id:
			return str(r["name"])
	return "-"

## A ship's filter facets: {faction, role, era, source}.
func facets(s: Dictionary) -> Dictionary:
	var r: Variant = s.get("roster")
	if r is Dictionary:
		return {"faction": str(r["faction"]), "role": Roster.role_label(str(r["hullRole"])), "era": str(r["era"]), "source": str(r["sourceKind"])}
	return {"faction": RACE_TO_FACTION.get(int(s["race"]), race_name(int(s["race"]))), "role": ROLE_NAMES.get(int(s["role"]), "Role %d" % int(s["role"])), "era": "Legacy", "source": "Legacy"}

## Filter choices: {factions (majors first), roles, eras (Legacy first, then canonical order)}.
func ship_facets() -> Dictionary:
	var factions := []
	var roles := []
	var eras := {}
	for s in data().get("ships", []):
		var fc := facets(s)
		if not factions.has(fc["faction"]):
			factions.append(fc["faction"])
		if not roles.has(fc["role"]):
			roles.append(fc["role"])
		eras[fc["era"]] = true
	var extra: Array = factions.filter(func(x): return not MAJOR_FACTIONS.has(x))
	extra.sort()
	roles.sort()
	var era_list := ["Legacy"]
	for e in Roster.ERA_ORDER:
		if eras.has(e):
			era_list.append(e)
	return {"factions": MAJOR_FACTIONS + extra, "roles": roles, "eras": era_list}

func pick_tab(id: String) -> void:
	tab = id
	selected = -1

## List rows for the current tab and filters: [{id, title, sub, badge}].
func rows() -> Array:
	var d := data()
	var q := query_text.strip_edges().to_lower()
	var out := []
	var fl := fields()
	match tab:
		"ships":
			for s in d.get("ships", []):
				var fc := facets(s)
				if faction != "" and fc["faction"] != faction:
					continue
				if role != "" and fc["role"] != role:
					continue
				if era != "" and fc["era"] != era:
					continue
				if source != "" and fc["source"] != source:
					continue
				var ro: Variant = s.get("roster")
				if q != "" and not ("%s %s %s %s" % [s["designClass"], s["designName"], s["description"], ro["source"] if ro is Dictionary else ""]).to_lower().contains(q):
					continue
				var badge := Roster.source_label(ro["sourceKind"]) if ro is Dictionary and ro["sourceKind"] != "Canon" else "%s ind" % Fmt.fmt(s["industryCost"])
				out.append({"id": int(s["id"]), "title": str(s["designClass"]), "sub": "%s · %s%s" % [s["designName"], Roster.faction_label(fc["faction"]), (" · " + Roster.era_label(fc["era"])) if ro is Dictionary else ""], "badge": badge})
		"technologies":
			for t in d.get("technologies", []):
				if (field < 0 or int(t["field"]) == field) and (q == "" or ("%s %s" % [t["name"], t["description"]]).to_lower().contains(q)):
					out.append({"id": int(t["id"]), "title": str(t["name"]), "sub": "%s level %d" % [fl[int(t["field"])] if int(t["field"]) < fl.size() else "-", int(t["level"])], "badge": "%s pts" % Fmt.fmt(t["cost"])})
		"structures":
			for s2 in d.get("buildings", []):
				if (field < 0 or int(s2["field"]) == field) and (q == "" or ("%s %s" % [s2["name"], s2["description"]]).to_lower().contains(q)):
					out.append({"id": int(s2["id"]), "title": str(s2["name"]), "sub": "%s %d" % [fl[int(s2["field"])] if int(s2["field"]) >= 0 and int(s2["field"]) < fl.size() else "-", int(s2["level"])], "badge": "%s ind" % Fmt.fmt(s2["industryCost"])})
		"minors":
			for r in d.get("minorRaces", []):
				if q == "" or ("%s %s %s %s" % [r["raceName"], r["homeworld"], r["temperament"], r["description"]]).to_lower().contains(q):
					out.append({"id": int(r["raceId"]), "title": str(r["raceName"]), "sub": "%s · %s" % [r["temperament"], r["homeworld"]], "badge": str(r["bonus"])})
		_:
			for r in d.get("races", []):
				if q == "" or str(r["name"]).to_lower().contains(q):
					out.append({"id": int(r["id"]), "title": str(r["name"]), "sub": "Major power" if int(r["id"]) <= 4 else "Minor race", "badge": ""})
	return out

## The selected row among `rs`, else the first, else {}.
func current(rs: Array) -> Dictionary:
	for r in rs:
		if int(r["id"]) == selected:
			return r
	return rs[0] if rs.size() > 0 else {}

## The entry with `id` in an encyclopedia list ("ships", "technologies", "buildings", "minorRaces").
func entry(list_key: String, id: int, key: String = "id") -> Dictionary:
	var out := {}
	for x in data().get(list_key, []):
		if int(x[key]) == id:
			out = x
	return out

func ships_of_race(id: int) -> int:
	return data().get("ships", []).filter(func(s): return int(s["race"]) == id).size()

## Whether the player's empire has researched a technology (false outside a game).
func researched(t: Dictionary) -> bool:
	return int(view()["empire"]["techs"][int(t["field"])]["level"]) >= int(t["level"])
