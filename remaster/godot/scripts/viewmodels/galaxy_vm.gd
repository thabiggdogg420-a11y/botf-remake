class_name GalaxyVM
extends ViewModel
## The galaxy screen's overlay (F1): alerts, galactic-event decisions, map layer toggles, drafted orders, the map legend and
## the status figures. Plain data only; GalaxyHud draws it. The map, the inspector and the order menu stay in
## scenes/galaxy and read the App API. See docs/CLIENT-ARCHITECTURE.md.

## Map layers: [key, label, hint, icon].
const LAYERS := [
	["territory", "Territory", "Colony influence of every known empire", "empire"],
	["supply", "Supply", "Supply range of the selected unit (from the core)", "range"],
	["routes", "Routes", "Plotted courses and ETAs", "arrow_right"],
	["trade", "Trade", "Trade routes", "trade"],
	["fog", "Fog", "Fog of war", "eye"],
	["grid", "Grid", "Sector grid and coordinates", "galaxy"],
	["labels", "Labels", "System names", "database"],
]

## Alerts shown at once in the panel; the rest are counted.
const ALERT_ROWS := 7

## {tone, title, count, items: [{text, tone, icon, target}], more}. `tone` is the worst severity; `target` is alert_target().
func alerts() -> Dictionary:
	var list: Array = view().get("alerts", [])
	var tone := "good"
	var items := []
	for a in list:
		var sev := str(a.get("severity", "info"))
		if sev == "alert":
			tone = "alert"
		elif tone != "alert":
			tone = "warn"
		if items.size() < ALERT_ROWS:
			var t := "alert" if sev == "alert" else ("warn" if sev == "warn" else "info")
			items.append({"text": str(a.get("text", "")), "tone": t, "icon": t, "target": alert_target(a)})
	var title := ("%d alert%s" % [list.size(), "" if list.size() == 1 else "s"]) if list.size() > 0 else "All systems nominal"
	return {"tone": tone, "title": title, "count": list.size(), "items": items, "more": maxi(0, list.size() - ALERT_ROWS)}

## What clicking an alert does: {} (nothing), or {do: "focus", system|fleet: id, x, y}, {do: "screen", id}, {do: "overlay", id}.
func alert_target(a: Dictionary) -> Dictionary:
	if a.has("systemId"):
		var s := session.find_by_id("systems", int(a["systemId"]))
		return {"do": "focus", "system": int(a["systemId"]), "x": int(s.get("x", 0)), "y": int(s.get("y", 0)), "found": not s.is_empty()}
	if a.has("fleetId"):
		var f := session.find_by_id("fleets", int(a["fleetId"]))
		return {"do": "focus", "fleet": int(a["fleetId"]), "x": int(f.get("x", 0)), "y": int(f.get("y", 0)), "found": not f.is_empty()}
	var kind := str(a.get("kind", ""))
	if kind in ["proposal", "offer", "callToArms"]:
		return {"do": "screen", "id": "diplomacy"}
	if kind == "event":
		return {"do": "overlay", "id": "events"}
	return {}

## The galactic-events pill: {} when no event is active, else {text, tone, open, icon}.
func events() -> Dictionary:
	var evs: Array = view().get("events", [])
	if evs.is_empty():
		return {}
	var open := 0
	for e in evs:
		if e.get("awaitingChoice", false) and int(e.get("drafted", -1)) < 0:
			open += 1
	var names := ", ".join(evs.slice(0, 2).map(func(e): return str(e["name"])))
	return {"open": open, "tone": "warn" if open > 0 else "tertiary", "icon": "events",
		"text": ("%d decision%s needed" % [open, "" if open == 1 else "s"]) if open > 0 else "Galactic events: " + names}

## Layer toggles with their current state: [{key, label, hint, icon, on}]. `state` is App.map_layers.
func layers(state: Dictionary) -> Array:
	var out := []
	for l in LAYERS:
		out.append({"key": l[0], "label": l[1], "hint": l[2], "icon": l[3], "on": bool(state.get(l[0], true))})
	return out

## Drafted orders: {count, items: [{id, summary}], lock}. `lock` is the reason orders cannot be cancelled now.
func orders() -> Dictionary:
	var pending: Array = view().get("pending", [])
	return {"count": pending.size(), "items": pending.map(func(p): return {"id": int(p["id"]), "summary": str(p["summary"])}), "lock": lock_reason()}

func cancel_order(id: int) -> void:
	cancel([id], "Order cancelled.")

## Map legend: [{label, color}] where color is a token the presentation maps: owner | alert | secondary | anomaly | dim.
func legend() -> Array:
	return [{"label": "Your ships", "color": "owner"}, {"label": "Contacts", "color": "alert"}, {"label": "Station", "color": "secondary"},
		{"label": "Anomaly", "color": "anomaly"}, {"label": "Unexplored", "color": "dim"}]

## The status strip: [{cap, icon, value, tone, tip}] over the chart (sectors charted, colonies, ships, contacts in range).
func status() -> Array:
	var v := view()
	var fog := str(v.get("fog", ""))
	var known := 0
	for i in fog.length():
		if fog[i] != "0":
			known += 1
	var total := maxi(1, fog.length())
	var contacts: Array = v.get("contacts", [])
	var hostile := contacts.filter(func(c): return c.get("atWar", false)).size()
	return [
		{"cap": "Charted", "icon": "galaxy", "value": "%d%%" % int(round(100.0 * known / total)), "tone": "",
			"tip": {"title": "Charted space", "value": "%d of %d sectors" % [known, total], "effect": "Scouts and sensors chart sectors. Unexplored sectors may hold systems and anomalies."}},
		{"cap": "Colonies", "icon": "colony", "value": str(v.get("colonies", []).size()), "tone": "",
			"tip": {"title": "Colonies", "value": str(v.get("colonies", []).size()), "effect": "Your inhabited systems. Open System (F2) to manage one."}},
		{"cap": "Ships", "icon": "ship", "value": str(v.get("fleets", []).size()), "tone": "",
			"tip": {"title": "Your ships", "value": str(v.get("fleets", []).size()), "effect": "Tab cycles through them on the chart."}},
		{"cap": "Contacts", "icon": "eye", "value": str(contacts.size()), "tone": "alert" if hostile > 0 else "",
			"tip": {"title": "Foreign contacts", "value": "%d in sensor range" % contacts.size(), "parts": [["At war with you", str(hostile), "alert" if hostile > 0 else ""]],
				"effect": "Contacts are only tracked inside your sensor range."}},
	]
