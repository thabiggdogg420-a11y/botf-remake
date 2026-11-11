extends SceneTree
## View-model unit tests: every screen's model driven headless through an AppSession, with no screens or nodes,
## on the fixture backend and again on BotfCore when the GDExtension is loaded. Also guards the layering rules
## (no threads, no file access outside AppLoader in the UI layers) and the start_campaign empire option.
##   godot --headless --audio-driver Dummy --path remaster/godot -s res://scripts/viewmodels/tests/test_viewmodels.gd
## Exit code 0 on success, 1 on any failure.

var failures := 0
var checks := 0

func _initialize() -> void:
	_run.call_deferred()

func check(cond: bool, what: String) -> void:
	checks += 1
	if cond:
		print("  ok  ", what)
	else:
		failures += 1
		printerr("  FAIL ", what)

func _run() -> void:
	_static_rules()
	_pure_models()
	var backends := ["fixture"]
	if Engine.has_singleton("BotfCore"):
		backends.append("auto")
	for b in backends:
		print("== backend: ", "fixture" if b == "fixture" else "native")
		_session_models(b)
	await _start_campaign_empire()
	print("%d checks, %d failures" % [checks, failures])
	quit(1 if failures > 0 else 0)

# ---------------------------------------------------------------------------------------------
# Layering and browser rules (docs/CLIENT-ARCHITECTURE.md)
# ---------------------------------------------------------------------------------------------
const UI_LAYERS := ["res://scenes/ui", "res://scenes/ui/screens", "res://scenes/ui/widgets", "res://scripts/viewmodels", "res://scripts"]
const FORBIDDEN := ["Thread.new", "WorkerThreadPool", "Mutex.new", "Semaphore.new", "FileAccess.", "DirAccess.", "ResourceLoader.", "preload(", " load(", "\tload(", "(load(", "OS.execute", "C:/", "C:\\\\", "LocalAssets"]

func _scripts_in(dir: String) -> Array:
	var out := []
	var d := DirAccess.open(dir)
	if d == null:
		return out
	for f in d.get_files():
		if f.ends_with(".gd"):
			if dir == "res://scripts" and not f.begins_with("app"):
				continue
			out.append(dir + "/" + f)
	return out

func _static_rules() -> void:
	print("== layering rules")
	var bad := []
	var scanned := 0
	for dir in UI_LAYERS:
		for path in _scripts_in(dir):
			if path.ends_with("app_loader.gd"):
				continue
			scanned += 1
			var text := FileAccess.get_file_as_string(path)
			for line in text.split("\n"):
				if line.strip_edges().begins_with("#"):
					continue
				for f in FORBIDDEN:
					if line.contains(f):
						bad.append("%s: %s" % [path.get_file(), f])
	check(scanned >= 30, "scanned %d UI-layer scripts" % scanned)
	check(bad.is_empty(), "no threads, file access or disk paths outside AppLoader in the UI layers %s" % [bad])
	var vm_nodes := []
	for path in _scripts_in("res://scripts/viewmodels"):
		var text := FileAccess.get_file_as_string(path)
		for n in [".new()\n\tadd_child", "Control.new", "Label.new", "Button.new", "add_child(", "Lcars."]:
			if text.contains(n) and not path.ends_with("fmt.gd"):
				vm_nodes.append("%s: %s" % [path.get_file(), n])
	check(vm_nodes.is_empty(), "view models create no nodes and do not use the widget layer %s" % [vm_nodes])
	check(not AppLoader.allowed("C:/Users/x/file.json") and AppLoader.allowed("res://a") and AppLoader.allowed("user://b"), "AppLoader serves only res:// and user://")
	check(AppLoader.json("res://scenes/ui/fixtures/campaign/meta.json") is Dictionary, "AppLoader reads JSON from the pack")
	check(AppLoader.resource("res://scenes/ui/fonts/inter.woff2") != null, "AppLoader loads a font resource")
	# M1-INT: the planet textures go through AppLoader (no disk paths, no env vars); the symbol fonts are fallbacks.
	var pt := FileAccess.get_file_as_string("res://scenes/galaxy/planet_textures.gd")
	var pt_bad := []
	for f in ["FileAccess.", "DirAccess.", "Image.load_from_file", "OS.get_environment", "get_executable_path", "globalize_path", "LocalAssets", "BOTF_"]:
		for line in pt.split("
"):
			if not line.strip_edges().begins_with("#") and line.contains(f):
				pt_bad.append(f)
	check(pt_bad.is_empty(), "planet_textures.gd reads nothing but through AppLoader %s" % [pt_bad])
	check(AppLoader.text("C:/x.json") == "" and AppLoader.text("res://does/not/exist.json") == "", "AppLoader.text refuses disk paths and returns \"\" for a missing file")
	check(not AppLoader.dir_exists("C:/Windows") and AppLoader.dir_exists("res://scenes"), "AppLoader.dir_exists serves the pack only")
	for glyph in [0x25cf, 0x25c7, 0x25b8, 0x25be, 0x2713, 0x2715, 0x2192, 0x221e]:
		check(Lcars.font("ui", 420).has_char(glyph), "the UI font draws U+%04X without a system font" % glyph)

# ---------------------------------------------------------------------------------------------
# Models that need no core
# ---------------------------------------------------------------------------------------------
func _sum(a: Array) -> int:
	var t := 0
	for x in a:
		t += int(x)
	return t

## The preset picker's model: the list, what a pick fills, the text, the options handed to the core. `nc` may have a core's
## `meta.presets` (the full seven) or none (the two built-ins).
func _preset_picker_model(nc: NewCampaignVM, label: String) -> void:
	var ids: Array = nc.presets().map(func(p): return str(p["id"]))
	check(ids.size() >= 2 and ids[0] == "classic" and ids[1] == "remastered", "%s: the picker leads with Classic and Remastered" % label)
	check(nc.preset == "remastered" and nc.preset_def()["name"] == "Remastered" and nc.rules_base() == "remastered", "%s: Remastered is the default preset" % label)
	check(nc.presets().all(func(p): return str(p["blurb"]) != "" and p.has("options") and p.has("lineup") and p.has("lineupPlayable")), "%s: every preset has a description, options and a lineup" % label)
	check(not nc.pick_preset("no-such-preset") and nc.preset == "remastered", "%s: an unknown preset changes nothing" % label)
	nc.size_id = "huge"
	nc.era = 4
	check(nc.pick_preset("classic") and nc.rules_base() == "classic" and nc.size_id == "medium" and nc.era == 1 and nc.shape == "elliptical", "%s: Classic fills size, era and shape" % label)
	check(nc.options()["preset"] == "classic" and nc.options()["shape"] == "elliptical" and int(nc.options()["systems"]) == 50, "%s: the options carry the preset id and the filled values" % label)
	check(not nc.threats_active() and nc.threats_text() == "Threats off" and nc.options()["threats"]["monsters"] == false and nc.options()["threats"]["borg"] == false, "%s: Classic leaves the threat switches off" % label)
	nc.pick_faction(3)
	check(nc.pick_preset("remastered") and nc.shape == "spiral" and nc.powers() >= 3, "%s: a pick never drops the powers in play below the faction" % label)
	if ids.size() < 7:
		check(nc.preset_text("remastered").contains("Powers:") and not nc.preset_text("remastered").contains("Not seated"), "%s: built-in text lists the powers and no pending note" % label)
		return
	check(ids == ["classic", "remastered", "dominion-war", "ages", "galaxies", "balance-of-power", "ultimate"], "%s: seven presets in screen order" % label)
	check(nc.pick_preset("ultimate") and nc.size_id == "large" and nc.shape == "irregular" and nc.difficulty == "hard" and nc.minor_density == "many", "%s: Ultimate fills a hard, large, irregular galaxy" % label)
	check(nc.options()["preset"] == "ultimate" and nc.options()["difficulty"] == "hard" and int(nc.options()["systems"]) == 80, "%s: Ultimate's options reach the core call" % label)
	check(nc.pick_preset("galaxies") and nc.size_id == "huge" and int(nc.options()["systems"]) == 120 and nc.options()["preset"] == "galaxies", "%s: Galaxies is a huge galaxy" % label)
	var gx := nc.preset_text("galaxies")
	check(gx.contains("Borg") and gx.contains("Not seated yet") and gx.contains("MOD-2") and gx.contains("stock powers"), "%s: a lineup the core cannot seat says so, and what it waits for" % label)
	check(nc.preset_text("dominion-war").contains("Dominion") and nc.preset_text("ultimate").contains("Content still to come: MOD-7"), "%s: preset text names its lineup and pending lanes" % label)
	check(not nc.preset_text("ages").contains("Not seated") and not nc.preset_text("ages").contains("MOD-4"), "%s: Ages seats the stock powers and waits for nothing" % label)
	# Era picker (docs/design/ERAS.md): a preset with the eras rule shows the timeline year and its own start level; others keep names.
	check(nc.pick_preset("ages") and int(nc.preset_eras().get("techCap", 0)) == 14, "%s: Ages carries an eras block, cap 14" % label)
	var ages_eras := nc.eras()
	check(ages_eras.size() == 5 and int(ages_eras[0]["year"]) == 2240 and int(ages_eras[4]["year"]) == 2380 and int(ages_eras[1]["level"]) == 3 and int(ages_eras[4]["level"]) == 11 and str(ages_eras[1]["label"]) == "2280  T3", "%s: Ages eras are 2240..2380 at tech 1/3/5/8/11" % label)
	nc.era = 2
	check(nc.era_name() == "2280 start" and nc.summary()[1].contains("2280 start"), "%s: the summary names the start year" % label)
	check(nc.pick_preset("galaxies") and int(nc.eras()[4]["level"]) == 14 and int(nc.eras()[3]["level"]) == 10 and int(nc.eras()[1]["level"]) == 4, "%s: Galaxies eras start at tech 1/4/7/10/14" % label)
	check(nc.pick_preset("remastered") and nc.preset_eras().is_empty() and int(nc.eras()[0]["year"]) == 0 and str(nc.eras()[0]["label"]).begins_with("Early T") and int(nc.eras()[4]["level"]) == 10, "%s: Remastered keeps the era names and classic levels" % label)
	check(nc.summary()[1].contains("Galaxies preset"), "%s: the summary names the preset" % label)
	check(nc.pick_preset("balance-of-power") and nc.rules_base() == "classic" and nc.size_id == "large", "%s: Balance of Power sits on the classic rules" % label)
	# Threat switches (docs/design/THREAT-ACTORS.md): the preset fills them, the panel edits them, the core call carries them.
	check(nc.pick_preset("dominion-war") and nc.threats_active() and bool(nc.threats["borgSphere"]) and bool(nc.threats["species8472"]) and bool(nc.threats["edoStation"]), "%s: Dominion War turns on the sphere, 8472 and DS9" % label)
	var ot: Dictionary = nc.options()["threats"]
	check(bool(ot["monsters"]) and bool(ot["borg"]) and ot["frequency"] == "normal" and (ot["disabled"] as Array).is_empty(), "%s: the options carry the threat switches" % label)
	check(nc.threats_text().contains("Borg sphere") and nc.threats_text().contains("monsters"), "%s: the summary line reports the threats" % label)
	nc.threats["borg"] = false
	nc.toggle_threat_kind("calamarain")
	check(not nc.threat_kind_on("calamarain") and nc.threat_kind_on("chodak") and (nc.options()["threats"]["disabled"] as Array).has("calamarain") and not bool(nc.options()["threats"]["borg"]), "%s: the Borg-only off switch and a per-actor toggle reach the options" % label)
	check(nc.threat_kinds().size() == 12 and nc.threat_kinds().all(func(k): return str(k["key"]) != "" and str(k["name"]) != ""), "%s: twelve toggleable actors" % label)
	check(nc.pick_preset("remastered") and not nc.threats_active() and (nc.threats["disabled"] as Array).is_empty(), "%s: picking Remastered clears the threat switches" % label)
	check(nc.pick_preset("galaxies") and bool(nc.threats["monsters"]) and bool(nc.threats["borg"]) and not bool(nc.threats["borgSphere"]), "%s: Galaxies runs the vanilla actors" % label)

func _pure_models() -> void:
	print("== pure models")
	# campaign options: the empire alias (M0 bug)
	var o := AppSession.campaign_options({"empire": 3, "era": 2})
	check(int(o.get("faction", 0)) == 3 and not o.has("empire"), "campaign_options maps empire to faction")
	o = AppSession.campaign_options({"empire": 3, "faction": 2})
	check(int(o["faction"]) == 2, "faction wins over empire when both are given")
	o = AppSession.campaign_options({"faction": 4, "empires": 2})
	check(int(o["empires"]) == 4, "powers in play cover the chosen power")
	# research
	var a := ResearchVM.rebalance([20, 20, 20, 20, 10, 10], 0, 50, [false, false, false, false, false, false])
	check(_sum(a) == 100 and int(a[0]) == 50, "rebalance keeps 100% and sets the field")
	a = ResearchVM.rebalance([20, 20, 20, 20, 10, 10], 0, 90, [false, true, false, false, false, false])
	check(_sum(a) == 100 and int(a[1]) == 20 and int(a[0]) == 80, "rebalance never moves a locked field and caps the slider")
	check(ResearchVM.focus_alloc(2) == [10, 10, 50, 10, 10, 10], "focus gives 50% to one field")
	# new campaign
	var nc := NewCampaignVM.new(null)
	nc.empires = 2
	nc.pick_faction(4)
	check(nc.powers() == 4 and int(nc.options()["faction"]) == 4 and int(nc.options()["empires"]) == 4, "picking a power raises the powers in play")
	nc.size_id = "huge"
	check(int(nc.options()["width"]) == 34 and int(nc.options()["height"]) == 24 and int(nc.options()["systems"]) == 120, "galaxy size feeds the options")
	nc.size_id = "medium"
	check(NewCampaignVM.new(null).size_id == "medium" and int(nc.options()["systems"]) == 50, "Medium is the default and holds 50 systems")
	for sz in NewCampaignVM.SIZES:
		var dens := float(sz["systems"]) / float(int(sz["width"]) * int(sz["height"]))
		check(dens > 0.13 and dens < 0.16 and int(sz["width"]) * int(sz["height"]) >= int(sz["systems"]), "size %s keeps the star density near 0.145 (%.3f)" % [sz["id"], dens])
	_preset_picker_model(NewCampaignVM.new(null), "no core")
	# diplomacy
	check(DiplomacyVM.deadline(10, 10)[1] == "alert" and DiplomacyVM.deadline(10, 13)[1] == "dim", "deadline tones")
	check(DiplomacyVM.proposal_title({"kind": "tribute", "creditDemand": 1500}) == "Tribute demand: ¢1,500", "tribute title formats credits")
	check(DiplomacyVM.status({"contact": true, "atWar": true, "tradeTreaty": true}) == "At war · trade pact", "relation status line")
	check(DiplomacyVM.event_tone(6) == "alert" and DiplomacyVM.event_tone(1) == "good" and DiplomacyVM.event_tone(99) == "dim", "event tones")
	# empire
	check(EmpireVM.minor_standing({"attitude": 40}, true)["bad"] and not EmpireVM.minor_standing({"attitude": 60}, true)["bad"], "secession margin under 15 is at risk")
	check(EmpireVM.minor_standing({"attitude": 79}, false)["bad"], "a candidate below the join line is short")
	check(EmpireVM.governor_note({"warning": "x", "workforce": "Manual"}) == ["x", true], "governor warning wins the note")
	# fleets
	var t := FleetsVM.totals([{"hull": 5, "maxHull": 10, "shields": 1, "maxShields": 2, "range": 4}, {"hull": 3, "maxHull": 6, "shields": 0, "maxShields": 2, "range": 2, "maintenance": 3}])
	check(t["hull"] == 8 and t["max_hull"] == 16 and t["range"] == 2 and t["upkeep"] == 3, "unit totals sum members and take the shortest range")
	# combat
	check(CombatVM.is_active({"alive": true, "active": true}) and not CombatVM.is_active({"alive": true, "active": true, "retreated": true}), "active ships")
	check(CombatVM.short_name({"name": "Galaxy — USS X"}) == "Galaxy" and CombatVM.short_name({"hidden": true, "name": "x"}) == "Unidentified vessel", "short names")
	var e := {"ships": [{"id": 1, "name": "A"}, {"id": 2, "name": "B"}]}
	var log := CombatVM.round_log(e, {"salvos": [{"from": 1, "to": 2, "beam": 3, "torpedo": 4}], "lost": [2], "ended": true})
	check(log.size() == 3 and log[0][1] == "A → B: 7 damage" and log[1][0] == "alert", "round log lines")
	# rules 4 (WEGO) helpers
	var w4 := {"wego": {"canWarp": false, "crew": "Veteran", "facings": [1, 2, 3, 4, 5, 6], "subsystems": {"weapons": 80}}}
	check(CombatVM.wego_of(w4)["crew"] == "Veteran" and CombatVM.wego_of({}).is_empty(), "wego block read, empty under rules 1-3")
	check(CombatVM.retreat_hidden(w4) and not CombatVM.retreat_hidden({"wego": {"canWarp": true}}) and not CombatVM.retreat_hidden({}), "Retreat hidden only when a rules 4 ship cannot warp")
	check(CombatVM.FACINGS.size() == 6 and CombatVM.SUBSYSTEMS.size() == 5, "six facings, five subsystems")
	var cvm := CombatVM.new(null)
	cvm.set_subsystem("engines")
	check(cvm.subsystem == "engines", "aim set")
	cvm.set_subsystem("bogus")
	check(cvm.subsystem == "none", "an unknown aim clears it")
	var cs := CombatVM.card_state({"alive": true, "beam": 2, "torpedo": 1, "next": {"maneuver": "Engage", "target": 2, "source": "order"}}, e["ships"], false)
	check(cs["line"] == "next: Engage → B #2  (ordered)" and cs["ordered"] and cs["flags"] == "B2 · T1", "ship card state")
	check(CombatVM.hail_outlook({"hail": {"accepts": true}}, "Klingons")[1] == "good", "hail outlook")
	var f := CombatVM.force([{"hull": 10, "maxHull": 20, "alive": true}, {"hidden": true, "hull": 5, "maxHull": 5, "alive": true}])
	check(f["hull"] == 10 and f["max"] == 20 and f["alive"] == 2, "force totals skip hidden hulls")
	# overlays
	check(OverlaysVM.categories([{"category": "research"}, {"category": "zzz"}, {"category": "combat"}]) == ["combat", "research", "zzz"], "briefing categories in display order")
	# system
	check(SystemVM.growth_text({"enabled": false}) == "fixed" and SystemVM.growth_text({"enabled": true, "min": 2, "max": 2}) == "+2", "growth text")
	check(SystemVM.forecast_growth_text({"min": -1, "max": 3}) == "-1..+3", "forecast growth text")
	DisplaySettings.web_preview = true
	check(not DisplaySettings.offered("display.windowMode") and not DisplaySettings.offered("display.renderScale") and DisplaySettings.offered("display.shadows"), "the web build hides window, frame-pacing and FSR settings")
	DisplaySettings.web_preview = false
	check(DisplaySettings.offered("display.windowMode"), "the desktop build offers every display setting")
	check(Fmt.fmt(1234567) == "1,234,567" and Fmt.signed(5) == "+5" and Fmt.signed(-5) == "-5", "number formats")

# ---------------------------------------------------------------------------------------------
# Models over a live session
# ---------------------------------------------------------------------------------------------
func _session_models(backend: String) -> void:
	var s := AppSession.create(backend)
	var errors := []
	s.failed.connect(func(m): errors.append(m))
	check(s.meta.get("empires", []).size() == 8, "session reads meta")
	var v := s.start("newCampaign", AppSession.campaign_options({"empire": 2, "seed": 424242, "era": 3, "preset": "remastered"}))
	check(not v.is_empty(), "newCampaign starts")
	if v.is_empty():
		return
	var changes := [0]
	s.view_changed.connect(func(): changes[0] += 1)
	s.enter(v)
	check(s.player() == 2, "the empire option makes the player the Klingon Empire")
	check(int(s.selection["systemId"]) != 0, "entering selects the first colony")
	# content pack loaded at boot (docs/DATA-FORMAT.md section 10)
	check(FileAccess.file_exists(AppSession.BASE_PACK) or not s.content.is_empty() or s.content_note != "", "boot tried the base content pack")
	if FileAccess.file_exists(AppSession.BASE_PACK):
		check(str(s.content.get("id", "")) == "botf-base", "boot installed the base content pack through loadContent (%s, %s)" % [s.content.get("id", "?"), backend])
		if backend != "fixture":
			check(str(s.content.get("checksum", "")).length() == 16 and bool(s.content.get("base", false)), "the core reports the base pack checksum %s" % s.content.get("checksum", ""))
	else:
		check(s.content_note.contains("no content pack"), "a missing pack is recorded, the embedded base stays")
	var gal := GalaxyVM.new(s)
	var ga := gal.alerts()
	check(ga["count"] == s.view.get("alerts", []).size() and ["good", "warn", "alert"].has(ga["tone"]), "galaxy alerts: %d, tone %s" % [ga["count"], ga["tone"]])
	check(gal.layers({"fog": false}).size() == 7 and not gal.layers({"fog": false}).filter(func(l): return l["key"] == "fog")[0]["on"], "galaxy layers carry their state")
	check(gal.orders()["count"] == s.view.get("pending", []).size() and gal.legend().size() == 6 and gal.status().size() == 4, "galaxy orders, legend and status")
	check(gal.alert_target({"kind": "proposal"})["id"] == "diplomacy" and gal.alert_target({"kind": "event"})["do"] == "overlay" and gal.alert_target({"kind": "x"}).is_empty(), "alert targets")
	var sys0: Dictionary = s.view["systems"][0]
	check(gal.alert_target({"systemId": sys0["id"]})["x"] == int(sys0["x"]), "an alert on a system points at its sector")
	# shell
	var shell := ShellVM.new(s)
	check(shell.stats().size() == 7 and shell.stats()[0]["cap"] == "Turn", "shell stats")
	check(shell.end_turn_block() == "" and shell.rail_extras().size() == 3, "shell end turn and rail")
	check(str(shell.heading()[1]).find("stardate " + TrekText.stardate_text(int(s.view["meta"]["turn"]))) >= 0, "the top bar prints the one stardate formula")
	var log_entries := OverlaysVM.new(s).log_entries()
	var log_ok := true
	for le in log_entries:
		log_ok = log_ok and str(le.get("voice", "")) != "" and str(le["voice"]).find("{") < 0
	check(log_ok, "every briefing entry has a voiced log line")
	# research
	var r := ResearchVM.new(s)
	r.fetch()
	check(r.available() and r.fields().size() == 6, "research has six fields")
	check(r.cap() == 10 and (r.fields()[0]["levels"] as Array).size() == 11, "a Remastered campaign's research ladder stops at level 10")
	check(r.apply_block() == "No changes", "no allocation change, no apply")
	r.slide(0, 50)
	check(r.total() == 100 and r.dirty() and r.apply_block() == "", "a slide keeps 100% and arms Apply")
	var before: int = s.view["pending"].size()
	check(r.apply() and s.view["pending"].any(func(p): return p["type"] == "AllocateResearch"), "apply drafts AllocateResearch")
	check(changes[0] >= 1, "an order refreshes the view")
	r.fetch()
	r.slide(1, 40)
	r.apply()
	check(s.view["pending"].filter(func(p): return p["type"] == "AllocateResearch").size() == 1, "a new allocation replaces the drafted one")
	# empire
	var em := EmpireVM.new(s)
	var tot := em.totals()
	var pop := 0
	for c in s.view["colonies"]:
		pop += int(c["population"])
	check(int(tot["pop"]) == pop, "empire totals sum the colonies")
	check(em.tab_labels().size() == 7 and str(em.tab_labels()[0][1]).begins_with("Colonies  "), "empire tabs carry counts")
	# fleets
	var fl := FleetsVM.new(s)
	var units := fl.units()
	var tf_extra := 0
	for t in s.view.get("taskForces", []):
		tf_extra += t["fleetIds"].size() - 1
	check(units.size() == s.view["fleets"].size() - tf_extra, "fleets group task forces into one unit")
	check(fl.form_block() != "", "forming a task force needs two marks")
	if units.size() > 0:
		check(fl.status(units[0]["lead"]) != "", "unit status line")
		fl.dest = 0
		check(fl.send_plan(units[0]["lead"])["block"] == "Choose a destination", "send-to needs a destination")
	# system
	var sy := SystemVM.new(s)
	var sid := sy.system_id()
	check(sid != 0 and sy.fetch(sid), "system loads the colony")
	if sid != 0 and not sy.detail.is_empty():
		var popc := int(sy.colony()["population"])
		var got := sy.set_workers("food", 10000)
		check(got <= popc and got <= int(sy.detail["capacityLimits"]["food"]), "workers are clamped to jobs and population")
		check(sy.used() <= popc, "the draft never exceeds the population")
		var ch: Array = sy.choices(false).filter(func(x): return sy.choice_block(x) == "")
		if ch.size() > 0:
			check(sy.enqueue(ch[0]) and s.view["pending"].any(func(p): return p["type"] == "EnqueueBuilding"), "enqueue drafts EnqueueBuilding")
	# intel
	var it := IntelVM.new(s)
	it.fetch()
	if it.available() and it.intel.get("rivals", []).size() > 0:
		var rid := int(it.intel["rivals"][0]["id"])
		var set := it.set_share(rid, "espionage", 300)
		var ops := 0
		for k in it.budget["rivals"]:
			ops += int(it.budget["rivals"][k]["espionage"]) + int(it.budget["rivals"][k]["sabotage"])
		check(set <= 100 and int(it.budget["security"]) == 100 - ops, "intel shares are clamped and security takes the rest")
		check(it.dirty() or set == int(it.original["rivals"][rid]["espionage"]), "a changed share makes the budget dirty")
	else:
		check(true, "intel has no rivals on this backend (skipped)")
	# diplomacy
	var dp := DiplomacyVM.new(s)
	check(dp.fetch(), "diplomacy loads")
	check(dp.pane in ["inbox", "dossier"], "diplomacy opens a pane")
	if not dp.target.is_empty():
		var ds := dp.dossier()
		check(ds["id"] == int(dp.target["id"]) and dp.is_selected(bool(dp.target["minor"]), int(dp.target["id"])), "dossier follows the selection")
	# database
	var db := DatabaseVM.new(s)
	db.init_faction()
	check(db.faction == "Klingon", "the database starts on the player's faction")
	var rows := db.rows()
	check(rows.size() > 0 and rows.all(func(x): return db.facets(db.entry("ships", int(x["id"])))["faction"] == "Klingon"), "faction filter keeps only Klingon ships")
	db.faction = ""
	check(db.rows().size() > rows.size(), "clearing the filter lists more ships")
	db.pick_tab("technologies")
	check(db.rows().size() > 0 and db.selected == -1, "tech tab lists technologies")
	# combat
	var cb := CombatVM.new(s)
	check(not cb.live() and cb.replays() is Array, "no battle: the archive")
	# overlays and end-turn check
	var ov := OverlaysVM.new(s)
	check(ov.end_summary("victory")["won"] and ov.end_summary("defeat")["stats"].size() == 8, "end-of-game summary")
	var chk := EndTurnVM.build(s)
	check(chk.has("warnings") and chk.has("forecast") and chk.has("alerts"), "end-turn check builds")
	_m1_ui_models(s)
	check(errors.is_empty(), "no core errors in the session %s" % [errors])

## M1-UI (docs/UI-DESIGN-SYSTEM.md): every key figure carries an explanation, the command palette lists every
## screen, colony and fleet, the Empire dashboard models and the WS02 options.
func _m1_ui_models(s: AppSession) -> void:
	var shell := ShellVM.new(s)
	var explained := true
	for st in shell.stats():
		explained = explained and st.has("icon") and st["explain"] is Dictionary and str(st["explain"].get("title", "")) != "" and str(st["explain"].get("effect", "")) != ""
	check(explained, "every top-bar figure has an icon and a cause-and-effect explanation")
	var credits: Dictionary = shell.stats()[1]["explain"]
	check(credits["parts"].size() == 5 and str(credits.get("forecast", "")) != "", "the treasury explains its parts and forecasts")
	var items := shell.palette_items()
	var ids := items.map(func(i): return str(i["id"]))
	var screens_ok := true
	for r in AppRoutes.GAME_SCREENS:
		screens_ok = screens_ok and ids.has("screen:" + r["id"])
	check(screens_ok, "the palette reaches every game screen")
	check(ids.has("overlay:events") and ids.has("overlay:victory") and ids.has("overlay:briefing") and ids.has("overlay:settings") and ids.has("cmd:saves"), "the palette reaches every overlay and the saves")
	check(ids.has("colony:%d" % int(s.view["colonies"][0]["systemId"])) and ids.has("fleet:%d" % int(s.view["fleets"][0]["id"])), "the palette lists colonies and fleets")
	check(CommandPalette.filter(items, "resea").size() >= 1 and str(CommandPalette.filter(items, "resea")[0]["id"]) == "screen:research", "palette search ranks a label prefix first")
	check(CommandPalette.filter(items, "zzzz-none").is_empty(), "palette search with no match is empty")
	var sy := SystemVM.new(s)
	check(sy.fetch(sy.system_id()), "system fetch")
	var k := sy.kpis()
	check(k.size() == 8 and k.all(func(x): return x["explain"] is Dictionary and x["explain"].has("effect")), "the colony's eight figures are explained")
	var em := EmpireVM.new(s)
	check(em.kpis().size() == 8 and em.contacts().size() == 4, "empire dashboard: eight figures, four other powers")
	check(em.colony_cards().size() == s.view["colonies"].size() and str(em.colony_cards()[0]["cls"]) != "", "a card per colony with its main planet class")
	check(em.income_parts().size() == 3 and int(em.income_parts()[2][1]) <= 0, "income parts: ship upkeep is negative")
	var nc := NewCampaignVM.new(s)
	nc.shape = "ring"
	nc.minor_density = "many"
	nc.difficulty = "hard"
	var o := nc.options()
	check(o["shape"] == "ring" and o["minorDensity"] == "many" and o["difficulty"] == "hard", "new campaign passes shape, minor density and difficulty")
	check(nc.minors_count() > 0 and nc.minors_count() <= 29, "minor count follows the density, capped at 29")
	nc.minor_density = "none"
	check(nc.minors_count() == 0, "no minor races at density none")
	check(NewCampaignVM.core_reads("seed") and NewCampaignVM.core_reads("shape") and NewCampaignVM.core_reads("minorDensity") and NewCampaignVM.core_reads("difficulty"), "the core reads shape, minor density and difficulty now: no option is marked pending")
	_preset_picker_model(NewCampaignVM.new(s), "core")
	nc.size_id = "medium"
	for d in [["none", 0], ["few", 7], ["some", 12], ["many", 17]]:
		nc.minor_density = d[0]
		check(nc.minors_count() == d[1], "medium %s gives the vanilla count %d" % [d[0], d[1]])

# ---------------------------------------------------------------------------------------------
# App.start_campaign honours the empire option (M0 bug: it was ignored and the player got the Federation)
# ---------------------------------------------------------------------------------------------
func _start_campaign_empire() -> void:
	print("== App.start_campaign empire option")
	var backends := ["fixture"]
	if Engine.has_singleton("BotfCore"):
		backends.append("")
	for b in backends:
		App.forced_backend = b
		var app: App = AppLoader.resource("res://scenes/ui/app.tscn").instantiate()
		app.size = Vector2(1920, 1080)
		app.settings["autosave"] = false
		root.add_child(app)
		await process_frame
		app.settings["autosave"] = false
		for empire in [3, 5]:
			var ok := app.start_campaign({"empire": empire, "era": 3, "seed": 424242, "preset": "remastered"})
			await process_frame
			check(ok and app.player() == empire, "%s: start_campaign({empire: %d}) plays empire %d (got %d)" % [app.core.backend, empire, empire, app.player()])
		var ok2 := app.start_campaign({"faction": 4, "era": 3, "seed": 424242, "preset": "remastered"})
		await process_frame
		check(ok2 and app.player() == 4, "%s: start_campaign({faction: 4}) still works" % app.core.backend)
		app.queue_free()
		await process_frame
	App.forced_backend = ""
