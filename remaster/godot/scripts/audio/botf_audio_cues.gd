class_name BotfAudioCues
extends RefCounted
## The cue registry: game events -> sound keys, buses and priorities. Keys follow the manifest key families
## (sfx.*, vo.*, mus.*, weapon.*), so a local file resolves for them exactly as in the Unreal client.
## Design reference only: UBotfAudioSubsystem / BotfAudioCues.h / BotfAudioEvents.h (docs/AUDIO-PLAN.md section 3).
## Everything here is pure (no nodes, no clock): unit-tested in scripts/audio/tests/test_audio.gd.

const BUS_MASTER := "Master"
const BUS_EFFECTS := "Effects"
const BUS_VOICE := "Voice"
const BUS_INTERFACE := "Interface"
const BUS_MUSIC := "Music"
const BUSES := ["Master", "Effects", "Voice", "Interface", "Music"]

const FACTION_PREFIX := {1: "fed", 2: "kli", 3: "rom", 4: "car", 5: "fer"}

## cue name -> {sfx: keys tried in order, voice: keys with {fac} {other} {speaker} {detail} placeholders,
## bus: bus of the sfx, priority, beep: play sfx.ui.beep when no voice file resolves and no sfx is listed}.
## Voice keys never cross factions: a missing line is silence (or the beep), never another race's voice.
const REGISTRY := {
	# Interface (hooked once at the app level, see BotfAudioSystem)
	"ui_click": {"sfx": ["sfx.ui.click", "sfx.ui.beep"], "bus": "Interface", "priority": 1},
	"ui_open": {"sfx": ["sfx.ui.open", "sfx.ui.beep"], "bus": "Interface", "priority": 1},
	"ui_close": {"sfx": ["sfx.ui.close", "sfx.ui.beep"], "bus": "Interface", "priority": 1},
	"end_turn": {"sfx": ["sfx.ui.endturn", "sfx.ui.beep"], "bus": "Interface", "priority": 2},
	"turn_start": {"sfx": ["sfx.ui.turn", "sfx.ui.beep"], "bus": "Interface", "priority": 1},
	"research_complete": {"sfx": ["sfx.ui.research", "sfx.ui.beep"], "bus": "Interface", "priority": 4,
		"voice": ["vo.advisor.research.{fac}"]},
	"construction_complete": {"sfx": ["sfx.ui.construction", "sfx.ui.beep"], "bus": "Interface", "priority": 3,
		"voice": ["vo.advisor.construction.{fac}"]},
	"colony_founded": {"sfx": ["sfx.ui.construction", "sfx.ui.beep"], "bus": "Interface", "priority": 4,
		"voice": ["vo.advisor.colony.{fac}"]},
	"outpost_built": {"sfx": ["sfx.ui.construction", "sfx.ui.beep"], "bus": "Interface", "priority": 4,
		"voice": ["vo.station.{fac}.{detail}", "vo.station.{fac}.built"]},
	"galactic_event": {"sfx": ["sfx.ui.beep"], "bus": "Interface", "priority": 5,
		"voice": ["vo.event.{detail}", "vo.advisor.event.{fac}"]},
	"treaty_signed": {"sfx": ["sfx.ui.beep"], "bus": "Interface", "priority": 5,
		"voice": ["vo.treaty.{detail}.{fac}", "vo.diplomacy.accept.{fac}"]},
	# Alerts and diplomacy
	"red_alert": {"sfx": ["sfx.alert.red"], "bus": "Effects", "priority": 8,
		"voice": ["vo.advisor.fleet.{fac}"]},
	"system_under_attack": {"sfx": ["sfx.alert.red"], "bus": "Effects", "priority": 8,
		"voice": ["vo.advisor.attack.{fac}"]},
	"first_contact": {"sfx": [], "bus": "Interface", "priority": 7, "beep": true,
		"voice": ["vo.diplomacy.greet.{other}", "vo.diplomacy.{other}", "vo.advisor.contact.{fac}"]},
	"diplomatic_proposal": {"sfx": [], "bus": "Interface", "priority": 6, "beep": true,
		"voice": ["vo.advisor.diplomacy.{fac}"]},
	"war_declared": {"sfx": ["sfx.alert.red"], "bus": "Effects", "priority": 9,
		"voice": ["vo.advisor.war.{fac}", "vo.diplomacy.war.{other}"]},
	"peace_made": {"sfx": ["sfx.ui.beep"], "bus": "Interface", "priority": 5,
		"voice": ["vo.diplomacy.peace.{other}", "vo.diplomacy.accept.{fac}"]},
	"victory": {"sfx": [], "bus": "Interface", "priority": 10,
		"voice": ["vo.victory.{detail}.{fac}", "vo.victory.win.{fac}"]},
	"defeat": {"sfx": [], "bus": "Interface", "priority": 10,
		"voice": ["vo.victory.defeat.{fac}"]},
	# Battle
	"encounter_opens": {"sfx": ["sfx.alert.red"], "bus": "Effects", "priority": 8},
	"order_accepted": {"sfx": ["sfx.ui.beep"], "bus": "Interface", "priority": 6,
		"voice": ["vo.combat.{detail}.{fac}", "vo.combat.ack.{fac}"]},
	"beam_fire": {"sfx": ["weapon.generic.beam"], "bus": "Effects", "priority": 3},
	"torpedo_launch": {"sfx": ["weapon.generic.torpedo"], "bus": "Effects", "priority": 3},
	"shield_hit": {"sfx": ["sfx.shield.hit"], "bus": "Effects", "priority": 2},
	"hull_hit": {"sfx": ["sfx.explosion.small"], "bus": "Effects", "priority": 2},
	"ship_destroyed": {"sfx": ["sfx.explosion.large"], "bus": "Effects", "priority": 5},
	"retreat": {"sfx": ["sfx.warp.out"], "bus": "Effects", "priority": 3},
	"fleet_arrives": {"sfx": ["sfx.warp.in"], "bus": "Effects", "priority": 2},
}

## Music contexts -> key chains (first with files wins), the generated fallback kind, and loop behaviour.
const MUSIC := {
	"title": {"keys": ["mus.botf.title", "mus.botf.all"], "tone": "title"},
	"galaxy": {"keys": ["mus.botf.galaxy.{fac}", "mus.botf.galaxy", "mus.botf.all"], "tone": "galaxy"},
	"battle": {"keys": ["mus.botf.combat.{fac}", "mus.botf.combat", "mus.botf.all"], "tone": "battle"},
	"tension": {"keys": ["mus.botf.combat.tension.{fac}", "mus.botf.combat.tension", "mus.botf.combat.{fac}", "mus.botf.combat", "mus.botf.all"], "tone": "tension"},
}

## The key chain of a music context for the player's faction ({fac} templates dropped when there is none).
static func music_keys(context: String, fac := "") -> PackedStringArray:
	var out := PackedStringArray()
	for tpl in MUSIC.get(context, {}).get("keys", []):
		var k := str(tpl)
		if k.contains("{fac}"):
			if fac == "":
				continue
			k = k.replace("{fac}", fac)
		out.append(k)
	return out

## Which screen plays which music context (App screen ids).
const SCREEN_MUSIC := {
	"title": "title", "new": "title", "saves": "title",
	"galaxy": "galaxy", "system": "galaxy", "empire": "galaxy", "research": "galaxy", "fleets": "galaxy",
	"diplomacy": "galaxy", "intel": "galaxy", "database": "galaxy",
	"combat": "battle",
}

## The music context for an App screen id ("" when the screen keeps whatever plays).
static func music_for_screen(screen: String) -> String:
	return str(SCREEN_MUSIC.get(screen, ""))


static func faction_prefix(empire_id: int) -> String:
	return str(FACTION_PREFIX.get(empire_id, ""))


static func is_cue(cue: String) -> bool:
	return REGISTRY.has(cue)


## {sfx: PackedStringArray, bus, voice: PackedStringArray, priority, beep, cue}. Unknown cue -> {} (silent).
## fac: the listening player's faction prefix; other: the other party (diplomacy speaker); detail: event / station /
## treaty / victory / combat-order detail. A voice template whose placeholder is empty is dropped.
static func cue_for(cue: String, fac := "", other := "", detail := "") -> Dictionary:
	var def: Dictionary = REGISTRY.get(cue, {})
	if def.is_empty():
		return {}
	var speaker := other if other != "" else fac
	var voice := PackedStringArray()
	for tpl in def.get("voice", []):
		var key := _fill(str(tpl), fac, other, speaker, detail)
		if key != "" and not voice.has(key):
			voice.append(key)
	var sfx := PackedStringArray()
	for k in def.get("sfx", []):
		if not sfx.has(str(k)):
			sfx.append(str(k))
	return {"cue": cue, "sfx": sfx, "bus": str(def.get("bus", "Effects")), "voice": voice,
		"priority": int(def.get("priority", 1)), "beep": bool(def.get("beep", false))}


static func _fill(tpl: String, fac: String, other: String, speaker: String, detail: String) -> String:
	for ph in [["{fac}", fac], ["{other}", other], ["{speaker}", speaker], ["{detail}", detail]]:
		if tpl.contains(ph[0]):
			if ph[1] == "":
				return ""
			tpl = tpl.replace(ph[0], ph[1])
	return tpl


## Minimum seconds between two plays of one key: weapons 0.08, UI 0.12, alerts 3, other sfx 0.15, voice 4.
static func cooldown_for(key: String, bus: String) -> float:
	if bus == BUS_VOICE:
		return 4.0
	if key.begins_with("weapon."):
		return 0.25 # with BotfCueScheduler.salvo_voices: up to 4 shots of one weapon in a quarter second
	if key.begins_with("sfx.alert"):
		return 3.0
	if bus == BUS_INTERFACE:
		return 0.12
	return 0.15


## Bus a bare key plays on when no cue is involved (BattleAudio hands over key lists).
static func bus_for_key(key: String) -> String:
	if key.begins_with("vo."):
		return BUS_VOICE
	if key.begins_with("mus."):
		return BUS_MUSIC
	if key.begins_with("sfx.ui."):
		return BUS_INTERFACE
	return BUS_EFFECTS


## Cues for a finished turn from the core's briefing, highest priority first, at most one per cue kind (a
## briefing can hold dozens of lines). briefing: {outcome, events: [{category, severity, text, ...}]};
## player_name and other_names (empire names, to tell who a diplomacy line is about) are optional.
## Each entry: {cue, other, detail}.
static func turn_cues(briefing: Dictionary, player_name := "", empires: Array = [], player_id := 0) -> Array:
	var out: Array = []
	var seen := {}
	_add(out, seen, "turn_start")
	for e in briefing.get("events", []):
		var cat := str(e.get("category", ""))
		var text := str(e.get("text", ""))
		var low := text.to_lower()
		match cat:
			"research": _add(out, seen, "research_complete")
			"construction": _add(out, seen, "construction_complete")
			"colonization": _add(out, seen, "colony_founded")
			"stations": _add(out, seen, "outpost_built")
			"events": _add(out, seen, "galactic_event", "", str(e.get("eventId", "")) if int(e.get("eventId", 0)) > 0 else "")
			"diplomacy":
				var other := other_prefix(text, empires, player_id)
				if low.contains("war"):
					_add(out, seen, "war_declared", other)
				elif low.contains("peace") or low.contains("armistice"):
					_add(out, seen, "peace_made", other)
				elif low.contains("treaty") or low.contains("alliance") or low.contains("pact"):
					_add(out, seen, "treaty_signed", other)
				elif low.contains("first contact") or low.contains("met ") or low.contains("contact"):
					_add(out, seen, "first_contact", other)
				else:
					_add(out, seen, "diplomatic_proposal", other)
			"combat":
				if str(e.get("severity", "")) in ["alert", "warn"] and (low.contains("attack") or low.contains("under fire")):
					_add(out, seen, "system_under_attack")
	var outcome := str(briefing.get("outcome", "ongoing"))
	if outcome == "victory":
		_add(out, seen, "victory")
	elif outcome == "defeat":
		_add(out, seen, "defeat")
	out.sort_custom(func(a, b): return priority_of(a.cue) > priority_of(b.cue))
	return out


static func _add(out: Array, seen: Dictionary, cue: String, other := "", detail := "") -> void:
	if not seen.has(cue):
		seen[cue] = true
		out.append({"cue": cue, "other": other, "detail": detail})


static func priority_of(cue: String) -> int:
	return int(REGISTRY.get(cue, {}).get("priority", 1))


## Faction prefix of the first non-player empire named in `text` ("" when none).
static func other_prefix(text: String, empires: Array, player_id: int) -> String:
	var best := ""
	var best_at := 1 << 30
	for emp in empires:
		if int(emp.get("id", 0)) == player_id:
			continue
		var nm := str(emp.get("name", ""))
		var at := text.find(nm) if nm != "" else -1
		if at >= 0 and at < best_at:
			best_at = at
			best = faction_prefix(int(emp.get("id", 0)))
	return best
