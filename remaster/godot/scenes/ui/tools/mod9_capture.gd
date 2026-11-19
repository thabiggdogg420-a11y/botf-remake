extends SceneTree
## MOD-9 frame proof (F4 round 3): frames of every cinematic and end-game speech through the real App, plus a listen log.
##   godot --path remaster/godot --audio-driver Dummy --resolution 1920x1080 -s res://scenes/ui/tools/mod9_capture.gd -- <out_dir> [items]
## Items (default all): movie-open movie-fed movie-kli movie-rom movie-car movie-fer dominion borg-arrival borg-sphere
## borg-assimilation end-<fac>-<outcome> (fac fed kli rom car fer dom, outcome conquest domination diplomatic defeat).
## Each cinematic is shot at several moments (<item>-<n>.png); prints "MOD9 <item>: ..." lines (movie length, the voice key
## the audio system resolved and the file it would play, subtitle text) and exits 1 when something expected is missing.

var app: App
var out_dir := "user://mod9"
var code := 0


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() > 0:
		out_dir = str(args[0])
	DirAccess.make_dir_recursive_absolute(out_dir)
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred(args.slice(1))


func _wait(n: int) -> void:
	for i in n:
		await process_frame


func _secs(s: float) -> void:
	await create_timer(s).timeout


func _shot(name: String) -> void:
	await _wait(2)
	var img := root.get_viewport().get_texture().get_image()
	img.resize(960, 540, Image.INTERPOLATE_LANCZOS)
	img.save_png("%s/%s.png" % [out_dir, name])


func _voice_report(item: String, key: String) -> void:
	var lib: BotfAudioLibrary = BotfAudioSystem.instance.library if BotfAudioSystem.instance != null else null
	var r: Dictionary = lib.resolve_files(PackedStringArray([key])) if lib != null else {}
	var files: Array = Array(r.get("files", PackedStringArray()))
	var sp: Dictionary = Cinema.speeches().get(key, {})
	print("MOD9 %s: voice %s -> %s (%.1f s) | %s" % [item, key, files[0] if not files.is_empty() else "NONE", float(sp.get("seconds", 0.0)), str(sp.get("text", "NO TEXT")).left(80)])
	if files.is_empty() or sp.is_empty():
		code = 1


func _run(items: Array) -> void:
	await _wait(10)
	if items.is_empty():
		items = ["movie-open", "movie-fed", "movie-kli", "movie-rom", "movie-car", "movie-fer", "dominion", "borg-arrival", "borg-sphere", "borg-assimilation"]
		for fac in ["fed", "kli", "rom", "car", "fer", "dom"]:
			for o in ["conquest", "domination", "diplomatic", "defeat"]:
				items.append("end-%s-%s" % [fac, o])
	for it: String in items:
		if it.begins_with("movie-"):
			var name := it.trim_prefix("movie-")
			var c := Cinema.play_movie(app, name)
			if c == null:
				print("MOD9 %s: NO MOVIE" % it)
				code = 1
				continue
			print("MOD9 %s: playing %s, %.1f s" % [it, Cinema.movie_path(name), c._player.get_stream_length()])
			var marks := [1.0, 4.0, 9.0, 16.0, 22.0] if name != "open" else [2.0, 10.0, 22.0, 36.0, 52.0, 66.0, 72.0]
			var last := 0.0
			for i in marks.size():
				await _secs(marks[i] - last)
				last = marks[i]
				if not is_instance_valid(c):
					print("MOD9 %s: ENDED EARLY before t=%.1f" % [it, last])
					code = 1
					break
				print("MOD9 %s: t=%.1f playing=%s pos=%.2f" % [it, last, c._player.is_playing(), c._player.stream_position])
				await _shot("%s-%d" % [it, i])
			if is_instance_valid(c):
				c.finish()
			await _wait(4)
		elif it == "dominion":
			var c := Cinema.dominion(app)
			_voice_report(it, "vo.victory.domination.dom")
			for i in 3:
				await _secs([1.5, 4.0, 5.0][i])
				await _shot("%s-%d" % [it, i])
			c.finish()
			await _wait(4)
		elif it.begins_with("borg-"):
			var k := "assimilation" if it == "borg-assimilation" else "arrival"
			var asset := "ship.borg.sphere" if it == "borg-sphere" else "ship.borg.tactical_cube"
			var c := Cinema.borg(app, k, asset, "The Borg have assimilated Vega" if k == "assimilation" else "")
			_voice_report(it, Cinema.BORG_LINES[k])
			print("MOD9 %s: hull %s hero=%s" % [it, asset, c._ship.hero_key])
			if c._ship.hero_key == "":
				code = 1
			var marks := [0.8, 2.0, 3.2, 6.0, 10.0, 14.5] if k == "arrival" else [0.8, 2.5, 5.0, 8.0]
			var last := 0.0
			for i in marks.size():
				await _secs(marks[i] - last)
				last = marks[i]
				await _shot("%s-%d" % [it, i])
			c.finish()
			await _wait(4)
		elif it.begins_with("end-"):
			var parts := it.split("-")
			var fac := parts[1]
			var outcome := parts[2]
			var emp: int = {"fed": 1, "kli": 2, "rom": 3, "car": 4, "fer": 5, "dom": 6}[fac]
			if app.view.is_empty() or int(app.view["meta"]["player"]) != emp:
				app.view = {}
				var opts := {"faction": emp, "seed": 424242, "era": 3, "ruleset": "reference", "width": 22, "height": 16, "systems": 48, "preset": "remastered"}
				if emp == 6:
					opts["lineup"] = ["federation", "klingon", "romulan", "cardassian", "ferengi", "dominion"]
				app.start_campaign(opts)
				app.goto("galaxy")
				await _wait(20)
			# The view as the core would leave it at the end: the achieved condition set for the preview.
			var v: Dictionary = app.view
			for cnd in v.get("victory", {}).get("conditions", []):
				cnd["achieved"] = outcome != "defeat" and str(cnd["condition"]).to_lower() == outcome
			for t in app.get_node("Toasts").get_children():
				t.queue_free()
			var m := Overlays.end_screen(app, "victory" if outcome != "defeat" else "defeat")
			var sp: Control = m.find_child("EndSpeech", true, false)
			var key := str(sp.get_meta("speech_key")) if sp != null else ""
			if sp == null:
				print("MOD9 %s: NO SPEECH PANEL" % it)
				code = 1
			else:
				_voice_report(it, key)
			await _secs(6.0)
			await _shot(it)
			app.close_modal()
			await _wait(4)
	print("MOD9 done, code ", code)
	quit(code)
