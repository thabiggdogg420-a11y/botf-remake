class_name NewCampaignVM
extends ViewModel
## New-campaign model: the chosen power, galaxy size, powers in play, seed, era, ruleset, preset and switches,
## and the options dictionary handed to App.start_campaign.

const SIZES := [
	{"id": "small", "label": "Small", "width": 12, "height": 9, "systems": 16, "blurb": "Quick, crowded: contact comes early."},
	{"id": "medium", "label": "Medium", "width": 16, "height": 12, "systems": 28, "blurb": "The classic reference galaxy."},
	{"id": "large", "label": "Large", "width": 22, "height": 16, "systems": 48, "blurb": "Room to expand before borders meet."},
	{"id": "huge", "label": "Huge", "width": 30, "height": 20, "systems": 80, "blurb": "A long campaign across a wide quadrant."},
]
const ERA_BLURBS := {
	1: "Warp-capable newcomers. Build everything from the ground up.",
	2: "Established powers with developed technology.",
	3: "Expanded empires, advanced shipyards.",
	4: "Advanced civilisations on the brink of dominance.",
	5: "Supreme technology from turn one.",
}

var faction := 1
var empires := 5
var size_id := "medium"
var era := 1
var ruleset := "reference"
var preset := "remastered"
var seed_value := 0
var events := true
var aggression := true
## WS02 options (docs/design/WS02-GALAXY-AND-STRATEGIC-MAP.md §1-2). The core's newCampaign does not read these yet
## (web/bridge/WebBridge.cpp newCampaign reads faction, empires, seed, era, ruleset, size, preset, events,
## aiAggression); they are passed through so the generator lane can pick them up, and the screen marks them.
var shape := "spiral"
var minor_density := "some"
var difficulty := "normal"
## The UI era look (docs/UI-DESIGN-SYSTEM.md §2.3), stored in settings.uiEra at launch.
var ui_era := "tng"

const SHAPES := [
	{"id": "spiral", "label": "Spiral", "blurb": "Three arms around a bright core; borders form along the arms."},
	{"id": "ring", "label": "Ring", "blurb": "A band of stars around an empty centre; every empire has two neighbours."},
	{"id": "irregular", "label": "Irregular", "blurb": "Clumps and voids; chokepoints decide wars."},
	{"id": "elliptical", "label": "Elliptical", "blurb": "An even oval; open borders, early contact."},
]
const DENSITIES := [
	{"id": "none", "label": "None", "share": 0.0}, {"id": "few", "label": "Few", "share": 0.12},
	{"id": "some", "label": "Some", "share": 0.2}, {"id": "many", "label": "Many", "share": 0.3},
]
const DIFFICULTIES := [
	{"id": "simple", "label": "Simple", "blurb": "Rivals expand slowly and forgive."},
	{"id": "easy", "label": "Easy", "blurb": "Rivals are cautious."},
	{"id": "normal", "label": "Normal", "blurb": "The reference balance."},
	{"id": "hard", "label": "Hard", "blurb": "Rivals build their best ships and press their advantage."},
	{"id": "impossible", "label": "Impossible", "blurb": "Rivals start ahead and never let up."},
]
## Option keys the core reads today; the others show a "generator pending" mark.
const CORE_READS := ["faction", "empires", "seed", "era", "ruleset", "width", "height", "systems", "events", "aiAggression", "preset"]

static func core_reads(key: String) -> bool:
	return key in CORE_READS

## Minor races the density gives on this size (capped at 29, the original's limit; WS02 §1).
func minors_count() -> int:
	var share := 0.2
	for d in DENSITIES:
		if d["id"] == minor_density:
			share = float(d["share"])
	return mini(29, int(round(int(size_def()["systems"]) * share)))

func roll_seed() -> void:
	seed_value = randi_range(100000, 999999)

func size_def() -> Dictionary:
	return SIZES.filter(func(s): return s["id"] == size_id)[0]

## Powers in play: at least as many as the chosen power's place in canonical order.
func powers() -> int:
	return max(empires, faction)

func pick_faction(id: int) -> void:
	faction = id
	empires = max(empires, id)

func options() -> Dictionary:
	var sz := size_def()
	return {"faction": faction, "empires": powers(), "seed": seed_value, "era": era, "ruleset": ruleset,
		"width": sz["width"], "height": sz["height"], "systems": sz["systems"], "events": events, "aiAggression": aggression, "preset": preset,
		"shape": shape, "minorDensity": minor_density, "difficulty": difficulty}

## Era buttons: [{id, name, level, blurb}].
func eras() -> Array:
	var out := []
	for e in meta().get("eras", []):
		var lvl := int(e["techLevel"] if ruleset == "reference" else e["prototypeTechLevel"])
		out.append({"id": int(e["id"]), "name": str(e["name"]), "level": lvl, "blurb": ERA_BLURBS.get(int(e["id"]), "")})
	return out

func era_name() -> String:
	for e in meta().get("eras", []):
		if int(e["id"]) == era:
			return str(e["name"])
	return ""

func faction_name(id: int, fallback: String) -> String:
	return session.empire_name(id).to_upper() if not meta().is_empty() else fallback

## The three summary lines under the launch button.
func summary() -> Array:
	var o := options()
	return ["%d major powers · %d systems · %dx%d sectors" % [o["empires"], o["systems"], o["width"], o["height"]],
		"%s era · %s rules · %s gameplay" % [era_name(), ruleset, preset.capitalize()],
		"%s galaxy · %s minor races (%d) · %s difficulty" % [shape.capitalize(), minor_density, minors_count(), difficulty.capitalize()],
		"Seed %d%s%s" % [seed_value, " · events" if events else "", " · aggressive AI" if aggression else ""]]
