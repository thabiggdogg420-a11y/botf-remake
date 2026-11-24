extends SceneTree
## Onboarding (ONBOARDING-AND-MOBILE-PLAN.md section 1): Quick Start and its settings, the five beats, the event beats, old saved
## tutorial settings, the tab reveal, the wordiness pass and the top-bar tooltips.
##   godot --headless --path remaster/godot --audio-driver Dummy -s res://scenes/ui/tests/test_onboarding.gd
## Exit code 0 on success, 1 on any failure.

var failures := 0
var checks := 0
var app: App

func _initialize() -> void:
	_run.call_deferred()

func check(cond: bool, what: String) -> void:
	checks += 1
	if cond:
		print("  ok  ", what)
	else:
		failures += 1
		printerr("  FAIL ", what)

func frames(n: int = 2) -> void:
	for k in n:
		await process_frame

func _words(t: String) -> int:
	var n := 0
	for tok in t.split(" ", false):
		if String(tok).strip_edges() != "":
			n += 1
	return n

func _new_app() -> void:
	if app:
		app.queue_free()
		await frames()
	App.forced_backend = "" if Engine.has_singleton("BotfCore") else "fixture"
	root.size = Vector2i(1920, 1080)
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	await frames(4)

func _run() -> void:
	var native := Engine.has_singleton("BotfCore")
	print("onboarding (%s backend)" % ("native" if native else "fixture"))
	await _new_app()
	_quick_options()
	await _title(native)
	_old_settings()
	await _beats(native)
	await _event_beats(native)
	await _tab_reveal(native)
	_wordiness()
	await _tooltips()
	if native:
		await _hold_back()
	print("%d checks, %d failures" % [checks, failures])
	quit(1 if failures > 0 else 0)

# ---------------------------------------------------------------------------------------------
func _quick_options() -> void:
	var q := NewCampaignVM.quick(app.session, 2)
	var o := q.quick_options()
	check(int(o["faction"]) == 2 and int(o["systems"]) == 30 and int(o["width"]) == 16 and int(o["height"]) == 13, "Quick Start: the Small galaxy (16x13, 30 systems) for the power picked")
	check(int(o["era"]) == 1, "Quick Start: the Early era")
	check(str(o["preset"]) == "classic" and str(o["ruleset"]) == "reference", "Quick Start: classic rules (the Classic preset)")
	check(str(o["difficulty"]) == "normal" and bool(o["aiAggression"]), "Quick Start: the normal AI, rivals still aggressive")
	check(int(o["aiWarHoldTurn"]) == 40 and int(NewCampaignVM.QUICK_AI_WAR_TURN) >= 40, "Quick Start: the AI's first war is held to turn 40")
	check(bool(o["quickStart"]), "Quick Start: marks the tutorial game")
	var c := NewCampaignVM.new(app.session).options()
	check(not c.has("aiWarHoldTurn") and not c.has("quickStart"), "Custom game options never carry the hold-back")

func _title(native: bool) -> void:
	app.goto("title", false)
	await frames(4)
	var t := app.current_screen()
	var qs := t.find_child("QuickStart", true, false) as Button
	check(qs != null and qs.text.strip_edges().to_lower() == "quick start", "the title offers Quick Start (%s)" % (qs.text if qs else "missing"))
	var nc := t.find_child("NewCampaign", true, false) as Button
	check(nc != null and nc.text.strip_edges().to_lower() == "custom game", "New campaign is relabelled Custom game (%s)" % (nc.text if nc else "missing"))
	for id in range(1, 6):
		check(t.find_child("Emblem%d" % id, true, false) is Button, "emblem %d can be tapped" % id)
	# tap the Klingon emblem, then Quick Start
	(t.find_child("Emblem2", true, false) as Button).pressed.emit()
	check(t.quick_faction() == 2 and (t.find_child("Emblem2", true, false) as Button).button_pressed and not (t.find_child("Emblem1", true, false) as Button).button_pressed, "tapping an emblem picks it")
	t.quick_seed = 31337
	app.settings["tutorial"] = -1   # a player who finished the tutorial before: Quick Start turns it back on
	t.quick_start()
	await frames(6)
	check(not app.view.is_empty() and app.screen == "galaxy", "Quick Start lands in the galaxy")
	if not app.view.is_empty():
		var m: Dictionary = app.view["meta"]
		check(int(m["player"]) == 2, "it plays the power tapped (%s)" % str(m["playerName"]))
		check(app.view["systems"].size() == 30 or not native, "Small galaxy: %d systems" % app.view["systems"].size())
		check(int(m["era"]) == 1 and str(m["eraName"]) == "Early", "Early era")
		check(str(m["rules"].get("presetName", "")) == "Classic" or not native, "Classic rules (%s)" % str(m["rules"].get("presetName", "")))
	check(app.tutorial_game and not app.tutorial_held, "it is the tutorial game")
	check(app.advisor_panel.advisor.tutorial_active() and app.advisor_panel.advisor.tutorial_step() == 0, "the tutorial is on at beat 1, whatever it was before")
	# a Custom game is not a tutorial game
	app.goto("title", false)
	await frames(2)
	app.goto("new", false)
	await frames(4)
	var ncs := app.current_screen()
	ncs.faction = 1
	ncs.seed_value = 31337
	ncs.start()
	await frames(6)
	check(not app.tutorial_game, "a Custom game is not the tutorial game")

func _old_settings() -> void:
	var adv := app.advisor_panel.advisor
	var cases := {0: 0, 1: 1, 2: 1, 3: 1, 4: 0, 5: 2, 6: 3, 7: -1, 8: -1, 99: -1}
	for old in cases:
		app.settings["tutorial"] = old
		app.settings.erase("tutorialBeats")
		var got := adv.tutorial_step()
		check(got == int(cases[old]), "old saved tutorial step %d maps to beat %d (got %d)" % [old, int(cases[old]), got])
	app.settings["tutorial"] = -1
	app.settings.erase("tutorialBeats")
	check(adv.tutorial_step() == -1 and not adv.tutorial_active(), "a finished old tutorial stays finished")
	app.settings["tutorial"] = 2
	app.settings["tutorialBeats"] = 1
	check(adv.tutorial_step() == 2, "a current setting is never remapped twice")

func _quick_game(seed_: int) -> void:
	await _new_app()
	app.goto("title", false)
	await frames(2)
	var t := app.current_screen()
	t.quick_seed = seed_
	t.quick_start()
	await frames(6)

func _beats(native: bool) -> void:
	await _quick_game(31337)
	var adv := app.advisor_panel.advisor
	var panel := app.advisor_panel
	check(adv.STEPS.size() == 5, "the tutorial is five beats")
	for st in adv.STEPS:
		check(_words(str(st["text"])) <= 12, "beat %s is one line of at most 12 words (%d)" % [st["id"], _words(str(st["text"]))])
	panel.update()
	await frames(2)
	var g := panel.current
	check(str(g.get("id", "")) == "scout" and g.get("beat", false), "beat 1 is the scout")
	check(str(g.get("text", "")) == "Send your scout to explore. Tap it, then a star.", "beat 1 says the plan's line")
	var spot := app.find_child("TutorialSpotlight", true, false) as Spotlight
	check(spot != null and spot.visible and spot.hole.size.x > 0, "a spotlight dims the screen except the scout")
	check(panel.find_child("AdvisorAct", true, false) == null, "a beat has no Next button")
	check(panel.find_child("AdvisorDismiss", true, false) != null, "Skip tutorial is on the card")
	# pressing nothing moves nothing: polls do not advance a beat
	for k in 5:
		panel.update()
	check(str(panel.current.get("id", "")) == "scout", "the beat waits for the player")
	# the player does it: select the scout, then a click on a star plots the course
	var scout: Dictionary = adv._scout()
	check(not scout.is_empty(), "the player has a scout")
	app.select_fleet(int(scout["id"]))
	panel.update()
	var rects := panel.target_rects()
	check(rects.size() == 1, "once the scout is selected the spotlight moves to a star")
	var t: Dictionary = adv._explore_target()
	var cell: int = int(t["y"]) * app.width() + int(t["x"])
	check(adv.beat_click(cell), "a click on a star sends the selected scout there")
	await frames(2)
	panel.update()
	check(str(panel.current.get("id", "")) == "build", "the course set, beat 2 (build) follows")
	# opening the system alone is not enough; queueing a build is
	app.goto("system")
	await frames(3)
	panel.update()
	check(str(panel.current.get("id", "")) == "build", "opening the system alone does not finish beat 2")
	var tg := panel.target_rects()
	check(tg.size() == 1, "on the system screen beat 2 lights the first build choice")
	var before_n: int = adv._builds()
	var choice: Button = app.current_screen().find_children("Choice_*", "Button", true, false).filter(func(b): return not (b as Button).disabled)[0]
	choice.pressed.emit()
	await frames(3)
	check(adv._builds() == before_n + 1, "queueing a build is what finishes beat 2")
	panel.update()
	check(str(panel.current.get("id", "")) == "end_turn", "beat 3 is End Turn")
	app.goto("galaxy")
	await frames(3)
	panel.update()
	check(panel.target_rects().size() == 1 and str(adv.targets(panel.current)[0]) == "EndTurn", "beat 3 spotlights the End turn button")
	var t0 := adv.turn()
	app.request_end_turn()
	await frames(4)
	check(adv.turn() == t0 + 1, "End turn ended the turn")
	panel.update()
	await frames(2)
	check(str(panel.current.get("id", "")) == "log" and app.has_modal(), "beat 4 is the captain's log, open (%s, modal %s, screen %s)" % [str(panel.current.get("id", "")), str(app.has_modal()), app.screen])
	check(panel.target_rects().size() == 1, "beat 4 lights the close button")
	var log_rows: int = app._modals.back().find_children("*", "Button", true, false).size()
	check(log_rows <= 4 + 3, "the tutorial's log is short (%d buttons)" % log_rows)
	app.close_all_modals()
	await frames(2)
	panel.update()
	var id4 := str(panel.current.get("id", ""))
	check(id4 == "colony" or id4 == "", "closing the log finishes beat 4 (next: %s)" % id4)
	# skipping
	adv.skip_tutorial()
	panel.update()
	check(not adv.tutorial_active() and app.settings["tutorial"] == -1, "Skip tutorial ends it")
	adv.restart_tutorial()
	check(adv.tutorial_active() and adv.tutorial_step() == 0, "Restart tutorial starts again at beat 1")
	# a loaded campaign shows no tutorial
	var blob := app.session.save_blob()
	var saved_name := "onboarding-test"
	app.settings["tutorial"] = 0
	check(app.load_game({"data": blob.get("data", ""), "scenario": blob.get("scenario", "campaign"), "name": saved_name}), "the save loads")
	await frames(4)
	check(app.tutorial_held and not app.advisor_panel.advisor.tutorial_active(), "a loaded campaign shows no tutorial")
	panel.update()
	check(not (panel.current.get("beat", false)), "no beat on a loaded campaign")
	check(not app.tutorial_game, "a loaded campaign is not the tutorial game")



func _end_turns_until_contact(limit: int) -> int:
	var n := 0
	while not TabReveal.contact_made(app.view) and n < limit and app.screen != "combat":
		app.close_all_modals()
		app.end_turn()
		await frames(3)
		n += 1
	app.close_all_modals()
	await frames(2)
	return n

## Does what each beat asks, as the player would, until the tutorial is over.
func _play_tutorial() -> void:
	var panel := app.advisor_panel
	for k in 40:
		panel.update()
		await frames(2)
		var g := panel.current
		if not g.get("beat", false) or g.get("event", false):
			break
		panel.advisor.perform(g)
		await frames(3)

func _event_beats(native: bool) -> void:
	await _quick_game(31337)
	var adv := app.advisor_panel.advisor
	var panel := app.advisor_panel
	check(not TabReveal.contact_made(app.view), "no contact at turn 0")
	panel.update()
	check(str(panel.current.get("id", "")) != "contact", "no contact line before contact")
	await _play_tutorial()
	check(not adv.tutorial_active(), "the five beats done (turn %d)" % adv.turn())
	var n: int = await _end_turns_until_contact(12)
	check(TabReveal.contact_made(app.view), "first contact on turn %d" % adv.turn())
	panel.update()
	var g := panel.current
	check(str(g.get("id", "")) == "contact" and g.get("event", false) and g.get("beat", false), "first contact fires its beat (%s)" % str(g.get("text", "")))
	check(_words(str(g.get("text", ""))) <= 12, "the contact line is 12 words or fewer (%d)" % _words(str(g.get("text", ""))))
	check(panel.target_rects().size() == 1 and str(adv.targets(g)[0]) == "Rail_diplomacy", "it points at the diplomacy tab")
	app.goto("diplomacy")
	await frames(3)
	panel.update()
	app.goto("galaxy")
	await frames(3)
	panel.update()
	check(str(panel.current.get("id", "")) != "contact", "once Diplomacy has been opened the contact line is taught and gone")
	# the first battle: one short line, once
	await _new_app()
	app.settings["tutorial"] = 0
	app.settings["tutorialBeats"] = 1
	app.start_drill("newBattleDrill", "Tactical drill")
	await frames(6)
	app.end_turn()
	for k in 600:
		await frames(1)
		if app.screen == "combat" and app.advisor_panel.visible:
			break
	panel = app.advisor_panel
	adv = panel.advisor
	panel.update()
	g = panel.current
	check(app.screen == "combat" and str(g.get("id", "")) == "battle" and g.get("event", false), "the first battle fires its beat (%s)" % str(g.get("text", "")))
	check(_words(str(g.get("text", ""))) <= 12, "the battle line is 12 words or fewer (%d)" % _words(str(g.get("text", ""))))
	check(panel.target_rects().size() == 1, "it points at Auto-resolve")
	adv.skip_tutorial()
	panel.update()
	check(not panel.current.get("event", false) and str(panel.current.get("id", "")) == "battle", "after Skip tutorial a battle gets the ordinary card, not a lesson")

func _rail(id: String) -> Button:
	return app.find_child("Rail_" + id, true, false) as Button

func _tab_reveal(native: bool) -> void:
	await _quick_game(31337)
	for id in ["empire", "diplomacy", "intel", "database"]:
		var b := _rail(id)
		check(b != null and b.modulate.a < 0.6, "tutorial game, turn 0: the %s tab is dimmed" % id)
	for id in ["galaxy", "system", "research", "fleets"]:
		check(_rail(id) != null and _rail(id).modulate.a > 0.99, "the %s tab is not dimmed" % id)
	app.goto("diplomacy")
	await frames(3)
	check(app.screen == "diplomacy", "a dimmed tab still opens")
	app.goto("galaxy")
	await frames(3)
	var n: int = await _end_turns_until_contact(12)
	check(TabReveal.contact_made(app.view), "contact on turn %d" % app.advisor_panel.advisor.turn())
	await frames(2)
	for id in ["diplomacy", "intel", "empire", "database"]:
		check(_rail(id).modulate.a > 0.99, "after contact the %s tab is lit" % id)
		check(_rail(id).find_child("NewMark", true, false) != null, "and carries the new mark")
	app.goto("diplomacy")
	await frames(3)
	app.goto("galaxy")
	await frames(3)
	check(_rail("diplomacy").find_child("NewMark", true, false) == null, "opening the tab clears its new mark")
	check(_rail("intel").find_child("NewMark", true, false) != null, "the other tabs keep theirs until opened")
	# a Custom game shows every tab as it always did
	await _new_app()
	app.goto("new", false)
	await frames(4)
	var ncs := app.current_screen()
	ncs.faction = 1
	ncs.seed_value = 31337
	ncs.start()
	await frames(6)
	var lit := true
	var marks := 0
	for s in AppRoutes.GAME_SCREENS:
		var b := _rail(str(s["id"]))
		lit = lit and b != null and b.modulate.a > 0.99
		marks += b.find_children("NewMark", "", true, false).size() if b else 0
	check(lit and marks == 0, "a Custom game: no tab dimmed, no new marks")
	# a loaded campaign is not dimmed either
	var blob := app.session.save_blob()
	app.load_game({"data": blob.get("data", ""), "scenario": blob.get("scenario", "campaign"), "name": "x"})
	await frames(4)
	lit = true
	for s in AppRoutes.GAME_SCREENS:
		lit = lit and _rail(str(s["id"])).modulate.a > 0.99
	check(lit, "a loaded campaign: no tab dimmed")

func _fill(text: String) -> String:
	var k := text.count("%s")
	var args := []
	for i in k:
		args.append("Proxima Centauri Outpost")   # the longest sort of name: three words
	return text % args if k > 0 else text

func _wordiness() -> void:
	var worst := 0
	for key in Advisor.TEXTS:
		var e: Array = Advisor.TEXTS[key]
		var text := _fill(str(e[1]))
		if key == "colony_ship":
			text = str(e[1]) % [_fill(str(Advisor.TEXTS["colony_free"][1]))]
		var w := _words(text)
		worst = maxi(worst, w)
		check(w <= 20, "advisor text '%s' is %d words (20 at most): %s" % [key, w, text])
		check(_words(str(e[0])) <= 5, "advisor title '%s' is short" % key)
	for st in Advisor.STEPS:
		check(_words(str(st["text"])) <= 12, "beat %s: %d words (12 at most)" % [st["id"], _words(str(st["text"]))])
	for k in Advisor.EVENT_BEATS:
		var tx := str(Advisor.EVENT_BEATS[k]["text"])
		if tx.contains("%s"):
			tx = tx % ["Negotiate"]
		check(_words(tx) <= 12, "event beat %s: %d words (12 at most)" % [k, _words(tx)])
	print("  longest advisor text: %d words" % worst)

func _tooltips() -> void:
	await _quick_game(31337)
	var shell := ShellVM.new(app.session)
	var tips := shell.figure_tips()
	for cap in ["Turn", "Research", "Industry", "Food", "Pop", "Intel", "Orders"]:
		check(tips.has(cap) and _words(str(tips[cap])) >= 4, "the %s figure explains itself: %s" % [cap, str(tips.get(cap, "")).replace("\n", " / ").left(70)])
	check(tips.size() == 8, "eight top-bar figures have tips (%d)" % tips.size())
	var tiles: Array = app.find_children("*", "KpiTile", true, false).filter(func(k): return k.is_visible_in_tree() and (k as KpiTile).get_parent().get_parent() == app._topbar)
	var bare := 0
	for t in tiles:
		if (t as KpiTile).tooltip_text == "" or (t as KpiTile).tip_text() != (t as KpiTile).tooltip_text:
			bare += 1
	check(tiles.size() == 8 and bare == 0, "all %d tiles on the top bar carry that text as their tooltip" % tiles.size())
	for st in shell.stats():
		check(_words(str(st["explain"].get("effect", ""))) <= 20, "the %s tooltip's explanation is 20 words or fewer" % st["cap"])

## Whether the save's RELATIONS block holds a war.
func _war_in(data: String) -> bool:
	var lines := data.split("\n")
	for i in lines.size():
		if lines[i].begins_with("RELATIONS "):
			var n := int(lines[i].split(" ")[1])
			for j in n:
				var c := lines[i + 1 + j].split(" ")
				if c.size() >= 4 and int(c[3]) == 1:
					return true
			return false
	return false

## The first turn a war stands between any two powers, or -1 by `limit`.
func _first_war(seed_: int, hold: int, limit: int) -> int:
	var o := NewCampaignVM.quick(app.session, 1).quick_options()
	o["seed"] = seed_
	o["aiWarHoldTurn"] = hold
	o.erase("quickStart")
	app.session.start("newCampaign", AppSession.campaign_options(o))
	for turn in range(1, limit + 1):
		var r := app.session.end_turn()
		if r.is_empty():
			return -2
		if _war_in(str(app.session.save_blob().get("data", ""))):
			return turn
	return -1

func _hold_back() -> void:
	await _new_app()
	var any_early := false
	for sd in [31337, 424242, 7, 99, 2024, 555, 8675]:
		var held := _first_war(sd, 40, 44)
		var free := _first_war(sd, 0, 44)
		print("  seed %d: first war with the hold-back: %s; without: %s" % [sd, str(held) if held > 0 else "none by 44", str(free) if free > 0 else "none by 44"])
		check(held == -1 or held >= 40, "seed %d: the held-back AI starts no war before turn 40 (first: %d)" % [sd, held])
		any_early = any_early or (free > 0 and free < 40)
	check(any_early, "without the hold-back the same AI does go to war before turn 40 on at least one seed")
	# the hold-back is part of the saved game: it survives save and load, and a game without it writes nothing new
	var o := NewCampaignVM.quick(app.session, 1).quick_options()
	o["seed"] = 31337
	o.erase("quickStart")
	app.session.start("newCampaign", AppSession.campaign_options(o))
	var blob := app.session.save_blob()
	check(str(blob.get("data", "")).contains("AI_WAR_HOLD 40"), "a Quick Start save records the hold-back")
	app.session.start("load", {"data": blob["data"], "scenario": blob.get("scenario", "campaign")})
	check(str(app.session.save_blob().get("data", "")).contains("AI_WAR_HOLD 40"), "and it survives a load")
	o.erase("aiWarHoldTurn")
	app.session.start("newCampaign", AppSession.campaign_options(o))
	check(not str(app.session.save_blob().get("data", "")).contains("AI_WAR_HOLD"), "a game without it saves no new section (old builds still read it)")
