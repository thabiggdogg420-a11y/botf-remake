extends SceneTree
## F3 "Planets and people": busts and the hail viewscreen, the 3D system view, voice acknowledgements on orders, and the
## advisor/tutorial. The last part is the F3 done-gate: a new player reaches turn 10 by pressing only the advisor's
## action button (the tutorial, then the hints), from the title screen, on the real core when it is loaded.
##   godot --headless --path remaster/godot --audio-driver Dummy -s res://scenes/ui/tests/test_f3_people.gd
## Exit code 0 on success, 1 on any failure. Prints the turn-10 transcript (one line per press).

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

func _run() -> void:
	_busts()
	_doubtful()
	var saved := {}
	AppSaves.load_settings(saved)
	var backend := "native" if Engine.has_singleton("BotfCore") else "fixture"
	await _new_app(backend)
	await _system_view()
	await _hail()
	_acks()
	await _turn10(backend)
	AppSaves.store_settings(saved)   # the run moved the tutorial step; give the player's settings back
	print("%d checks, %d failures" % [checks, failures])
	quit(1 if failures > 0 else 0)

func _new_app(backend: String) -> void:
	if app:
		app.queue_free()
		await frames()
	App.forced_backend = "fixture" if backend == "fixture" else ""
	app = load("res://scenes/ui/app.tscn").instantiate()
	app.size = Vector2(1920, 1080)
	root.add_child(app)
	await frames()

func _busts() -> void:
	var keys: Array = HailBust.KEYS.duplicate()
	for k in ["kli", "rom", "car", "fer", "dom", "borg", "breen"]:
		keys.append("mirror." + k)
	for k in keys:
		var sh := HailBust.shapes(k)
		var ok := (sh["body"] as Array).size() >= 3
		for poly in sh["body"]:
			ok = ok and not Geometry2D.triangulate_polygon(poly).is_empty()
		check(ok, "bust %s: a closed silhouette (%d shapes)" % [k, (sh["body"] as Array).size()])
	check(HailBust.key_for(1, true) == "terran" and HailBust.key_for(2, true) == "mirror.kli" and HailBust.key_for(6) == "dom", "Mirror pack: the Federation hails as the Terran Empire, the others in Alliance harness")
	check(HailBust.shapes("borg")["glow"].size() > 0 and HailBust.shapes("dom") != HailBust.shapes("fed"), "Borg implant glows; each power has its own outline")

func _doubtful() -> void:
	check(AudioDoubtful.blocked("res://scripts/audio/pack/fed/voice/ds9_voice_fed_ack_s01e05_883.ogg"), "doubtful DS9 'Aye, sir' clip is blocked (AUDIO-DOUBTFUL.md)")
	check(AudioDoubtful.blocked("x/trek/voy/voice/fed_ack_s01e09_2038.wav"), "doubtful VOY ack blocked by library path too")
	check(not AudioDoubtful.blocked("res://scripts/audio/pack/fed/voice/ds9_voice_fed_ack_s01e10_1942.ogg"), "passed DS9 'Understood.' clip still plays")

func _start_campaign() -> void:
	app.goto("new")
	await frames()
	var nc := app.current_screen()
	nc.faction = 1
	nc.seed_value = 31337
	nc.start()
	await frames(4)

func _system_view() -> void:
	app.settings["tutorial"] = -1
	await _start_campaign()
	check(not app.view.is_empty(), "campaign started for the system view")
	app.goto("system")
	await frames(4)
	var sv: SystemView3D = app.current_screen().find_child("SystemView3D", true, false)
	check(sv != null, "opening a system shows the 3D system view")
	if sv:
		var n := 0
		for b in sv._bodies:
			if b:
				n += 1
		check(sv.world.find_child("Star", true, false) != null and n > 0, "the star and %d textured or procedural planets are in the scene" % n)
		check(sv.home_index >= 0 and sv.structure_nodes.size() > 0, "the colony world carries its structures (%s)" % str(sv.structure_nodes))
		check(sv.overlay != null and sv.planet_screen(sv.home_index).x >= 0, "LCARS labels track the planets on screen")
		await frames(3)
		# F3 review: the view fills its band and the colony world reads (was a strip with ~12 px planets)
		var lo := 1e9
		var hi := -1e9
		for i in sv._bodies.size():
			if sv._bodies[i]:
				var p := sv.planet_screen(i)
				lo = minf(lo, p.x)
				hi = maxf(hi, p.x)
		check(sv.size.x > 1200.0 and sv.left_inset > 0.0, "the 3D view spans the band behind the title (%.0f px wide)" % sv.size.x)
		check(sv.planet_screen_radius(sv.home_index) >= 25.0, "the colony world is drawn large (%.1f px radius)" % sv.planet_screen_radius(sv.home_index))
		check(sv.selected == sv.home_index and sv.world.find_child("SelectRing", true, false) != null, "the colony world is selected and ringed")
		check(sv._works != null and sv._works.get_child_count() > 0, "the colony's surface works face the camera (%d kinds)" % (sv._works.get_child_count() if sv._works else 0))
		check(not HailBust.mirror_active(), "a plain game is not read as the Mirror Universe (meta.empireNames is always sent)")

func _hail() -> void:
	app.goto("diplomacy")
	await frames(3)
	var ds := app.current_screen()
	var others: Array = app.view.get("empires", []).filter(func(e): return int(e.get("empire", 0)) != app.player())
	var tid := int(others[0]["empire"]) if not others.is_empty() else 2
	var m: Control = ds.open_hail(app.empire_name(tid), tid, false, "At peace")
	await frames(2)
	var hv: Node = m.find_child("HailViewscreen", true, false)
	check(hv is HailViewscreen and (hv as HailViewscreen).empire == tid, "Hail opens the viewscreen for power %d" % tid)
	check(m.find_child("HailNegotiate", true, false) != null, "the hail scene offers Negotiate")
	if hv is HailViewscreen:
		var h := hv as HailViewscreen
		var pic := AppLoader.art(str(PortraitSlot.MAJOR_KEYS.get(tid, "")), "portrait")
		check(h.portrait_keys() == [str(PortraitSlot.MAJOR_KEYS.get(tid, ""))], "hail looks up the power's own portrait (%s)" % str(h.portrait_keys()))
		if pic:
			check(not h.shows_bust(), "the private portrait pack's picture fills the hail screen, not the bust")
		else:
			check(h.shows_bust(), "no portrait pack here: the hail screen draws the bust")
	app.close_all_modals()
	await frames()

func _acks() -> void:
	var sys := BotfAudioSystem.instance
	if sys == null:
		print("  SKIP voice acks: no audio autoload")
		return
	var r := sys.library.resolve_files(sys.era_keys(BotfAudioCues.order_voice_keys("Move", "fed")))
	if r.is_empty():
		print("  SKIP voice acks: no audio pack on this machine")
		return
	var bad := false
	for f in r["files"]:
		bad = bad or AudioDoubtful.blocked(f)
	check(str(r["key"]).contains(".ack."), "a fleet order resolves to an acknowledgement group (%s, %d clips)" % [r["key"], r["files"].size()])
	check(not bad, "no doubtful clip is among the acknowledgements")
	sys._last_ack = -100.0
	var out := sys.campaign_ack({"type": "MoveFleet"})
	check(out.has("voice") and str(out["voice"]).contains(".ack."), "MoveFleet is acknowledged by voice (%s)" % str(out.get("voice", "")))
	check(sys.campaign_ack({"type": "EnqueueBuilding"}).is_empty(), "queue orders keep the beep only")

## The F3 done-gate.
func _turn10(backend: String) -> void:
	await _new_app(backend)
	app.settings["tutorial"] = 0          # a first run
	await _start_campaign()
	check(int(app.view["meta"]["turn"]) <= 1, "new campaign at turn %d (%s backend)" % [int(app.view["meta"]["turn"]), backend])
	var panel := app.advisor_panel
	var presses := 0
	var tutorial_steps := {}
	var covered := []
	var pointed := 0
	print("TURN10 transcript (backend %s, seed 31337):" % backend)
	while int(app.view.get("meta", {}).get("turn", 0)) < 10 and presses < 400:
		panel.update()
		var g := panel.current
		if g.is_empty():
			check(false, "advisor has guidance on turn %d (screen %s)" % [int(app.view["meta"]["turn"]), app.screen])
			break
		var btn: Button = panel.find_child("AdvisorAct", true, false)
		if btn == null:
			check(false, "advisor shows an action button")
			break
		if g.get("tutorial", false):
			tutorial_steps[str(g["id"])] = true
		# F3 review: the card never sits on the control the step points at, and that control is ringed
		panel._place()
		var rects := panel.target_rects()
		if not rects.is_empty():
			pointed += 1
			var hl: Control = app.find_child("AdvisorHighlight", true, false)
			if hl == null or not hl.visible:
				covered.append(str(g["id"]) + " (no ring)")
		for r in rects:
			if panel.get_global_rect().intersects(r):
				covered.append(str(g["id"]))
		var t0 := int(app.view["meta"]["turn"])
		btn.pressed.emit()       # the only input the run gives
		presses += 1
		await frames(2)
		print("  T%02d %-8s %-22s -> %s%s" % [t0, app.screen, str(g["title"]), str(g["act_label"]), ("  [tutorial %d/%d]" % [int(g["step"]), int(g["steps"])]) if g.get("tutorial", false) else ""])
	var turn := int(app.view.get("meta", {}).get("turn", 0))
	check(turn >= 10, "reached turn %d by advisor presses only (%d presses)" % [turn, presses])
	check(pointed > 0 and covered.is_empty(), "the advisor card left every pointed-at control clear and ringed (%d steps pointed, covered: %s)" % [pointed, str(covered)])
	check(tutorial_steps.size() >= 6, "the tutorial taught the loop (%s)" % ", ".join(tutorial_steps.keys()))
	check(str(app.view["meta"].get("outcome", "ongoing")) == "ongoing", "the empire is still in the game at turn %d" % turn)
	check(app.view.get("colonies", []).size() >= 1, "colonies at turn %d: %d" % [turn, app.view.get("colonies", []).size()])
	var charted: int = app.view.get("systems", []).filter(func(s): return AppSession.is_charted(s)).size()
	print("TURN10 result: turn %d, presses %d, colonies %d, charted systems %d, credits %s" % [turn, presses, app.view.get("colonies", []).size(), charted, str(app.view.get("empire", {}).get("credits", "?"))])
