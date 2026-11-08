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
		"width": sz["width"], "height": sz["height"], "systems": sz["systems"], "events": events, "aiAggression": aggression, "preset": preset}

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
		"Seed %d%s%s" % [seed_value, " · events" if events else "", " · aggressive AI" if aggression else ""]]
