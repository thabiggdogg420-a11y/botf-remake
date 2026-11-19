extends SceneTree
## F5 adaptive music: BotfMusicDirector's moods from a view, the tier table from tools/identify_music.py, and segment picks.
##   godot --headless --audio-driver Dummy --path remaster/godot -s res://scripts/audio/tests/test_music_director.gd

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


func _run() -> void:
	print("Music director tests")
	var n := BotfMusicDirector.load_table()
	check(n >= 150, "tier table loads every segment (%d)" % n)
	var tiers := {}
	for t in ["calm", "build", "peak"]:
		tiers[t] = 0
	var doc: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(BotfMusicDirector.TABLE_PATH))
	check(doc.get("tracks", []).size() == 16, "16 tracks identified")
	for tr in doc.get("tracks", []):
		check(str(tr.get("identified_as", "")) != "", "track %d is named" % int(tr.track))
		var seen := {}
		for s in tr.get("segments", []):
			seen[str(s.tier)] = true
			tiers[str(s.tier)] += 1
		if tr.get("segments", []).size() >= 3:
			check(seen.size() == 3, "track %d has calm, build and peak segments" % int(tr.track))
	check(BotfMusicDirector.tier_of("res://scripts/audio/pack/fed/music/music_music_t02_s07.ogg") != "", "pack file names map to the table")

	# Moods from the view.
	var sys := [{"id": 1, "x": 5, "y": 5}, {"id": 2, "x": 20, "y": 10}]
	var peace := {"empires": [{"empire": 2, "atWar": false}], "systems": sys, "colonies": [{"systemId": 1}], "contacts": []}
	check(BotfMusicDirector.mood_for(peace, 1) == "calm", "at peace: calm")
	var war := peace.duplicate(true)
	war.empires[0].atWar = true
	check(BotfMusicDirector.mood_for(war, 1) == "build", "at war, nothing near home: build")
	war.contacts = [{"id": 9, "owner": 2, "atWar": true, "x": 7, "y": 4}]
	check(BotfMusicDirector.mood_for(war, 1) == "peak", "hostile fleet two sectors from a colony: peak")
	war.contacts = [{"id": 9, "owner": 2, "atWar": true, "x": 12, "y": 4}]
	check(BotfMusicDirector.mood_for(war, 1) == "build", "hostile fleet far away: build")
	check(BotfMusicDirector.mood_for(war, 1, true) == "peak", "a colony lost while at war: peak")
	check(BotfMusicDirector.mood_for(peace, 1, true) == "calm", "no war: calm even after a loss (a revolt is not a fight)")
	var peaceful_contact := peace.duplicate(true)
	peaceful_contact.contacts = [{"id": 9, "owner": 2, "atWar": false, "x": 5, "y": 5}]
	check(BotfMusicDirector.mood_for(peaceful_contact, 1) == "calm", "a friendly fleet at home does not raise the mood")
	check(BotfMusicDirector.player_colonies(war, 1) == 1, "colony count reads the player's own colonies")

	# Segment picks follow the mood and keep game order.
	var pl := PackedStringArray()
	for s in 15:
		pl.append("res://scripts/audio/pack/fed/music/music_music_t02_s%02d.ogg" % s)
	for mood in ["calm", "build", "peak"]:
		var pos := BotfMusicDirector.first_index(pl, 0, mood)
		check(BotfMusicDirector.allows(mood, pl[pos]), "%s: the first segment fits the mood" % mood)
		var visited := {}
		var misfits := 0
		for k in 30:
			var nxt := BotfMusicDirector.next_index(pl, pos, mood)
			if not BotfMusicDirector.allows(mood, pl[nxt]):
				misfits += 1
			visited[nxt] = true
			pos = nxt
		check(misfits == 0, "%s: 30 steps all fit the mood" % mood)
		check(visited.size() >= 3, "%s: cycles through %d segments, not one on repeat" % [mood, visited.size()])
	var calm_set := {}
	var peak_set := {}
	for i in pl.size():
		if BotfMusicDirector.allows("calm", pl[i]):
			calm_set[i] = true
		if BotfMusicDirector.allows("peak", pl[i]):
			peak_set[i] = true
	check(calm_set.size() < pl.size() and peak_set.size() < pl.size() and calm_set.hash() != peak_set.hash(), "calm and peak play different segment sets")
	check(BotfMusicDirector.allows("peak", "res://x/unknown.ogg"), "an unknown file is never skipped")

	await _live()
	print("%d checks, %d failures" % [checks, failures])
	quit(1 if failures else 0)


## The audio system plays the strategic track adaptively: calm segments at peace, a crossfade to a peak segment at once when
## the mood rises to peak, and the next segment after one ends fits the mood.
func _live() -> void:
	var sys := BotfAudioSystem.new()
	root.add_child(sys)
	await process_frame
	sys.music_mood = "calm"
	sys.set_music("galaxy")
	if sys._playlist.is_empty():
		print("  (no local music pack: live checks skipped)")
		sys.queue_free()
		return
	check(BotfMusicDirector.allows("calm", sys._playlist[sys._playlist_pos]), "galaxy music starts on a calm-mood segment")
	# From a calm-tier segment (peak does not allow it) the rise must cut over; from a build segment it may play on.
	for i in sys._playlist.size():
		if BotfMusicDirector.tier_of(sys._playlist[i]) == "calm":
			sys._playlist_pos = i
			break
	var front := sys._music_front
	sys.set_music_mood("peak")
	check(sys.music_mood == "peak" and BotfMusicDirector.tier_of(sys._playlist[sys._playlist_pos]) in ["peak", "build"], "peak: a peak-mood segment is chosen at once")
	check(sys._music_front != front, "peak from a calm segment crossfades straight away")
	var at := sys._playlist_pos
	sys.set_music_mood("calm")
	check(sys._playlist_pos == at, "a fall to calm waits for the segment to end")
	sys._on_music_finished(sys._music[sys._music_front])
	check(BotfMusicDirector.allows("calm", sys._playlist[sys._playlist_pos]), "the next segment fits calm")
	sys.set_music("battle")
	var bpos := sys._playlist_pos
	sys.set_music_mood("peak")
	check(sys._playlist_pos == bpos and not sys._adaptive_here(), "battle music is not adaptive (approach / tension unchanged)")
	sys.adaptive_music = false
	sys.set_music("galaxy")
	check(sys._playlist_pos == 0, "adaptive off: the track starts at its first segment, as before")
	sys.queue_free()
	await process_frame
