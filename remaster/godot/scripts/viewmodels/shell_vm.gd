class_name ShellVM
extends ViewModel
## The in-game chrome: the top bar's key figures (each with its explanation, docs/UI-DESIGN-SYSTEM.md §8), the
## rail's extra entries and the command palette's items.

## Heading lines: [player name, "era · rules · stardate"].
func heading() -> Array:
	var m: Dictionary = view()["meta"]
	var turn := int(m["turn"])
	return [str(m["playerName"]).to_upper(), "%s era · %s rules · stardate %d.%d" % [m.get("eraName", ""), m.get("rules", {}).get("presetName", ""), 2360 + turn / 4, (turn % 4) * 25]]

## Sums over the colonies for the explanations: food balance, starving colonies, idle shipyards.
func _colony_sums() -> Dictionary:
	var food_bal := 0
	var short := []
	var idle := []
	for c in view().get("colonies", []):
		var bal := int(c["economy"]["food"]) - int(c["population"])
		food_bal += bal
		if int(c["economy"].get("foodDeficit", 0)) > 0:
			short.append(str(c["name"]))
		if not (c.get("building") is Dictionary):
			idle.append(str(c["name"]))
	return {"food": food_bal, "short": short, "idle": idle}

## Top-bar stats left of the spacer: [{cap, icon, value, delta, tone, tip, explain}].
func stats() -> Array:
	var v := view()
	var e: Dictionary = v["empire"]
	var inc: Dictionary = e.get("income", {})
	var net := int(inc.get("net", 0))
	var sums := _colony_sums()
	var credits := int(e.get("credits", 0))
	var credit_ex := {"title": "Treasury", "value": "¢" + Fmt.fmt(credits),
		"parts": [["Colony income (net of upkeep)", Fmt.signed(int(inc.get("colonies", 0))), "good"], ["Trade routes", Fmt.signed(int(inc.get("trade", 0))), ""],
			["Ship upkeep", "-" + str(int(inc.get("ships", 0))), "alert"], ["Drafted spending this turn", "-" + str(int(inc.get("spend", 0))), ""], ["Net per turn", Fmt.signed(net), "alert" if net < 0 else "good"]],
		"effect": "Build or buy orders spend from the treasury when the turn ends. Scrapping ships cuts upkeep; trade treaties and routes add income.",
		"forecast": ("Treasury runs dry in %d turns at this rate." % int(ceil(float(credits) / -net))) if net < 0 else "Treasury after this turn: ¢%s." % Fmt.fmt(credits + net)}
	var rr := int(e.get("researchOutput", 0))
	var research_ex := {"title": "Research output", "value": "%d per turn" % rr,
		"parts": [["Reserve (unallocated)", str(int(e.get("researchReserve", 0))), ""]],
		"effect": "Laboratory workers and research structures produce it; the Research screen splits it across the six fields."}
	var food_ex := {"title": "Food", "value": str(int(e.get("food", 0))),
		"parts": [["Produced per turn", str(int(e.get("food", 0))), "good"], ["Eaten (1 per population)", "-" + str(int(e.get("population", 0))), ""], ["Balance", Fmt.signed(sums["food"]), "alert" if sums["food"] < 0 else "good"]],
		"effect": "A colony short of food stops growing and then shrinks. Move workers to food or build farms." + (" Short now: %s." % ", ".join(sums["short"]) if sums["short"].size() > 0 else "")}
	return [
		{"cap": "Turn", "icon": "turn", "value": str(int(v["meta"]["turn"])), "delta": "", "tone": "", "tip": "Current turn. Enter ends the turn.",
			"explain": {"title": "Turn", "value": str(int(v["meta"]["turn"])), "effect": "Every drafted order resolves when the turn ends (Enter)."}},
		{"cap": "Credits", "icon": "credits", "value": Fmt.fmt(credits), "delta": Fmt.signed(net), "tone": "alert" if net < 0 else "secondary",
			"tip": "Treasury ¢%s. Per turn: colonies %s, trade %s, ships -%s. Drafted spending ¢%s." % [Fmt.fmt(credits), inc.get("colonies", 0), inc.get("trade", 0), inc.get("ships", 0), e.get("draftCredits", 0)], "explain": credit_ex},
		{"cap": "Research", "icon": "research", "value": "%d/t" % rr, "delta": "", "tone": "info", "tip": "Research points per turn across the empire. Reserve %s." % e.get("researchReserve", 0), "explain": research_ex},
		{"cap": "Industry", "icon": "industry", "value": str(int(e.get("industry", 0))), "delta": "", "tone": "", "tip": "Total industry per turn of all colonies.",
			"explain": {"title": "Industry", "value": "%d per turn" % int(e.get("industry", 0)), "effect": "Factory workers produce it; each colony spends its own on its build queue." + (" Idle queues: %s." % ", ".join(sums["idle"]) if sums["idle"].size() > 0 else "")}},
		{"cap": "Food", "icon": "food", "value": str(int(e.get("food", 0))), "delta": Fmt.signed(sums["food"]), "tone": "alert" if sums["food"] < 0 else "good", "tip": "Total food produced per turn.", "explain": food_ex},
		{"cap": "Pop", "icon": "population", "value": str(int(e.get("population", 0))), "delta": "", "tone": "", "tip": "Total population.",
			"explain": {"title": "Population", "value": str(int(e.get("population", 0))), "effect": "Population works the colonies. It grows while food is in surplus and capacity allows; terraforming raises capacity."}},
		{"cap": "Intel", "icon": "intel", "value": str(int(e.get("intelligence", 0))), "delta": "", "tone": "tertiary", "tip": "Intelligence points per turn.",
			"explain": {"title": "Intelligence", "value": str(int(e.get("intelligence", 0))), "effect": "Spent on espionage and sabotage, and on internal security against enemy agents (Intel screen)."}},
	]

func orders_stat() -> Dictionary:
	var n: int = view().get("pending", []).size()
	return {"cap": "Orders", "icon": "orders", "value": str(n), "tone": "", "tip": "Orders drafted this turn. They resolve when the turn ends.",
		"explain": {"title": "Orders this turn", "value": str(n), "effect": "Drafted orders resolve together when the turn ends. Preview the turn from the Empire screen."}}

## Why End turn is disabled, or "".
func end_turn_block() -> String:
	return "Finish the battle in progress first" if session.battle_active() else ""

## Rail entries below the screens: [{id, label, hint, icon, key}]; ids are "combat" (a screen) or overlay kinds.
func rail_extras() -> Array:
	var out := [
		{"id": "events", "label": "Events", "hint": "Galactic events and decisions", "icon": "events", "key": "E"},
		{"id": "victory", "label": "Victory", "hint": "Victory conditions and megaprojects", "icon": "victory", "key": "V"},
		{"id": "briefing", "label": "Chronicle", "hint": "Last turn briefing and chronicle", "icon": "chronicle", "key": "C"},
	]
	if session.battle_active():
		out.push_front({"id": "combat", "label": "Battle", "hint": "Tactical battle in progress", "icon": "combat", "key": ""})
	return out

const SCREEN_ICONS := {"galaxy": "galaxy", "system": "system", "empire": "empire", "research": "research", "fleets": "fleets", "diplomacy": "diplomacy", "intel": "intel", "database": "database"}

## Command palette items (docs/UI-DESIGN-SYSTEM.md §9): [{id, label, sub, icon, group}]. Ids: "screen:<id>",
## "overlay:<kind>", "colony:<systemId>", "fleet:<fleetId>", "cmd:<name>".
func palette_items() -> Array:
	var out := []
	for s in AppRoutes.GAME_SCREENS:
		out.append({"id": "screen:" + s["id"], "label": s["label"], "sub": "%s · %s" % [s["key"], s["hint"]], "icon": SCREEN_ICONS.get(s["id"], "arrow_right"), "group": "screen"})
	for o in [["events", "Galactic events", "E"], ["victory", "Victory conditions", "V"], ["briefing", "Chronicle", "C"], ["settings", "Settings", "F10"], ["help", "Keyboard help", "?"], ["menu", "Game menu", "Esc"]]:
		out.append({"id": "overlay:" + o[0], "label": o[1], "sub": o[2], "icon": {"events": "events", "victory": "victory", "briefing": "chronicle", "settings": "settings", "help": "info", "menu": "menu"}[o[0]], "group": "panel"})
	out.append({"id": "cmd:saves", "label": "Load or delete saves", "sub": "F9", "icon": "save", "group": "command"})
	out.append({"id": "cmd:quicksave", "label": "Quicksave", "sub": "Ctrl+S", "icon": "save", "group": "command"})
	out.append({"id": "cmd:preview", "label": "Preview turn", "sub": "what the turn will do", "icon": "clock", "group": "command"})
	if end_turn_block() == "":
		out.append({"id": "cmd:endturn", "label": "End turn", "sub": "Enter", "icon": "turn", "group": "command"})
	for c in view().get("colonies", []):
		out.append({"id": "colony:%d" % int(c["systemId"]), "label": str(c["name"]), "sub": "colony · pop %d/%d" % [int(c["population"]), int(c["capacity"])], "icon": "colony", "group": "colony"})
	for f in view().get("fleets", []):
		out.append({"id": "fleet:%d" % int(f["id"]), "label": "%s #%d" % [f["name"], int(f["id"])], "sub": "ship · %s" % str(f.get("className", "")), "icon": "ship", "group": "fleet"})
	for e in view().get("empires", []):
		if e.get("contact", false):
			out.append({"id": "screen:diplomacy", "label": str(e["name"]), "sub": "diplomacy · " + ("at war" if e.get("atWar", false) else "trust %d" % int(e.get("trust", 0))), "icon": "diplomacy", "group": "empire"})
	return out
