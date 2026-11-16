extends SceneTree
## Cue trace: every game event that should make a sound is followed from the event to a real file in the built audio
## pack (no generated tone, no beep stand-in). Covers the cue registry for each faction, the turn-briefing events,
## every battle weapon family x faction, the battle effect cues (shield, hull, destroyed, cloak, retreat) and the order
## and bark voices, in every era. Red alert must be a recorded klaxon, not a beep.
##   godot --headless --audio-driver Dummy --path remaster/godot -s res://scripts/audio/tests/test_cue_trace.gd
## Needs the pack (remaster/tools/pack_audio_web.py, gitignored). Without it the mapping checks run and the file
## checks are reported as SKIP.

var failures := 0
var checks := 0
var lib: BotfAudioLibrary
var have_pack := false
var misses: PackedStringArray = []


func check(cond: bool, what: String) -> void:
	checks += 1
	if cond:
		print("  ok  ", what)
	else:
		failures += 1
		printerr("  FAIL ", what)


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	print("Cue trace")
	lib = BotfAudioLibrary.new()
	have_pack = lib.loaded_packs.size() > 0
	print("  pack: %s" % (", ".join(lib.loaded_packs) if have_pack else "none (file checks skipped)"))
	_registry_cues()
	_briefing_events()
	_battle_weapons()
	_battle_effects()
	_battle_voice()
	_red_alert()
	_no_disk_reads()
	if not misses.is_empty():
		print("  untraced: ", misses.size())
	lib.finish_tasks()
	print("%d checks, %d failures" % [checks, failures])
	quit(1 if failures > 0 else 0)


## First key of `keys` (era twins first) that resolves to a loadable stream. {} when none.
func trace(keys: PackedStringArray, era: String) -> Dictionary:
	var ks := BotfAudioCues.with_era(keys, era) if lib.has_era(era) else keys
	var r := lib.resolve_files(ks)
	if r.is_empty():
		return {}
	var s := lib.stream_of(r)
	if s == null or s.get_length() <= 0.05:
		return {}
	r["length"] = s.get_length()
	return r


# ------------------------------------------------------------------------------------------------------------
func _registry_cues() -> void:
	var facs := ["fed", "kli", "rom", "car", "fer"]
	for cue_name in BotfAudioCues.REGISTRY:
		var def: Dictionary = BotfAudioCues.REGISTRY[cue_name]
		check(def.has("bus") and def.has("priority"), "cue %s has a bus and a priority" % cue_name)
		check(not def.get("sfx", []).is_empty() or def.has("voice") or def.get("beep", false),
			"cue %s has some sound to make" % cue_name)
		if not have_pack:
			continue
		for fac in facs:
			var other: String = facs[(facs.find(fac) + 1) % facs.size()]
			var c := BotfAudioCues.cue_for(cue_name, fac, other, "")
			var heard := false
			for era in BotfAudioCues.ERAS:
				var via_voice: bool = not c.voice.is_empty() and not trace(c.voice, era).is_empty()
				var via_sfx: bool = not c.sfx.is_empty() and not trace(c.sfx, era).is_empty()
				var via_beep: bool = c.beep and not trace(PackedStringArray(["sfx.ui.beep"]), era).is_empty()
				if via_voice or via_sfx or via_beep:
					heard = true
				else:
					heard = false
					break
			if not heard:
				misses.append("%s/%s" % [cue_name, fac])
			check(heard, "cue %s (%s) reaches a recorded file in every era" % [cue_name, fac])


# ------------------------------------------------------------------------------------------------------------
func _briefing_events() -> void:
	var empires := [{"id": 1, "name": "Federation"}, {"id": 2, "name": "Klingon"}, {"id": 3, "name": "Romulan"}]
	var events := [
		["research", "Warp 3 researched", "research_complete"],
		["construction", "Shipyard built", "construction_complete"],
		["colonization", "Colony founded at Vega. Production begins next turn.", "colony_founded"],
		["stations", "Outpost built", "outpost_built"],
		["events", "Plasma storm", "galactic_event"],
		["diplomacy", "Klingon declare war", "war_declared"],
		["diplomacy", "Peace with Klingon", "peace_made"],
		["diplomacy", "Treaty with Romulan", "treaty_signed"],
		["diplomacy", "First contact with Romulan", "first_contact"],
		["diplomacy", "Klingon send a proposal", "diplomatic_proposal"],
		["combat", "Sol is under attack", "system_under_attack"],
	]
	for ev in events:
		var sev := "alert" if ev[0] == "combat" else "info"
		var out := BotfAudioCues.turn_cues({"outcome": "ongoing", "events": [{"category": ev[0], "text": ev[1], "severity": sev}]},
			"Federation", empires, 1)
		var names := out.map(func(c): return c.cue)
		check(names.has(ev[2]), "briefing '%s' (%s) -> cue %s" % [ev[1], ev[0], ev[2]])
		check(names.has("turn_start"), "briefing always opens with turn_start")
	for outcome in ["victory", "defeat"]:
		var out2 := BotfAudioCues.turn_cues({"outcome": outcome, "events": []}, "", [], 1)
		check(out2.any(func(c): return c.cue == outcome), "outcome %s -> cue %s" % [outcome, outcome])
	# every cue the briefing can produce is in the registry
	for ev in events:
		var out3 := BotfAudioCues.turn_cues({"outcome": "ongoing", "events": [{"category": ev[0], "text": ev[1], "severity": "alert"}]}, "", empires, 1)
		for c in out3:
			check(BotfAudioCues.is_cue(c.cue), "briefing cue %s is registered" % c.cue)


# ------------------------------------------------------------------------------------------------------------
func _battle_weapons() -> void:
	var seen_families := {}
	for weapon in BattleTimeline.WEAPON_TYPES:
		var torp := BattleTimeline.TORPEDO_TYPES.has(weapon)
		for owner in [1, 2, 3, 4, 5]:
			var keys := BattleTimeline.weapon_sound_keys(weapon, 2 if torp else 0, owner)
			check(keys.size() >= 2 and keys[keys.size() - 1] == ("weapon.generic.torpedo" if torp else "weapon.generic.beam"),
				"%s/%d keys end on the generic %s" % [weapon, owner, "torpedo" if torp else "beam"])
			if not have_pack:
				continue
			for era in BotfAudioCues.ERAS:
				var r := trace(keys, era)
				if r.is_empty():
					misses.append("weapon %s/%d/%s" % [weapon, owner, era])
				check(not r.is_empty(), "%s fired by empire %d (%s era) reaches a recorded file" % [weapon, owner, era])
				if not r.is_empty():
					seen_families[("torpedo" if torp else "beam")] = true
					if torp:
						check(not str(r.key).contains("beam"), "%s never plays a beam sound (%s)" % [weapon, r.key])
	# unknown weapon names fall to a generic by family: 0 beam, 1 pulse (beam), 2 torpedo
	for fam in [0, 1, 2]:
		var keys2 := BattleTimeline.weapon_sound_keys("Mystery", fam, 1)
		check(keys2.size() == 1 and keys2[0] == ("weapon.generic.torpedo" if fam == 2 else "weapon.generic.beam"),
			"unknown weapon, family %d -> %s" % [fam, keys2[0]])
		if have_pack:
			check(not trace(keys2, "ds9").is_empty(), "family %d generic sound is a recorded file" % fam)
	if have_pack:
		check(seen_families.has("beam") and seen_families.has("torpedo"), "both beam and torpedo families have sounds")


func _battle_effects() -> void:
	for cue_name in ["ShieldHit", "HullHit", "ShipDestroyed", "Cloak", "Decloak", "Retreat"]:
		check(BattleTimeline.CUE_PRIORITY.has(cue_name), "battle cue %s has a priority" % cue_name)
		for owner in [0, 1, 2, 3, 4, 5]:
			var keys := BattleTimeline.cue_keys(cue_name, owner)
			check(not keys.is_empty() and keys[0] != "sfx.ui.beep", "battle cue %s/%d is mapped (not the default beep)" % [cue_name, owner])
			if have_pack:
				var ok := true
				for era in BotfAudioCues.ERAS:
					ok = ok and not trace(keys, era).is_empty()
				if not ok:
					misses.append("battle %s/%d" % [cue_name, owner])
				check(ok, "battle cue %s/%d reaches a recorded file in every era" % [cue_name, owner])


func _battle_voice() -> void:
	var facs := {1: "fed", 2: "kli", 3: "rom", 4: "car", 5: "fer"}
	for owner in facs:
		for d in BotfAudioCues.CHATTER_DETAILS:
			var keys := BattleTimeline.bark_keys(d, owner)
			check(keys.size() >= 1 and keys[0] == "vo.combat.%s.%s" % [d, facs[owner]], "bark %s/%s maps to its voice key" % [d, facs[owner]])
			if have_pack:
				var ok := not trace(keys, "ds9").is_empty()
				if not ok:
					misses.append("bark %s/%s" % [d, facs[owner]])
				check(ok, "bark %s by %s has a recorded line" % [d, facs[owner]])
	for m in BotfAudioCues.ORDER_DETAILS:
		for fac in facs.values():
			var keys2 := BotfAudioCues.order_voice_keys(m, fac, "ds9")
			check(not keys2.is_empty(), "order %s acknowledged by %s has voice keys" % [m, fac])
			if have_pack:
				var ok2 := not trace(keys2, "ds9").is_empty()
				if not ok2:
					misses.append("order %s/%s" % [m, fac])
				check(ok2, "order %s acknowledged by %s reaches a recorded line" % [m, fac])


# ------------------------------------------------------------------------------------------------------------
## Red alert is the recorded klaxon: a real file, a long one, never the UI beep, in every era and on every cue that
## raises it. (The generated fallback is a beep: if it were what played, resolve_files would return nothing.)
func _red_alert() -> void:
	for cue_name in ["red_alert", "system_under_attack", "war_declared", "encounter_opens"]:
		check(BotfAudioCues.REGISTRY[cue_name].sfx.has("sfx.alert.red"), "cue %s raises sfx.alert.red" % cue_name)
	check(BattleTimeline.cue_keys("Nothing", 1)[0] == "sfx.ui.beep", "an unmapped battle cue is the quiet UI beep, never an alert")
	if not have_pack:
		print("  SKIP red alert files (no pack)")
		return
	var beep_files := lib.files_for("sfx.ui.beep")
	for era in BotfAudioCues.ERAS:
		var r := trace(PackedStringArray(["sfx.alert.red"]), era)
		check(not r.is_empty(), "sfx.alert.red (%s) reaches a recorded file" % era)
		if r.is_empty():
			continue
		check(float(r.length) >= 1.2, "red alert (%s) is a sustained klaxon, %.1f s (%s)" % [era, r.length, r.files[0].get_file()])
		check(not beep_files.has(r.files[0]), "red alert (%s) is not the UI beep file" % era)
	# the show's own klaxon is present for at least one era (TNG), so the era twin wins where the show has one
	var any_show := false
	for era in BotfAudioCues.ERAS:
		any_show = any_show or not lib.files_for("sfx.alert.red." + era).is_empty()
	check(any_show, "a show klaxon exists as an era twin of sfx.alert.red")


# ------------------------------------------------------------------------------------------------------------
## The runtime audio code never names a disk path: no D:\, no LocalAssets, no environment path, no raw FileAccess.
func _no_disk_reads() -> void:
	var banned := ["LocalAssets", "D:/", "D:\\", "OS.get_environment", "FileAccess.open", "DirAccess"]
	for f in DirAccess.get_files_at("res://scripts/audio"):
		if not f.ends_with(".gd"):
			continue
		var text := FileAccess.get_file_as_string("res://scripts/audio/" + f)
		var bad := ""
		for line in text.split("
"):
			var code := line.strip_edges()
			if code.begins_with("#"):
				continue
			for b in banned:
				if code.contains(b):
					bad = b
		check(bad == "", "scripts/audio/%s reads nothing from disk outside AppLoader%s" % [f, "" if bad == "" else " (found " + bad + ")"])
