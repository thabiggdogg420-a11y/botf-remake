extends SceneTree
## Headless test of the audio system: cue registry and resolution, scheduler, ducking math, the five buses, volume
## persistence through the client settings file, file/tone resolution, and the app-level hooks (music per screen,
## end-turn and briefing cues, button clicks, contacts, battle tension).
##   godot --headless --path remaster/godot -s res://scripts/audio/tests/test_audio.gd

var failures := 0
var checks := 0


func check(cond: bool, what: String) -> void:
	checks += 1
	if cond:
		print("  ok  ", what)
	else:
		failures += 1
		printerr("  FAIL ", what)


func _initialize() -> void:
	_run.call_deferred()


func frames(n := 2) -> void:
	for k in n:
		await process_frame


func _run() -> void:
	print("Audio system tests")
	_registry()
	_turn_cues()
	_scheduler()
	_mix()
	_buses()
	await _library()
	await _system()
	_local_content()
	await _persistence()
	await _app_hooks()
	_eras_and_orders()
	await _voice_layers()   # last: it moves the audio clock far ahead
	print("%d checks, %d failures" % [checks, failures])
	quit(1 if failures > 0 else 0)


# ------------------------------------------------------------------------------------------------------------
func _registry() -> void:
	var c := BotfAudioCues.cue_for("research_complete", "fed")
	check(c.sfx == PackedStringArray(["sfx.ui.research", "sfx.ui.beep"]) and c.bus == "Interface" and c.priority == 4, "research cue: keys, bus, priority")
	check(c.voice == PackedStringArray(["vo.advisor.research.fed"]), "research voice: the event-specific advisor key")
	check(BotfAudioCues.cue_for("research_complete", "").voice.is_empty(), "no faction -> no voice keys (never another race)")
	var fc := BotfAudioCues.cue_for("first_contact", "fed", "kli")
	check(fc.voice == PackedStringArray(["vo.diplomacy.greet.kli", "vo.diplomacy.kli", "vo.advisor.contact.fed"]) and fc.beep, "first contact: the speaker is the other empire, beep fallback")
	check(BotfAudioCues.cue_for("victory", "rom", "", "scientific").voice[0] == "vo.victory.scientific.rom", "victory voice by type")
	check(BotfAudioCues.cue_for("war_declared", "fed", "car").sfx == PackedStringArray(["sfx.alert.red"]), "war declared sounds the red alert")
	check(BotfAudioCues.cue_for("nonsense").is_empty(), "unknown cue is silent")
	check(BotfAudioCues.cooldown_for("weapon.x", "Effects") == 0.25 and BotfAudioCues.cooldown_for("sfx.alert.red", "Effects") == 3.0
		and BotfAudioCues.cooldown_for("sfx.ui.click", "Interface") == 0.12 and BotfAudioCues.cooldown_for("vo.x", "Voice") == 4.0, "cooldown table")
	# Salvo (R3.5): 10 torpedoes in a quarter second play 8 overlapping launches, then the key rests.
	var sal := BotfCueScheduler.new()
	var played := 0
	for k in 10:
		if sal.request_sfx("weapon.generic.torpedo", "Effects", 3, 1.0 + k * 0.025).play:
			played += 1
	check(played == 8, "a torpedo salvo plays as 8 overlapping launches (%d)" % played)
	BotfAudioSystem._ensure_limiter()
	var mi := AudioServer.get_bus_index("Master")
	var has_lim := false
	for e in AudioServer.get_bus_effect_count(mi):
		has_lim = has_lim or AudioServer.get_bus_effect(mi, e) is AudioEffectHardLimiter
	check(has_lim, "master bus carries a hard limiter (salvos never clip)")
	var ui := BotfCueScheduler.new()
	check(ui.request_sfx("sfx.ui.click", "Interface", 1, 1.0).play and not ui.request_sfx("sfx.ui.click", "Interface", 1, 1.01).play, "non-weapon keys still play once per cooldown")
	check(BotfAudioCues.bus_for_key("vo.advisor.fed") == "Voice" and BotfAudioCues.bus_for_key("sfx.ui.click") == "Interface"
		and BotfAudioCues.bus_for_key("weapon.a.b") == "Effects" and BotfAudioCues.bus_for_key("mus.botf.all") == "Music", "bus by key family")
	for ctx in BotfAudioCues.MUSIC:
		check(BotfAudioCues.music_keys(ctx, "kli")[-1] == "mus.botf.all", "music %s falls back to mus.botf.all" % ctx)
	check(BotfAudioCues.music_keys("galaxy", "kli")[0] == "mus.botf.galaxy.kli" and BotfAudioCues.music_keys("galaxy", "")[0] == "mus.botf.galaxy", "music follows the player's faction")
	check(BotfAudioCues.music_for_screen("title") == "title" and BotfAudioCues.music_for_screen("research") == "galaxy"
		and BotfAudioCues.music_for_screen("combat") == "battle", "screen -> music context")


func _turn_cues() -> void:
	var empires := [{"id": 1, "name": "Federation"}, {"id": 2, "name": "Klingon Empire"}, {"id": 4, "name": "Cardassian Union"}]
	var b := {"outcome": "ongoing", "events": [
		{"category": "construction", "severity": "good", "text": "Sol completed Laboratory."},
		{"category": "construction", "severity": "good", "text": "Sol completed Shipyard."},
		{"category": "research", "severity": "info", "text": "Warp 5 researched."},
		{"category": "diplomacy", "severity": "alert", "text": "The Klingon Empire declares war on the Federation."},
		{"category": "diplomacy", "severity": "info", "text": "Treaty signed with the Cardassian Union."},
	]}
	var cues := BotfAudioCues.turn_cues(b, "Federation", empires, 1)
	var names: Array = cues.map(func(c): return c.cue)
	check(names == ["war_declared", "treaty_signed", "research_complete", "construction_complete", "turn_start"], "turn cues: one per kind, highest priority first (%s)" % [names])
	check(cues[0].other == "kli" and cues[1].other == "car", "diplomacy cues name the other empire")
	var v := BotfAudioCues.turn_cues({"outcome": "victory", "events": []}, "Federation", empires, 1)
	check(v[0].cue == "victory", "victory outranks the rest")
	check(BotfAudioCues.turn_cues({"outcome": "defeat", "events": []})[0].cue == "defeat", "defeat cue")
	check(BotfAudioCues.turn_cues({"events": []}).size() == 1, "a quiet turn: only turn_start")


func _scheduler() -> void:
	var s := BotfCueScheduler.new()
	check(s.request_sfx("sfx.ui.click", "Interface", 1, 10.0).play, "first play allowed")
	check(not s.request_sfx("sfx.ui.click", "Interface", 1, 10.05).play, "UI repeat inside 0.12 s dropped")
	check(s.request_sfx("sfx.ui.click", "Interface", 1, 10.2).play, "UI repeat after 0.12 s allowed")
	check(s.request_sfx("sfx.alert.red", "Effects", 8, 20.0).play and not s.request_sfx("sfx.alert.red", "Effects", 8, 22.0).play, "alerts have a 3 s cooldown")
	check(s.request_sfx("sfx.alert.red", "Effects", 8, 23.5).play, "alert again after 3 s")
	# Cap and culling
	var c := BotfCueScheduler.new()
	c.max_sfx = 3
	var h := []
	for i in 3:
		h.append(c.request_sfx("k%d" % i, "Effects", 2 + i, float(i) * 0.01 + 5.0))
	check(c.active_sfx() == 3, "three effects active")
	var low := c.request_sfx("low", "Effects", 1, 5.5)
	check(not low.play, "a new low priority sound never culls a higher one")
	var hi := c.request_sfx("hi", "Effects", 9, 5.6)
	check(hi.play and hi.cull == h[0].handle, "a higher priority sound culls the lowest (oldest) one")
	c.sfx_finished(hi.handle)
	check(c.active_sfx() == 2, "finished effects free their slot")
	# Voice queue
	var v := BotfCueScheduler.new()
	check(v.request_voice("vo.a", 3, 0.0) and v.request_voice("vo.b", 7, 0.1) and v.request_voice("vo.c", 5, 0.2), "voice lines queue")
	check(not v.request_voice("vo.b", 7, 0.3), "the same line queued twice is dropped")
	var first := v.next_voice(0.5, false)
	check(first.key == "vo.b", "highest priority voice first")
	check(v.next_voice(0.6, true).is_empty() and v.voice_active(), "busy channel starts nothing")
	check(v.next_voice(1.0, false).is_empty(), "gap of 0.6 s after a line ends")
	check(v.next_voice(1.7, false).key == "vo.c", "next line after the gap, by priority")
	check(not v.request_voice("vo.b", 7, 2.0), "a played line is on a 4 s cooldown")
	check(v.request_voice("vo.b", 7, 4.6), "and allowed again after it")
	var t := BotfCueScheduler.new()
	t.request_voice("vo.stale", 5, 0.0)
	check(t.next_voice(7.0, false).is_empty() and t.queued_voices() == 0, "queued lines older than 6 s are dropped")
	var q := BotfCueScheduler.new()
	q.max_voice_queue = 2
	q.request_voice("a", 2, 0.0)
	q.request_voice("b", 3, 0.0)
	check(not q.request_voice("c", 2, 0.0) and q.request_voice("d", 6, 0.0) and q.queued_voices() == 2, "a full queue yields only to a higher priority")
	check([q.pick_variant("x", 3), q.pick_variant("x", 3), q.pick_variant("x", 3), q.pick_variant("x", 3)] == [0, 1, 2, 0], "variants rotate")


func _mix() -> void:
	check(is_equal_approx(BotfAudioMix.duck_target(true), 0.45) and BotfAudioMix.duck_target(false) == 1.0, "duck target 45 % under voice")
	var g := 1.0
	var t := 0.0
	while g > 0.4500001 and t < 2.0:
		g = BotfAudioMix.duck_step(g, 0.45, 1.0 / 60.0)
		t += 1.0 / 60.0
	check(is_equal_approx(g, 0.45) and t > 0.1 and t < 0.14, "ducks fully in about 0.12 s (%.3f)" % t)
	t = 0.0
	while g < 0.9999999 and t < 3.0:
		g = BotfAudioMix.duck_step(g, 1.0, 1.0 / 60.0)
		t += 1.0 / 60.0
	check(is_equal_approx(g, 1.0) and t > 0.65 and t < 0.75, "recovers in about 0.7 s (%.3f)" % t)
	var s := {"vol_master": 0.5, "vol_effects": 0.8, "vol_voice": 1.0, "vol_interface": 0.6, "vol_music": 0.5}
	check(is_equal_approx(BotfAudioMix.bus_gain(s, "Effects", 0.45), 0.36), "effects bus = slider x duck")
	check(is_equal_approx(BotfAudioMix.bus_gain(s, "Music", 0.45), 0.225), "music bus = slider x duck")
	check(is_equal_approx(BotfAudioMix.bus_gain(s, "Voice", 0.45), 1.0) and is_equal_approx(BotfAudioMix.bus_gain(s, "Interface", 0.45), 0.6), "voice and interface are never ducked")
	check(is_equal_approx(BotfAudioMix.heard_gain(s, "Effects", 1.0), 0.4), "heard level includes the master slider")
	check(BotfAudioMix.to_db(0.0) == -80.0 and absf(BotfAudioMix.to_db(1.0)) < 0.001 and absf(BotfAudioMix.to_db(0.5) + 6.0206) < 0.01, "slider -> decibels")
	var x := BotfAudioMix.crossfade(0.5)
	check(is_equal_approx(x.x * x.x + x.y * x.y, 1.0) and BotfAudioMix.crossfade(0.0) == Vector2(1, 0) and BotfAudioMix.crossfade(1.0).y > 0.999, "equal-power crossfade")
	var old := {"volume": 0.4, "uiScale": 1.0}
	BotfAudioMix.fill_defaults(old)
	check(is_equal_approx(old.vol_master, 0.4) and not old.has("volume") and old.has("vol_music") and old.vol_music == BotfAudioMix.DEFAULTS.vol_music, "old single volume migrates to master; the rest default")
	var bad := {"vol_voice": 5.0, "vol_effects": -1.0}
	BotfAudioMix.fill_defaults(bad)
	check(bad.vol_voice == 1.0 and bad.vol_effects == 0.0, "sliders clamp to 0..1")


func _buses() -> void:
	var names := BotfAudioSystem.bus_names()
	check(names == PackedStringArray(["Master", "Effects", "Voice", "Interface", "Music"]), "five buses in order from default_bus_layout.tres (%s)" % [names])
	for b in ["Effects", "Voice", "Interface", "Music"]:
		check(AudioServer.get_bus_send(AudioServer.get_bus_index(b)) == &"Master", "%s sends to Master" % b)


# ------------------------------------------------------------------------------------------------------------
func _write_wav(path: String, seconds: float) -> void:
	var n := int(seconds * 8000)
	var d := PackedByteArray()
	d.resize(n * 2)
	for i in n:
		d.encode_s16(i * 2, int(sin(float(i) * 0.2) * 8000.0))
	var w := AudioStreamWAV.new()
	w.format = AudioStreamWAV.FORMAT_16_BITS
	w.mix_rate = 8000
	w.data = d
	w.save_to_wav(path)


func _library() -> void:
	var dir := ProjectSettings.globalize_path("user://audio_test")
	DirAccess.make_dir_recursive_absolute(dir.path_join("audio/vo.advisor.fed"))
	_write_wav(dir.path_join("audio/sfx.ui.beep.wav"), 0.1)
	_write_wav(dir.path_join("audio/vo.advisor.fed/a.wav"), 0.3)
	_write_wav(dir.path_join("audio/vo.advisor.fed/b.wav"), 0.3)
	_write_wav(dir.path_join("audio/one.wav"), 0.2)
	var cj := FileAccess.open(dir.path_join("audio/cues.json"), FileAccess.WRITE)
	cj.store_string(JSON.stringify({"cues": {"sfx.ui.research": ["user://audio_test/audio/one.wav"], "vo.diplomacy.kli": ["missing.wav"]}}))
	cj.close()
	var lib := BotfAudioLibrary.new()
	check(lib.load_cues("user://audio_test/audio/cues.json"), "cues.json loads (through AppLoader, user://)")
	check(lib.cues_path.ends_with("cues.json") and lib.cue_files.has("sfx.ui.research"), "cues.json lists its keys")
	lib.cue_files["sfx.ui.beep"] = PackedStringArray(["user://audio_test/audio/sfx.ui.beep.wav"])
	lib.cue_files["vo.advisor.fed"] = PackedStringArray(["user://audio_test/audio/vo.advisor.fed/a.wav", "user://audio_test/audio/vo.advisor.fed/b.wav"])
	check(lib.files_for("sfx.ui.research").size() == 1, "a cue key resolves through cues.json")
	check(lib.files_for("/etc/disk.wav").is_empty() and not AppLoader.allowed("C:/x.wav"), "disk paths are never served")
	check(lib.files_for("vo.diplomacy.kli").is_empty(), "a missing listed file is not resolved")
	check(lib.files_for("vo.advisor.fed").size() == 2, "a key lists its files")
	var r := lib.resolve_files(PackedStringArray(["sfx.ui.nothing", "sfx.ui.beep"]))
	check(r.get("key", "") == "sfx.ui.beep", "the key chain falls back to the next key with a file")
	check(lib.stream_of(r) is AudioStreamWAV, "a wav loads as a stream")
	check(lib.resolve_files(PackedStringArray(["vo.advisor.rom"])).is_empty(), "no files -> unresolved (voice stays silent)")
	for k in ["sfx.ui.click", "sfx.ui.open", "sfx.ui.endturn", "sfx.ui.turn", "sfx.ui.research", "sfx.ui.construction", "sfx.alert.red", "weapon.PhaserArray.fed", "sfx.explosion.large", "sfx.warp.in"]:
		check(lib.tone_for(k) is AudioStreamWAV, "generated tone for %s" % k)
	check(lib.tone_for("vo.advisor.fed") == null and lib.tone_for("mus.botf.all") == null, "no tone for voice or music keys")
	for m in ["title", "galaxy", "battle", "tension"]:
		var ms := lib.music_tone(m) as AudioStreamWAV
		check(ms != null and ms.loop_mode == AudioStreamWAV.LOOP_FORWARD and ms.data.size() > 100000, "generated %s bed loops" % m)
	lib.finish_tasks()


func _sys() -> BotfAudioSystem:
	return BotfAudioSystem.instance


func _system() -> void:
	var sys := _sys()
	check(sys != null, "the BotfAudio autoload is running")
	if sys == null:
		return
	sys.clock_override = 100.0
	sys.recent.clear()
	var r := sys.cue("research_complete", "fed")
	check(r.get("sfx", "") == "sfx.ui.research", "research cue plays its tone: %s" % [r])
	check(not r.has("voice"), "voice is silent when no file exists")
	check(sys.recent.back().bus == "Interface", "interface cues play on the Interface bus")
	sys.clock_override = 100.01
	check(sys.cue("research_complete", "fed").is_empty(), "the same cue inside its cooldown is dropped")
	sys.clock_override = 101.0
	check(sys.play_keys(PackedStringArray(["weapon.PhaserArray.fed", "weapon.generic.beam"]), 3) != "", "battle keys play a generated tone")
	check(sys.recent.back().bus == "Effects", "weapons play on the Effects bus")
	check(sys.play_keys(PackedStringArray(["vo.combat.kill.zzz"]), 5) == "", "a battle bark without a file is silent")
	# Voice from a real file: ducking
	var dir := ProjectSettings.globalize_path("user://audio_test/vo")
	DirAccess.make_dir_recursive_absolute(dir)
	_write_wav(dir.path_join("kli.wav"), 0.5)
	sys.library.cue_files["vo.diplomacy.greet.kli"] = PackedStringArray(["user://audio_test/vo/kli.wav"])
	sys.clock_override = 110.0
	var vr := sys.cue("first_contact", "fed", "kli")
	check(vr.get("voice", "") == "vo.diplomacy.greet.kli", "a voice file queues its line")
	sys._tick(0.016)
	check(sys._voice.playing and sys.scheduler.voice_active(), "the voice line starts on the Voice bus")
	for i in 20:
		sys._tick(0.016)
	check(sys.duck_gain < 0.6 and BotfAudioSystem.bus_linear("Effects") < sys.get_volume("Effects") * 0.7, "effects dip under voice (gain %.2f)" % sys.duck_gain)
	check(BotfAudioSystem.bus_linear("Music") < sys.get_volume("Music") * 0.7 and BotfAudioSystem.bus_linear("Voice") > sys.get_volume("Voice") * 0.95, "music dips, voice does not")
	sys._voice.stop()
	for i in 90:
		sys.clock_override += 0.016
		sys._tick(0.016)
	check(is_equal_approx(sys.duck_gain, 1.0) and absf(BotfAudioSystem.bus_linear("Effects") - sys.get_volume("Effects")) < 0.01, "the duck releases when the voice ends")
	# Music
	sys.set_music("title")
	check(sys.music_context == "title" and sys._music[sys._music_front].playing, "title music plays")
	var first := sys._music_front
	sys.set_music("galaxy")
	check(sys._music_front != first and sys._fading, "changing context crossfades to the other player")
	sys._tick(0.8)
	var mid := BotfAudioSystem.instance._music[first].volume_db
	check(mid > -20.0 and mid < 0.0 and sys._music[sys._music_front].volume_db > -20.0, "mid-crossfade both beds are audible")
	sys._tick(1.5)
	check(not sys._fading and not sys._music[first].playing and absf(sys._music[sys._music_front].volume_db) < 0.1, "crossfade completes and stops the old bed")
	sys.set_music("battle")
	sys._combat_activity("weapon.PhaserArray.fed")
	check(sys.music_context == "tension", "combat fire raises battle tension")
	sys.set_music("galaxy")
	sys.clock_override = -1.0


func _persistence() -> void:
	var sys := _sys()
	if sys == null:
		return
	var path := BotfAudioSystem.SETTINGS_PATH
	var backup := FileAccess.get_file_as_string(path) if FileAccess.file_exists(path) else ""
	App.forced_backend = "fixture"
	var app: App = load("res://scenes/ui/app.tscn").instantiate()
	app.size = Vector2(1920, 1080)
	root.add_child(app)
	await frames()
	check(app.settings.has("vol_master") and not app.settings.has("volume"), "the app settings carry the five sliders")
	for bus in ["Master", "Effects", "Voice", "Interface", "Music"]:
		check(sys.get_volume(bus) == app.settings[BotfAudioMix.key_for(bus)], "%s slider shares the app settings" % bus)
	# the Settings screen has five sliders and drives the buses live
	var modal := app.open_overlay("settings")
	await frames()
	var sliders := []
	for bus in BotfAudioCues.BUSES:
		var row := modal.find_child("Volume_" + bus, true, false)
		check(row != null, "settings has a %s slider" % bus)
		if row != null:
			sliders.append(row.find_child("Slider", true, false))
	check(modal.find_child("Sound volume", true, false) == null, "the single volume slider is gone")
	var want := {"Master": 0.35, "Effects": 0.55, "Voice": 0.75, "Interface": 0.25, "Music": 0.15}
	for i in sliders.size():
		(sliders[i] as HSlider).value = want[BotfAudioCues.BUSES[i]]
	await frames()
	for bus in want:
		check(absf(BotfAudioSystem.bus_linear(bus) - want[bus]) < 0.02, "%s bus follows its slider live (%.2f)" % [bus, BotfAudioSystem.bus_linear(bus)])
	app.save_settings()
	var saved: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(path))
	check(is_equal_approx(saved.vol_master, 0.35) and is_equal_approx(saved.vol_music, 0.15) and is_equal_approx(saved.vol_interface, 0.25), "the sliders are written to the settings file")
	# a fresh start reads them back
	app.queue_free()
	await frames()
	for b in BotfAudioCues.BUSES:
		sys.set_volume(b, 1.0)
	sys.load_settings_file()
	for bus in want:
		check(absf(BotfAudioSystem.bus_linear(bus) - want[bus]) < 0.02, "%s volume restored from the file after a restart (%.2f)" % [bus, BotfAudioSystem.bus_linear(bus)])
	# migration of the old single volume
	var f := FileAccess.open(path, FileAccess.WRITE)
	f.store_string(JSON.stringify({"volume": 0.3, "uiScale": 1.0}))
	f.close()
	sys.load_settings_file()
	check(absf(BotfAudioSystem.bus_linear("Master") - 0.3) < 0.02, "an old settings file's volume becomes the master slider")
	# put the user's file back
	if backup != "":
		var g := FileAccess.open(path, FileAccess.WRITE)
		g.store_string(backup)
		g.close()
	else:
		DirAccess.remove_absolute(path)
	sys.load_settings_file()


func _app_hooks() -> void:
	var sys := _sys()
	if sys == null:
		return
	App.forced_backend = "fixture"
	var app: App = load("res://scenes/ui/app.tscn").instantiate()
	app.size = Vector2(1920, 1080)
	root.add_child(app)
	await frames()
	check(sys.music_context == "title", "the title screen plays title music")
	sys.recent.clear()
	sys.clock_override = 200.0
	app.goto("new")
	await frames()
	check(sys.music_context == "title", "the New campaign screen keeps the title music")
	# enter a game via the fixture
	var ok := app.start_campaign({"empire": 1, "era": 3, "seed": 424242, "preset": "remastered"})
	await frames()
	if not ok:
		app.start_drill("tacticalDrill", "Tactical drill")
	check(sys.music_context == "galaxy" and app.screen == "galaxy", "entering the game switches to galaxy music")
	sys.clock_override = 201.0
	app.goto("research")
	await frames()
	check(sys.music_context == "galaxy", "strategy screens share the galaxy music")
	check(sys.recent.any(func(r): return r.cue == "ui_open"), "opening a screen plays the open cue")
	# button clicks come from the app-level node_added hook, not per button
	var b := Button.new()
	app.add_child(b)
	await frames()
	sys.clock_override = 202.0
	sys.recent.clear()
	b.pressed.emit()
	check(sys.recent.size() == 1 and sys.recent[0].key == "sfx.ui.click", "any button click plays the click cue")
	# end turn
	sys.clock_override = 210.0
	sys.recent.clear()
	app.request_end_turn()
	await frames()
	var cues: Array = sys.recent.map(func(r): return r.cue)
	check(cues.has("end_turn"), "ending the turn plays the end-turn cue (%s)" % [cues])
	check(cues.has("turn_start"), "the turn briefing plays the turn-start cue")
	check(cues.has("construction_complete") or cues.has("research_complete"), "the fixture briefing's construction/research completes play their cues")
	check(not cues.has("ui_click"), "the end-turn button does not also click")
	# a battle
	sys.clock_override = 220.0
	app.goto("combat")
	await frames()
	check(sys.music_context == "battle", "the battle screen plays battle music")
	app.encounter = {"rounds": [{}], "ships": []}
	sys._poll_app()
	check(sys.music_context == "tension", "battle tension starts once a round has been fought")
	app.goto("galaxy", false)
	await frames()
	check(sys.music_context == "galaxy", "leaving the battle returns to galaxy music")
	# red alert on a new contact
	sys.clock_override = 230.0
	sys._seen_contacts.clear()
	sys._poll_app()
	sys.recent.clear()
	app.view["contacts"] = app.view.get("contacts", []).duplicate()
	app.view["contacts"].append({"id": 98765, "owner": 3, "atWar": true, "x": 1, "y": 1, "name": "Probe", "ownerName": "Romulan Star Empire", "className": "Scout", "shipClass": "Scout", "hull": 1, "maxHull": 1, "shields": 0, "maxShields": 0, "attack": 0, "taskForce": false})
	sys.clock_override = 240.0
	sys._poll_app()
	check(sys.recent.any(func(r): return r.cue == "red_alert"), "a new contact sounds the red alert")
	app.queue_free()
	await frames()


## The rebuilt BOTF audio (private, gitignored). Runs only when LocalAssets/audio/cues.json is found
## ($BOTF_AUDIO_DIR or the usual LocalAssets locations); otherwise it reports a skip.
func _local_content() -> void:
	var lib := BotfAudioLibrary.new()
	if lib.cue_files.is_empty():
		print("  skip local audio content: no LocalAssets/audio/cues.json on this machine")
		return
	print("  local audio: %d keys from %s" % [lib.cue_files.size(), lib.cues_path])
	var want := ["sfx.ui.click", "sfx.ui.beep", "sfx.alert.red", "sfx.explosion.large", "weapon.generic.beam", "weapon.generic.torpedo",
		"mus.botf.title", "mus.botf.all", "vo.combat.kill.kli", "vo.combat.ack.fed", "vo.diplomacy.greet.rom", "vo.advisor.war.car",
		"vo.victory.defeat.fer", "vo.victory.conquest.kli"]
	for fac in ["fed", "kli", "rom", "car", "fer"]:
		want.append_array(["mus.botf.galaxy." + fac, "mus.botf.combat." + fac, "mus.botf.combat.tension." + fac, "vo.combat.shields.50." + fac, "vo.advisor.colony." + fac])
	for k in want:
		var r := lib.resolve_files(PackedStringArray([k]))
		var s := lib.stream_of(r) if not r.is_empty() else null
		check(s != null, "local file loads for %s" % k)
	var ogg := lib.stream_of(lib.resolve_files(PackedStringArray(["vo.combat.kill.kli"]))) as AudioStreamOggVorbis
	check(ogg != null and ogg.get_length() > 0.3, "a voice ogg decodes with a length (%.1f s)" % (ogg.get_length() if ogg else 0.0))
	var wav := lib.stream_of(lib.resolve_files(PackedStringArray(["sfx.explosion.large"]))) as AudioStreamWAV
	check(wav != null and wav.mix_rate == 22050, "an effect wav loads at its own rate")
	for ctx in ["title", "galaxy", "battle", "tension"]:
		check(lib.has_files(BotfAudioCues.music_keys(ctx, "kli")), "music context %s has local files for the Klingons" % ctx)


# ------------------------------------------------------------------------------------------------------------
# M1-AUDIO: eras, order acknowledgement, bridge chatter, loads through AppLoader
# ------------------------------------------------------------------------------------------------------------
func _eras_and_orders() -> void:
	check(BotfAudioCues.era_code("NextGeneration") == "tng" and BotfAudioCues.era_code("ds9") == "ds9" and BotfAudioCues.era_code("Original") == "tos"
		and BotfAudioCues.era_code("zzz") == "", "era codes from roster names")
	check(BotfAudioCues.with_era(PackedStringArray(["weapon.Phaser.fed", "weapon.generic.beam"]), "tng")
		== PackedStringArray(["weapon.Phaser.fed.tng", "weapon.Phaser.fed", "weapon.generic.beam.tng", "weapon.generic.beam"]), "era twins go before each key")
	check(BotfAudioCues.with_era(PackedStringArray(["sfx.ui.click"]), "tng") == PackedStringArray(["sfx.ui.click"]), "UI keys have no era twin")
	check(BotfAudioCues.with_era(PackedStringArray(["a"]), "") == PackedStringArray(["a"]), "no era -> keys unchanged")
	check(BotfAudioCues.order_voice_keys("Engage", "fed", "ds9") == PackedStringArray(["vo.combat.engage.fed.ds9", "vo.combat.engage.fed", "vo.combat.ack.fed.ds9", "vo.combat.ack.fed"]), "Engage: engage line, then acknowledgement")
	check(BotfAudioCues.order_voice_keys("Retreat", "kli")[0] == "vo.combat.retreat.kli" and BotfAudioCues.order_voice_keys("Retreat", "").is_empty(), "orders never cross factions")
	var missing := []
	for mv in CombatVM.MANEUVERS:
		if not BotfAudioCues.ORDER_DETAILS.has(mv.key):
			missing.append(mv.key)
	check(missing.is_empty(), "every combat order button has a voice mapping %s" % [missing])
	check(BotfAudioSystem.chatter_kind("weapon.PhotonTorpedo.fed") == "torpedo" and BotfAudioSystem.chatter_kind("weapon.generic.beam") == "beam"
		and BotfAudioSystem.chatter_kind("sfx.explosion.small") == "hull" and BotfAudioSystem.chatter_kind("sfx.explosion.large") == "destroyed"
		and BotfAudioSystem.chatter_kind("sfx.shield.hit") == "shield" and BotfAudioSystem.chatter_kind("sfx.ui.click") == "", "effects map to chatter kinds")
	check(BotfAudioSystem._combat_detail("vo.combat.shields.50.fed.ds9") == "shields.50" and BotfAudioSystem._combat_detail("vo.combat.torpedo.fed") == "torpedo", "bark detail from a key")
	var ch := BotfBridgeChatter.new(7)
	check(ch.next_line(10.0) == "", "no chatter before any event")
	for i in 5:
		ch.note("beam", 10.0 + i * 0.2)
	ch.note("torpedo", 11.0)
	var d1 := ch.next_line(12.0)
	check(d1 in ["phaser", "torpedo", "target", "encounter"], "chatter follows the action (%s)" % d1)
	ch.commit(d1, 12.0)
	for i in 6:
		ch.note("shield", 13.0 + i * 0.1)
	check(ch.next_line(14.0) == "", "chatter holds a gap of %.0f s" % ch.min_gap)
	ch.note_voice(30.0)
	for i in 6:
		ch.note("shield", 33.0 + i * 0.1)
	var d2 := ch.next_line(38.5)
	check(d2 != "" and d2 != d1, "next line differs from the last (%s)" % d2)
	var a := BotfBridgeChatter.new(3)
	var b := BotfBridgeChatter.new(3)
	for c in [a, b]:
		for i in 8:
			c.note("hull", 1.0 + i * 0.1)
			c.note("beam", 1.0 + i * 0.1)
	var la := a.next_line(5.0)
	check(la == b.next_line(5.0) and la != "", "same seed, same line (%s)" % la)
	var c3 := BotfBridgeChatter.new(1)
	c3.max_per_fight = 2
	for t in [10.0, 30.0, 50.0]:
		c3.note("destroyed", t)
		var dd := c3.next_line(t + 0.5)
		if dd != "":
			c3.commit(dd, t + 0.5)
	check(c3.lines_said == 2, "chatter stops at the per-fight cap")


func _voice_layers() -> void:
	var sys := _sys()
	if sys == null:
		return
	var dir := ProjectSettings.globalize_path("user://audio_test/orders")
	DirAccess.make_dir_recursive_absolute(dir)
	for n in ["ack", "retreat", "underattack", "torpedo", "ack_tng"]:
		_write_wav(dir.path_join(n + ".wav"), 0.2)
	var lib := sys.library
	for k in ["ack", "retreat", "underattack", "torpedo"]:
		lib.cue_files["vo.combat.%s.fed" % k] = PackedStringArray(["user://audio_test/orders/%s.wav" % k])
	sys._app = null
	sys.enabled = true
	sys.era = ""
	sys.clock_override = 500.0
	sys.recent.clear()
	var got := sys.order_ack("Retreat")
	check(got.get("sfx", "") == "sfx.ui.beep", "an order plays the beep even with no faction")
	check(not got.has("voice"), "no player faction -> no crew voice")
	var fake := _FakeApp.new()
	sys._app = fake
	sys.clock_override = 510.0
	got = sys.order_ack("Retreat")
	check(got.get("voice", "") == "vo.combat.retreat.fed", "Retreat is acknowledged with the retreat line: %s" % [got])
	sys.clock_override = 520.0
	sys.scheduler = BotfCueScheduler.new()
	got = sys.order_ack("Engage")
	check(got.get("voice", "") == "vo.combat.ack.fed", "Engage falls back to the acknowledgement group: %s" % [got])
	sys.clock_override = 530.0
	sys.scheduler = BotfCueScheduler.new()
	sys.recent.clear()
	var pill := Button.new()
	pill.name = "Maneuver_Evade"
	root.add_child(pill)
	await frames()
	pill.pressed.emit()
	check(sys.recent.any(func(r): return r.cue == "order_accepted"), "pressing Maneuver_Evade fires the order cue")
	check(not sys.recent.any(func(r): return r.cue == "ui_click"), "and not also the generic click")
	lib.cue_files["vo.combat.ack.fed.tng"] = PackedStringArray(["user://audio_test/orders/ack_tng.wav"])
	lib.eras_present["tng"] = true
	sys.set_era("NextGeneration")
	check(sys.current_era() == "tng" and sys.settings.get("audio_era") == "tng", "era set from a roster name and kept in settings")
	sys.clock_override = 540.0
	sys.scheduler = BotfCueScheduler.new()
	got = sys.order_ack("Engage")
	check(got.get("voice", "") == "vo.combat.ack.fed.tng", "the era's own line is preferred: %s" % [got])
	sys.set_era("auto")
	check(sys.current_era() == BotfAudioCues.DEFAULT_ERA, "auto -> the default era")
	sys.set_music("battle")
	sys.scheduler = BotfCueScheduler.new()
	sys.chatter = BotfBridgeChatter.new(5)
	sys.clock_override = 600.0
	for i in 4:
		sys.chatter.note("torpedo", 599.0 + i * 0.2)
		sys.chatter.note("hull", 599.0 + i * 0.2)
	var line := sys.chatter_tick()
	check(line.begins_with("vo.combat."), "a fight produces a chatter line: %s" % line)
	sys.chatter_enabled = false
	sys.chatter = BotfBridgeChatter.new(5)
	sys.chatter.note("hull", 700.0)
	sys.clock_override = 701.0
	check(sys.chatter_tick() == "", "chatter can be switched off")
	sys.chatter_enabled = true
	sys._app = null
	pill.queue_free()
	fake.free()


class _FakeApp extends Node:
	var screen := "combat"
	var view := {}
	var encounter := {}
	func player() -> int:
		return 1
