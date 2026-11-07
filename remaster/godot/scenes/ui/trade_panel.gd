class_name TradePanel
extends VBoxContainer
## Trade routes: the routes you run, what each pays, and the routes you could still establish.
## Core calls: tradeRoutes (bridge query: value, partner and blocker for each route and candidate), submit
## EstablishTradeRoute / CancelTradeRoute, and the diplomacy screen for the trade pact a route needs.

var app: App
var source := 0
var partner := 0
var data: Dictionary = {}

static func make(app_: App) -> TradePanel:
	var t := TradePanel.new()
	t.app = app_
	t.name = "TradePanel"
	t.add_theme_constant_override("separation", 10)
	t.size_flags_vertical = Control.SIZE_EXPAND_FILL
	t.refresh()
	return t

func _lock() -> String:
	return "Finish the battle in progress first" if app.view.get("session", {}).get("active", false) else ""

func refresh() -> void:
	for c in get_children():
		c.queue_free()
	var r: Variant = app.core.request("tradeRoutes")
	if not r is Dictionary:
		add_child(Lcars.empty_note("Trade routes unavailable: %s" % app.core.last_error))
		return
	data = r
	var inc: Dictionary = app.view["empire"]["income"]
	var head := Lcars.hbox(24)
	var active := 0
	var total := 0
	for rt in data["routes"]:
		if rt.get("active", false):
			active += 1
			total += int(rt["value"])
	head.add_child(Lcars.stat("Trade income", Lcars.signed(inc["trade"]), "good" if int(inc["trade"]) > 0 else "",
		"Each active route pays 5 + one tenth of the smaller of its two colonies' populations, every turn. Only routes to an empire at peace with a trade pact pay."))
	head.add_child(Lcars.stat("Routes", "%d active of %d" % [active, data["routes"].size()], "", "A colony runs one route at a time."))
	head.add_child(Lcars.stat("Active value", Lcars.signed(total) + "/turn", "good" if total > 0 else ""))
	add_child(head)
	if not data.get("advancedRules", true):
		add_child(Lcars.para("Classic rules: the first route to an empire also signs the trade pact.", Lcars.TEXT3, 13))
	add_child(_routes())
	add_child(_new_route())

func _routes() -> Control:
	var f := Lcars.frame("Your routes", "%d" % data["routes"].size(), "secondary")
	f.name = "RouteList"
	var b := Lcars.body(f)
	if data["routes"].is_empty():
		b.add_child(Lcars.empty_note("No trade routes yet. Befriend a major power, sign a trade pact in Diplomacy, then link one of your colonies to one of theirs."))
	var lock := _lock()
	for rt in data["routes"]:
		var row := Lcars.hbox(10)
		row.name = "Route_%d" % int(rt["from"])
		var txt := Lcars.label("%s  →  %s" % [rt["fromName"], rt["toName"]], "", 15, Lcars.TEXT)
		txt.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		row.add_child(txt)
		row.add_child(Lcars.label(str(rt["partnerName"]), "Muted", 13))
		var blocker := str(rt.get("blocker", ""))
		if rt.get("active", false):
			var v := Lcars.label("+%d/turn" % int(rt["value"]), "Mono", 15, Lcars.GOOD)
			v.tooltip_text = "5 + min(population of both colonies) / 10"
			v.mouse_filter = Control.MOUSE_FILTER_PASS
			row.add_child(v)
		else:
			var bd := Lcars.badge("pays nothing", "alert")
			bd.tooltip_text = blocker + " - the route stays established but pays 0 until this clears (would be +%d/turn)." % int(rt["value"])
			row.add_child(bd)
			row.add_child(Lcars.label(blocker, "", 12, Lcars.WARN))
		var from_id := int(rt["from"])
		var partner_id := int(rt["partner"])
		var names := "%s to %s" % [rt["fromName"], rt["toName"]]
		var cancel := Lcars.pill("Cancel", "alert", func(): _cancel(from_id, partner_id, names), lock)
		cancel.name = "CancelRoute_%d" % from_id
		row.add_child(cancel)
		b.add_child(row)
	return f

func _cancel(from_id: int, partner_id: int, names: String) -> void:
	var target := partner_id
	if target == 0:
		for e in app.view["empires"]:
			if e.get("contact", false):
				target = int(e["empire"])
				break
	app.confirm("Cancel the route %s?" % names, "The route stops paying at turn end. You can establish it again later.", "Cancel route",
		func():
			if app.issue({"type": "CancelTradeRoute", "systemId": from_id, "targetEmpire": target}, [], "Trade route cancelled."):
				refresh())

func _new_route() -> Control:
	var f := Lcars.frame("Establish a route", "", "primary")
	f.name = "NewRoute"
	var b := Lcars.body(f)
	var cands: Array = data["candidates"]
	if cands.is_empty():
		b.add_child(Lcars.empty_note("No route to offer: you need a free colony, and a colony of a contacted empire inside your sensor range."))
		return f
	var sources := {}
	for c in cands:
		sources[int(c["from"])] = str(c["fromName"])
	if not sources.has(source):
		source = int(sources.keys()[0])
	var targets: Array = cands.filter(func(c): return int(c["from"]) == source)
	targets.sort_custom(func(a, b2):
		var ab := str(a["blocker"]) != ""
		var bb := str(b2["blocker"]) != ""
		if ab != bb:
			return bb
		return int(a["value"]) > int(b2["value"]))
	var chosen := {}
	for t in targets:
		if int(t["to"]) == partner:
			chosen = t
	if chosen.is_empty():
		chosen = targets[0]
		partner = int(chosen["to"])
	var from_row := Lcars.hbox(8)
	from_row.add_child(Lcars.caption("From your colony"))
	var ob := OptionButton.new()
	ob.name = "RouteSource"
	var idx := 0
	for sid in sources.keys():
		ob.add_item(str(sources[sid]), int(sid))
		if int(sid) == source:
			ob.selected = idx
		idx += 1
	ob.item_selected.connect(func(i): source = ob.get_item_id(i); partner = 0; refresh())
	from_row.add_child(ob)
	b.add_child(from_row)
	var list := Lcars.vbox(4)
	for t in targets:
		var blocker := str(t["blocker"])
		var row := Lcars.hbox(8)
		var sel := Lcars.toggle("%s (%s)" % [t["toName"], t["partnerName"]], int(t["to"]) == partner, func(_on): partner = int(t["to"]); refresh())
		sel.name = "RouteTarget_%d" % int(t["to"])
		sel.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		row.add_child(sel)
		row.add_child(Lcars.label("+%d/turn" % int(t["value"]), "Mono", 14, Lcars.GOOD if blocker == "" else Lcars.TEXT3))
		if blocker != "":
			row.add_child(Lcars.label(blocker, "", 12, Lcars.WARN))
		list.add_child(row)
	b.add_child(Lcars.scroll(list))
	var go_blocker := _lock()
	if go_blocker == "":
		go_blocker = app.dry_run({"type": "EstablishTradeRoute", "systemId": source, "targetSystemId": int(chosen["to"]), "targetEmpire": int(chosen["partner"])})
	var acts := Lcars.hbox(8)
	var cmd := {"type": "EstablishTradeRoute", "systemId": source, "targetSystemId": int(chosen["to"]), "targetEmpire": int(chosen["partner"])}
	var est := Lcars.pill("Establish: %s → %s  +%d/turn" % [chosen["fromName"], chosen["toName"], int(chosen["value"])], "primary", func():
		if app.issue(cmd, [], "Trade route %s to %s established." % [chosen["fromName"], chosen["toName"]]):
			partner = 0
			refresh(), go_blocker)
	est.name = "EstablishRoute"
	acts.add_child(est)
	if str(chosen["blocker"]) == "An accepted trade treaty is required":
		var pid := int(chosen["partner"])
		acts.add_child(Lcars.pill("Open diplomacy", "ghost", func(): app.set_meta("diplomacy_focus", {"minor": false, "id": pid}); app.goto("diplomacy"), "", "Propose a trade pact to " + str(chosen["partnerName"])))
	b.add_child(acts)
	b.add_child(Lcars.para("Value is the credit income the turn resolver pays for the pair: 5 + one tenth of the smaller colony's population.", Lcars.TEXT3, 12))
	return f
