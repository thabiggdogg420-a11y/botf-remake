class_name OverlaysVM
extends ViewModel
## Data for the modal overlays: the turn briefing and its categories, galactic events and their choices, victory
## progress and the end-of-game summary. Presentation: scenes/ui/screens/overlays.gd.

const ORDER := ["victory", "combat", "events", "diplomacy", "stations", "intel", "construction", "research", "colonization", "exploration", "fleets", "economy"]
const CONDITION_BLURB := {
	"Conquest": "Be the last power holding colonies.",
	"Domination": "Hold 60% of all star systems after turn 20.",
	"Scientific": "Master every field, then complete the era megaproject.",
	"Diplomatic": "Lead an alliance bloc holding two-thirds of the galaxy's population.",
	"Vendetta": "Eliminate both of your sworn racial rivals, from turn 20.",
}
const CATEGORY_TONE := {"combat": "alert", "victory": "good", "events": "secondary", "diplomacy": "tertiary", "research": "info", "intel": "tertiary", "construction": "primary", "stations": "secondary"}

## The last briefing: the one from the latest end of turn, else the core's `briefing`. null before any.
func briefing() -> Variant:
	var b: Variant = session.briefing
	if not b is Dictionary:
		var fetched: Variant = query("briefing")
		if fetched is Dictionary:
			b = fetched
			session.briefing = b
	return b

## The briefing's events as the empire's own log (scripts/trek_text.gd): each entry keeps the core's line in "text" and carries
## the empire's wording in "voice". Also adds the lines the core leaves to the UI (construction complete, food, treasury).
func log_entries() -> Array:
	var b: Variant = briefing()
	if not b is Dictionary:
		return []
	var names := {}
	for c in view().get("colonies", []):
		names[int(c["systemId"])] = str(c["name"])
	return TrekText.log_entries(player(), b, {"own_name": str(view()["meta"].get("playerName", "")), "sysnames": names})

## Briefing event categories in display order (known first, then any others as they appear).
static func categories(events: Array) -> Array:
	var cats := []
	for c in ORDER:
		for e in events:
			if e["category"] == c and not cats.has(c):
				cats.append(c)
	for e in events:
		if not cats.has(e["category"]):
			cats.append(e["category"])
	return cats

static func severity_tone(sev: String) -> String:
	return "alert" if sev == "alert" else ("warn" if sev == "warn" else ("good" if sev == "good" else "info"))

## Galactic events, awaiting a choice first, then by deadline.
func sorted_events() -> Array:
	var evs: Array = view().get("events", []).duplicate()
	evs.sort_custom(func(a, b):
		if a["awaitingChoice"] != b["awaitingChoice"]:
			return a["awaitingChoice"]
		return int(a["deadline"]) < int(b["deadline"]))
	return evs

func choice_block(o: Dictionary) -> String:
	return "Finish the battle in progress first" if session.battle_active() else str(o.get("blocker", ""))

func resolve_event_choice(e: Dictionary, o: Dictionary) -> bool:
	return issue({"type": "ResolveEventChoice", "eventId": int(e["id"]), "eventOption": int(o["index"])}, e.get("draftIds", []), "%s: “%s” takes effect next turn." % [e["name"], o["label"]])

## Conquered worlds waiting for the player's Liberate / Subjugate / Exterminate decision (schema 15).
func aftermaths() -> Array:
	return view().get("aftermaths", [])

func aftermath_block(o: Dictionary) -> String:
	return "Finish the battle in progress first" if session.battle_active() else str(o.get("blocker", ""))

func resolve_aftermath(a: Dictionary, o: Dictionary) -> bool:
	return issue({"type": "ResolveAftermath", "systemId": int(a["systemId"]), "eventOption": int(o["id"])}, [], "%s: %s." % [a["name"], str(o["name"]).to_lower()])

## The headline of one aftermath card.
static func aftermath_line(a: Dictionary) -> String:
	var who := str(a.get("minor", ""))
	if who == "":
		who = str(a.get("formerName", "the defenders"))
	return "Taken from %s: %d of %d people remain. Decide within %d turn%s, or the world stays subjugated." % [who, int(a.get("population", 0)), int(a.get("populationBefore", 0)), int(a.get("turnsLeft", 0)), "" if int(a.get("turnsLeft", 0)) == 1 else "s"]

## End of game: {won, winner, reason, stats: [[label, value]]}.
func end_summary(outcome: String) -> Dictionary:
	var v := view()
	var won := outcome == "victory"
	var winner := int(v["meta"].get("winner", 0))
	# Hotseat (MULTIPLAYER.md decision 1): the humans who share the win; the end screen judges the human in the chair by it
	# and names every allied human. One-player games have no hotseat list and keep the outcome as it is.
	var winners: Array = v["meta"].get("winners", [])
	var hotseat: Array = v["meta"].get("hotseat", [])
	var allied := ""
	if not hotseat.is_empty() and outcome != "ongoing":
		won = winners.has(player())
		var names := PackedStringArray()
		for h in winners:
			names.append(session.empire_name(int(h)))
		if names.size() >= 2:
			allied = "Shared victory: %s, allied players." % ", ".join(names)
		elif names.size() == 1:
			allied = "%s wins." % names[0]
	var reason := "Your empire commands the quadrant."
	for c in v.get("victory", {}).get("conditions", []):
		if c["achieved"] and won:
			reason = "%s victory: %s" % [c["name"], c["explanation"]]
	if not won:
		reason = ("%s prevailed." % session.empire_name(winner)) if winner != 0 and winner != player() else ("Your last colony has fallen." if v.get("colonies", []).is_empty() else "Your empire has been defeated.")
	var techs := 0
	for t in v["empire"].get("techs", []):
		techs += int(t["level"])
	var mine := 0
	for s in v.get("stations", []):
		if s.get("mine", false):
			mine += 1
	var contacts := 0
	for e in v.get("empires", []):
		if e.get("contact", false):
			contacts += 1
	var stats := [["Turns", v["meta"]["turn"]], ["Colonies", v["colonies"].size()], ["Population", Fmt.fmt(v["empire"]["population"])], ["Ships", v["fleets"].size()],
		["Stations", mine], ["Treasury", Fmt.fmt(v["empire"]["credits"])], ["Tech levels", techs], ["Contacts", contacts]]
	return {"won": won, "winner": winner, "reason": reason, "stats": stats, "allied": allied, "winners": winners}
