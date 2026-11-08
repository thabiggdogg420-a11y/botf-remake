class_name AppRoutes
extends RefCounted
## The screen registry: every screen id, its rail label, F-key, hint and presentation script. The shell (App) builds
## the rail from GAME_SCREENS and loads screens through script_for(). See docs/CLIENT-ARCHITECTURE.md.

const GAME_SCREENS := [
	{"id": "galaxy", "label": "Galaxy", "key": "F1", "hint": "Star chart, fleets and orders", "script": "res://scenes/galaxy/galaxy_screen.gd"},
	{"id": "system", "label": "System", "key": "F2", "hint": "Colony management", "script": "res://scenes/ui/screens/system_screen.gd"},
	{"id": "empire", "label": "Empire", "key": "F3", "hint": "All colonies and fleets", "script": "res://scenes/ui/screens/empire_screen.gd"},
	{"id": "research", "label": "Research", "key": "F4", "hint": "Technology and allocation", "script": "res://scenes/ui/screens/research_screen.gd"},
	{"id": "fleets", "label": "Fleets", "key": "F5", "hint": "Ships and task forces", "script": "res://scenes/ui/screens/fleets_screen.gd"},
	{"id": "diplomacy", "label": "Diplomacy", "key": "F6", "hint": "Empires, minors and treaties", "script": "res://scenes/ui/screens/diplomacy_screen.gd"},
	{"id": "intel", "label": "Intel", "key": "F7", "hint": "Espionage and security", "script": "res://scenes/ui/screens/intel_screen.gd"},
	{"id": "database", "label": "Database", "key": "F8", "hint": "Ships, technologies, structures", "script": "res://scenes/ui/screens/database_screen.gd"},
]
const OUTSIDE_SCREENS := {
	"title": "res://scenes/ui/screens/title_screen.gd",
	"new": "res://scenes/ui/screens/new_campaign_screen.gd",
	"saves": "res://scenes/ui/screens/saves_screen.gd",
	"combat": "res://scenes/ui/screens/combat_screen.gd",
}
## Screens shown without the in-game chrome (top bar and rail).
const BARE := ["title", "new", "saves"]

static func script_for(id: String) -> String:
	for s in GAME_SCREENS:
		if s["id"] == id:
			return s["script"]
	return OUTSIDE_SCREENS.get(id, "")

static func is_bare(id: String) -> bool:
	return id in BARE
