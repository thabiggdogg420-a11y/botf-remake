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
## The preset id (docs/design/PACKS-AND-PRESETS.md): "classic", "remastered", or a mod preset such as "dominion-war". The core
## resolves it to overlay packs, rule flags and a lineup; the picker fills the other options from its defaults (pick_preset).
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
## What a client with no core's preset list offers: the two built-ins. Every field the core's `meta.presets` carries is here, so the
## picker needs no second code path.
const PRESET_FALLBACK := [
	{"id": "classic", "name": "Classic", "rulesBase": "classic", "overlays": [], "lineup": ["Federation", "Klingon", "Romulan", "Cardassian", "Ferengi"],
		"lineupPlayable": true, "lineupNote": "", "needs": [], "flags": {},
		"blurb": "The original BOTF rules: outposts and starbases, the original random events, conquest and domination only. Even, uniform galaxy.",
		"options": {"empires": 5, "era": 1, "size": "medium", "shape": "elliptical", "minorDensity": "some", "difficulty": "normal", "events": true, "aiAggression": true}},
	{"id": "remastered", "name": "Remastered", "rulesBase": "remastered", "overlays": [], "lineup": ["Federation", "Klingon", "Romulan", "Cardassian", "Ferengi"],
		"lineupPlayable": true, "lineupNote": "", "needs": [], "flags": {},
		"blurb": "The default: every BOTF feature plus our layers. Galaxy features, curated events, WEGO combat, the expanded ship roster and the extra victories.",
		"options": {"empires": 5, "era": 1, "size": "medium", "shape": "spiral", "minorDensity": "some", "difficulty": "normal", "events": true, "aiAggression": true}},
]

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
const CORE_READS := ["faction", "empires", "lineup", "seed", "era", "ruleset", "width", "height", "systems", "events", "aiAggression", "preset",
	"shape", "minorDensity", "difficulty"]

static func core_reads(key: String) -> bool:
	return key in CORE_READS

## Presets in screen order: the core's registry (meta.presets), else the two built-ins.
func presets() -> Array:
	var m: Dictionary = session.meta if session != null else {}
	var p: Variant = m.get("presets", null)
	return p if p is Array and not (p as Array).is_empty() else PRESET_FALLBACK

## The chosen preset's record, or {} when the id is not offered.
func preset_def(id: String = "") -> Dictionary:
	var want := preset if id == "" else id
	for p in presets():
		if str(p["id"]) == want:
			return p
	return {}

func preset_name() -> String:
	var d := preset_def()
	return str(d["name"]) if not d.is_empty() else preset.capitalize()

## "classic" or "remastered": the rule set under the preset (the event checkbox label follows it).
func rules_base() -> String:
	var d := preset_def()
	return str(d["rulesBase"]) if not d.is_empty() else preset

## Chooses a preset and fills the options it sets (era, size, shape, minor races, difficulty, events, rivals, powers in play).
## Options it does not set keep the player's choice. The faction stays valid: powers in play never drop below its place.
func pick_preset(id: String) -> bool:
	var d := preset_def(id)
	if d.is_empty():
		return false
	preset = id
	var o: Dictionary = d.get("options", {})
	if o.has("era"):
		era = int(o["era"])
	if o.has("size") and SIZES.any(func(s): return s["id"] == o["size"]):
		size_id = str(o["size"])
	if o.has("shape") and SHAPES.any(func(s): return s["id"] == o["shape"]):
		shape = str(o["shape"])
	if o.has("minorDensity") and DENSITIES.any(func(s): return s["id"] == o["minorDensity"]):
		minor_density = str(o["minorDensity"])
	if o.has("difficulty"):
		difficulty = str(o["difficulty"])
	if o.has("events"):
		events = bool(o["events"])
	if o.has("aiAggression"):
		aggression = bool(o["aiAggression"])
	if o.has("empires"):
		empires = maxi(int(o["empires"]), faction)
	# A preset that names powers beyond the first five in canonical order (Dominion War, Galaxies) picks that lineup.
	lineup = []
	if bool(d.get("lineupPlayable", true)):
		var ids := []
		for n in d.get("lineup", []):
			var at := POWER_KEYS.find(str(n).to_lower())
			if at >= 0 and not ids.has(at + 1):
				ids.append(at + 1)
		ids.sort()
		if ids.size() >= 2 and ids != range(1, ids.size() + 1):
			lineup = ids
			if not ids.has(faction):
				faction = int(ids[0])
	return true

## The picker's text for a preset: what it is, who plays, and what it is still waiting for.
func preset_text(id: String) -> String:
	var d := preset_def(id)
	if d.is_empty():
		return ""
	var lines := [str(d["blurb"]), "Powers: " + " · ".join(d.get("lineup", []))]
	if not bool(d.get("lineupPlayable", true)):
		lines.append("Not seated yet: " + str(d.get("lineupNote", "")) + " This game plays the stock powers.")
	var needs: Array = d.get("needs", [])
	if not needs.is_empty():
		lines.append("Content still to come: " + ", ".join(needs) + ".")
	return "\n".join(lines)

## Minor races the density gives on this size (capped at 29, the original's limit). With a core, its own count for this
## exact galaxy; else the mirrored table.
func minors_count() -> int:
	var pv := preview_galaxy()
	if not pv.is_empty():
		return pv["stars"].filter(func(s): return int(s[2]) == 9).size()
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

## Race keys of the eight playable powers, in empire-id order (schema 17, docs/design/EMPIRE-SLOTS.md). The core sends the same
## keys in meta.empires; this list is the fallback for a backend that does not.
const POWER_KEYS := ["federation", "klingon", "romulan", "cardassian", "ferengi", "dominion", "borg", "breen"]
## The powers in play as empire ids, ascending. Empty means the canonical prefix 1..powers() (the pre-17 behaviour: two to
## five powers in Federation, Klingon, Romulan, Cardassian, Ferengi order). The power picker fills it.
var lineup: Array = []

## Powers in play: the lineup, else at least as many as the chosen power's place in canonical order.
func powers() -> int:
	return lineup.size() if not lineup.is_empty() else max(empires, faction)

## The empire ids in play, ascending.
func lineup_ids() -> Array:
	return lineup.duplicate() if not lineup.is_empty() else range(1, powers() + 1)

func pick_faction(id: int) -> void:
	faction = id
	if lineup.is_empty():
		empires = max(empires, id)
	elif not lineup.has(id):
		lineup.append(id)
		lineup.sort()

## Why a power cannot be switched off (the player's own power, or the last two), "" when it can.
func toggle_block(id: int) -> String:
	if id == faction:
		return "This is your power; pick another card to change it."
	if lineup_ids().has(id) and powers() <= 2:
		return "A campaign needs at least two powers."
	return ""

func toggle_power(id: int) -> void:
	if toggle_block(id) != "":
		return
	var ids := lineup_ids()
	if ids.has(id):
		ids.erase(id)
	else:
		ids.append(id)
	ids.sort()
	lineup = ids

## The lineup as race keys, in empire-id order (what the core's newCampaign "lineup" takes).
func lineup_keys() -> Array:
	var out := []
	for id in lineup_ids():
		out.append(power_key(int(id)))
	return out

func power_key(id: int) -> String:
	for e in meta().get("empires", []):
		if int(e.get("id", 0)) == id and e.has("key"):
			return str(e["key"])
	return POWER_KEYS[id - 1] if id >= 1 and id <= POWER_KEYS.size() else ""

func options() -> Dictionary:
	var sz := size_def()
	var o := {"faction": faction, "empires": powers(), "seed": seed_value, "era": era, "ruleset": ruleset,
		"width": sz["width"], "height": sz["height"], "systems": sz["systems"], "events": events, "aiAggression": aggression, "preset": preset,
		"shape": shape, "minorDensity": minor_density, "difficulty": difficulty}
	if not lineup.is_empty():
		o["lineup"] = lineup_keys()
	return o

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
		"%s era · %s rules · %s preset" % [era_name(), ruleset, preset_name()],
		"%s galaxy · %s minor races (%d) · %s difficulty" % [shape.capitalize(), minor_density, minors_count(), difficulty.capitalize()],
		"Seed %d%s%s" % [seed_value, " · events" if events else "", " · aggressive AI" if aggression else ""]]
