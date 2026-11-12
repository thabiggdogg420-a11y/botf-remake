extends SceneTree
## Headless test runner for the BotfCore GDExtension. A GDScript port of a subset of
## remaster/web/test/bridge.test.mjs. Run: remaster/godot/run-tests.sh
## (or godot --headless --path remaster/godot -s res://tests/run_tests.gd).

var core: Object
var passed := 0
var failed := 0
var current := ""

func _initialize() -> void:
	core = Engine.get_singleton("BotfCore") if Engine.has_singleton("BotfCore") else null
	print("BOTF Godot core tests")
	if core == null:
		printerr("FAIL: BotfCore singleton missing (build the extension: core/build.sh, then import the project)")
		quit(1)
		return
	for t in [
		"test_meta", "test_new_campaign", "test_invalid_order", "test_orders", "test_forecast_and_turns",
		"test_save_load_determinism", "test_battle_drill", "test_rules3_events", "test_diplomacy_drill",
		"test_fixture_determinism", "test_presets", "test_gameplay_queries", "test_uncharted_view", "test_default_size_is_medium", "test_view_does_not_touch_the_sim",
	]:
		current = t
		var failures_before := failed
		var t0 := Time.get_ticks_msec()
		call(t)
		if failed == failures_before:
			passed += 1
			print("  ok  %s  (%d ms)" % [t, Time.get_ticks_msec() - t0])
	print("%d passed, %d failed" % [passed, failed])
	quit(1 if failed > 0 else 0)

# --- helpers -----------------------------------------------------------------------------------

func api(method: String, args := {}) -> Variant:
	var reply: Dictionary = core.request(method, args)
	if not reply.get("ok", false):
		check(false, "%s: %s" % [method, reply.get("error", "?")])
		return null
	return reply.get("result")

func attempt(method: String, args := {}) -> Dictionary:
	return core.request(method, args)

func check(cond: bool, what: String) -> bool:
	if not cond:
		failed += 1
		printerr("  FAIL %s: %s" % [current, what])
	return cond

func end_turn() -> Dictionary:
	var status: Dictionary = api("endTurn")
	var guard := 0
	while status.status == "battle":
		var encounter = api("tactical")
		check(encounter is Dictionary and encounter.ships is Array, "battle exposes a tactical encounter")
		api("tacticalAuto")
		status = api("endTurn")
		guard += 1
		if not check(guard < 20, "battles terminate"):
			break
	check(status.status == "complete", "turn completes")
	return status.get("briefing", {})

func load_fixture(name: String) -> Dictionary:
	var text := FileAccess.get_file_as_string("res://fixtures/%s.json" % name)
	return JSON.parse_string(text)

const CAMPAIGN := {"faction": 1, "empires": 5, "seed": 424242, "era": 1, "ruleset": "reference"}

# --- tests -------------------------------------------------------------------------------------

func test_meta() -> void:
	var meta: Dictionary = api("meta")
	check(meta.empires.size() == 8, "eight playable powers (five original majors, Dominion, Borg, Breen)")
	check(meta.researchFields.size() == 6, "six research fields")
	check(meta.maneuvers.any(func(m): return m.key == "Engage"), "Engage maneuver")

func test_new_campaign() -> void:
	var view: Dictionary = api("newCampaign", {"faction": 2, "empires": 5, "seed": 424242, "era": 1, "ruleset": "reference"})
	check(view.meta.player == 2, "player is faction 2")
	check(view.meta.turn == 0, "turn 0")
	check(typeof(view.meta.turn) == TYPE_INT, "integral numbers come back as int")
	check(view.fog.length() == view.meta.width * view.meta.height, "fog covers the map")
	check(view.colonies[0].population > 0, "home colony populated")
	for c in view.contacts:
		check(view.fog[c.y * view.meta.width + c.x] == "2", "contacts only in sensor range")
	check(view.fleets.size() >= 2, "player starts with ships")

func test_invalid_order() -> void:
	api("newCampaign", CAMPAIGN)
	var before = api("fingerprint")
	var bad := attempt("submit", {"command": {"type": "Colonize", "fleetId": 999}})
	check(bad.ok == false and String(bad.error) != "", "invalid order rejected with a reason")
	check(api("fingerprint") == before, "rejected order changes nothing")
	var dry: Dictionary = api("dryRun", {"command": {"type": "AllocateResearch", "researchAllocation": [50, 50, 50, 0, 0, 0]}})
	check(dry.ok == false and "100 percent" in dry.error, "dry run explains allocation error")

func test_orders() -> void:
	var view: Dictionary = api("newCampaign", CAMPAIGN)
	var home: Dictionary = view.colonies[0]
	var detail: Dictionary = api("colony", {"systemId": home.systemId})
	var pop: int = detail.colony.population
	var food: int = mini(detail.capacityLimits.food, ceili(pop / 2.0))
	var industry: int = mini(detail.capacityLimits.industry, pop - food)
	var preview: Dictionary = api("workforcePreview", {"systemId": home.systemId, "food": food, "industry": industry, "research": 0})
	check(preview.error == "", "workforce preview ok")
	api("submit", {"command": {"type": "AssignWorkers", "systemId": home.systemId, "foodWorkers": food, "industryWorkers": industry, "researchWorkers": 0}, "cancelIds": preview.cancelIds})
	api("submit", {"command": {"type": "AllocateResearch", "researchAllocation": [20, 20, 20, 20, 10, 10]}})
	view = api("view")
	check(view.pending.any(func(p): return p.type == "AllocateResearch"), "research order pending")

func test_forecast_and_turns() -> void:
	api("newCampaign", CAMPAIGN)
	var before = api("fingerprint")
	var forecast: Dictionary = api("forecast")
	check(api("fingerprint") == before, "forecast commits nothing")
	var briefing := end_turn()
	check(briefing.creditsAfter == forecast.creditsAfter, "forecast matches the resolved turn")
	for i in 9:
		end_turn()
	var view: Dictionary = api("view")
	check(view.meta.turn == 10, "reached turn 10")
	api("research")
	api("diplomacy")
	api("intel")

func test_save_load_determinism() -> void:
	api("newCampaign", CAMPAIGN)
	for i in 3:
		end_turn()
	var fp = api("fingerprint")
	var save: Dictionary = api("save")
	check(save.kind == "game", "game save")
	end_turn()
	check(api("fingerprint") != fp, "turn changes the state")
	var loaded: Dictionary = api("load", {"data": save.data})
	check(loaded.meta.turn == 3, "load restores the turn")
	check(api("fingerprint") == fp, "load restores the fingerprint")
	end_turn()
	var a = api("fingerprint")
	api("load", {"data": save.data})
	end_turn()
	check(api("fingerprint") == a, "same save + end turn = same result")

## Presets through the bridge: the registry, a stacked game, and a save that carries its packs and restores them.
func test_presets() -> void:
	var list: Array = api("listPresets")
	var ids: Array = list.map(func(p): return p.id)
	check(ids == ["classic", "remastered", "dominion-war", "ages", "galaxies", "balance-of-power", "ultimate", "klingon-civil-war", "mirror-universe"], "nine presets in screen order: %s" % str(ids))
	check(api("meta").presets.size() == list.size(), "meta carries the presets")
	check(list.all(func(p): return p.blurb != "" and p.has("options") and p.has("lineupPlayable")), "every preset has text, options and a lineup verdict")
	check(not attempt("newCampaign", {"preset": "no-such-preset", "faction": 1, "empires": 2, "seed": 1}).ok, "an unknown preset is refused")
	var c := CAMPAIGN.duplicate()
	c["preset"] = "ultimate"
	var view: Dictionary = api("newCampaign", c)
	check(view.meta.rules.preset == "ultimate" and view.meta.rules.presetName == "Ultimate", "the view names the preset")
	var info: Dictionary = api("contentInfo")
	check(not info.base and info.id == "botf-base+ultimate" and info.stack.size() == 1, "its overlay stack is the active content")
	for i in 3:
		end_turn()
	var fp = api("fingerprint")
	var save: Dictionary = api("save")
	check(str(save.data).contains("PACKS ultimate 1 ultimate "), "the save records the preset and its packs")
	api("loadBaseContent")
	check(api("contentInfo").base, "base content is back")
	var loaded: Dictionary = api("load", {"data": save.data})
	check(loaded.meta.rules.preset == "ultimate" and api("contentInfo").id == "botf-base+ultimate", "loading rebuilds the stack")
	check(api("fingerprint") == fp, "the loaded game has the saved fingerprint")
	end_turn()
	var a = api("fingerprint")
	api("load", {"data": save.data})
	end_turn()
	check(api("fingerprint") == a, "same save + end turn = same result under a stack")
	api("newCampaign", CAMPAIGN)
	check(api("contentInfo").base and api("view").meta.rules.preset == "remastered", "a Remastered game restores the base pack")

func test_battle_drill() -> void:
	api("newBattleDrill")
	var status: Dictionary = api("endTurn")
	check(status.status == "battle", "drill pauses for battle")
	var encounter: Dictionary = api("tactical")
	var mine: Array = encounter.ships.filter(func(s): return s.mine and s.alive)
	check(mine.size() >= 2, "player has ships in the battle")
	api("tacticalOrder", {"all": true, "maneuver": "Engage"})
	var after: Dictionary = api("tacticalRound")
	check(after.round == 1, "one round resolved")
	check(not attempt("submit", {"command": {"type": "AllocateResearch", "researchAllocation": [100, 0, 0, 0, 0, 0]}}).ok, "strategic orders locked in battle")
	var save: Dictionary = api("save")
	check(save.kind == "session", "mid-battle session save")
	api("load", {"data": save.data})
	check(api("tactical").round == 1, "session resumes at round 1")
	check(api("tacticalAuto").finished == true, "auto-resolve finishes")
	var result: Dictionary = api("endTurn")
	check(result.status == "complete", "turn completes after battle")
	check(result.briefing.events.any(func(e): return e.category == "combat"), "combat in briefing")
	check(api("replays").size() == 1, "one replay")

func test_rules3_events() -> void:
	api("newBattleDrill", {"station": true})
	check(api("endTurn").status == "battle", "station drill battle")
	var e: Dictionary = api("tactical")
	check(e.rules == 4 and e.ticksPerRound == 12, "Remastered drill runs rules 4, 12 ticks")
	var fire = null
	for i in 12:
		if fire != null or e.finished:
			break
		e = api("tacticalRound")
		var r: Dictionary = e.rounds[e.rounds.size() - 1]
		check(r.events.size() > 0, "every rules 3 round has events")
		for ev in r.events:
			if ev.kind == "Fire":
				fire = ev
				break
	check(fire != null, "weapons fire within a few rounds")
	if fire != null:
		check(fire.key == "weapon.%s" % fire.weapon, "fire event key")

func test_diplomacy_drill() -> void:
	var v: Dictionary = api("newDiplomacyDrill")
	check(v.meta.scenario == "diplomacy", "diplomacy scenario")
	var d: Dictionary = api("diplomacy")
	check(d.callsToArms.size() == 1, "one call to arms")
	check(d.proposals.any(func(p): return p.incoming and p.kind == "tribute"), "tribute demand")

## Determinism across builds: the native extension must reproduce the fingerprint the wasm core
## recorded in fixtures/determinism.json (tools/dump-fixtures.sh).
func test_fixture_determinism() -> void:
	# Rules 4 (the Remastered default, WEGO) and rules 3 ("wego": false) each carry a wasm-recorded fingerprint.
	for name in ["determinism", "determinism-rules3"]:
		var fx := load_fixture(name)
		api("newCampaign", fx.args.campaign)
		for i in int(fx.args.turns):
			end_turn()
		check(api("fingerprint") == fx.result, "%s: fingerprint %s == wasm %s" % [name, api("fingerprint"), fx.result])
	# Schema 15 galaxy shapes, densities and difficulties: every wasm-recorded fingerprint, and the preview matches the galaxy.
	var gx := load_fixture("determinism-galaxy")
	for run in gx.result:
		api("newCampaign", run.campaign)
		for i in int(run.turns):
			end_turn()
		check(api("fingerprint") == run.result, "galaxy %s/%s/%s: fingerprint %s == wasm %s" % [run.campaign.shape, run.campaign.minorDensity, run.campaign.difficulty, api("fingerprint"), run.result])
		var pv: Dictionary = api("galaxyPreview", run.campaign)
		check(pv.stars.size() == int(run.preview) and pv.stars.size() == int(run.campaign.systems) and pv.shape == run.campaign.shape, "galaxy preview holds every star of the %s galaxy" % run.campaign.shape)
	var sl := load_fixture("determinism-slots")
	for run in sl.result:
		api("newCampaign", run.campaign)
		for i in int(run.turns):
			end_turn()
		check(api("fingerprint") == run.result, "lineup %s: fingerprint %s == wasm %s" % [str(run.campaign.lineup), api("fingerprint"), run.result])
		var spv: Dictionary = api("galaxyPreview", run.campaign)
		check(spv.stars.size() == int(run.preview) and spv.stars.size() == int(run.campaign.systems), "lineup preview holds every star")
	# Threat actors (schema 20): monsters, Borg, replacements and a seated Borg empire, native against wasm, with the spawn count.
	var tx := load_fixture("determinism-threats")
	for run in tx.result:
		api("newCampaign", run.campaign)
		for i in int(run.turns):
			end_turn()
		var tv: Dictionary = api("view")
		check(api("fingerprint") == run.result, "threats %s: fingerprint %s == wasm %s" % [JSON.stringify(run.campaign.threats), api("fingerprint"), run.result])
		check(int(tv.threatsSpawned) == int(run.spawned) and int(run.spawned) > 2, "threats: the native core spawned %d actors, wasm %d" % [int(tv.threatsSpawned), int(run.spawned)])
		check(JSON.stringify(tv.threatSetup) == JSON.stringify(run.threats), "threats: the view reports the switches the game was started with")
	# Campaign eras (schema 22): Ages and Galaxies at every start year, native against wasm, with the year, cap and start level.
	var ex := load_fixture("determinism-eras")
	for run in ex.result:
		api("newCampaign", run.campaign)
		for i in int(run.turns):
			end_turn()
		var ev: Dictionary = api("view")
		var tag := "%s/%d" % [run.campaign.preset, int(run.campaign.era)]
		check(api("fingerprint") == run.result, "eras %s: fingerprint %s == wasm %s" % [tag, api("fingerprint"), run.result])
		check(int(ev.meta.year) == int(run.year) and int(ev.meta.techCap) == int(run.techCap) and int(ev.meta.techCap) == 14 and int(run.year) >= 2240, "eras %s: year %d and cap %d match the wasm core" % [tag, int(ev.meta.year), int(ev.meta.techCap)])
		check(int(api("research").fields[0].level) == int(run.level), "eras %s: research level matches" % tag)
	api("loadBaseContent")
	# Packs and presets (schema 17): every preset's fingerprint, stack checksum and recorded preset id, native against wasm.
	var px := load_fixture("determinism-presets")
	for run in px.result:
		api("newCampaign", run.campaign)
		for i in int(run.turns):
			end_turn()
		var info: Dictionary = api("contentInfo")
		check(api("fingerprint") == run.result, "preset %s: fingerprint %s == wasm %s" % [run.campaign.preset, api("fingerprint"), run.result])
		check(info.id == run.content.id and info.checksum == run.content.checksum and info.stack.size() == int(run.content.stack), "preset %s: stack %s %s == wasm %s %s" % [run.campaign.preset, info.id, info.checksum, run.content.id, run.content.checksum])
		check(api("view").meta.rules.preset == run.preset, "preset %s: the view names its preset" % run.campaign.preset)
	# Dominion content (schema 19): the Dominion War preset as the Federation and as the Dominion, 30 turns, native against wasm.
	var dx := load_fixture("determinism-dominion")
	for run in dx.result:
		api("newCampaign", run.campaign)
		for i in int(run.turns):
			end_turn()
		var dinfo: Dictionary = api("contentInfo")
		check(api("fingerprint") == run.result, "Dominion War as empire %d: fingerprint %s == wasm %s" % [int(run.campaign.faction), api("fingerprint"), run.result])
		check(dinfo.id == run.content.id and dinfo.checksum == run.content.checksum and dinfo.stack.size() == int(run.content.stack), "Dominion War stack %s %s == wasm %s %s" % [dinfo.id, dinfo.checksum, run.content.id, run.content.checksum])
	api("loadBaseContent")
	# Economy and rules mechanics (schema 23): presets carrying the economy flags, 50 turns, native against wasm.
	var exx := load_fixture("determinism-economy")
	for run in exx.result:
		api("newCampaign", run.campaign)
		for i in int(run.turns):
			end_turn()
		check(api("fingerprint") == run.result, "economy preset %s: fingerprint %s == wasm %s" % [run.campaign.preset, api("fingerprint"), run.result])
	api("loadBaseContent")
	# Scenarios (schema 25): each scenario for 50 turns, native against wasm.
	var sx := load_fixture("determinism-scenarios")
	for run in sx.result:
		api("newCampaign", run.campaign)
		for i in int(run.turns):
			end_turn()
		check(api("fingerprint") == run.result, "scenario %s: fingerprint %s == wasm %s" % [run.campaign.scenario, api("fingerprint"), run.result])
	api("loadBaseContent")
	# Balance and AI (schema 24): difficulty tiers incl. Expert, galaxy options, the balance presets, 40 turns, native against wasm.
	var bx := load_fixture("determinism-balance")
	for run in bx.result:
		api("newCampaign", run.campaign)
		for i in int(run.turns):
			end_turn()
		check(api("fingerprint") == run.result, "balance run %s: fingerprint %s == wasm %s" % [JSON.stringify(run.campaign), api("fingerprint"), run.result])
	api("loadBaseContent")
	var fx_view := load_fixture("campaign-view")
	var raw: String = core.request_json("newCampaign", JSON.stringify(fx_view.args))
	var reparsed: Dictionary = JSON.parse_string(raw)
	check(JSON.stringify(reparsed.result) == JSON.stringify(fx_view.result), "new campaign view matches the wasm fixture")

func test_gameplay_queries() -> void:
	# R2 surface: queries the Godot screens depend on.
	api("newCampaign", CAMPAIGN)
	var routes: Dictionary = api("tradeRoutes")
	check(routes.has("routes") and routes.has("candidates") and routes.has("advancedRules"), "tradeRoutes shape")
	var view: Dictionary = api("view")
	var sid: int = view.colonies[0].systemId
	var contact := 0
	for e in view.empires:
		if e.contact:
			contact = e.empire
	var cancel := attempt("dryRun", {"command": {"type": "CancelTradeRoute", "systemId": sid, "targetEmpire": contact if contact else 2}})
	check(cancel.ok and cancel.result is Dictionary, "dryRun answers a cancel-route command")
	var retreat: Dictionary = api("dryRun", {"command": {"type": "RetreatFleet", "fleetId": view.fleets[0].id}})
	check(retreat.ok, "RetreatFleet validates for an own ship: %s" % retreat.get("error", ""))
	var many: Array = api("dryRunMany", {"items": [{"command": {"type": "RetreatFleet", "fleetId": view.fleets[0].id}}, {"command": {"type": "RetreatFleet", "fleetId": 987654}}]})
	check(many.size() == 2 and many[0] == "" and many[1] != "", "dryRunMany answers one reason per item")
	var ct = api("canTerraform", {"fleetId": view.fleets[0].id, "systemId": sid, "planetIndex": 0})
	check(ct is bool, "canTerraform answers a bool")
	var dt = api("describeTurn")
	check(dt is String, "describeTurn answers the last briefing as a string")
	end_turn()
	var dt2 = api("describeTurn")
	var parsed = JSON.parse_string(dt2) if dt2 is String and dt2 != "" else null
	check(parsed is Dictionary and parsed.has("turn"), "describeTurn carries the resolved turn after endTurn")

## Simon, 10 Nov 2026: the player view lists every star from turn 1. One the player has not explored is "uncharted" and
## carries only id, x, y, a catalogue designation (name) and explored:false.
func test_uncharted_view() -> void:
	var view: Dictionary = api("newCampaign", {"faction": 1, "empires": 5, "seed": 424242, "era": 1, "ruleset": "reference", "width": 34, "height": 24, "systems": 120})
	var w := int(view.meta.width)
	check(view.systems.size() == 120, "every star is listed from turn 1 (%d)" % view.systems.size())
	var charted := 0
	var uncharted := 0
	var seen := {}
	for sy in view.systems:
		var cell := int(sy.y) * w + int(sy.x)
		check(not seen.has(sy.id), "system ids are unique")
		seen[sy.id] = true
		if sy.get("explored", true):
			charted += 1
			check(str(view.fog[cell]) != "0", "a charted system sits on explored fog (%s)" % sy.name)
			check(sy.has("planets") and sy.has("owner") and sy.has("capacity"), "a charted system keeps its full entry")
		else:
			uncharted += 1
			var keys: Array = sy.keys()
			keys.sort()
			check(keys == ["explored", "id", "name", "x", "y"], "an uncharted entry carries only id, x, y, name, explored: %s" % [keys])
			var rx := RegEx.new()
			rx.compile("^FGC-[0-9]{3}$")
			check(rx.search(str(sy.name)) != null, "the uncharted name is a catalogue designation (%s)" % sy.name)
			check(str(view.fog[cell]) == "0", "an uncharted star lies in unexplored fog")
	var explored_cells := 0
	for sy in view.systems:
		if str(view.fog[int(sy.y) * w + int(sy.x)]) != "0":
			explored_cells += 1
	check(charted == explored_cells and charted > 0 and uncharted == 120 - charted, "the explored count is unchanged: %d charted, %d uncharted" % [charted, uncharted])
	# Nothing about an uncharted star leaks through any other list of the view.
	var uncharted_ids := {}
	var uncharted_cells := {}
	for sy in view.systems:
		if not sy.get("explored", true):
			uncharted_ids[int(sy.id)] = true
			uncharted_cells[int(sy.y) * w + int(sy.x)] = true
	for c in view.colonies:
		check(not uncharted_ids.has(int(c.systemId)), "no colony entry points at an uncharted star")
	for t in view.get("tradeRoutes", []):
		check(not uncharted_ids.has(int(t["from"])) and not uncharted_ids.has(int(t["to"])), "no trade route ends at an uncharted star")
	for c in view.contacts:
		check(not uncharted_cells.has(int(c.y) * w + int(c.x)), "no foreign contact sits on an uncharted star")
	# After exploring, the entry fills in: play a few turns, the charted count never drops.
	for i in 5:
		end_turn()
	var later: Dictionary = api("view")
	var charted2 := 0
	for sy in later.systems:
		if sy.get("explored", true):
			charted2 += 1
	check(later.systems.size() == 120 and charted2 >= charted, "turns keep all 120 stars listed and never un-chart one (%d -> %d)" % [charted, charted2])

## The view is a read: a fixed-seed autoplay to turn 20 reaches the same state with or without the view being read every turn,
## and the hash is the one the core produced before the view change (recorded from the unchanged simulation sources).
const FINGERPRINT_T20 := "8684820387310109984" # re-pinned for schema 25 (the header line is in the payload; the body is unchanged, see SCENARIOS.md section 9); schema 24: the header line is in the payload, and the AI now builds its race academy (BALANCE-AI.md section 4); native and wasm agree

func _autoplay_fingerprint(read_view: bool) -> String:
	api("newCampaign", CAMPAIGN)
	for i in 20:
		end_turn()
		if read_view:
			api("view")
	return str(api("fingerprint"))

func test_default_size_is_medium() -> void:
	var o := AppSession.campaign_options({"faction": 1})
	check(int(o["width"]) == 22 and int(o["height"]) == 16 and int(o["systems"]) == 50, "no size given: Medium 22x16, 50 systems (%s)" % [o])
	var h := AppSession.campaign_options({"faction": 1, "size": "huge"})
	check(int(h["width"]) == 34 and int(h["systems"]) == 120 and not h.has("size"), "size huge: 34x24, 120 systems")
	var e := AppSession.campaign_options({"faction": 1, "width": 10, "height": 8, "systems": 14})
	check(int(e["width"]) == 10 and int(e["systems"]) == 14, "explicit dimensions are kept")
	var v: Dictionary = api("newCampaign", AppSession.campaign_options({"faction": 1, "seed": 7}))
	check(int(v.meta.width) == 22 and int(v.meta.height) == 16 and v.systems.size() == 50, "a default new game is Medium: %dx%d, %d systems" % [v.meta.width, v.meta.height, v.systems.size()])

func test_view_does_not_touch_the_sim() -> void:
	var a := _autoplay_fingerprint(false)
	var b := _autoplay_fingerprint(true)
	check(a == b, "reading the view every turn does not change the state (%s vs %s)" % [a, b])
	print("  fingerprint turn 20: ", a)
	check(a == FINGERPRINT_T20, "the turn-20 fingerprint matches the pre-change core (%s)" % a)
