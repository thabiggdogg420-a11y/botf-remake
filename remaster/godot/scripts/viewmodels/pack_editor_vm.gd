class_name PackEditorVM
extends ViewModel
## Pack editor model (lane MOD-10, docs/design/MOD-TOOLS.md): a draft user pack, its overlay and its preset. The draft holds only
## what the author changed (patches, appended records, balance and era overrides, rule flags, lineup, options); `overlay_text()` and
## `preset_text()` write the two files; `validate()` hands them to the core's own validators (validatePack) and keeps the core's
## message for inline display; `save()` installs the pack in the core (installUserPack) and writes the files to the user data folder.
## Current values come from the core (packPart: the base pack with the overlays under this pack), so what the editor shows is
## what the game will run. It never writes a shipped pack: the core refuses a shipped id.

## Sections the editor edits. "fields" are the editable keys (t: int | dec | bool | str); `name` is the display field; `clone` allows
## adding a record by copying one. The core's typed parse still checks every value, so a bad number comes back as its error.
const SECTIONS := [
	{"id": "roster", "label": "Ship classes", "name": "className", "clone": true, "fields": [
		{"k": "className", "t": "str"}, {"k": "industryCost", "t": "int"}, {"k": "buildTurns", "t": "int"}, {"k": "maintenance", "t": "int"},
		{"k": "hull", "t": "int"}, {"k": "shields", "t": "int"}, {"k": "attack", "t": "int"}, {"k": "torpedoSharePercent", "t": "int"},
		{"k": "speed", "t": "int"}, {"k": "range", "t": "int"}, {"k": "sensors", "t": "int"}, {"k": "cloak", "t": "bool"},
		{"k": "buildable", "t": "bool"}, {"k": "description", "t": "str"}]},
	{"id": "ships", "label": "Legacy ships", "name": "name", "clone": true, "fields": [
		{"k": "name", "t": "str"}, {"k": "industryCost", "t": "int"}, {"k": "maintenance", "t": "int"}, {"k": "hull", "t": "int"},
		{"k": "shields", "t": "int"}, {"k": "attack", "t": "int"}, {"k": "range", "t": "dec"}, {"k": "speed", "t": "dec"},
		{"k": "troopCapacity", "t": "int"}, {"k": "description", "t": "str"}]},
	{"id": "buildings", "label": "Structures", "name": "name", "clone": false, "fields": [
		{"k": "name", "t": "str"}, {"k": "industryCost", "t": "int"}, {"k": "energyCost", "t": "int"}, {"k": "populationCost", "t": "int"},
		{"k": "moraleBonus", "t": "int"}, {"k": "technologyLevel", "t": "int"}, {"k": "description", "t": "str"}]},
	{"id": "technologies", "label": "Technologies", "name": "name", "clone": false, "fields": [
		{"k": "name", "t": "str"}, {"k": "cost", "t": "int"}, {"k": "description", "t": "str"}]},
	{"id": "races", "label": "Races", "name": "name", "clone": false, "fields": [
		{"k": "name", "t": "str"},
		{"k": "major.traitEffects.researchPct", "t": "int"}, {"k": "major.traitEffects.industryPct", "t": "int"}, {"k": "major.traitEffects.foodPct", "t": "int"},
		{"k": "major.traitEffects.creditsPct", "t": "int"}, {"k": "major.traitEffects.shipCostPct", "t": "int"}, {"k": "major.traitEffects.intelPct", "t": "int"},
		{"k": "major.traitEffects.securityPct", "t": "int"}, {"k": "major.traitEffects.groundDefensePct", "t": "int"}, {"k": "major.traitEffects.morale", "t": "int"}]},
	{"id": "minors", "label": "Minor races", "name": "name", "clone": true, "fields": [
		{"k": "name", "t": "str"}, {"k": "homeworld", "t": "str"}, {"k": "techLevel", "t": "int"}, {"k": "population", "t": "int"},
		{"k": "bonusValue", "t": "int"}, {"k": "description", "t": "str"}]},
]

## Balance values (the pack's `balance` block): key, label, identity value, what it scales.
const BALANCE := [
	{"k": "researchCostPercent", "label": "Research cost %", "def": 100}, {"k": "shipCostPercent", "label": "Ship cost %", "def": 100},
	{"k": "researchCurveLow", "label": "Research curve, level 2 %", "def": 100}, {"k": "researchCurveHigh", "label": "Research curve, level 10 %", "def": 100},
	{"k": "structureCostPercent", "label": "Structure cost %", "def": 100}, {"k": "batteryDefensePercent", "label": "Battery defence %", "def": 100},
	{"k": "shieldDefensePercent", "label": "Shield grid defence %", "def": 100}, {"k": "populationCapacityPercent", "label": "Population capacity %", "def": 100},
	{"k": "populationGrowthPercent", "label": "Population growth %", "def": 100}, {"k": "weaponOutputPercent", "label": "Weapon damage %", "def": 100},
	{"k": "intelOutputPercent", "label": "Espionage output %", "def": 100}, {"k": "aiIntelPercent", "label": "AI spy output %", "def": 100},
	{"k": "minorGiftCost", "label": "Minor gift cost (credits)", "def": 50}, {"k": "minorGiftAttitude", "label": "Minor gift attitude", "def": 20},
	{"k": "aiAlwaysStarbases", "label": "AI always builds starbases (0/1)", "def": 0}, {"k": "aiNewestShips", "label": "AI builds newest ships (0/1)", "def": 0},
	{"k": "aiStandardBuild", "label": "AI standard build order (0/1)", "def": 0}, {"k": "aiAggressiveExpansion", "label": "AI aggressive expansion (0/1)", "def": 0},
	{"k": "starTypesPerRace", "label": "Race star types (0/1)", "def": 0},
]

## Era timeline: the keys of the `eras` block the editor writes, and the block a new timeline starts from.
const ERA_ARRAYS := {"years": 5, "startTech": 5, "tagYears": 8}
const ERA_DEFAULT := {"turnsPerYear": 4, "years": [2240, 2280, 2320, 2350, 2380], "startTech": [1, 3, 5, 7, 10], "techCap": 10,
	"tagYears": [2150, 2245, 2270, 2350, 2370, 2379, 2380, 2400]}
const ERA_LABELS := ["Early", "Developing", "Mid", "Advanced", "Supreme"]

const FLAGS := ["stations", "galaxyFeatures", "curatedEvents", "alternativeVictories", "expandedRoster", "flightModel", "wegoCombat", "empireRules", "eras",
	"structureUpgrades", "autoUpgrades", "noUpgrades", "sharedMorale", "protectorateHq", "allDilithium", "interceptOrders", "raidOrders", "raidBonus",
	"crewExperience", "exploitFixes"]
const ECONOMY_ON := ["interceptOrders", "raidOrders", "crewExperience"]   ## Economy flags that are on in both rule bases.
const EMPIRES := ["Federation", "Klingon", "Romulan", "Cardassian", "Ferengi", "Dominion", "Borg", "Breen"]
const SIZES := ["small", "medium", "large", "huge"]
const SHAPES := ["legacy", "irregular", "spiral", "ring", "elliptical"]
const DENSITIES := ["legacy", "none", "few", "some", "many"]
const DIFFICULTIES := ["simple", "easy", "normal", "hard", "expert", "impossible"]
const TABS := [["identity", "Pack"], ["roster", "Ship classes"], ["ships", "Legacy ships"], ["buildings", "Structures"], ["technologies", "Technologies"],
	["races", "Races"], ["minors", "Minor races"], ["balance", "Balance"], ["eras", "Eras"], ["preset", "Rules and lineup"]]

# ---- the draft ----------------------------------------------------------------------------------------------------------
var pack_id := ""
var pack_name := ""
var blurb := ""
var revision := 0
var order := 900
var under: Array = []                      ## Shipped (or user) overlay ids beneath this pack, in stack order.
var patches := {}                          ## section -> {id: {field: value}}
var appends := {}                          ## section -> [whole records]
var balance := {}                          ## key -> value (overrides only)
var eras := {}                             ## the `eras` block overrides ({} = none)
var extra := {}                            ## overlay keys the editor does not model (e.g. events), kept so a save loses nothing
var rules_base := "remastered"
var flags := {}                            ## flag -> bool, only where it differs from the rule base's default
var lineup: Array = ["Federation", "Klingon", "Romulan", "Cardassian", "Ferengi"]
var options := {"empires": 5, "era": 1, "size": "medium", "shape": "spiral", "minorDensity": "some", "difficulty": "normal", "events": true, "aiAggression": true}
var threats: Variant = null                ## the preset's threat defaults, passed through unedited
var needs: Array = []
var dirty := false
var error := ""                            ## the core's refusal of the current draft, "" when it is valid
var error_tab := ""                        ## the tab the refusal points at
var status := {}                           ## {id, stackId, checksum} of the last valid draft
var _cache := {}                           ## section -> records under `under`

func supported() -> bool:
	return UserPacks.supported(session.core)

func new_pack(build_on: String = "") -> void:
	pack_id = ""; pack_name = ""; blurb = ""; revision = 0; order = 900
	under = []; patches = {}; appends = {}; balance = {}; eras = {}; extra = {}; flags = {}; threats = null; needs = []
	rules_base = "remastered"
	lineup = ["Federation", "Klingon", "Romulan", "Cardassian", "Ferengi"]
	options = {"empires": 5, "era": 1, "size": "medium", "shape": "spiral", "minorDensity": "some", "difficulty": "normal", "events": true, "aiAggression": true}
	_cache = {}
	if build_on != "":
		_adopt_preset(build_on)
	dirty = false
	validate()

## Starts from a preset: its overlays go under the pack and its rules, lineup and options become the draft's.
func _adopt_preset(preset_id: String) -> void:
	for p in session.meta.get("presets", []):
		if str(p["id"]) != preset_id:
			continue
		under = (p.get("overlays", []) as Array).duplicate()
		rules_base = str(p.get("rulesBase", "remastered"))
		flags = (p.get("flags", {}) as Dictionary).duplicate()
		lineup = (p.get("lineup", lineup) as Array).duplicate()
		options = (p.get("options", {}) as Dictionary).duplicate(true)
		threats = options.get("threats", null)
		options.erase("threats")
		needs = []
		return

## Loads a saved user pack back into the draft. False (with `error` set) when its files are missing or unreadable.
func load_pack(id: String) -> bool:
	var files := UserPacks.read(id)
	var ov: Variant = JSON.parse_string(str(files["overlay"]))
	var pr: Variant = JSON.parse_string(str(files["preset"]))
	if not (ov is Dictionary) or not (pr is Dictionary):
		error = "The files of pack \"%s\" could not be read." % id
		return false
	new_pack()
	pack_id = id
	pack_name = str(ov.get("name", id))
	revision = int(ov.get("revision", 0))
	blurb = str(pr.get("blurb", ""))
	order = int(pr.get("order", 900))
	under = (pr.get("overlays", []) as Array).filter(func(o): return str(o) != id)
	rules_base = str((pr.get("rules", {}) as Dictionary).get("base", "remastered"))
	flags = _norm((pr.get("rules", {}) as Dictionary).get("flags", {}))
	lineup = (pr.get("lineup", lineup) as Array).duplicate()
	options = _norm(pr.get("options", {}))
	threats = options.get("threats", null)
	options.erase("threats")
	needs = (pr.get("needs", []) as Array).duplicate()
	for key in ov:
		var v: Variant = _norm(ov[key])
		if key in ["format", "kind", "id", "name", "revision", "requires"]:
			continue
		elif key == "balance":
			balance = v
		elif key == "eras":
			eras = v
		elif SECTIONS.any(func(s): return s["id"] == key):
			var d: Dictionary = v
			var pm := {}
			for entry in d.get("patch", []):
				var e: Dictionary = (entry as Dictionary).duplicate()
				var rid := int(e["id"])
				e.erase("id")
				pm[rid] = _flatten(e)
			if not pm.is_empty():
				patches[key] = pm
			if not (d.get("append", []) as Array).is_empty():
				appends[key] = d["append"]
			for other in d:
				if other != "patch" and other != "append":
					extra[key] = v   # an unknown verb: kept as written, the core will say what it thinks of it
		else:
			extra[key] = v
	dirty = false
	validate()
	return true

# ---- numbers and records ---------------------------------------------------------------------------------------------------
## JSON numbers arrive as floats; whole ones go back as ints so the core's strict reader sees integers.
static func _norm(v: Variant) -> Variant:
	if v is float and is_finite(v) and floorf(v) == v and absf(v) < 9.0e15:
		return int(v)
	if v is Array:
		return (v as Array).map(func(x): return _norm(x))
	if v is Dictionary:
		var out := {}
		for k in v:
			out[k] = _norm(v[k])
		return out
	return v

## {"major": {"traitEffects": {"researchPct": 8}}} -> {"major.traitEffects.researchPct": 8}
static func _flatten(d: Dictionary, prefix: String = "") -> Dictionary:
	var out := {}
	for k in d:
		var key := prefix + str(k)
		if d[k] is Dictionary and (key == "major" or key == "major.traitEffects" or key == "flight"):
			out.merge(_flatten(d[k], key + "."))
		else:
			out[key] = d[k]
	return out

## The inverse of _flatten for writing a patch entry.
static func _nest(flat: Dictionary) -> Dictionary:
	var out := {}
	for k in flat:
		var parts: PackedStringArray = str(k).split(".")
		var at: Dictionary = out
		for i in parts.size() - 1:
			if not at.has(parts[i]):
				at[parts[i]] = {}
			at = at[parts[i]]
		at[parts[parts.size() - 1]] = flat[k]
	return out

static func section_def(id: String) -> Dictionary:
	for s in SECTIONS:
		if s["id"] == id:
			return s
	return {}

## Records of a section as the core merges them under this pack's `under` overlays (before this pack's own changes).
func base_records(section: String) -> Array:
	var r: Variant = _part(section)
	return r if r is Array else []

## The merged JSON of one pack key under `under` (cached): an Array for a record section, a Dictionary for balance or eras, else null.
func _part(key: String) -> Variant:
	if not _cache.has(key):
		_cache[key] = _norm(session.core.request("packPart", {"under": under, "key": key}))
	return _cache[key]

func _base_record(section: String, id: int) -> Dictionary:
	for rec in base_records(section):
		if int(rec["id"]) == id:
			return rec
	return {}

func _added(section: String, id: int) -> Dictionary:
	for rec in appends.get(section, []):
		if int(rec["id"]) == id:
			return rec
	return {}

## The list the editor shows: [{id, name, changed, added}] in id order.
func rows(section: String, filter: String = "") -> Array:
	var out := []
	var name_field := str(section_def(section).get("name", "name"))
	var all: Array = base_records(section).duplicate()
	all.append_array(appends.get(section, []))
	for rec in all:
		var rid := int(rec["id"])
		var nm := str(field_value(section, rid, name_field))
		if filter != "" and nm.to_lower().find(filter.to_lower()) < 0 and str(rid) != filter:
			continue
		out.append({"id": rid, "name": nm, "changed": patches.get(section, {}).has(rid), "added": not _added(section, rid).is_empty()})
	return out

## The value a field has under this draft: the patch, else the appended record, else the base record's.
func field_value(section: String, id: int, field: String) -> Variant:
	var p: Dictionary = patches.get(section, {}).get(id, {})
	if p.has(field):
		return p[field]
	var rec := _added(section, id)
	if rec.is_empty():
		rec = _base_record(section, id)
	return _dig(rec, field)

func base_value(section: String, id: int, field: String) -> Variant:
	var rec := _added(section, id)
	if rec.is_empty():
		rec = _base_record(section, id)
	return _dig(rec, field)

static func _dig(rec: Dictionary, field: String) -> Variant:
	var at: Variant = rec
	for part in field.split("."):
		if at is Dictionary and (at as Dictionary).has(part):
			at = at[part]
		else:
			return null
	return at

func is_changed(section: String, id: int, field: String) -> bool:
	return patches.get(section, {}).get(id, {}).has(field)

## Reads `text` as the field's type. Returns {"ok": bool, "value": Variant, "error": String}.
static func parse_field(type: String, text: Variant) -> Dictionary:
	var t := str(text).strip_edges()
	match type:
		"int":
			if t.is_valid_int():
				return {"ok": true, "value": int(t)}
			return {"ok": false, "error": "A whole number is expected here."}
		"dec":
			if t.is_valid_float():
				var f := float(t)
				return {"ok": true, "value": int(f) if floorf(f) == f else f}
			return {"ok": false, "error": "A number is expected here."}
		"bool":
			if text is bool:
				return {"ok": true, "value": text}
			if t == "true" or t == "false":
				return {"ok": true, "value": t == "true"}
			return {"ok": false, "error": "true or false is expected here."}
	return {"ok": true, "value": str(text)}

## Edits one field. A value equal to the underlying one removes the patch. Returns "" or why the text is not a value (the core's
## range and cross-field checks come back through validate()).
func set_field(section: String, id: int, field: String, text: Variant) -> String:
	var type := "str"
	for f in section_def(section).get("fields", []):
		if f["k"] == field:
			type = str(f["t"])
	var r := parse_field(type, text)
	if not r["ok"]:
		return str(r["error"])
	var value: Variant = r["value"]
	var rec := _added(section, id)
	if not rec.is_empty():
		_set_deep(rec, field, value)   # a record this pack adds is edited in place
	else:
		var sec: Dictionary = patches.get(section, {})
		var entry: Dictionary = sec.get(id, {})
		var base: Variant = base_value(section, id, field)
		if base == null and type == "int":
			base = 0   # an unset optional field (trait effects) reads as zero
		if _same(value, base):
			entry.erase(field)
		else:
			entry[field] = value
		if entry.is_empty():
			sec.erase(id)
		else:
			sec[id] = entry
		if sec.is_empty():
			patches.erase(section)
		else:
			patches[section] = sec
	dirty = true
	validate()
	return ""

static func _same(a: Variant, b: Variant) -> bool:
	if (a is int or a is float) and (b is int or b is float):
		return absf(float(a) - float(b)) < 1e-12
	return typeof(a) == typeof(b) and a == b

static func _set_deep(rec: Dictionary, field: String, value: Variant) -> void:
	var parts := field.split(".")
	var at := rec
	for i in parts.size() - 1:
		if not at.has(parts[i]):
			at[parts[i]] = {}
		at = at[parts[i]]
	at[parts[parts.size() - 1]] = value

## Drops every change to one record.
func revert(section: String, id: int) -> void:
	var sec: Dictionary = patches.get(section, {})
	sec.erase(id)
	if sec.is_empty():
		patches.erase(section)
	dirty = true
	validate()

## Adds a record by copying `source_id` (ship classes, legacy ships, minor races). Returns the new id, or -1.
func add_record(section: String, source_id: int, new_name: String) -> int:
	var def := section_def(section)
	if not bool(def.get("clone", false)):
		return -1
	var src := _added(section, source_id)
	if src.is_empty():
		src = _base_record(section, source_id)
	if src.is_empty():
		return -1
	var top := -1
	for rec in base_records(section):
		top = maxi(top, int(rec["id"]))
	for rec in appends.get(section, []):
		top = maxi(top, int(rec["id"]))
	var copy: Dictionary = src.duplicate(true)
	copy["id"] = top + 1
	copy[str(def["name"])] = new_name
	if not appends.has(section):
		appends[section] = []
	appends[section].append(copy)
	dirty = true
	validate()
	return top + 1

## Removes the most recently added record of a section (ids must stay continuous, so only the last can go).
func remove_last_added(section: String) -> bool:
	var a: Array = appends.get(section, [])
	if a.is_empty():
		return false
	a.pop_back()
	if a.is_empty():
		appends.erase(section)
	dirty = true
	validate()
	return true

# ---- balance, eras, rules, lineup ---------------------------------------------------------------------------------------------
func balance_default(key: String) -> int:
	var under_balance: Variant = _part("balance")
	if under_balance is Dictionary and (under_balance as Dictionary).has(key):
		return int(under_balance[key])
	for b in BALANCE:
		if b["k"] == key:
			return int(b["def"])
	return 0

func balance_value(key: String) -> int:
	return int(balance[key]) if balance.has(key) else balance_default(key)

func set_balance(key: String, text: Variant) -> String:
	var r := parse_field("int", text)
	if not r["ok"]:
		return str(r["error"])
	if int(r["value"]) == balance_default(key):
		balance.erase(key)
	else:
		balance[key] = int(r["value"])
	dirty = true
	validate()
	return ""

## True when the stack has an era timeline: this pack's block or an overlay under it.
func has_timeline() -> bool:
	return not eras.is_empty() or _part("eras") is Dictionary

## True when the timeline comes from an overlay under this pack (it can be edited here but not switched off).
func timeline_inherited() -> bool:
	return _part("eras") is Dictionary

## The timeline's value for a key (arrays by index), from this pack's block else the overlays under it.
func era_block() -> Dictionary:
	var under_eras: Variant = _part("eras")
	var out: Dictionary = (under_eras as Dictionary).duplicate(true) if under_eras is Dictionary else {}
	for k in eras:
		out[k] = eras[k]
	return out

func set_timeline(on: bool) -> void:
	if on and not has_timeline():
		eras = ERA_DEFAULT.duplicate(true)
		set_flag("expandedRoster", true)
		set_flag("eras", true)
	elif not on and not timeline_inherited():
		eras = {}
		set_flag("eras", false)
	dirty = true
	validate()

func set_era(key: String, index: int, text: Variant) -> String:
	var r := parse_field("int", text)
	if not r["ok"]:
		return str(r["error"])
	var block := era_block()
	if ERA_ARRAYS.has(key):
		var arr: Array = (block.get(key, ERA_DEFAULT[key]) as Array).duplicate()
		arr[index] = int(r["value"])
		eras[key] = arr
	else:
		eras[key] = int(r["value"])
	dirty = true
	validate()
	return ""

## Years in service for roster classes: [{id, from, until}].
func era_windows() -> Array:
	return (era_block().get("classes", []) as Array).duplicate(true)

func set_era_window(id: int, from_text: Variant, until_text: Variant) -> String:
	var f := parse_field("int", from_text)
	var u := parse_field("int", until_text)
	if not f["ok"] or not u["ok"]:
		return "Whole years are expected here."
	var list := era_windows()
	var done := false
	for w in list:
		if int(w["id"]) == id:
			w["from"] = int(f["value"]); w["until"] = int(u["value"]); done = true
	if not done:
		list.append({"id": id, "from": int(f["value"]), "until": int(u["value"])})
	eras["classes"] = list
	dirty = true
	validate()
	return ""

func remove_era_window(id: int) -> void:
	eras["classes"] = era_windows().filter(func(w): return int(w["id"]) != id)
	dirty = true
	validate()

## The flag's default under a rule base (mirrors Presets.h checkFlags).
static func flag_default(name_: String, base: String) -> bool:
	if name_ in ECONOMY_ON:
		return true
	if name_ in ["structureUpgrades", "autoUpgrades", "noUpgrades", "sharedMorale", "protectorateHq", "allDilithium", "raidBonus", "exploitFixes"]:
		return false
	return (base == "remastered" and name_ != "eras") or name_ == "stations" or name_ == "empireRules"

func flag_value(name_: String) -> bool:
	return bool(flags[name_]) if flags.has(name_) else flag_default(name_, rules_base)

func set_flag(name_: String, on: bool) -> void:
	if on == flag_default(name_, rules_base):
		flags.erase(name_)
	else:
		flags[name_] = on
	dirty = true

func set_rules_base(base: String) -> void:
	if base == rules_base:
		return
	var effective := {}
	for f in FLAGS:
		effective[f] = flag_value(f)
	rules_base = base
	flags = {}
	for f in FLAGS:
		if bool(effective[f]) != flag_default(f, base):
			flags[f] = effective[f]
	dirty = true
	validate()

func toggle_empire(key: String, on: bool) -> void:
	if on and not lineup.has(key):
		lineup.append(key)
	elif not on:
		lineup.erase(key)
	dirty = true
	validate()

func move_empire(key: String, delta: int) -> void:
	var i := lineup.find(key)
	var j := i + delta
	if i < 0 or j < 0 or j >= lineup.size():
		return
	lineup.remove_at(i)
	lineup.insert(j, key)
	dirty = true
	validate()

func set_option(key: String, value: Variant) -> void:
	options[key] = value
	dirty = true
	validate()

func set_identity(id: String, name_: String, blurb_: String) -> void:
	pack_id = id.strip_edges()
	pack_name = name_
	blurb = blurb_
	dirty = true
	validate()

# ---- the two files -----------------------------------------------------------------------------------------------------------
static func _j(v: Variant) -> String:
	return JSON.stringify(_norm(v), "", false)

func overlay_text() -> String:
	var req := ["botf-base"]
	req.append_array(under)
	var lines := ["{\"format\":1,\"kind\":\"overlay\",\"id\":%s,\"name\":%s,\"revision\":%d,\"requires\":%s" % [_j(pack_id), _j(pack_name), revision, _j(req)]]
	if not balance.is_empty():
		var b := {}
		for item in BALANCE:
			if balance.has(item["k"]):
				b[item["k"]] = balance[item["k"]]
		lines.append("\"balance\":" + _j(b))
	for s in SECTIONS:
		var sid := str(s["id"])
		var parts := []
		var pm: Dictionary = patches.get(sid, {})
		if not pm.is_empty():
			var ids := pm.keys()
			ids.sort()
			var recs := []
			for rid in ids:
				var flat: Dictionary = (pm[rid] as Dictionary).duplicate()
				if sid == "races":
					# The overlay replaces a record's traitEffects object whole, so write the effects already under this pack too.
					var te: Dictionary = {}
					var cur: Variant = _dig(_base_record("races", int(rid)), "major.traitEffects")
					if cur is Dictionary:
						te = (cur as Dictionary).duplicate()
					var any := false
					for k in flat.keys():
						if str(k).begins_with("major.traitEffects."):
							te[str(k).trim_prefix("major.traitEffects.")] = flat[k]
							flat.erase(k)
							any = true
					if any:
						flat["major.traitEffects"] = te
				var entry := {"id": int(rid)}
				entry.merge(_nest(flat))
				recs.append(_j(entry))
			parts.append("\"patch\":[\n" + ",\n".join(recs) + "]")
		var ap: Array = appends.get(sid, [])
		if not ap.is_empty():
			parts.append("\"append\":[\n" + ",\n".join(ap.map(func(r): return _j(r))) + "]")
		if not parts.is_empty():
			lines.append("\"%s\":{%s}" % [sid, ",".join(parts)])
	if not eras.is_empty():
		lines.append("\"eras\":" + _j(eras))
	for k in extra:
		lines.append("\"%s\":%s" % [k, _j(extra[k])])
	return ",\n".join(lines) + "}\n"

func preset_text() -> String:
	var ov := under.duplicate()
	ov.append(pack_id)
	var opts := options.duplicate(true)
	if threats != null:
		opts["threats"] = threats
	var d := {"format": 1, "id": pack_id, "name": pack_name, "blurb": blurb, "order": order, "overlays": ov,
		"rules": {"base": rules_base, "flags": flags}, "lineup": lineup, "options": opts, "needs": needs}
	return _j(d) + "\n"

# ---- validation and saving ------------------------------------------------------------------------------------------------------
## Hands the draft to the core's validators. Sets `error` (the core's own message) and `error_tab`; true when valid.
func validate() -> bool:
	error = ""; error_tab = ""; status = {}
	if pack_id == "":
		error = "Give the pack an id (letters, digits and dashes) on the Pack tab."
		error_tab = "identity"
		return false
	if not _valid_id(pack_id):
		error = "The pack id may use only lowercase letters, digits and dashes (up to 40)."
		error_tab = "identity"
		return false
	if pack_name.strip_edges() == "":
		error = "Give the pack a name on the Pack tab."
		error_tab = "identity"
		return false
	var r := session.core.invoke("validatePack", {"overlay": overlay_text(), "preset": preset_text()})
	if r.get("ok", false):
		status = r["result"]
		return true
	error = str(r.get("error", "The core refused the pack."))
	error_tab = _tab_of(error)
	return false

static func _valid_id(id: String) -> bool:
	if id.length() < 1 or id.length() > 40:
		return false
	for c in id:
		if not (c >= "a" and c <= "z" or c >= "0" and c <= "9" or c == "-"):
			return false
	return true

## Which tab a core message is about, from the section it names.
static func _tab_of(msg: String) -> String:
	var m := msg.to_lower()
	for s in SECTIONS:
		if m.contains("." + str(s["id"])) or m.contains(str(s["id"]) + "[") or m.contains("overlay.") and m.contains(str(s["id"])):
			return str(s["id"])
	if m.contains("balance"):
		return "balance"
	if m.contains("eras") or m.contains("start years") or m.contains("start tech") or m.contains("techcap") or m.contains("retirement year"):
		return "eras"
	if m.contains("shipped") or m.contains("overlay id") or m.contains("overlay.id") or m.contains("overlay.name"):
		return "identity"
	if m.contains("preset") or m.contains("lineup") or m.contains("flag"):
		return "preset"
	return "identity"

## Installs the pack in the core and writes its two files. Returns "" or the error to show. Never touches a shipped pack
## (the core refuses its id).
func save() -> String:
	if not validate():
		return error
	revision += 1
	var ov := overlay_text()
	var pr := preset_text()
	var r := session.core.invoke("installUserPack", {"overlay": ov, "preset": pr})
	if not r.get("ok", false):
		revision -= 1
		error = str(r.get("error", "The core refused the pack."))
		return error
	var err := UserPacks.write(pack_id, ov, pr)
	if err != "":
		return err
	dirty = false
	session.refresh_meta()
	return ""

func delete_pack() -> void:
	if pack_id == "":
		return
	session.core.invoke("removeUserPack", {"id": pack_id})
	UserPacks.remove(pack_id)
	session.refresh_meta()
	new_pack()

## Saved packs for the picker: [{id, name}].
func saved_packs() -> Array:
	var out := []
	for id in UserPacks.ids():
		var ov: Variant = JSON.parse_string(UserPacks.read(id)["overlay"])
		out.append({"id": id, "name": str((ov as Dictionary).get("name", id)) if ov is Dictionary else id})
	return out

## Presets a new pack can build on: every registered preset (each user pack included).
func build_on_choices() -> Array:
	return session.meta.get("presets", []).map(func(p): return [str(p["id"]), str(p["name"])])

## What the draft changes, in a line per section (the summary under the tabs).
func summary() -> Array:
	var out := []
	for s in SECTIONS:
		var sid := str(s["id"])
		var n: int = (patches.get(sid, {}) as Dictionary).size()
		var a: int = (appends.get(sid, []) as Array).size()
		if n > 0 or a > 0:
			out.append("%s: %d changed, %d added" % [s["label"], n, a])
	if not balance.is_empty():
		out.append("Balance: %d value%s" % [balance.size(), "" if balance.size() == 1 else "s"])
	if not eras.is_empty():
		out.append("Eras: timeline set")
	return out
