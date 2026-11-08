class_name ShellVM
extends ViewModel
## The in-game chrome: the top bar's stats and buttons and the rail's extra entries.

## Heading lines: [player name, "era · rules · stardate"].
func heading() -> Array:
	var m: Dictionary = view()["meta"]
	var turn := int(m["turn"])
	return [str(m["playerName"]).to_upper(), "%s era · %s rules · stardate %d.%d" % [m.get("eraName", ""), m.get("rules", {}).get("presetName", ""), 2360 + turn / 4, (turn % 4) * 25]]

## Top-bar stats left of the spacer: [{cap, value, tone, tip}].
func stats() -> Array:
	var v := view()
	var e: Dictionary = v["empire"]
	var inc: Dictionary = e.get("income", {})
	var net := int(inc.get("net", 0))
	return [
		{"cap": "Turn", "value": str(int(v["meta"]["turn"])), "tone": "", "tip": "Current turn. Enter ends the turn."},
		{"cap": "Credits", "value": "%s  %s" % [Lcars.fmt(e.get("credits", 0)), Lcars.signed(net)], "tone": "alert" if net < 0 else "secondary",
			"tip": "Treasury ¢%s. Per turn: colonies %s, trade %s, ships -%s. Drafted spending ¢%s." % [Lcars.fmt(e.get("credits", 0)), inc.get("colonies", 0), inc.get("trade", 0), inc.get("ships", 0), e.get("draftCredits", 0)]},
		{"cap": "Research", "value": "%d/t" % int(e.get("researchOutput", 0)), "tone": "", "tip": "Research points per turn across the empire. Reserve %s." % e.get("researchReserve", 0)},
		{"cap": "Industry", "value": str(int(e.get("industry", 0))), "tone": "", "tip": "Total industry per turn of all colonies."},
		{"cap": "Food", "value": str(int(e.get("food", 0))), "tone": "good", "tip": "Total food produced per turn."},
		{"cap": "Pop", "value": str(int(e.get("population", 0))), "tone": "", "tip": "Total population."},
		{"cap": "Intel", "value": str(int(e.get("intelligence", 0))), "tone": "tertiary", "tip": "Intelligence points per turn."},
	]

func orders_stat() -> Dictionary:
	return {"cap": "Orders", "value": str(view().get("pending", []).size()), "tone": "", "tip": "Orders drafted this turn. They resolve when the turn ends."}

## Why End turn is disabled, or "".
func end_turn_block() -> String:
	return "Finish the battle in progress first" if session.battle_active() else ""

## Rail entries below the screens: [{id, label, hint}]; ids are "combat" (a screen) or overlay kinds.
func rail_extras() -> Array:
	var out := [
		{"id": "events", "label": "Events", "hint": "Galactic events and decisions"},
		{"id": "victory", "label": "Victory", "hint": "Victory conditions and megaprojects"},
		{"id": "briefing", "label": "Chronicle", "hint": "Last turn briefing and chronicle"},
	]
	if session.battle_active():
		out.push_front({"id": "combat", "label": "Battle", "hint": "Tactical battle in progress"})
	return out
