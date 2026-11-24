class_name TabReveal
extends RefCounted
## Gradual reveal (ONBOARDING-AND-MOBILE-PLAN.md 1.3). In the tutorial game (a Quick Start) the side tabs with nothing to show
## yet are dimmed: Council, Diplomats, Fleet Intel and Records (the empire, diplomacy, intel and database screens). They still
## open when pressed. Each one gets a small "new" mark when it first has something to show, until it is first opened.
## Custom games, loaded games and drills show every tab as they always did. State lives on the App (tutorial_game, tab_lit,
## tab_seen); nothing is saved.

const TABS := ["empire", "diplomacy", "intel", "database"]
## The turn Records lights up on when nobody has been met by then (the tutorial's three turns are over).
const RECORDS_TURN := 6

## Whether the reveal applies to the running game.
static func active(app: App) -> bool:
	return app.tutorial_game and not app.tutorial_held and not app.view.is_empty()

## A major power has been met, or a minor race.
static func contact_made(view: Dictionary) -> bool:
	for e in view.get("empires", []):
		if e.get("contact", false) and not e.get("eliminated", false):
			return true
	for s in view.get("systems", []):
		var m: Variant = s.get("minor", null)
		if m is Dictionary and int(s.get("owner", 0)) == 0 and not (m as Dictionary).get("unknown", false) and AppSession.is_charted(s):
			return true
	return false

## How many powers and minor races have been met (what the beat and the Contacts tile both count).
static func met_count(view: Dictionary) -> int:
	var n := 0
	for e in view.get("empires", []):
		if e.get("contact", false) and not e.get("eliminated", false):
			n += 1
	for s in view.get("systems", []):
		var m: Variant = s.get("minor", null)
		if m is Dictionary and int(s.get("owner", 0)) == 0 and not (m as Dictionary).get("unknown", false) and AppSession.is_charted(s):
			n += 1
	return n

## Whether the tab has something to show now (the test behind lighting it).
static func has_content(app: App, id: String) -> bool:
	var v := app.view
	if contact_made(v):
		return true
	match id:
		"empire":
			return v.get("colonies", []).size() >= 2   # a second colony is something to govern
		"database":
			return int(v.get("meta", {}).get("turn", 0)) >= RECORDS_TURN
	return false

## Lights every tab that now has something to show. A lit tab stays lit.
static func refresh(app: App) -> void:
	if not active(app):
		return
	for id in TABS:
		if not app.tab_lit.get(id, false) and has_content(app, id):
			app.tab_lit[id] = true

static func dimmed(app: App, id: String) -> bool:
	return id in TABS and active(app) and not app.tab_lit.get(id, false)

## Lit but not yet opened: the tab carries the "new" mark.
static func is_new(app: App, id: String) -> bool:
	return id in TABS and active(app) and app.tab_lit.get(id, false) and not app.tab_seen.get(id, false)

static func mark_seen(app: App, id: String) -> void:
	if id in TABS and app.tab_lit.get(id, false):
		app.tab_seen[id] = true

## Dims or marks a rail button for its tab.
static func style(app: App, b: Button, id: String) -> void:
	if dimmed(app, id):
		b.modulate = Color(1, 1, 1, 0.42)
		b.tooltip_text += "  (nothing here yet)"
	elif is_new(app, id):
		var mark := Lcars.badge("new", "good")
		mark.name = "NewMark"
		mark.mouse_filter = Control.MOUSE_FILTER_IGNORE
		mark.position = Vector2(app.RAIL_W - 50, -13)   # on the tab's top edge, above the label (which sits mid-height)
		b.add_child(mark)
