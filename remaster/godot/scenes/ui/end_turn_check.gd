class_name EndTurnCheck
extends RefCounted
## The end-turn check (presentation; the data comes from EndTurnVM): what the turn will do (the core's forecast of the turn, with this turn's drafted orders
## applied) and what has been left idle (empty queues, unstaffed workers, ships with no orders, starving or
## unpowered colonies). Warnings carry the screen to fix them on and, where one order cures them, a fix that
## the core has already dry-run (dryRunMany), so a button is only live when the order would be accepted.
##
## Core calls: forecast (what the turn does), describeTurn (last turn's briefing), dryRunMany (the fixes).

## {warnings: [{kind, tone, text, screen, system, fleet, fix: {label, command, reason}}], forecast: Dictionary,
##  last_turn: Dictionary, alerts: int}. Built by EndTurnVM (scripts/viewmodels/end_turn_vm.gd).
static func build(app: App) -> Dictionary:
	return EndTurnVM.build(app.session)

## A ship with nothing to do this turn: no course, no drafted order, no standing job.
static func is_idle(app: App, f: Dictionary) -> bool:
	return EndTurnVM.is_idle(app.session, f)

static func should_prompt(app: App, check: Dictionary) -> bool:
	if check["alerts"] > 0:
		return true
	return app.settings.get("confirmEndTurn", false) and check["warnings"].size() > 0

## The forecast block: credits, research, completions and growth, each with its source.
static func forecast_panel(app: App, check: Dictionary) -> Control:
	var fc: Dictionary = check["forecast"]
	var col := Lcars.vbox(6)
	col.name = "TurnForecast"
	if fc.is_empty() or fc.has("error") and not fc.has("credits"):
		col.add_child(Lcars.para("No forecast: %s" % str(fc.get("error", "the core gave none")), Lcars.TEXT3, 13))
		return col
	var g := Lcars.hbox(24)
	g.add_child(Lcars.stat("Credits", "%s  %s" % [Lcars.fmt(fc["creditsAfter"]), Lcars.signed(fc["credits"])], "alert" if int(fc["credits"]) < 0 else "secondary",
		"Treasury after this turn. Colonies and trade income, less structure and ship upkeep (ships %d, trade %d)." % [int(fc["fleetUpkeep"]), int(fc["tradeIncome"])]))
	g.add_child(Lcars.stat("Research", Lcars.fmt(fc["research"]), "info", "Research points pooled this turn and split by your allocation."))
	g.add_child(Lcars.stat("Fleet upkeep", "-%d" % int(fc["fleetUpkeep"]), "warn", "Military upkeep of every ship and station."))
	g.add_child(Lcars.stat("Trade", Lcars.signed(fc["tradeIncome"]), "good" if int(fc["tradeIncome"]) > 0 else "", "Income from established trade routes."))
	col.add_child(g)
	var lines: Array = []
	for fd in fc.get("fields", []):
		if int(fd["levelsGained"]) > 0:
			var tech := _tech_name(app, int(fd["field"]))
			lines.append("%s advances %d level%s." % [tech, int(fd["levelsGained"]), "" if int(fd["levelsGained"]) == 1 else "s"])
	for c in fc.get("colonies", []):
		var cname := str(app.find_by_id("systems", int(c["systemId"])).get("name", "Colony"))
		if str(c.get("completed", "")) != "":
			lines.append("%s completes %s." % [cname, c["completed"]])
		if int(c.get("population", 0)) != 0:
			lines.append("%s population %s." % [cname, Lcars.signed(c["population"])])
	if lines.is_empty():
		lines.append("No construction completes and no technology level is gained this turn.")
	for ln in lines.slice(0, 8):
		col.add_child(Lcars.label("•  " + ln, "", 14, Lcars.TEXT2))
	if lines.size() > 8:
		col.add_child(Lcars.label("... and %d more" % (lines.size() - 8), "Muted", 12))
	return col

static func _tech_name(app: App, field: int) -> String:
	for t in app.view["empire"].get("techs", []):
		if int(t["field"]) == field:
			return str(t["name"])
	return "Field %d" % field

## Opens the check as a modal. `end_label` is the primary button; "preview" shows the check without ending.
static func open(app: App, check: Dictionary, preview: bool = false) -> Control:
	var col := Lcars.vbox(12)
	col.name = "EndTurnCheck"
	var turn := int(app.view["meta"]["turn"])
	col.add_child(Lcars.caption("What turn %d will do" % turn))
	col.add_child(forecast_panel(app, check))
	var last: Dictionary = check["last_turn"]
	if not last.is_empty():
		col.add_child(Lcars.label("Last turn (%d): %d orders applied, credits %s to %s, research +%s." % [int(last.get("turn", 0)), int(last.get("ordersApplied", 0)), Lcars.fmt(last.get("creditsBefore", 0)), Lcars.fmt(last.get("creditsAfter", 0)), Lcars.fmt(last.get("researchGained", 0))], "Muted", 13))
	col.add_child(Lcars.separator())
	var warns: Array = check["warnings"]
	col.add_child(Lcars.caption("Left undone  %d" % warns.size() if warns.size() > 0 else "Nothing left undone"))
	var list := Lcars.vbox(6)
	list.name = "Warnings"
	for w in warns:
		list.add_child(_warning_row(app, w))
	if warns.is_empty():
		list.add_child(Lcars.label("Every colony has work queued and staffed, and every ship has orders.", "Muted", 14))
	var sc := Lcars.scroll(list)
	sc.custom_minimum_size.y = min(300, 56 * max(1, warns.size()) + 8)
	col.add_child(sc)
	var row := Lcars.hbox(10)
	row.alignment = BoxContainer.ALIGNMENT_END
	var modal: Control
	row.add_child(Lcars.pill("Close" if preview else "Go back", "ghost", func(): app.close_modal(modal), "", "", "Esc"))
	if not preview:
		var go := Lcars.pill("End turn anyway" if warns.size() > 0 else "End turn", "alert" if check["alerts"] > 0 else "secondary", func():
			app.close_modal(modal)
			app.end_turn())
		go.name = "EndTurnAnyway"
		row.add_child(go)
	col.add_child(row)
	modal = app.open_modal(col, "End-turn check" if not preview else "Turn preview", Vector2(820, 0), "alert" if check["alerts"] > 0 else "secondary")
	return modal

static func _warning_row(app: App, w: Dictionary) -> Control:
	var tone := str(w["tone"])
	var r := Lcars.hbox(8)
	r.add_child(Lcars.badge({"alert": "ALERT", "warn": "WARN", "info": "NOTE"}.get(tone, "NOTE"), {"alert": "alert", "warn": "secondary", "info": "dim"}.get(tone, "dim")))
	var t := Lcars.para(str(w["text"]), Lcars.ALERT if tone == "alert" else Lcars.TEXT2, 14)
	t.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	r.add_child(t)
	if w.has("fix"):
		var fx: Dictionary = w["fix"]
		r.add_child(Lcars.pill(str(fx["label"]), "secondary", func():
			if app.issue(fx["command"], [], "%s." % fx["label"]):
				app.close_all_modals()
				app.request_end_turn(), str(fx["reason"])))
	var screen := str(w["screen"])
	var sid := int(w.get("system", 0))
	var fid := int(w.get("fleet", 0))
	r.add_child(Lcars.pill("Fix", "ghost", func():
		app.close_all_modals()
		if sid != 0:
			app.select_system(sid)
		if fid != 0:
			app.select_fleet(fid)
		app.goto(screen)))
	return r
