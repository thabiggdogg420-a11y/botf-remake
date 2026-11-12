class_name TrekText
extends RefCounted
## The Star Trek voice of the game, on the Godot side only (the core's C++ sentences are never edited).
##
##  * ONE stardate formula (stardate / stardate_text) for the top bar, the chronicle, the saves list and the end screen.
##  * Turn reports as a first-person log in each empire's own voice: Federation captain's log, Klingon battle record,
##    Romulan report to the Senate, Cardassian report to Central Command, Ferengi ledger, Dominion Vorta report.
##  * A phrase table keyed by event kind and empire with several variants per key, chosen by a seed so a report does not
##    repeat itself. The core's line is classified by pattern (CLASSIFIERS), its facts are pulled out, and the empire's
##    variant is filled in. A line no pattern knows keeps its own text inside the empire's frame (never dropped).
##  * Rail and top-bar terms per empire.
##
## Voice indexes: 0 Federation, 1 Klingon, 2 Romulan, 3 Cardassian, 4 Ferengi, 5 Dominion (empire ids 1..5, key "dominion" or 6).
## All lines are written in-house; nothing is quoted from the shows.

const VOICE_KEYS := ["fed", "kli", "rom", "car", "fer", "dom"]

# ---------------------------------------------------------------------------------------------
# Stardate: base + turn * step per interface era. One formula, no per-screen copies.
# ---------------------------------------------------------------------------------------------
const ERA_STARDATE := {
	"tos": [1312.4, 6.5],
	"movie": [7410.2, 6.5],
	"tng": [41153.7, 9.5],
	"voy": [48315.6, 9.5],
	"late": [78183.4, 9.5],
}

## The era whose stardate scale is used when a caller gives none (the interface era setting).
static func current_era() -> String:
	var e := str(UiTokens.era)
	return e if ERA_STARDATE.has(e) else "tng"

static func stardate(turn: int, era_id: String = "") -> float:
	var def: Array = ERA_STARDATE.get(era_id if ERA_STARDATE.has(era_id) else current_era(), ERA_STARDATE["tng"])
	return float(def[0]) + float(turn) * float(def[1])

static func stardate_text(turn: int, era_id: String = "") -> String:
	return "%.1f" % stardate(turn, era_id)

# ---------------------------------------------------------------------------------------------
# Voices
# ---------------------------------------------------------------------------------------------
static func voice_of(empire: Variant) -> int:
	if empire is String:
		var s := String(empire).to_lower()
		if s == "dominion" or s == "dom":
			return 5
		return 0
	var i := int(empire)
	if i >= 1 and i <= 6:
		return i - 1
	return 0

const LOG_TITLE := ["Captain's log", "Battle record of the Empire", "Report to the Senate", "Report to Central Command", "Ledger of the Alliance", "Vorta report to the Founders"]
const LOG_SHORT := ["Captain's log", "Battle record", "Senate report", "Command report", "Ledger", "Vorta report"]
const LOG_LEAD := ["Captain's log, stardate %s.", "Record of the Empire, stardate %s.", "To the Senate, stardate %s.", "To Central Command, stardate %s.", "Ledger entry, stardate %s.", "To the Founders, stardate %s."]

static func log_title(empire: Variant) -> String:
	return LOG_TITLE[voice_of(empire)]

static func log_short(empire: Variant) -> String:
	return LOG_SHORT[voice_of(empire)]

## "Captain's log, stardate 47305.3." in the empire's own wording.
static func log_header(empire: Variant, turn: int, era_id: String = "") -> String:
	return LOG_LEAD[voice_of(empire)] % stardate_text(turn, era_id)

# ---------------------------------------------------------------------------------------------
# Terms per empire (rail labels are kept short for the 150 px rail)
# ---------------------------------------------------------------------------------------------
const RAIL := {
	"galaxy": ["Cartography", "Star chart", "Star map", "Sector map", "Trade chart", "Sector map"],
	"system": ["Colony", "Holdfast", "Province", "Prefecture", "Holding", "Holding"],
	"empire": ["Council", "High Council", "Senate", "Detapa", "Commerce", "Founders"],
	"research": ["Science", "Forge", "Directorate", "Ministry", "Acquisition", "Genetics"],
	"fleets": ["Starfleet", "Warfleet", "Fleet", "Command", "Alliance", "Jem'Hadar"],
	"diplomacy": ["Diplomats", "Embassy", "Envoys", "Foreign", "Negotiate", "Envoys"],
	"intel": ["Fleet Intel", "Spies", "Tal Shiar", "Obsidian", "Informants", "Vorta"],
	"database": ["Records", "Archive", "Archive", "Archive", "Rules", "Records"],
	"events": ["Bulletin", "Messengers", "Messengers", "Bulletins", "Market news", "Reports"],
	"victory": ["Objectives", "Glory", "Destiny", "Ascendancy", "Profit", "Order"],
	"briefing": ["Log", "Record", "Dispatch", "Briefing", "Ledger", "Report"],
}

static func rail_label(empire: Variant, id: String, fallback: String) -> String:
	if not RAIL.has(id):
		return fallback
	return str(RAIL[id][voice_of(empire)])

## Single words that change by empire: "credits" (Ferengi count latinum), "governor".
static func term(empire: Variant, key: String, fallback: String) -> String:
	var v := voice_of(empire)
	match key:
		"credits":
			return "Latinum" if v == 4 else fallback
		"governor":
			return ["Colonial administrator", "Governor", "Proconsul", "Prefect", "Overseer", "Overseer"][v]
		"events_title":
			return ["Subspace bulletin", "Messengers of the Empire", "Dispatches", "Bulletins from Central Command", "Market news", "Reports to the Founders"][v]
		"events_empty":
			return ["No priority transmissions.", "No messengers wait at the gate.", "Nothing to report to the Senate.", "No bulletins concern Central Command.", "The market is quiet. Nothing to buy or sell.", "Nothing to report to the Founders."][v]
		"log_filed":
			return ["Log filed", "Let it stand", "Noted", "Filed", "Entered", "Recorded"][v]
		"contact_title":
			return "Sensors detect hostile vessels"
		"unknown_vessel":
			return "Unidentified vessel"
		"legend_ships":
			return "Starfleet vessels" if v == 0 else fallback
	return fallback

# ---------------------------------------------------------------------------------------------
# Classifying the core's lines
# ---------------------------------------------------------------------------------------------
## [kind, regex, [argument names]]. First match wins. Group count must equal the name count.
const CLASSIFIERS := [
	["threat_battle", "^Battle at sector (\\d+),(\\d+) against the (.+?): (\\d+) ships? lost, (\\d+) withdrew(?:\\. The .+ was destroyed)?\\.$", ["x", "y", "what", "lost", "fled"]],
	["threat_alert", "^Starfleet Intelligence: (.+) detected entering the quadrant\\.$", ["what"]],
	["threat_sighted", "^Long-range sensors: (.+) detected at sector (\\d+),(\\d+)\\.$", ["what", "x", "y"]],
	["threat_left", "^(.+?) (?:withdrew from|left) the quadrant\\.$", ["what"]],
	["threat_dead", "^The (.+) was destroyed at sector (\\d+),(\\d+)\\.$", ["what", "x", "y"]],
	["threat_siege", "^The (.+) besieges (.+)\\.$", ["what", "sys"]],
	["assimilated", "^The Borg have assimilated (.+?) \\((.+)\\)\\.$", ["sys", "owner"]],
	["assimilated_us", "^The Borg Collective assimilated (.+)\\.$", ["sys"]],
	["assimilated_ships", "^The Borg assimilated (\\d+) wrecked ship\\(s\\) at sector (\\d+),(\\d+)\\.$", ["n", "x", "y"]],
	["bombard", "^The (.+?) (bombards|strips|raids) (.+?)(?:: population -(\\d+))?\\.$", ["what", "verb", "sys", "n"]],
	["world_destroyed", "^(.+?) was (stripped of life|destroyed) by the (.+) \\((.+)\\)\\.$", ["sys", "how", "what", "owner"]],
	["threat_station", "^A station at sector (\\d+),(\\d+) fell to the (.+)\\.$", ["x", "y", "what"]],
	["battle_truce", "^Battle at sector (\\d+),(\\d+): (\\d+) ships? lost, (\\d+) withdrew under a hailed truce\\.$", ["x", "y", "lost", "fled"]],
	["battle", "^Battle at sector (\\d+),(\\d+): (\\d+) ships? lost, (\\d+) withdrew\\.$", ["x", "y", "lost", "fled"]],
	["invasion_blocked", "^Invasion of (.+) blocked by defending ships\\.$", ["sys"]],
	["invasion_failed", "^Invasion of (.+) failed\\.$", ["sys"]],
	["system_taken", "^(.+) captured by (.+)\\.$", ["sys", "by"]],
	["station_lost", "^(.+) station at (.+) destroyed by (.+)\\.$", ["owner", "place", "by"]],
	["colony_founded", "^Colony founded at (.+)\\. Production begins next turn\\.$", ["sys"]],
	["discovered", "^Discovered (.+)\\.$", ["sys"]],
	["first_contact", "^First contact with (.+?)\\.(?: Diplomatic channels opened\\.)?$", ["who"]],
	["research_done", "^(.+) advanced to level (\\d+)\\. Benefits apply next turn\\.$", ["field", "lvl"]],
	["terraform_done", "^Terraforming of (.+) completed\\. Habitable capacity expanded to (\\d+)\\.$", ["planet", "cap"]],
	["survey", "^(.+) surveyed the (.+) at (.+?): \\+(\\d+) (.+?) research(.*)\\.$", ["ship", "feature", "place", "gain", "field", "extra"]],
	["feature_gone", "^The (.+) at (.+) has collapsed; the sector is passable again\\.$", ["feature", "place"]],
	["construction_done", "^([^:.]+) completed ([^:.]+)\\.$", ["sys", "what"]],
	["ship_launched", "^(.+) launched at (.+)\\.$", ["ship", "sys"]],
	["repaired", "^(.+) repaired at (.+)\\.$", ["ship", "place"]],
	["outpost_done", "^Outpost completed at (.+)\\. Supply and sensor range extended\\.$", ["place"]],
	["outpost_abandoned", "^Fleet (\\d+): outpost assembly at (.+) abandoned \\((.+)\\)\\.$", ["fleet", "place", "why"]],
	["route_paused", "^Fleet (\\d+): route paused, no supplied path\\.$", ["fleet"]],
	["windfall", "^(?:System )?([^:.]+): local trade windfall \\(\\+(\\d+) credits\\)\\.$", ["sys", "n"]],
	["celebration", "^(?:System )?([^:.]+): civic celebration \\(\\+(\\d+) morale\\)\\.$", ["sys", "n"]],
	["unrest", "^(?:System )?([^:.]+): civil unrest \\(-(\\d+) morale\\)\\.$", ["sys", "n"]],
	["pop_up", "^(?:System )?([^:.]+): population \\+(\\d+)\\. Governor \\((.+)\\) assigned citizens\\.$", ["sys", "n", "gov"]],
	["pop_unassigned", "^(?:System )?([^:.]+): population \\+(\\d+)\\. New citizens are unassigned\\.$", ["sys", "n"]],
	["pop_down", "^(?:System )?([^:.]+): population -(\\d+) from food shortage\\.$", ["sys", "n"]],
	["governor_warning", "^(?:System )?([^:.]+) governor warning: (.+)\\.$", ["sys", "why"]],
	["construction_paused", "^(?:System )?([^:.]+): construction paused\\. (.+)\\.$", ["sys", "why"]],
	["starbase_work", "^(?:System )?([^:.]+): (\\d+) industry to the starbase at (.+) \\((\\d+)/(\\d+)\\)\\.$", ["sys", "n", "place", "done", "cost"]],
	["megaproject_work", "^(?:System )?([^:.]+): (\\d+) industry to the (.+) \\((\\d+)/(\\d+)\\)\\.$", ["sys", "n", "name", "done", "cost"]],
	["sabotage_hit", "^Sabotage disrupted production at (?:system )?([^.]+)\\.$", ["sys"]],
	["intel_report", "^Intelligence report: (.+) has (\\d+) colonies and roughly (\\d+) credits\\.$", ["who", "colonies", "credits"]],
	["intel_blueprints", "^Intelligence: stolen blueprints advanced our research by (\\d+) level\\(s\\)\\.$", ["n"]],
	["intel_espionage_ok", "^Intelligence: Espionage against (.+) succeeded(.*)\\.$", ["who", "extra"]],
	["intel_sabotage_ok", "^Intelligence: Sabotage against (.+) succeeded\\.$", ["who"]],
	["intel_exposed", "^Intelligence: our agents were exposed by (.+)\\.$", ["who"]],
	["intel_recalled", "^Intelligence: agents recalled from (.+?)(?: .*)?\\.$", ["who"]],
	["intel_alert", "^Security Alert: Hostile agent activity detected from (.+)!$", ["who"]],
	["war_on_us", "^(.+) is now at war with us\\.$", ["who"]],
	["peace_with", "^(.+) accepted peace\\.$", ["who"]],
	["offer_accepted", "^(.+) accepted (peace terms|the trade treaty)\\.$", ["who", "what"]],
	["offer_declined", "^(.+) declined (peace terms|the trade treaty)\\.$", ["who", "what"]],
	["alliance_honored", "^(.+) honored its alliance with (.+) and declared war on (.+)\\.$", ["ally", "defender", "aggressor"]],
	["minor_joined_us", "^(.+) accepted membership! Colony established\\.$", ["who"]],
	["minor_seceded_us", "^(.+) seceded from our empire\\..*$", ["who"]],
	["minor_seceded", "^(.+) seceded from (.+)\\.$", ["who", "by"]],
	["minor_conquered", "^(.+) was conquered by (.+?)(?: and torn from .+)?\\.$", ["who", "by"]],
	["proposal", "^Diplomacy: (.+) proposes a (.+) \\(proposal \\d+, respond by turn (\\d+)\\)\\.$", ["who", "what", "due"]],
	["tribute", "^Diplomacy: (.+) demands (\\d+) credits in tribute \\(proposal \\d+, respond by turn (\\d+)\\)\\.$", ["who", "n", "due"]],
	["gift", "^Diplomacy: (.+) offers a gift of (\\d+) credits \\(proposal \\d+, respond by turn (\\d+)\\)\\.$", ["who", "n", "due"]],
	["call_refused", "^Diplomacy: We refused (.+)'s call to arms against (.+?)\\..*$", ["who", "third"]],
	["call_to_arms", "^Diplomacy: (.+) calls on us to honor our alliance against (.+?)\\..*$", ["who", "third"]],
	["war_declared", "^(?:Diplomacy|Galactic news): (.+) declared war on (.+)\\.$", ["a", "b"]],
	["peace_between", "^(?:Diplomacy|Galactic news): (.+) and (.+) made peace\\.$", ["a", "b"]],
	["treaty_broken", "^(?:Diplomacy|Galactic news): (.+) broke its treaty with (.+) by declaring war\\..*$", ["a", "b"]],
	["eliminated", "^(?:Diplomacy|Galactic news): (.+) has been eliminated; its treaties are dissolved\\.$", ["a"]],
	["minor_joined", "^(?:Diplomacy|Galactic news): (.+) joined (.+)\\.$", ["who", "by"]],
	["treaty_signed", "^(?:Diplomacy|Galactic news) \\((.+?)(?: / (.+?))?\\): (.+?) ratified\\.$", ["a", "b", "what"]],
	["treaty_signed_peace", "^(?:Diplomacy|Galactic news) \\((.+?)(?: / (.+?))?\\): Peace treaty concluded\\.$", ["a", "b"]],
	["treaty_ended", "^(?:Diplomacy|Galactic news) \\((.+?)(?: / (.+?))?\\): (.+?) (?:expired|dissolved.*|terminated due to war)\\.?$", ["a", "b", "what"]],
	["proposal_ended", "^(?:Diplomacy|Galactic news) \\((.+?)(?: / (.+?))?\\): (.+?) proposal (?:rejected by target|lapsed without answer|cancelled by the declaration of war)\\.$", ["a", "b", "what"]],
	["gift_delivered", "^(?:Diplomacy|Galactic news) \\((.+?)(?: / (.+?))?\\): (?:Gift|Tribute) of (\\d+) credits (?:delivered|paid)\\.$", ["a", "b", "n"]],
	["victory", "^(.+) campaign victory\\.$", ["who"]],
	["defeat", "^(.+) campaign defeat\\.$", ["who"]],
	["event_plague", "^(?:The )?[Pp]lague(?: at (?:system )?([^:.]+))?:? ?(.*)$", ["sys", "detail"]],
	["event_supernova", "^Supernova warning:? ?(.*)$", ["detail"]],
	["event_derelict", "^Derelict vessel([^:]*):? ?(.*)$", ["qual", "detail"]],
	["event_unrest", "^(?:Labor unrest|Strike|The strike)(?: at (?:system )?([^:.]+))?:? ?(.*)$", ["sys", "detail"]],
	["event_breakthrough", "^Scientific breakthrough([^:]*):? ?(.*)$", ["qual", "detail"]],
	["event_trade", "^(?:Trade boom|The trade boom)([^:]*):? ?(.*)$", ["qual", "detail"]],
	["event_rift", "^Subspace rift([^:]*):? ?(.*)$", ["qual", "detail"]],
]

static var _regex := {}

static func _rx(i: int) -> RegEx:
	if not _regex.has(i):
		var r := RegEx.new()
		r.compile(str(CLASSIFIERS[i][1]))
		_regex[i] = r
	return _regex[i]

## {kind, args} for a core line, or {kind: "", args: {}} when no pattern knows it.
static func classify(text: String) -> Dictionary:
	for i in CLASSIFIERS.size():
		var m := _rx(i).search(text)
		if m == null:
			continue
		var names: Array = CLASSIFIERS[i][2]
		var args := {}
		for k in names.size():
			args[names[k]] = m.get_string(k + 1).strip_edges()
		return {"kind": CLASSIFIERS[i][0], "args": args}
	return {"kind": "", "args": {}}

# ---------------------------------------------------------------------------------------------
# The phrase table. PHRASES[kind] = six arrays (voice order above), several variants each.
# {name} slots are filled from the classified arguments; {sys} etc. are already resolved to names.
# ---------------------------------------------------------------------------------------------
const PHRASES := {
	"station_lost": [
		["The {owner} station at {place} was destroyed by the {by}. We mourn the crew, and we will not forget it.", "Station lost at {place}: destroyed by the {by}. I will write to the families myself."],
		["The {owner} station at {place} was destroyed by the {by}. Its defenders died well.", "{place} is rubble: the {by} destroyed the {owner} station. Hear the names."],
		["The {owner} station at {place} is gone, destroyed by the {by}. Someone will account for it.", "Station at {place} destroyed by the {by}. Do not let the details travel."],
		["The {owner} station at {place} has been destroyed by the {by}. Casualty lists are being prepared.", "Station lost at {place}, destroyed by the {by}. An inquiry is ordered."],
		["The {owner} station at {place} was destroyed by the {by}. That is a great deal of inventory, gone.", "Station at {place} destroyed by the {by}. File the insurance claim."],
		["The {owner} station at {place} was destroyed by the {by}. It will be replaced.", "Station at {place} lost to the {by}."],
	],
	"colony_founded": [
		["We have raised a new colony at {sys}. The first settlers are already at work.", "A fresh world, {sys}, now carries our flag, and the first crews are breaking ground.", "{sys} is settled. A small step, but one more place where we are expected."],
		["{sys} is ours. We planted the banner there and the settlers will build.", "A new holding at {sys}. Let any rival who wants it come and say so.", "We have claimed {sys}. The first warriors-turned-farmers are digging in."],
		["A colony at {sys} is established. Its location will be useful, and its purpose need not be advertised.", "{sys} is settled quietly. The Senate will be told what it needs to know.", "We hold {sys} now. Whoever watches it will see only farmers."],
		["A new settlement has been registered at {sys}. Production begins at the next reckoning.", "{sys} is now under Union administration. The census begins at once.", "Central Command is informed: {sys} has been settled and its quotas set."],
		["Entry: a new holding at {sys}. Overhead is low and the returns look promising.", "We have staked a claim on {sys}. A claim that pays is the only kind worth staking.", "{sys} is settled. Every new colony is a customer waiting to happen."],
		["{sys} has been settled. It will serve.", "A colony is established at {sys}. Its inhabitants have been informed of their purpose.", "The Founders' order extends to {sys}."],
	],
	"discovered": [
		["Our survey has charted {sys}. There is always more out here than the maps admit.", "We have catalogued {sys} for the first time. The records are updated.", "Sensors resolve a new system, {sys}. Worth a closer look when time allows."],
		["Our scouts have found {sys}. Whatever lives there will hear of us.", "{sys} is on our charts now. A new hunting ground.", "The scouts return: {sys} lies open to the Empire."],
		["Our eyes have found {sys}. It may serve us, or someone may be using it already.", "{sys} is charted. Information is its own kind of wealth.", "A new system, {sys}, enters our private maps."],
		["{sys} has been surveyed and added to the registry of the Union.", "A system designated {sys} is added to the strategic survey.", "The survey of {sys} is complete. The file goes to Command."],
		["We have found {sys}. Somebody, somewhere, will pay for the map.", "{sys} is charted, and I have already priced the survey data.", "A new system, {sys}. New lanes mean new customers."],
		["{sys} is observed.", "The system {sys} is now within the Founders' knowledge.", "Observation of {sys} is complete."],
	],
	"first_contact": [
		["We have made first contact with the {who}. Diplomatic channels are open, and I intend to proceed with care.", "Contact with the {who}. I will remind the crew that first impressions last.", "The {who} are on our long-range scanners and now on our comm channel. Let us hope for good manners on all sides."],
		["We have met the {who}. Let us see whether they are worth fighting or worth fearing.", "Contact with the {who}. They have answered our hail, which is more than most do.", "The {who} are before us. The Empire will take their measure."],
		["Contact with the {who} is established. They know less about us than we intend to learn of them.", "The {who} have been located. Say nothing that we cannot deny later.", "We have met the {who}. Their motives are, as yet, unclear."],
		["Contact with the {who} is logged. Their capabilities are to be assessed immediately.", "First contact with the {who}, recorded and reported to Command.", "The {who} are noted in the registry. The Order is already preparing a file."],
		["First contact with the {who}. Potential customers, potential rivals, and in either case, potential margin.", "We have met the {who}. I opened with a greeting and an offer, in that order.", "The {who} are in range. I trust their credit is good."],
		["The {who} are located. They are not yet of consequence.", "Contact with the {who}. The Founders will be told of their weaknesses.", "The {who} have been identified. Their submission is a matter of time."],
	],
	"battle": [
		["Engagement at sector {x},{y}. We lost {lost}, and {fled} withdrew. I will remember every one of them.", "Battle joined at sector {x},{y}: {lost} ships lost, {fled} broke off. A hard day for the fleet.", "There was fighting at sector {x},{y}. {lost} ships did not return and {fled} fell back. We will review what went wrong."],
		["Battle at sector {x},{y}. {lost} ships were lost and {fled} fled. Their names are spoken tonight.", "We fought at sector {x},{y}. {lost} ships fell, {fled} withdrew. Those who withdrew will answer for it.", "Blood was spilled at sector {x},{y}: {lost} lost, {fled} gone from the line. It is recorded."],
		["There was an engagement at sector {x},{y}. We lost {lost} ships and {fled} withdrew. It will not be spoken of widely.", "Hostilities at sector {x},{y}: {lost} ships lost, {fled} withdrawn. The Senate will hear a shorter account.", "Sector {x},{y}: {lost} ships lost, {fled} pulled back. Someone was careless."],
		["Combat at sector {x},{y}. Losses: {lost} ships. Withdrawn: {fled}. Casualty reports are being compiled.", "Battle report, sector {x},{y}: {lost} vessels lost, {fled} retired. The matter is under review.", "The fleet engaged at sector {x},{y}. {lost} ships lost, {fled} withdrew. An inquiry will follow."],
		["Fighting at sector {x},{y}. We lost {lost} ships and {fled} ran for it. Write that off, and sharpen the next quarter.", "A very expensive afternoon at sector {x},{y}: {lost} ships lost, {fled} withdrawn. War is good for business only if you are selling.", "Sector {x},{y}: {lost} ships gone, {fled} retreated. I have asked for the repair estimates."],
		["Engagement at sector {x},{y}. {lost} lost, {fled} withdrew. Replacements are being grown.", "Battle at sector {x},{y}. Losses are acceptable. Losses are always acceptable.", "{lost} ships were lost at sector {x},{y}, and {fled} retreated. The retreating will not be forgiven."],
	],
	"threat_alert": [
		["Starfleet Intelligence warns that {what} has entered the quadrant. I have put the fleet on alert and asked every colony to review its defences.", "{what} is reported inside the quadrant. We prepare as best we can."],
		["The warning sensors report {what} in the quadrant. Let the warriors sharpen their blades for something worth the effort.", "{what} has come among us. Glory awaits whoever stops it."],
		["Our listening posts report {what} in the quadrant. The Senate need not be told yet.", "{what} is moving in the quadrant. Quietly, assess the risk to our worlds."],
		["{what} is reported inside the quadrant. All colonies are to review their defences.", "Central Command notes {what} in the quadrant. Readiness is raised."],
		["{what} has been sighted in the quadrant. That is bad for trade. Insure what you can.", "A warning: {what} is in the quadrant. Reroute the freighters."],
		["{what} is in the quadrant. The Founders are informed.", "{what} has entered the quadrant. It is a variable to be managed."],
	],
	"threat_dead": [
		["The {what} was destroyed at sector {x},{y}. I never thought I would write those words, and I am grateful to every crew that made them true.", "The {what} is gone from sector {x},{y}. The quadrant is safer tonight."],
		["The {what} has fallen at sector {x},{y}! Tell the story at every table.", "The {what} is dead at sector {x},{y}. A worthy kill."],
		["The {what} is destroyed at sector {x},{y}. That ends one problem; others remain.", "Sensors confirm the {what} destroyed at sector {x},{y}."],
		["The {what} was destroyed at sector {x},{y}. The threat is closed in the registry.", "Confirmed: the {what} is destroyed at sector {x},{y}."],
		["The {what} was destroyed at sector {x},{y}. Salvage rights are being examined.", "The {what} at sector {x},{y} is gone. The market will be relieved."],
		["The {what} was destroyed at sector {x},{y}.", "The {what} no longer exists at sector {x},{y}."],
	],
	"threat_battle": [
		["We engaged the {what} at sector {x},{y}. We lost {lost} ships and {fled} withdrew. I will remember every one of them.", "Battle with the {what} at sector {x},{y}: {lost} ships lost, {fled} broke off."],
		["We fought the {what} at sector {x},{y}. {lost} ships fell, {fled} withdrew. It was a fight worth the singing.", "The {what} met our fleet at sector {x},{y}: {lost} lost, {fled} fled the line."],
		["Contact with the {what} at sector {x},{y} ended in battle. We lost {lost} ships, {fled} withdrew. The Senate will hear a shorter account.", "Sector {x},{y}: {lost} ships lost against the {what}. {fled} pulled back."],
		["Combat with the {what} at sector {x},{y}. Losses: {lost}. Withdrawn: {fled}. An inquiry will follow.", "The fleet engaged the {what} at sector {x},{y}. {lost} lost, {fled} retired."],
		["A costly afternoon against the {what} at sector {x},{y}: {lost} ships lost, {fled} withdrew. Add it to the loss column.", "{lost} ships gone to the {what} at sector {x},{y}; {fled} ran. I have asked for the estimates."],
		["Engagement with the {what} at sector {x},{y}. {lost} lost, {fled} withdrew.", "The {what} at sector {x},{y} took {lost} ships from us. {fled} retreated."],
	],
	"assimilated": [
		["{sys} has been assimilated by the Borg. There is nothing to say that does not feel too small. I am sorry.", "The Borg took {sys} ({owner}). We will not forget its people."],
		["{sys} has been taken by the Borg. We resist. Resistance is not futile; it is the whole of honour.", "The Borg have consumed {sys}. Let them come to the next one."],
		["The Borg have assimilated {sys}. The Senate will want someone to blame, and there is no one.", "{sys} is lost to the Borg ({owner}). Seal what remains of the records."],
		["{sys} has been assimilated. The system is struck from the registry.", "The Borg assimilated {sys} ({owner}). Evacuation plans for neighbours are in force."],
		["{sys} was assimilated. Everything on it is a total loss. Everything.", "The Borg have taken {sys} ({owner}). I have never seen a worse quarter."],
		["{sys} was assimilated by the Borg. Their adaptation is noted. It will not be permitted to continue.", "{sys} is lost to the Collective."],
	],
	"bombard": [
		["The {what} {verb} {sys}. We are sending what help we can.", "{sys} is under attack by the {what}. Hold on."],
		["The {what} {verb} {sys}. Send ships; the people there are ours.", "{sys} burns under the {what}. Answer it."],
		["The {what} {verb} {sys}. The losses will be reported when they can be counted.", "{sys} is under attack by the {what}. Quietly reinforce."],
		["The {what} {verb} {sys}. Defence batteries are engaged.", "{sys} is attacked by the {what}. Command has been notified."],
		["The {what} {verb} {sys}. Every hour costs us revenue.", "{sys} is under assault by the {what}. Please do something."],
		["The {what} {verb} {sys}. It will be corrected.", "{sys} is attacked by the {what}."],
	],
	"battle_truce": [
		["At sector {x},{y} a truce was hailed before the fighting got out of hand. We lost {lost} ships; {fled} withdrew. I count that a victory of a kind.", "A truce was called at sector {x},{y} after {lost} ships were lost. The guns are quiet for now."],
		["At sector {x},{y} the enemy begged for a truce after {lost} ships were lost. We granted it, for now.", "A truce was hailed at sector {x},{y}. {fled} ran, {lost} died. We heard them out."],
		["At sector {x},{y} a truce was arranged before matters worsened. We lost {lost} ships. Convenient for everyone, for now.", "A truce at sector {x},{y}: {lost} ships lost, {fled} withdrawn. Such arrangements are rarely permanent."],
		["A truce was hailed at sector {x},{y} and accepted, pending instructions. {lost} ships lost.", "The engagement at sector {x},{y} ended under a hailed truce. {lost} vessels lost, {fled} withdrawn."],
		["A truce at sector {x},{y}, after {lost} ships lost. Cheaper than finishing the fight, if less satisfying.", "Sector {x},{y}: truce hailed, {lost} ships lost, {fled} withdrawn. Peace is a fine way to protect inventory."],
		["A truce was hailed at sector {x},{y}. It was permitted. It will not last.", "The fighting at sector {x},{y} ended under a truce. {lost} ships lost."],
	],
	"system_taken": [
		["{sys} has fallen to the {by}. I am sorry for the people there, and we will do what we can.", "The {by} have taken {sys}. It is a loss we do not accept."],
		["{sys} has fallen to the {by}. They will answer for it.", "The {by} hold {sys}. Not for long."],
		["{sys} has passed to the {by}. The Senate will be displeased and then occupied with blaming someone.", "The {by} control {sys} now. The Tal Shiar had no warning, or says it had none."],
		["{sys} is under {by} control. The prefect has been reassigned, and the family of the prefect, too.", "The {by} have occupied {sys}. A reconquest is being drawn up."],
		["{sys} was seized by the {by}. I want an inventory of everything they did not get.", "The {by} have taken {sys}. That cost us dearly. It will cost them."],
		["{sys} is held by the {by}. This is temporary.", "{sys} is lost to the {by}. A correction will follow."],
	],
	"system_won": [
		["{sys} is ours. I hope the people there will see the fleet as a relief and not a burden.", "{sys} has come under our protection. Order will be restored there before anything else."],
		["{sys} is ours! The banner flies over it, and the songs begin tonight.", "We have taken {sys}. Its defenders fought well, and now they are ours to command."],
		["{sys} is ours. It was a clean, quiet affair, as it should be.", "{sys} has come under our standard. A useful piece placed in a long game."],
		["{sys} is secured. The registry has been amended, and the population informed of its new obligations.", "Central Command is informed that {sys} is now Union territory."],
		["{sys} is ours. New territory, new customers. I will inspect the books personally.", "We hold {sys}. Everything on it is now our property, and I intend to count it."],
		["{sys} has been absorbed. Resistance was noted and no longer exists.", "{sys} is under the Founders' order."],
	],
	"invasion_blocked": [
		["Our landing at {sys} was turned back by the defending ships. We will need more fleet before we try again.", "The invasion of {sys} could not proceed; its defenders held the orbit."],
		["Our landing at {sys} was stopped by the ships above it. The warriors are eager to try again.", "{sys}'s orbit is held by the enemy. Send more ships; we will clear it."],
		["The landing at {sys} was refused by its defenders. We should not have looked surprised.", "The attempt at {sys} was blocked. Quietly withdraw the transports."],
		["The landing at {sys} was prevented by defending vessels. The operation is suspended.", "Invasion of {sys} blocked. Fleet assets will be reassessed."],
		["The landing at {sys} was blocked and the transports returned unused. Money wasted on fuel.", "Our troops never reached {sys}. Mark that as a loss on the ledger."],
		["The invasion of {sys} was obstructed. The obstruction will be removed.", "{sys}'s defenders hold the orbit. Not for long."],
	],
	"invasion_failed": [
		["Our troops were repulsed at {sys}. We will honour those who fell and learn from this.", "The landing at {sys} failed. I take responsibility for the plan."],
		["The assault on {sys} failed. Those who fell did so well. The rest will try again.", "We were thrown back at {sys}. It is a stain we will wash out."],
		["The landing at {sys} failed. Whoever planned it will explain why.", "{sys} did not fall. Do not speak of this at the next session."],
		["Ground forces failed at {sys}. The commander has been recalled for examination.", "The invasion of {sys} failed. Casualty figures are being withheld."],
		["Our troops lost at {sys}. Please add the funeral costs to the loss column.", "{sys} held. A very poor return on a very large outlay."],
		["The assault on {sys} failed. The unit responsible will be replaced.", "{sys} was not taken. A new unit will be grown."],
	],
	"repaired": [
		["{ship} was repaired at {place} and is ready for duty again.", "The engineers have finished with {ship} at {place}. She is fit to fly."],
		["{ship} is mended at {place}. Her crew are eager for another fight.", "{ship} has been patched at {place}. The scars are honourable."],
		["{ship} is repaired at {place}. She was never really in trouble, you understand.", "{ship} is whole again at {place}, with no questions asked."],
		["{ship} has completed repairs at {place} and is returned to the line.", "Repairs to {ship} at {place} are done. The work order is closed."],
		["{ship} is repaired at {place}. The bill is in the folder; try to reduce the next one.", "{ship} is back in service at {place}. Billable hours noted."],
		["{ship} is restored at {place}.", "{ship} is repaired at {place} and returns to service."],
	],
	"ship_launched": [
		["{ship} is commissioned from {sys}. May she serve well and come home.", "We have launched {ship} at {sys}. I will join her crew's first watch myself."],
		["{ship} is launched from {sys}. Let her find an enemy.", "{ship} leaves the yards at {sys}. The warriors cheer."],
		["{ship} has left the yards at {sys} without ceremony. She will be useful.", "{ship} is launched at {sys}. Her orders are sealed."],
		["{ship} is launched at {sys} and assigned to the fleet. The shipyard hit its quota.", "The yards at {sys} have delivered {ship}. Her crew is vetted."],
		["{ship} is delivered at {sys}, on budget and ahead of the rival bid.", "{ship} has left {sys}. Please mind the paint; it was expensive."],
		["{ship} is produced at {sys}. She is expendable.", "{ship} leaves {sys}. She will obey."],
	],
	"research_done": [
		["Our science teams report a breakthrough: {field} has reached level {lvl}. The benefits begin next turn.", "{field} is now at level {lvl}. Our scientists have earned a good night's sleep.", "A new understanding in {field}: level {lvl}. I look forward to seeing what it makes possible."],
		["The forge has finished {field} at level {lvl}. May it be put to good use soon.", "{field}, level {lvl}. Our engineers earned a feast.", "Our smiths have taken {field} to level {lvl}. The Empire grows sharper."],
		["{field} reaches level {lvl}. The directorate has not said how, which is the point.", "Our researchers raised {field} to level {lvl}. The rest of the galaxy need not know.", "{field}, level {lvl}. Another quiet advantage."],
		["{field} has been advanced to level {lvl}. The ministry meets its target.", "Level {lvl} in {field} is achieved. The scientists will be rewarded, within reason.", "Research in {field} reaches level {lvl}. Output exceeds the requirement."],
		["{field} at level {lvl}. It cost a fortune. Let us find the buyer.", "A new level, {lvl}, in {field}. Every advance is an asset, and assets should be sold or rented.", "{field} reaches level {lvl}. Acquisitions asks me to note that the patents are ours."],
		["{field} is advanced to level {lvl}. The Founders will use it.", "Level {lvl} in {field}. The geneticists were not praised, because they were not required to be.", "Our research in {field} is complete at level {lvl}."],
	],
	"terraform_done": [
		["Terraforming of {planet} is complete, and its habitable capacity now stands at {cap}. A world a little kinder to its people.", "{planet} has been reshaped. It can now sustain {cap}. The engineers have my thanks."],
		["{planet} has been reshaped and now supports {cap}. A good place to raise a warrior.", "The works at {planet} are done. Capacity is {cap}. Let it fill with children."],
		["{planet} is transformed. Capacity is now {cap}. Most would not notice the difference.", "The reshaping of {planet} is finished, and it supports {cap}. Quietly done."],
		["{planet} has been terraformed to a capacity of {cap}. The population will be increased accordingly.", "Terraforming at {planet} concluded. Capacity: {cap}. Quotas revised."],
		["{planet} is terraformed to a capacity of {cap}. Real estate is only worth what you do to it.", "The work at {planet} is done, capacity {cap}. The land value has gone up. So will the rent."],
		["{planet} is reshaped. Capacity {cap}.", "Terraforming of {planet} is complete. It is ready for use."],
	],
	"survey": [
		["{ship} has surveyed the {feature} at {place} and gained {gain} {field} research{extra}. A satisfying piece of science.", "A study of the {feature} at {place} by {ship}: +{gain} {field} research{extra}."],
		["{ship} studied the {feature} at {place}: +{gain} {field} research{extra}. The scholars may now stop complaining.", "{ship} dared the {feature} at {place} and returned with +{gain} {field} research{extra}."],
		["{ship} observed the {feature} at {place} and brought back +{gain} {field} research{extra}. Nobody noticed.", "{ship} took readings of the {feature} at {place}: +{gain} {field} research{extra}."],
		["{ship} surveyed the {feature} at {place}: +{gain} {field} research{extra}. The data is filed.", "Survey by {ship}, {feature} at {place}: +{gain} {field} research{extra}."],
		["{ship} surveyed the {feature} at {place}: +{gain} {field} research{extra}. Cheaper than buying it.", "{ship} came back from the {feature} at {place} with +{gain} {field} research{extra}. Mark it as profit."],
		["{ship} surveyed the {feature} at {place}: +{gain} {field} research{extra}.", "The {feature} at {place} is catalogued by {ship}: +{gain} {field} research{extra}."],
	],
	"feature_gone": [
		["The {feature} at {place} has collapsed, and the sector is passable again. Nature changes our maps for us.", "The {feature} at {place} is gone. Navigation will be glad of it."],
		["The {feature} at {place} has collapsed. The route is open, and so is the hunt.", "{place} is clear: the {feature} has faded."],
		["The {feature} at {place} has collapsed. A convenient road is open.", "{place} is passable again. The {feature} is no more."],
		["The {feature} at {place} has collapsed. The sector is open to traffic.", "{place} has cleared. The {feature} no longer obstructs."],
		["The {feature} at {place} is gone. A new trade lane, free of charge.", "The {feature} at {place} has collapsed. Passage is open, and so are the tariffs."],
		["The {feature} at {place} has collapsed. The way is open.", "{place} is clear."],
	],
	"outpost_done": [
		["Our outpost at {place} is complete. Its sensors and supply range extend our reach.", "The outpost at {place} is operational. A quiet place to stop and resupply."],
		["The outpost at {place} stands. Our ships will drink there.", "{place} now has a fort. Let it be tested."],
		["The outpost at {place} is operational. It sees more than it says.", "An outpost at {place} is complete. Its range is useful to us and nobody else."],
		["The outpost at {place} is complete and operating. Supply and sensor range extended.", "Outpost construction at {place} is finished."],
		["The outpost at {place} is built. It will pay for itself in fuel sales alone.", "Outpost done at {place}. Every depot is a store with the lights on."],
		["The outpost at {place} is established.", "Outpost operational at {place}."],
	],
	"outpost_abandoned": [
		["Fleet {fleet}'s outpost at {place} was set aside ({why}). We may return to it.", "The outpost work at {place} was abandoned ({why}). Plans change."],
		["Fleet {fleet} abandoned the outpost at {place} ({why}). A battle came first.", "No outpost at {place} for now ({why})."],
		["Fleet {fleet} stopped work at {place} ({why}). Nothing was lost that we cannot recover.", "The outpost at {place} was dropped ({why})."],
		["Fleet {fleet} suspended the outpost at {place} ({why}). The project is recorded as incomplete.", "Outpost work at {place} abandoned ({why})."],
		["Fleet {fleet} left the outpost at {place} unfinished ({why}). Sunk costs, I fear.", "No outpost at {place} yet ({why}). Annoying, not fatal."],
		["Fleet {fleet} abandoned the outpost at {place} ({why}).", "Outpost at {place} suspended ({why})."],
	],
	"route_paused": [
		["Fleet {fleet} has stopped short: no supplied path onward. We will need to extend our reach or choose another course.", "Fleet {fleet} is holding position; there is no supplied route ahead."],
		["Fleet {fleet} stands idle; no supplied route lies ahead. Give it a road or give it a target.", "Fleet {fleet} has stopped, short of fuel and short of patience."],
		["Fleet {fleet} has halted. The route is unsupplied. Someone failed to plan.", "Fleet {fleet} awaits orders at the edge of our supply."],
		["Fleet {fleet}'s route is paused: no supplied path. A request for supply extension is filed.", "Fleet {fleet} has halted for lack of a supplied route."],
		["Fleet {fleet} has stopped; there is no supplied route. Every idle day costs us.", "Fleet {fleet} is stuck without supply. Find the cheapest way to move it."],
		["Fleet {fleet} has stopped. No supply path.", "Fleet {fleet} is halted at the edge of supply."],
	],
	"windfall": [
		["Good news from {sys}: a trade windfall adds {n} credits. Commerce helps people thrive.", "A fortunate week at {sys}: +{n} credits."],
		["The traders of {sys} have brought {n} credits to our coffers. Let them feast.", "A windfall of {n} credits at {sys}. Not glorious, but welcome."],
		["{sys} reports an unexpected {n} credits. I would like to know why.", "Trade at {sys} has yielded {n} credits more than expected."],
		["A trade windfall at {sys} adds {n} credits to the treasury. Accounted for.", "{sys} reports a surplus of {n} credits. The quota is exceeded."],
		["{sys}: windfall, +{n} credits. This is what I am for.", "A lucky break at {sys}: +{n} credits. The first rule is to stay open for business."],
		["{sys} has yielded {n} credits. It is recorded.", "A windfall of {n} credits arrives from {sys}."],
	],
	"celebration": [
		["There is a civic celebration at {sys}, and morale is up by {n}. Good for a colony to enjoy itself.", "{sys} is holding a festival. Morale +{n}."],
		["{sys} has made a feast, and morale rises by {n}. The songs are loud.", "A celebration at {sys}: morale +{n}. Let them have it."],
		["The people of {sys} are celebrating, and morale is up {n}. I trust the celebration will end on schedule.", "{sys} celebrates; morale +{n}. They should be watched, in the nicest possible way."],
		["{sys} reports a state celebration, and morale is raised by {n}. Attendance was compulsory.", "A holiday at {sys}: morale +{n}."],
		["Festival at {sys}: morale +{n}. Happy workers, and a very good day for the food stalls.", "{sys} is celebrating. Morale up {n}, concessions up more."],
		["{sys} celebrates. Morale +{n}. It has been permitted.", "Celebration at {sys}: morale +{n}."],
	],
	"unrest": [
		["Civil unrest at {sys}; morale is down {n}. I would rather listen than lecture.", "{sys} is restless, and morale has dropped by {n}. We should find out why."],
		["{sys} is in turmoil, morale down {n}. The people lack discipline.", "Unrest at {sys}: morale -{n}. Someone has dishonoured the colony."],
		["Unrest at {sys}; morale is down {n}. Find who is behind it.", "{sys} is unsettled. Morale -{n}. Informants are being asked."],
		["Civil disorder at {sys}; morale is down {n}. Security has been dispatched.", "Unrest at {sys}. Morale -{n}. Order will be restored, and examples will be made."],
		["There is unrest at {sys}; morale is down {n}. Unhappy workers are bad for the margin.", "{sys}: morale -{n}. Labour trouble is almost always a pricing problem."],
		["Unrest at {sys}. Morale -{n}. It will be corrected.", "{sys} is disturbed. Morale -{n}."],
	],
	"pop_up": [
		["{sys} has grown by {n}. The governor ({gov}) has assigned the new citizens.", "The population at {sys} is up {n}, and the governor ({gov}) has put them to work."],
		["{sys} grows by {n}. The governor ({gov}) has put them to work.", "New hands at {sys}: +{n}. They are assigned."],
		["{sys} is up by {n}. The new citizens are assigned ({gov}).", "A small increase at {sys}: +{n}, placed by the governor ({gov})."],
		["The population of {sys} has increased by {n}, assigned under policy ({gov}).", "{sys}: +{n} citizens, assigned ({gov})."],
		["{sys} is up {n} heads, assigned ({gov}). More workers, more customers.", "Population +{n} at {sys} ({gov}). Growth is profit with a pulse."],
		["{sys} increases by {n}, assigned ({gov}).", "{sys}: +{n}, assigned."],
	],
	"pop_unassigned": [
		["{sys} has grown by {n}. The new citizens are waiting for work; they need assigning.", "The population at {sys} is up {n}, and nobody has found them a job yet."],
		["{sys} has grown by {n}. The new hands are idle; give them tasks.", "New citizens at {sys}: +{n}. Idle hands are a danger."],
		["{sys} is up by {n}, unassigned. Idle people talk.", "{sys}: +{n} unassigned citizens."],
		["{sys}: population +{n}, unassigned. The workforce plan needs revision.", "{sys} reports +{n} citizens without assignment."],
		["{sys}: +{n} unassigned. Every idle worker is an unpaid invoice.", "Population +{n} at {sys}, no work for them. Fix it."],
		["{sys}: +{n}, unassigned.", "{sys} has {n} citizens awaiting orders."],
	],
	"pop_down": [
		["{sys} has lost {n} to a shortage of food. We must feed these people.", "Hunger at {sys}: population down {n}. This should never have happened."],
		["{sys} loses {n} to hunger. The warriors will starve with the weak. Send food.", "{sys}: -{n} from famine. A disgrace."],
		["{sys} has lost {n} to hunger. Someone will be asked about the harvest.", "{sys}: -{n}, starvation. Inconvenient."],
		["Starvation at {sys}: population -{n}. The agricultural ministry will explain.", "{sys} lost {n} citizens to food shortage. An inquiry opens."],
		["{sys} is down {n} from food shortage. Fewer workers, fewer sales. Buy grain.", "Hunger at {sys}: -{n} population. A needless loss, and bad business."],
		["{sys} loses {n}. Food is insufficient.", "{sys}: -{n}. The shortage will be corrected."],
	],
	"governor_warning": [
		["The governor at {sys} advises that {why}. I will look into it.", "A warning from {sys}'s governor: {why}."],
		["The governor at {sys} reports {why}. Fix it.", "{sys}: {why}. Strengthen it."],
		["The governor at {sys} notes that {why}. Quietly arranged corrections are best.", "{sys}'s governor reports: {why}."],
		["Governor's warning at {sys}: {why}. A response is required.", "{sys}: {why}. The prefect has been informed."],
		["{sys}'s governor warns that {why}. A problem is an unsold opportunity.", "{sys}: {why}. It will cost us."],
		["{sys}: {why}.", "A governor's note at {sys}: {why}."],
	],
	"construction_paused": [
		["Construction at {sys} is paused. {why}. We will resolve it as quickly as we can.", "The engineers at {sys} cannot proceed: {why}."],
		["Work at {sys} has stopped. {why}. Idle hands help no one.", "Building at {sys} is held up. {why}."],
		["Construction at {sys} is on hold. {why}. We will not say so publicly.", "Work at {sys} has paused: {why}."],
		["Construction at {sys} is suspended. {why}. A corrective order is required.", "Production at {sys} is halted: {why}."],
		["Work at {sys} is paused. {why}. Every idle day costs money.", "{sys}: construction stopped. {why}."],
		["Construction at {sys} is suspended. {why}.", "{sys}: production paused. {why}."],
	],
	"construction_done": [
		["The engineers report that {what} at {sys} is complete.", "{sys} has finished {what}. The crews have earned a rest.", "Chief Engineer's report: {what} at {sys} is finished and operational."],
		["{what} at {sys} is finished. Build more, and bigger.", "{sys} has completed {what}. Let it be used well."],
		["{what} at {sys} is complete. Visit it only if you were told to.", "{sys} has finished {what}. It was built without fuss."],
		["{what} at {sys} is complete and operational. The schedule was met.", "Construction of {what} at {sys} is concluded. Filed."],
		["{what} at {sys} is built. Costs recovered in due course, I hope.", "{sys} has finished {what}. Let us see what it earns."],
		["{what} at {sys} is complete.", "{sys}: {what} is built."],
	],
	"starbase_work": [
		["{sys} has put {n} industry into the starbase at {place} ({done} of {cost}).", "The starbase at {place} is {done} of {cost} complete, thanks to {sys}."],
		["{sys} sends {n} industry to the starbase at {place} ({done} of {cost}). The fortress rises.", "The starbase at {place}: {done} of {cost}."],
		["{sys} feeds {n} industry into the starbase at {place} ({done} of {cost}).", "The starbase at {place}: {done} of {cost}. Slowly."],
		["{sys}: {n} industry assigned to the starbase at {place} ({done} of {cost}).", "The starbase at {place} is {done} of {cost} complete."],
		["{sys} puts {n} industry into the starbase at {place} ({done} of {cost}). An investment, not a cost.", "The starbase at {place}: {done} of {cost} paid in."],
		["{sys}: {n} to the starbase at {place} ({done} of {cost}).", "The starbase at {place}: {done} of {cost}, by the Founders' will."],
	],
	"megaproject_work": [
		["{sys} has given {n} industry to the {name} ({done} of {cost}). A long labour of ours.", "The {name} advances: {done} of {cost}, with {n} from {sys}."],
		["{sys} has given {n} industry to the {name} ({done} of {cost}). A great deed is under way.", "The {name} grows: {done} of {cost}."],
		["{sys} gives {n} industry to the {name} ({done} of {cost}). It will be a surprise.", "The {name}: {done} of {cost}, with {n} from {sys}."],
		["{sys} has assigned {n} industry to the {name} ({done} of {cost}). On schedule.", "{name}: {done} of {cost}. {sys} contributes {n}."],
		["{sys} puts {n} industry into the {name} ({done} of {cost}). The biggest investment we make.", "The {name}: {done} of {cost}."],
		["{sys} contributes {n} to the {name} ({done} of {cost}).", "{name}: {done} of {cost}."],
	],
	"sabotage_hit": [
		["Production at {sys} has been sabotaged. We will find who is responsible.", "Saboteurs struck {sys}. Security will be reviewed."],
		["Saboteurs hit {sys}. Cowards fight in the dark.", "{sys}'s works were sabotaged. Find them."],
		["{sys} was sabotaged. Not unexpected, and not forgotten.", "Production at {sys} was disrupted. The Tal Shiar is looking into it."],
		["Sabotage at {sys}. The Order will find the cell.", "Production at {sys} has been disrupted. Investigators are on site."],
		["Sabotage at {sys}. A disruption that costs me real money.", "{sys}'s production was sabotaged. I have a short list of suspects, each with a motive."],
		["{sys} was disrupted. The cause will be removed.", "Sabotage at {sys}. The saboteurs will be found."],
	],
	"intel_report": [
		["Our intelligence report: the {who} have {colonies} colonies and roughly {credits} credits. A useful picture.", "Intelligence on the {who}: {colonies} colonies, about {credits} credits."],
		["Our spies report the {who} hold {colonies} colonies and about {credits} credits. Soft targets.", "The {who}: {colonies} colonies, about {credits} credits."],
		["Report on the {who}: {colonies} colonies, roughly {credits} credits. Knowledge is a weapon.", "Our agents count {colonies} colonies and about {credits} credits for the {who}."],
		["Report: the {who} hold {colonies} colonies and approximately {credits} credits. Filed.", "Order assessment, {who}: {colonies} colonies, about {credits} credits."],
		["Intelligence on the {who}: {colonies} colonies, about {credits} credits. I would like to see their ledgers.", "The {who} have {colonies} colonies and roughly {credits} credits. A lot of money for such a thin plan."],
		["The {who} hold {colonies} colonies and about {credits} credits.", "Report on the {who}: {colonies} colonies, about {credits} credits."],
	],
	"intel_blueprints": [
		["Our agents have obtained blueprints that advance our research by {n} level(s). It is not a thing we are proud of, but it helps.", "Stolen blueprints advanced our research by {n} level(s)."],
		["Our spies brought back plans: research advances {n} level(s). Stealing is a skill.", "Plans stolen: research +{n} level(s)."],
		["Our people lifted blueprints that advance our research by {n} level(s). Nobody saw.", "Research advances {n} level(s) from stolen plans."],
		["Acquired blueprints advance our research by {n} level(s). The operation is closed.", "Stolen plans: +{n} research level(s)."],
		["Stolen blueprints: +{n} research level(s). It is not stealing if you only borrow without asking.", "We acquired blueprints worth {n} research level(s) of free work."],
		["Acquired plans advance research by {n} level(s).", "Research +{n} level(s) from taken blueprints."],
	],
	"intel_espionage_ok": [
		["Our espionage against the {who} succeeded{extra}. We now see a little further.", "We have gathered intelligence on the {who}{extra}."],
		["Our spies against the {who} succeeded{extra}. The weak will be known.", "Espionage on the {who} worked{extra}."],
		["Espionage against the {who} succeeded{extra}. Nobody is the wiser.", "Our operation against the {who} succeeded{extra}."],
		["Espionage on the {who} succeeded{extra}. The file is expanded.", "The Order's operation against the {who} succeeded{extra}."],
		["Our agents were successful against the {who}{extra}. Information is merchandise.", "Espionage on the {who} paid off{extra}."],
		["Espionage on the {who} succeeded{extra}.", "The {who} have been observed{extra}."],
	],
	"intel_sabotage_ok": [
		["Our sabotage against the {who} succeeded. I do not enjoy this part of command.", "Sabotage against the {who} worked."],
		["Sabotage against the {who} succeeded. A strike from the shadows.", "Our saboteurs struck the {who}. The cowards among us had a use."],
		["Our sabotage against the {who} succeeded. No fingerprints.", "The {who} have suffered a setback that nobody will explain."],
		["Sabotage against the {who} succeeded. The operation is closed.", "The {who} have been disrupted. Results are being compiled."],
		["Sabotage against the {who} succeeded. Their loss is nobody's gain but ours.", "We have set the {who} back. Bad for them, good for the books."],
		["Sabotage against the {who} succeeded.", "The {who} have been damaged."],
	],
	"intel_exposed": [
		["Our agents were exposed by the {who}. They will be missed.", "The {who} caught our agents. I regret this."],
		["Our agents were exposed by the {who}. Dishonourable of them to look.", "The {who} caught our spies. They will be avenged."],
		["Our agents were exposed by the {who}. It happens. Deny everything.", "The {who} have found our people. Deny it all."],
		["Our agents were exposed by the {who}. An inquiry will determine who was careless.", "Agents lost to the {who}. The cell is closed."],
		["Our agents were exposed by the {who}. A bad return on a long investment.", "The {who} caught our agents. Let us see what that will cost."],
		["Our agents were exposed by the {who}.", "Agents lost to the {who}."],
	],
	"intel_recalled": [
		["Our agents have been recalled from the {who}. It is safer.", "We have pulled our agents out of the {who}."],
		["Our agents are recalled from the {who}. They will fight another way.", "Our spies leave the {who}."],
		["Our agents have come home from the {who}. Do not ask what they saw.", "Agents recalled from the {who}."],
		["Agents recalled from the {who}. The deployment is concluded.", "Our agents are withdrawn from the {who}."],
		["Agents recalled from the {who}. The deployment was no longer profitable.", "We have withdrawn our agents from the {who}."],
		["Agents recalled from the {who}.", "The {who} are no longer watched."],
	],
	"intel_alert": [
		["Security alert: agents of the {who} have been detected. We will tighten our defences.", "Hostile agent activity from the {who}. I have asked Security for a full review."],
		["Agents of the {who} are loose among us. Hunt them.", "Security alert: the {who} are spying on us. They will regret it."],
		["The {who} have agents among us. Pretend to know nothing, and watch them.", "Hostile activity from the {who}. The Tal Shiar has been told."],
		["Alert: the {who} are running agents in our territory. The Order is mobilised.", "Hostile agent activity from the {who}. Arrests will follow."],
		["Security alert: the {who} have agents about. I have doubled the locks.", "Hostile agents from the {who}. Hold on to your wallets."],
		["Agents of the {who} are detected. They will be removed.", "Hostile activity from the {who}."],
	],
	"war_on_us": [
		["The {who} are now at war with us. I regret it, but the Federation will defend its own.", "War with the {who}. All hands to their stations. We will do what we must."],
		["The {who} are at war with us. At last, an honest enemy.", "War with the {who}. This is a good day."],
		["The {who} are at war with us. A pity, and not a surprise.", "War with the {who}. It was always a matter of time."],
		["The {who} are now at war with us. The Union will answer.", "War with the {who}. Central Command has issued orders."],
		["The {who} are at war with us. Prices will move, as will the people who sell shipping insurance.", "War with the {who}. Terrible for trade. Excellent for arms."],
		["The {who} are at war with us. They will be dealt with.", "War with the {who}. The outcome is not in doubt."],
	],
	"peace_with": [
		["The {who} have accepted peace. I hope it lasts.", "Peace with the {who}. The guns are quiet."],
		["The {who} accepted peace. It is the peace of the victor.", "Peace with the {who}. For now."],
		["The {who} accepted peace. It will serve us.", "Peace with the {who}. We shall see what they do with it."],
		["The {who} accepted peace. The matter is closed.", "Peace with the {who}. Orders suspended."],
		["The {who} accepted peace. At last, we can talk about something worthwhile.", "Peace with the {who}. Trade resumes."],
		["The {who} accepted peace. The Founders permit it.", "Peace with the {who}, by the Founders' leave."],
	],
	"offer_accepted": [
		["The {who} accepted our {what}. A good day for diplomacy.", "The {who} agreed to {what}. I will thank them in person."],
		["The {who} accepted {what}. They are wise.", "The {who} accept {what}. We will see how well they keep it."],
		["The {who} accepted {what}. As expected.", "The {who} have agreed to {what}. We will use it."],
		["The {who} have accepted {what}. The agreement is filed.", "The {who} accept {what}. Command notes it."],
		["The {who} accepted {what}. A deal is a deal, and I will have it in writing.", "The {who} accept {what}. Terms are favourable."],
		["The {who} accepted {what}. Expected.", "The {who} accept {what}."],
	],
	"offer_declined": [
		["The {who} declined {what}. We will try again, and in better words.", "The {who} refused {what}. I am disappointed, not discouraged."],
		["The {who} declined {what}. Cowards.", "The {who} refused {what}. Let them remember it."],
		["The {who} declined {what}. A signal in itself.", "The {who} refused {what}. We have not forgotten."],
		["The {who} declined {what}. The refusal is recorded.", "The {who} refused {what}. Further steps are being considered."],
		["The {who} declined {what}. Negotiation is just the start of a price.", "The {who} turned down {what}. We can wait for a better offer."],
		["The {who} declined {what}. This was unwise.", "The {who} refused {what}."],
	],
	"alliance_honored": [
		["The {ally} has honoured its alliance with the {defender} and declared war on the {aggressor}. The galaxy has grown more dangerous.", "The {ally} stood by the {defender} against the {aggressor}. Loyalty of that kind is rare."],
		["The {ally} stands with the {defender} against the {aggressor}. A fight worth joining.", "The {ally} has joined the {defender}'s war on the {aggressor}."],
		["The {ally} has joined the {defender} against the {aggressor}. Alliances are such inconvenient things.", "The {ally} backs the {defender} against the {aggressor}. Noted."],
		["The {ally} honours its pact with the {defender} and declares war on the {aggressor}. A new front.", "The {ally} has entered the war against the {aggressor}, at the side of the {defender}."],
		["The {ally} has honoured its alliance with the {defender} and gone to war with the {aggressor}. War enlarges everything but prudence.", "The {ally} joined the {defender} against the {aggressor}. Buy low."],
		["The {ally} joins the {defender} against the {aggressor}.", "The {ally} has declared war on the {aggressor}."],
	],
	"minor_joined_us": [
		["The {who} have accepted full membership. A new colony is established, and I welcome them.", "The {who} are with us now. Our family grows."],
		["The {who} have taken our banner as members. Strength is joined.", "The {who} are ours, and willingly. A good beginning."],
		["The {who} have accepted membership. A pleasing outcome, achieved without noise.", "The {who} are among our members now. They will be useful."],
		["The {who} have accepted membership. A colony is established.", "The {who} are now within the Union's order."],
		["The {who} have accepted membership. New market, new tariffs, same smile.", "The {who} are in. I have drawn up the contract."],
		["The {who} are absorbed into our order.", "The {who} accept the Founders' protection."],
	],
	"minor_joined": [
		["The {who} have joined the {by}. The balance of the quadrant shifts a little.", "The {who} now stand with the {by}."],
		["The {who} have joined the {by}. Let them see how they fare.", "The {who} have chosen the {by}."],
		["The {who} have joined the {by}. Their loyalty bears watching.", "The {who} have gone to the {by}."],
		["The {who} have joined the {by}. The ledger is amended.", "The {who} are now members of the {by}."],
		["The {who} have joined the {by}. Somebody out-bid us.", "The {who} have signed with the {by}."],
		["The {who} have joined the {by}.", "The {who} are taken by the {by}."],
	],
	"minor_seceded_us": [
		["The {who} have left us. Their loyalty wore thin, and for that we are partly to blame.", "The {who} have seceded. We must ask what we neglected."],
		["The {who} have broken from us. They will learn what that costs.", "The {who} have left. They are traitors."],
		["The {who} have seceded. We will remember who encouraged it.", "The {who} are gone. Someone was careless with them."],
		["The {who} have seceded from us. The loss is registered and the matter referred.", "The {who} are lost to the Union. An example will be made."],
		["The {who} have seceded. We lost a customer through poor service.", "The {who} are out. A bad quarter."],
		["The {who} have seceded. They will be recovered.", "The {who} have left. It will be remedied."],
	],
	"minor_seceded": [
		["The {who} have seceded from the {by}. Such moments change the map.", "The {who} have left the {by}."],
		["The {who} have left the {by}. They are free for the taking.", "The {who} have seceded from the {by}. Opportunity."],
		["The {who} have left the {by}. A useful weakness.", "The {who} are no longer held by the {by}."],
		["The {who} have seceded from the {by}. The situation is monitored.", "The {who} have left the {by}. Opportunities are being assessed."],
		["The {who} have seceded from the {by}. Open for offers.", "The {who} are loose from the {by}. A new bidder is welcome."],
		["The {who} have left the {by}.", "The {who} have seceded from the {by}."],
	],
	"minor_conquered": [
		["The {who} were conquered by the {by}. I am sorry for them.", "The {who} have fallen to the {by}. It is a sad chapter."],
		["The {who} were conquered by the {by}. Weakness invites it.", "The {by} have taken the {who}."],
		["The {who} have fallen to the {by}. A change in the balance.", "The {by} have swallowed the {who}."],
		["The {who} were conquered by the {by}. The registry is revised.", "The {by} have seized the {who}."],
		["The {who} were conquered by the {by}. A market lost.", "The {by} have absorbed the {who}. They had better be paying."],
		["The {who} were conquered by the {by}.", "The {who} are taken by the {by}."],
	],
	"proposal": [
		["The {who} propose a {what}. We have until turn {due} to reply, and I intend to give it proper thought.", "A proposal from the {who}: {what}. Reply by turn {due}."],
		["The {who} propose a {what}. Reply by turn {due}; make it short.", "The {who} offer a {what}. We have until turn {due}."],
		["The {who} propose a {what}. Reply by turn {due}, and say little.", "A proposal from the {who}: {what}. Answer by turn {due}."],
		["The {who} propose a {what}. A reply is due by turn {due}.", "A proposal received from the {who}: {what}. Response required by turn {due}."],
		["The {who} propose a {what}. We have until turn {due} to name a price.", "A proposal from the {who}: {what}. Let me read the small print before turn {due}."],
		["The {who} propose a {what}. Answer by turn {due}.", "A proposal from the {who}: {what}. Respond by turn {due}."],
	],
	"tribute": [
		["The {who} demand {n} credits in tribute. Reply by turn {due}. I will not be coerced, but I will think before I refuse.", "Tribute demanded by the {who}: {n} credits. Reply by turn {due}."],
		["The {who} demand {n} credits in tribute. Reply by turn {due}. They will find us costly.", "The {who} demand {n} credits. Let them come for it."],
		["The {who} want {n} credits in tribute. Reply by turn {due}.", "A demand from the {who}: {n} credits. Answer by turn {due}."],
		["The {who} demand {n} credits in tribute. Reply by turn {due}. The matter is under review.", "Tribute of {n} credits demanded by the {who}. Decide by turn {due}."],
		["The {who} demand {n} credits in tribute. Reply by turn {due}. The cheapest war is the one you pay to avoid.", "The {who} want {n} credits. We have until turn {due} to see if it is worth it."],
		["The {who} demand {n} credits in tribute. Reply by turn {due}.", "Tribute of {n} credits is demanded by the {who}."],
	],
	"gift": [
		["The {who} offer a gift of {n} credits. Reply by turn {due}. I appreciate the gesture.", "A gift from the {who}: {n} credits. Reply by turn {due}."],
		["The {who} offer us {n} credits. Reply by turn {due}. A tribute, at last.", "The {who} offer a gift of {n} credits. Answer by turn {due}."],
		["The {who} offer a gift of {n} credits. Reply by turn {due}. Gifts are never free.", "A gift of {n} credits from the {who}. What do they want?"],
		["The {who} offer a gift of {n} credits. Reply by turn {due}. It is recorded as a donation.", "A gift of {n} credits is offered by the {who}. Reply by turn {due}."],
		["The {who} offer a gift of {n} credits. Reply by turn {due}. A gift is just an investment with better manners.", "The {who} give us {n} credits. Reply by turn {due}, and keep the receipt."],
		["The {who} offer a gift of {n} credits. Reply by turn {due}.", "A gift of {n} credits from the {who}."],
	],
	"call_refused": [
		["We have refused the {who}'s call to arms against the {third}. I stand by it, though the {who} will trust us less.", "We declined to join the {who} against the {third}."],
		["We refused the {who}'s call to arms against the {third}. Let them think what they like.", "We have not joined the {who} against the {third}."],
		["We refused the {who}'s call to arms against the {third}. Their trust in us has fallen.", "We stayed out of the {who}'s war against the {third}."],
		["We refused the {who}'s call to arms against the {third}. Trust has fallen by 20.", "The call from the {who} against the {third} is refused."],
		["We refused the {who}'s call to arms against the {third}. Trust fell, and trust is expensive to rebuild.", "We declined the {who}'s war on the {third}. Their goodwill will cost us."],
		["We refused the {who}'s call to arms against the {third}.", "The {who} are refused."],
	],
	"call_to_arms": [
		["The {who} call on us to honour our alliance against the {third}. Declare war before the call falls due, or refuse it now; either choice carries a cost.", "An ally, the {who}, asks us to join them against the {third}. I will have to weigh what we owe."],
		["The {who} call us to arms against the {third}. Declare war, or refuse it now.", "The {who} summon us against the {third}. Our honour is in question."],
		["The {who} call on us to join them against the {third}. A hard choice: either way we lose something.", "The {who} want us against the {third}. Decide quietly."],
		["The {who} call on us to join them against the {third}. A decision is required before the call falls due.", "The {who} call for aid against the {third}. A refusal costs 20 trust."],
		["The {who} call on us to join them against the {third}. Declare war, or refuse and pay 20 in trust.", "The {who} ask for help against the {third}. Alliances are not free."],
		["The {who} call us against the {third}. Declare war, or refuse.", "The {who} summon us against the {third}."],
	],
	"war_declared": [
		["The {a} have declared war on the {b}. I hope it ends soon.", "War between the {a} and the {b}: the quadrant holds its breath."],
		["The {a} have declared war on the {b}. A good fight begins.", "The {a} go to war against the {b}. Let it be glorious."],
		["The {a} declared war on the {b}. Interesting.", "War: the {a} against the {b}. We shall see who benefits."],
		["The {a} have declared war on the {b}. The matter is recorded.", "War between the {a} and the {b}."],
		["The {a} declared war on the {b}. Trade will be volatile.", "War: {a} against {b}. The lanes will be tense."],
		["The {a} declared war on the {b}.", "War: {a} against {b}."],
	],
	"peace_between": [
		["The {a} and the {b} have made peace. Good news for everyone out here.", "Peace between the {a} and the {b}. A welcome end."],
		["The {a} and the {b} made peace. The fighting stops.", "Peace between the {a} and the {b}. For now."],
		["The {a} and the {b} have made peace. Understand what that means for us.", "The {a} and the {b} are at peace. Watch for what changes."],
		["The {a} and the {b} have made peace. The border files are updated.", "Peace between the {a} and the {b}."],
		["The {a} and the {b} made peace. Customers on both sides again.", "Peace between the {a} and the {b}. Open the lanes."],
		["The {a} and the {b} have made peace, by the Founders' leave.", "A peace is struck between the {a} and the {b}."],
	],
	"treaty_broken": [
		["The {a} broke their treaty with the {b} by declaring war. Every power in the quadrant takes note.", "The {a} betrayed their treaty with the {b}. Faith is a fragile thing."],
		["The {a} broke their treaty with the {b} and declared war. A dishonourable act.", "The {a} betrayed the {b}. Remember it."],
		["The {a} broke their treaty with the {b} by declaring war. We note it carefully.", "The {a} betrayed their treaty with the {b}. A useful lesson."],
		["The {a} broke their treaty with the {b} by declaring war. The precedent is recorded.", "The {a} violated their treaty with the {b}."],
		["The {a} broke their treaty with the {b} by declaring war. A contract is a contract, until it is not.", "The {a} tore up a treaty with the {b}. Bad for confidence."],
		["The {a} broke their treaty with the {b}.", "The {a} betrayed the {b}."],
	],
	"eliminated": [
		["The {a} have been eliminated; their treaties are dissolved. A sad day for the quadrant.", "The {a} are gone from the galaxy. I will mark the day."],
		["The {a} are eliminated. They fought and lost.", "The {a} are no more. Let their name be remembered."],
		["The {a} have been eliminated. A rival less.", "The {a} are gone. Their treaties with them."],
		["The {a} have been eliminated. Their treaties are dissolved.", "The {a} no longer exist as a power."],
		["The {a} have been eliminated. Their contracts are void. Pity.", "The {a} are no more; their debts with them."],
		["The {a} are eliminated.", "The {a} are no more."],
	],
	"treaty_signed": [
		["The {a} and the {b} have ratified a {what}. Good news for the quadrant.", "A {what} is ratified between the {a} and the {b}."],
		["The {a} and the {b} have sealed a {what}. Let them honour it.", "A {what} binds the {a} and the {b}."],
		["The {a} and the {b} have ratified a {what}. Note who is on whose side.", "A {what}: {a} and {b}."],
		["A {what} is ratified between the {a} and the {b}. The registry is updated.", "The {a} and the {b} have concluded a {what}."],
		["The {a} and the {b} have ratified a {what}. Good for trade, and for those who read treaties closely.", "A {what} is signed by the {a} and the {b}."],
		["The {a} and the {b} ratify a {what}.", "A {what} is signed: {a} and {b}."],
	],
	"treaty_signed_peace": [
		["The {a} and the {b} have concluded a peace treaty. May it last.", "Peace is concluded between the {a} and the {b}."],
		["The {a} and the {b} have made peace. The war is over.", "A peace treaty between the {a} and the {b}."],
		["The {a} and the {b} have made peace. The map will change.", "A peace treaty is concluded: {a} and {b}."],
		["A peace treaty between the {a} and the {b} is filed.", "The {a} and the {b} are at peace."],
		["The {a} and the {b} have concluded peace. Open for trade.", "A peace treaty between the {a} and the {b}. Good for the lanes."],
		["The {a} and the {b} are at peace.", "A peace treaty between the {a} and the {b}."],
	],
	"treaty_ended": [
		["The {what} between the {a} and the {b} has ended. A pity.", "A {what} is no more between the {a} and the {b}."],
		["The {what} between the {a} and the {b} has ended. Let them fight.", "The {what} of the {a} and the {b} has lapsed."],
		["The {what} between the {a} and the {b} has ended. It was inevitable.", "A {what} lapses between the {a} and the {b}."],
		["The {what} between the {a} and the {b} has ended. The file is closed.", "A {what} is ended: {a} and {b}."],
		["The {what} between the {a} and the {b} has ended. The contract has expired.", "A {what} between the {a} and the {b} is void."],
		["The {what} between the {a} and the {b} has ended.", "A {what} has ended: {a} and {b}."],
	],
	"proposal_ended": [
		["A proposal for a {what} between the {a} and the {b} came to nothing.", "The {what} proposal involving the {a} and the {b} is closed."],
		["A {what} proposal between the {a} and the {b} came to nothing.", "The {a} and the {b} will not have a {what}."],
		["A {what} proposal between the {a} and the {b} came to nothing. Interesting.", "The {what} between the {a} and the {b} is not to be."],
		["The {what} proposal between the {a} and the {b} is closed without agreement.", "A {what} between the {a} and the {b} is closed."],
		["A {what} between the {a} and the {b} came to nothing. No deal, no commission.", "The {a} and the {b} could not agree on a {what}."],
		["A {what} between the {a} and the {b} is closed.", "The {what} proposal is void."],
	],
	"gift_delivered": [
		["A gift of {n} credits was delivered between the {a} and the {b}. A fine gesture.", "{n} credits passed between the {a} and the {b}, a good deed."],
		["{n} credits changed hands between the {a} and the {b}.", "{n} credits delivered between the {a} and the {b}."],
		["{n} credits changed hands between the {a} and the {b}. Note the direction.", "{n} credits passed between the {a} and the {b}. Note why."],
		["{n} credits were transferred between the {a} and the {b}. Recorded.", "A transfer of {n} credits between the {a} and the {b}."],
		["{n} credits changed hands between the {a} and the {b}. I want a cut.", "{n} credits passed between the {a} and the {b}. I did not arrange it, and I am annoyed."],
		["{n} credits passed between the {a} and the {b}.", "{n} credits delivered: {a} and {b}."],
	],
	"victory": [
		["Our campaign is won. I am proud of every officer and every crew. This log is not an ending, only a pause.", "We have won. I hope we have earned the quiet that follows."],
		["Victory! The Empire stands unbeaten. Let it be sung for a hundred years.", "We have won. Qapla'."],
		["The campaign is ours. The Senate may now take credit.", "We have won. It is time to make the rest of the quadrant understand it."],
		["Victory is recorded. The Union is ascendant.", "The campaign is won. Order has been established."],
		["We won. Everything is profit from here, and I intend to collect.", "The campaign is won. Let us tally what we are owed."],
		["The campaign is won. All is as the Founders willed.", "We have won. Resistance is ended."],
	],
	"defeat": [
		["Our campaign is lost. I will answer for it. I am sorry.", "We have been defeated. This log ends with an apology, and a hope."],
		["The campaign is lost. We have no honour left but our names.", "Defeat. Let it be recorded, and let the survivors strive on."],
		["The campaign is lost. The Senate will want someone to blame.", "We are beaten. There is nothing to add."],
		["The campaign is lost. The Union has been dealt a grave blow.", "Defeat is recorded. The responsible parties are to be identified."],
		["The campaign is lost. Cancel the invoices.", "We lost. Close the books."],
		["The campaign is lost. The Founders will be informed.", "Defeat. The Vorta will answer for it."],
	],
	"event_plague": [
		["Medical alert{sys_at}: a plague. {detail}", "A plague has broken out{sys_at}. {detail}"],
		["A sickness stalks us{sys_at}. {detail}", "Plague{sys_at}. {detail}"],
		["A plague{sys_at}. {detail} Let it not leave the colony.", "Quarantine matters{sys_at}. {detail}"],
		["Plague{sys_at}. {detail} The ministry is informed.", "Disease outbreak{sys_at}. {detail}"],
		["A plague{sys_at}. {detail} Bad for business.", "Epidemic{sys_at}. {detail}"],
		["Disease{sys_at}. {detail}", "A plague{sys_at}. {detail}"],
	],
	"event_supernova": [
		["Science officers warn of a supernova. {detail}", "A star is about to die. {detail}"],
		["A star near us is dying. {detail}", "A supernova warning. {detail}"],
		["A supernova is approaching. {detail} Do what is necessary.", "A star will go nova. {detail}"],
		["Supernova warning. {detail} Contingency orders are filed.", "A star will explode. {detail}"],
		["A supernova is coming. {detail} Save the inventory first.", "Supernova warning. {detail}"],
		["A star will die. {detail}", "Supernova warning. {detail}"],
	],
	"event_derelict": [
		["We have found a derelict vessel. {detail}", "Sensors report a derelict ship. {detail}"],
		["A dead ship drifts in our space. {detail}", "A derelict. {detail}"],
		["A derelict has been found. {detail} It will be examined privately.", "An abandoned ship. {detail}"],
		["A derelict vessel has been found. {detail} Salvage rights are claimed.", "Derelict located. {detail}"],
		["A derelict, free for the taking. {detail}", "A ship nobody wants. {detail} I want it."],
		["A derelict vessel. {detail}", "A derelict ship. {detail}"],
	],
	"event_unrest": [
		["Labour unrest{sys_at}. {detail} I would rather talk than punish.", "The workers are unhappy{sys_at}. {detail}"],
		["The workers are restless{sys_at}. {detail}", "Labour unrest{sys_at}. {detail}"],
		["Labour trouble{sys_at}. {detail} Discreet solutions are preferred.", "Unrest among the workers{sys_at}. {detail}"],
		["Strike action{sys_at}. {detail} Security is informed.", "Labour unrest{sys_at}. {detail}"],
		["A strike{sys_at}. {detail} A strike is just a price negotiation with the lights off.", "Labour unrest{sys_at}. {detail}"],
		["Unrest{sys_at}. {detail}", "A strike{sys_at}. {detail}"],
	],
	"event_breakthrough": [
		["A scientific breakthrough. {detail} Our people have done well.", "Our scientists have a breakthrough. {detail}"],
		["A breakthrough in our forges. {detail}", "A scientific breakthrough. {detail}"],
		["A breakthrough, quietly. {detail}", "Our scientists have succeeded. {detail}"],
		["A scientific breakthrough. {detail} The ministry is pleased.", "Breakthrough. {detail}"],
		["A scientific breakthrough. {detail} Patents will be filed.", "Our researchers have struck gold. {detail}"],
		["A breakthrough. {detail}", "Breakthrough achieved. {detail}"],
	],
	"event_trade": [
		["Trade is booming. {detail} Commerce serves everyone.", "A trade boom. {detail}"],
		["Trade is booming; the feast halls are full. {detail}", "A trade boom. {detail} Let the warriors eat well."],
		["Trade is booming, and the Senate need not know how much. {detail}", "A boom in commerce. {detail}"],
		["A trade boom. {detail} Revenue exceeds the quota.", "Trade is booming. {detail}"],
		["A trade boom! {detail} This is the good part of the cycle.", "Trade is booming. {detail} Be generous with the samples."],
		["A trade boom, reported to the Founders. {detail}", "Commerce flourishes. {detail}"],
	],
	"event_rift": [
		["A subspace rift. {detail} The scientists are fascinated, and I am wary.", "Sensors report a subspace rift. {detail}"],
		["A subspace rift. {detail} Fools will fly into it.", "A rift in space. {detail}"],
		["A subspace rift. {detail} Avoid it, or use it.", "A rift opens. {detail}"],
		["A subspace rift. {detail} Navigational warnings are issued.", "A rift is detected. {detail}"],
		["A subspace rift. {detail} Closes lanes, opens opportunities.", "A rift opens. {detail}"],
		["A subspace rift. {detail}", "A rift. {detail}"],
	],
	"treasury_low": [
		["Our treasury stands at {credits} and is falling. We should find savings, or income, soon.", "The treasury is thin: {credits} credits. I would like options before it is empty."],
		["Our treasury is down to {credits}. Warriors do not eat glory.", "The coffers hold {credits}. Fix it before the fleet notices."],
		["The treasury stands at {credits}. A careless person would call that a concern.", "We hold {credits} credits. Reduce the spending, quietly."],
		["Warning: the treasury has fallen to {credits}. Expenditure must be reduced.", "The treasury holds {credits} credits. A review is ordered."],
		["The treasury is at {credits}. That is not a number a Ferengi should ever write down.", "Treasury: {credits}. Panic is bad for business; shouting is not."],
		["The treasury holds {credits}. It will be corrected.", "Treasury: {credits}. Insufficient."],
	],
	"treasury_empty": [
		["The treasury is overdrawn at {credits}. Starfleet cannot operate like this. We must cut costs now.", "We are in debt: {credits} credits. This is a crisis and I want a plan."],
		["The treasury is overdrawn at {credits}. A warrior can fight on an empty belly, but a fleet cannot.", "We are in debt: {credits}. Fix it."],
		["The treasury is overdrawn at {credits}. This must not be known beyond this room.", "We are in debt: {credits}. Find the money."],
		["The treasury is overdrawn at {credits}. Emergency measures are authorised.", "The treasury is in deficit: {credits}. The ministry will answer."],
		["The treasury is overdrawn at {credits}. This is, in the strongest sense, a disaster.", "We are in the red: {credits}. Sell something."],
		["The treasury is overdrawn at {credits}. The deficit will be corrected.", "Treasury deficit: {credits}."],
	],
	"food_short": [
		["{sys} cannot feed its people, and the colony is short of food. We should move workers to the farms.", "Food is short at {sys}. I want the colonists fed before anything else."],
		["{sys} is short of food. Send hunters and farmers; the weak must not starve.", "Hunger at {sys}. Feed them."],
		["{sys} is short of food. The harvest has been mismanaged.", "A food shortage at {sys}. Correct it quietly."],
		["{sys} is short of food. The agriculture ministry must respond.", "Food shortage at {sys}. Corrective orders are required."],
		["{sys} is short of food. Starving workers do not buy much.", "Food shortage at {sys}. Buy grain."],
		["{sys} is short of food. It will be corrected.", "Food shortage at {sys}."],
	],
}

## The frame around a line no pattern knows. {text} is the core's own sentence, kept as is.
const FALLBACK := [
	["{text}", "For the record: {text}"],
	["{text} Let it be recorded.", "{text} So it was."],
	["{text} That is all that will be said.", "{text}"],
	["{text} Filed for Central Command.", "{text}"],
	["{text} Entered in the ledger.", "{text}"],
	["{text} Noted.", "{text}"],
]

# ---------------------------------------------------------------------------------------------
# Building a line
# ---------------------------------------------------------------------------------------------
static func _pick(variants: Array, seed_value: int) -> String:
	if variants.is_empty():
		return ""
	return str(variants[absi(seed_value) % variants.size()])

static func seed_for(turn: int, index: int, kind: String) -> int:
	return ("%d|%d|%s" % [turn, index, kind]).hash()

static func _first_upper(s: String) -> String:
	return s if s.is_empty() else s.substr(0, 1).to_upper() + s.substr(1)

static func _fill(template: String, args: Dictionary) -> String:
	var out := template
	for k in args:
		out = out.replace("{%s}" % k, str(args[k]))
	var leftover := RegEx.new()
	leftover.compile("\\{[a-z_]+\\}")
	return leftover.sub(out, "", true)

## Resolves "System 12" or "12" to a colony or system name when the context knows it.
static func _name_sys(raw: String, ctx: Dictionary) -> String:
	var names: Dictionary = ctx.get("sysnames", {})
	var digits := raw.strip_edges()
	if digits.begins_with("System "):
		digits = digits.substr(7)
	if digits.is_valid_int() and names.has(int(digits)):
		return str(names[int(digits)])
	return raw if not digits.is_valid_int() else "system %s" % digits

## The empire's voiced line for a core line. `ctx`: {own_name: String, sysnames: {id: name}}. Always returns a sentence.
static func voice_line(empire: Variant, text: String, turn: int, index: int, ctx: Dictionary = {}) -> String:
	var v := voice_of(empire)
	var cls := classify(text)
	var kind: String = cls["kind"]
	var args: Dictionary = cls["args"].duplicate()
	if kind == "":
		return _fill(_pick(FALLBACK[v], seed_for(turn, index, "fallback")), {"text": text})
	if kind == "system_taken":
		if str(args["by"]) == str(ctx.get("own_name", "\u0000")):
			kind = "system_won"
	if args.has("sys"):
		args["sys"] = _name_sys(str(args["sys"]), ctx)
	if kind in ["event_plague", "event_unrest"]:
		var s := str(args.get("sys", ""))
		args["sys_at"] = (" at " + _name_sys(s, ctx)) if s != "" else ""
	if args.has("qual"):
		var q := str(args["qual"]).strip_edges()
		if q != "":
			args["detail"] = _first_upper(q) + (": " + str(args["detail"]) if str(args["detail"]) != "" else "")
	if args.has("detail"):
		args["detail"] = _first_upper(str(args["detail"]))
	if args.has("credits") and kind.begins_with("treasury"):
		args["credits"] = str(args["credits"])
	var table: Array = PHRASES.get(kind, [])
	if table.size() != 6:
		return _fill(_pick(FALLBACK[v], seed_for(turn, index, "fallback")), {"text": text})
	var line := _fill(_pick(table[v], seed_for(turn, index, kind)), args)
	return line.replace("  ", " ").strip_edges()

## The log entries of a turn briefing in the empire's voice: [{...entry, "voice": String}], plus the lines the core leaves to the UI
## (construction complete, food shortage, treasury warnings). `briefing` is the core's briefing Dictionary.
static func log_entries(empire: Variant, briefing: Dictionary, ctx: Dictionary = {}) -> Array:
	var turn := int(briefing.get("turn", 0))
	var out: Array = []
	var seen_completed := {}
	var events: Array = briefing.get("events", [])
	for i in events.size():
		var e: Dictionary = events[i].duplicate()
		e["voice"] = voice_line(empire, str(e.get("text", "")), turn, i, ctx)
		out.append(e)
	var idx := events.size()
	for col in briefing.get("colonies", []):
		var done := str(col.get("completed", ""))
		if done != "":
			var already := false
			for e in events:
				if str(e.get("text", "")).find(done) >= 0:
					already = true
			if not already:
				var t := _fill(_pick(PHRASES["construction_done"][voice_of(empire)], seed_for(turn, idx, "construction_done")), {"sys": str(col.get("name", "")), "what": done})
				out.append({"category": "construction", "severity": "good", "text": t, "voice": t, "systemId": int(col.get("systemId", 0)), "synthetic": true})
				idx += 1
		if int(col.get("economy", {}).get("foodDeficit", 0)) > 0:
			var tf := _fill(_pick(PHRASES["food_short"][voice_of(empire)], seed_for(turn, idx, "food_short")), {"sys": str(col.get("name", ""))})
			out.append({"category": "colonization", "severity": "warn", "text": tf, "voice": tf, "systemId": int(col.get("systemId", 0)), "synthetic": true})
			idx += 1
	var after := int(briefing.get("creditsAfter", 0))
	var before := int(briefing.get("creditsBefore", 0))
	if after < 0:
		var te := _fill(_pick(PHRASES["treasury_empty"][voice_of(empire)], seed_for(turn, idx, "treasury_empty")), {"credits": str(after)})
		out.append({"category": "economy", "severity": "alert", "text": te, "voice": te, "synthetic": true})
	elif after < before and after < 3 * maxi(1, before - after):
		var tl := _fill(_pick(PHRASES["treasury_low"][voice_of(empire)], seed_for(turn, idx, "treasury_low")), {"credits": str(after)})
		out.append({"category": "economy", "severity": "warn", "text": tl, "voice": tl, "synthetic": true})
	return out

## Every kind the phrase table covers (for the tests and for the report).
static func kinds() -> Array:
	return PHRASES.keys()
