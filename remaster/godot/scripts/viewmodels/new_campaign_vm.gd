class_name NewCampaignVM
extends ViewModel
## New-campaign model: the chosen power, galaxy size, powers in play, seed, era, ruleset, preset and switches,
## and the options dictionary handed to App.start_campaign.

## The one place the galaxy sizes live on the Godot side (the web client's twin is web/src/screens/NewCampaign.tsx SIZES).
## Simon, 10 Nov 2026: Medium is the default and holds about 50 systems (the original's limits were its engine's, not a design
## choice). Each grid keeps star density near 0.145 systems per sector. Only Medium = ~50 is his ruling; the rest is scaling
## and a one-line change each.
const SIZES := [
	{"id": "small", "label": "Small", "width": 16, "height": 13, "systems": 30, "blurb": "Quick, crowded: contact comes early."},
	{"id": "medium", "label": "Medium", "width": 22, "height": 16, "systems": 50, "blurb": "The default: room to grow, rivals within reach."},
	{"id": "large", "label": "Large", "width": 28, "height": 20, "systems": 80, "blurb": "Room to expand before borders meet."},
	{"id": "huge", "label": "Huge", "width": 34, "height": 24, "systems": 120, "blurb": "A long campaign across a wide quadrant."},
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
	{"id": "none", "label": "None"}, {"id": "few", "label": "Few"}, {"id": "some", "label": "Some"}, {"id": "many", "label": "Many"},
]
## Minor races per density for Small / Medium / Large / Huge: a mirror of the core's `minorRaceTarget` (Rules.cpp), used when
## no core can be asked (the fixture backend). The vanilla counts are Small 0/4/4/6, Medium 0/7/12/17, Large 0/10/16/24, maximum 29;
## Small "some" is 5 so the steps stay distinct, and Huge is ours.
const MINOR_TABLE := {"none": [0, 0, 0, 0], "few": [4, 7, 10, 14], "some": [5, 12, 16, 22], "many": [6, 17, 24, 29]}
## What each level changes (the core's `difficultyProfile`, Rules.cpp): rival income, industry and research; rival starting
## credits; how strong rivals judge themselves when they weigh a war; how ready their spies are. The player's economy never moves.
const DIFFICULTIES := [
	{"id": "simple", "label": "Simple", "blurb": "Rivals produce 75% of normal, start poorer, hesitate to go to war and spy slowly."},
	{"id": "easy", "label": "Easy", "blurb": "Rivals produce 90% of normal and are cautious."},
	{"id": "normal", "label": "Normal", "blurb": "The reference balance."},
	{"id": "hard", "label": "Hard", "blurb": "Rivals produce 115% of normal, judge themselves stronger and spy harder."},
	{"id": "impossible", "label": "Impossible", "blurb": "Rivals produce 135% of normal, start 40% richer and never let up. They cheat; you do not."},
]
## Option keys the core reads. Shape, minor-race density and difficulty joined them with the schema-15 generator.
const CORE_READS := ["faction", "empires", "seed", "era", "ruleset", "width", "height", "systems", "events", "aiAggression", "preset",
	"shape", "minorDensity", "difficulty"]

static func core_reads(key: String) -> bool:
	return key in CORE_READS

## Minor races the density gives on this size (capped at 29, the original's limit). With a core, its own count for this
## exact galaxy; else the mirrored table.
func minors_count() -> int:
	var pv := preview_galaxy()
	if not pv.is_empty():
		return pv["stars"].filter(func(s): return int(s[2]) == 6).size()
	var n := int(size_def()["systems"])
	var row := 0 if n <= 36 else (1 if n <= 64 else (2 if n <= 100 else 3))
	return mini(mini(29, maxi(0, n / 2 - 3)), int(MINOR_TABLE[minor_density][row]))

## The generated galaxy for the current options, from the core ({width, height, shape, stars [[x, y, kind]]}), or {} when
## the backend is not the core. Cached on the options, so redrawing costs nothing.
var _pv_key := ""
var _pv: Dictionary = {}
func preview_galaxy() -> Dictionary:
	if session == null or session.core == null or not session.core.is_native():
		return {}
	var o := AppSession.campaign_options(options())
	var key := JSON.stringify(o)
	if key != _pv_key:
		_pv_key = key
		var r := session.core.invoke("galaxyPreview", o)
		_pv = r["result"] if r.get("ok", false) and r["result"] is Dictionary else {}
	return _pv

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
