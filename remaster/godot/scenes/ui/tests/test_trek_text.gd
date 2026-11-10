extends SceneTree
## Stardate formula, per-empire log voice, the phrase table's coverage and variety, and the classifier on the core's own sentences.
##   godot --headless --path remaster/godot -s res://scenes/ui/tests/test_trek_text.gd
## Exit code 0 on success, 1 on any failure.

var failures := 0

func check(cond: bool, what: String) -> void:
	if cond:
		print("  ok  ", what)
	else:
		failures += 1
		printerr("  FAIL ", what)

## Lines exactly as the core writes them (game/Source/BotfSim), one per kind the table must voice.
const CORE_LINES := {
	"colony_founded": "Colony founded at Vega. Production begins next turn.",
	"discovered": "Discovered Rigel.",
	"first_contact": "First contact with Klingon Empire.",
	"battle": "Battle at sector 7,10: 2 ships lost, 1 withdrew.",
	"battle_truce": "Battle at sector 7,10: 0 ships lost, 3 withdrew under a hailed truce.",
	"system_taken": "Sol captured by Romulan Star Empire.",
	"invasion_blocked": "Invasion of Tau Ceti blocked by defending ships.",
	"invasion_failed": "Invasion of Tau Ceti failed.",
	"repaired": "Galaxy - Command Cruiser repaired at Sol.",
	"ship_launched": "Oberth - Science Vessel launched at Sol.",
	"research_done": "Weapons advanced to level 4. Benefits apply next turn.",
	"terraform_done": "Terraforming of Sol III completed. Habitable capacity expanded to 18.",
	"windfall": "System 3: local trade windfall (+25 credits).",
	"celebration": "System 3: civic celebration (+10 morale).",
	"unrest": "System 3: civil unrest (-10 morale).",
	"pop_up": "System 3: population +2. Governor (Balanced) assigned citizens.",
	"pop_unassigned": "System 3: population +1. New citizens are unassigned.",
	"pop_down": "System 3: population -1 from food shortage.",
	"governor_warning": "System 3 governor warning: no free labs.",
	"construction_paused": "System 3: construction paused. Needs a shipyard.",
	"starbase_work": "System 3: 40 industry to the starbase at sector 5,5 (120/400).",
	"megaproject_work": "System 3: 40 industry to the Genesis Initiative (120/9000).",
	"outpost_done": "Outpost completed at sector 5,5. Supply and sensor range extended.",
	"outpost_abandoned": "Fleet 12: outpost assembly at sector 5,5 abandoned (the transport received new orders).",
	"route_paused": "Fleet 12: route paused, no supplied path.",
	"station_lost": "Federation station at sector 5,5 destroyed by Klingon Empire.",
	"survey": "Oberth - Science Vessel surveyed the Nebula at sector 4,4: +30 Computers research and +10 intelligence.",
	"sabotage_hit": "Sabotage disrupted production at system 3.",
	"intel_report": "Intelligence report: Klingon Empire has 9 colonies and roughly 1200 credits.",
	"intel_blueprints": "Intelligence: stolen blueprints advanced our research by 1 level(s).",
	"intel_espionage_ok": "Intelligence: Espionage against Romulan Star Empire succeeded.",
	"intel_sabotage_ok": "Intelligence: Sabotage against Romulan Star Empire succeeded.",
	"intel_exposed": "Intelligence: our agents were exposed by Cardassian Union.",
	"intel_alert": "Security Alert: Hostile agent activity detected from Cardassian Union!",
	"war_on_us": "Klingon Empire is now at war with us.",
	"peace_with": "Ferengi Alliance accepted peace.",
	"offer_accepted": "Romulan Star Empire accepted the trade treaty.",
	"offer_declined": "Romulan Star Empire declined peace terms.",
	"alliance_honored": "Klingon Empire honored its alliance with Federation and declared war on Romulan Star Empire.",
	"minor_joined_us": "Bajorans accepted membership! Colony established.",
	"minor_seceded_us": "Bajorans seceded from our empire. Neglect and war exhausted their loyalty; the colony is independent again.",
	"minor_conquered": "Bajorans was conquered by Cardassian Union and torn from Federation.",
	"proposal": "Diplomacy: Klingon Empire proposes a Trade Pact (proposal 4, respond by turn 14).",
	"tribute": "Diplomacy: Klingon Empire demands 300 credits in tribute (proposal 5, respond by turn 14).",
	"gift": "Diplomacy: Klingon Empire offers a gift of 200 credits (proposal 6, respond by turn 14).",
	"call_to_arms": "Diplomacy: Klingon Empire calls on us to honor our alliance against Romulan Star Empire. Declare war before the call falls due, or refuse it now; either way a refusal costs 20 trust.",
	"call_refused": "Diplomacy: We refused Klingon Empire's call to arms against Romulan Star Empire. Klingon Empire's trust in us fell by 20.",
	"war_declared": "Galactic news: Klingon Empire declared war on Romulan Star Empire.",
	"peace_between": "Galactic news: Klingon Empire and Romulan Star Empire made peace.",
	"treaty_broken": "Galactic news: Klingon Empire broke its treaty with Romulan Star Empire by declaring war. Every empire takes note.",
	"eliminated": "Galactic news: Ferengi Alliance has been eliminated; its treaties are dissolved.",
	"treaty_signed": "Galactic news (Klingon Empire / Romulan Star Empire): Trade Pact ratified.",
	"treaty_ended": "Diplomacy (Federation / Klingon Empire): Trade Pact expired.",
	"victory": "Federation campaign victory.",
	"defeat": "Federation campaign defeat.",
	"event_plague": "Plague at system 3: quarantine imposed (-10 morale, losses halved, ends next turn).",
	"event_supernova": "Supernova warning: stellar shielding is under construction; losses will be limited to 30%.",
	"event_derelict": "Derelict vessel salvaged: +60 Energy research.",
	"event_unrest": "Labor unrest at system 3: martial law ends the strike (-10 morale).",
	"event_breakthrough": "Scientific breakthrough applied to Energy: +40 research.",
	"event_trade": "Trade boom: +120 credits from booming interstellar commerce.",
	"event_rift": "Subspace rift: A subspace rift opened at sector 8,8.",
}

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	# One stardate formula: monotone, one decimal, the same for every caller.
	check(TrekText.stardate(0, "tng") < TrekText.stardate(1, "tng") and TrekText.stardate(1, "tng") < TrekText.stardate(2, "tng"), "stardate rises each turn")
	check(TrekText.stardate_text(12, "tng") == TrekText.stardate_text(12, "tng") and TrekText.stardate_text(12, "tng").find(".") > 0, "stardate text is stable and carries a decimal")
	check(TrekText.stardate(5, "tos") != TrekText.stardate(5, "tng"), "each era has its own scale")
	check(TrekText.stardate_text(3, "nonsense") == TrekText.stardate_text(3, TrekText.current_era()), "an unknown era falls back to the interface era")
	# Titles per empire are distinct and carry the stardate.
	var titles := {}
	for e in range(1, 7):
		titles[TrekText.log_title(e)] = true
		check(TrekText.log_header(e, 4, "tng").find(TrekText.stardate_text(4, "tng")) > 0, "log header %d carries the stardate" % e)
	check(titles.size() == 6, "six empires, six log titles")
	check(TrekText.log_title(1) == "Captain's log" and TrekText.log_title(5).find("Ledger") >= 0, "Federation captain's log, Ferengi ledger")
	# The classifier knows every core sentence in the list, and the table covers every kind with six voices of several variants.
	for kind in CORE_LINES:
		var c := TrekText.classify(CORE_LINES[kind])
		check(c["kind"] == kind, "classifies %s (got %s)" % [kind, c["kind"]])
	for kind in TrekText.kinds():
		var t: Array = TrekText.PHRASES[kind]
		var ok := t.size() == 6
		for v in t:
			ok = ok and v.size() >= 2
		check(ok, "phrase table %s: six voices, 2+ variants each" % kind)
	# Unknown lines are kept, never dropped.
	var raw := "Something the core has not told us about."
	for e in range(1, 7):
		check(TrekText.voice_line(e, raw, 3, 0).find("Something the core") >= 0, "unknown line kept in voice %d" % e)
	# Every voiced line is clean: no leftover slots, not empty, and each empire sounds different.
	var ctx := {"own_name": "Federation", "sysnames": {3: "Vega"}}
	var leftovers := 0
	var empties := 0
	var distinct_per_kind := 0
	for kind in CORE_LINES:
		var seen := {}
		for e in range(1, 7):
			var l := TrekText.voice_line(e, CORE_LINES[kind], 7, 2, ctx)
			if l == "":
				empties += 1
			if l.find("{") >= 0 or l.find("}") >= 0:
				leftovers += 1
				printerr("    slot left: ", l)
			seen[l] = true
		if seen.size() == 6:
			distinct_per_kind += 1
		else:
			printerr("    not distinct: ", kind, " -> ", seen.keys())
	check(empties == 0, "no empty voiced line")
	check(leftovers == 0, "no unfilled {slot} in any voiced line")
	check(distinct_per_kind == CORE_LINES.size(), "six empires give six different lines for every kind")
	# Variants are chosen by seed: across turns the same event reads differently.
	var variants := {}
	for turn in range(1, 40):
		variants[TrekText.voice_line(1, CORE_LINES["colony_founded"], turn, 0, ctx)] = true
	check(variants.size() >= 2, "the same event varies with the seed (%d variants seen)" % variants.size())
	check(TrekText.voice_line(2, CORE_LINES["colony_founded"], 9, 1, ctx) == TrekText.voice_line(2, CORE_LINES["colony_founded"], 9, 1, ctx), "the seed is deterministic")
	# Own conquest reads as ours, a loss as theirs.
	var won := TrekText.voice_line(1, "Vega captured by Federation.", 5, 0, {"own_name": "Federation"})
	var lost := TrekText.voice_line(1, "Vega captured by Klingon Empire.", 5, 0, {"own_name": "Federation"})
	check(won != lost and (won.find("ours") >= 0 or won.find("our protection") >= 0), "capturing a system by us is voiced as ours")
	# The briefing wrapper adds what the core leaves to the UI.
	var briefing := {"turn": 9, "events": [{"category": "colonization", "text": CORE_LINES["colony_founded"], "severity": "info"}],
		"colonies": [{"name": "Vega", "systemId": 3, "completed": "Laboratory", "economy": {"foodDeficit": 2}}], "creditsBefore": 100, "creditsAfter": -20}
	var entries := TrekText.log_entries(1, briefing, ctx)
	check(entries.size() == 4, "colony line + construction complete + food short + empty treasury (got %d)" % entries.size())
	var has_done := false
	var has_treasury := false
	for e in entries:
		check(str(e.get("voice", "")) != "", "every entry carries a voice line")
		has_done = has_done or (str(e["voice"]).find("Laboratory") >= 0)
		has_treasury = has_treasury or e.get("category", "") == "economy"
	check(has_done and has_treasury, "construction complete and treasury warning are written")
	# Rail terms: short enough for the 150 px rail.
	for id in TrekText.RAIL:
		for i in range(6):
			check(str(TrekText.RAIL[id][i]).length() <= 12, "rail %s/%d fits (%s)" % [id, i, TrekText.RAIL[id][i]])
	check(TrekText.term(5, "credits", "Credits") == "Latinum" and TrekText.term(1, "credits", "Credits") == "Credits", "Ferengi count latinum")
	if failures == 0:
		print("trek_text: all checks passed")
	else:
		printerr("trek_text: %d FAILED" % failures)
	quit(1 if failures > 0 else 0)
