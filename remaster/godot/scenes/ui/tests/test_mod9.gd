extends SceneTree
## F4 round 3 (MOD-9 and end-game speeches, the flagship beacon): the pure parts, headless.
##   godot --headless --path remaster/godot --audio-driver Dummy -s res://scenes/ui/tests/test_mod9.gd
## The frames and the listen check are tools/mod9_capture.gd's (a real window); this suite pins the rules the screens follow.

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
	# The Dominion speaks with its own key family; the Borg and Breen still have none.
	check(BotfAudioCues.faction_prefix(6) == "dom" and BotfAudioCues.faction_prefix(7) == "", "Dominion voice prefix, none for the Borg")
	var c := BotfAudioCues.cue_for("war_declared", "fed", "dom")
	check(Array(c.voice).has("vo.diplomacy.war.dom"), "a Dominion war declaration asks for the Dominion's line")

	# End-game speech keys follow the achieved condition.
	var v := {"victory": {"conditions": [{"condition": "Conquest", "achieved": false}, {"condition": "Diplomatic", "achieved": true}]}}
	check(Overlays.speech_key(true, v, "kli") == "vo.victory.diplomatic.kli", "diplomatic win -> diplomatic speech")
	check(Overlays.speech_key(false, v, "kli") == "vo.victory.defeat.kli", "a loss -> the defeat speech")
	var sci := {"victory": {"conditions": [{"condition": "Scientific", "achieved": true}]}}
	check(Overlays.speech_key(true, sci, "fed") == "vo.victory.win.fed", "a scientific win falls back to the win speech")
	check(Overlays.speech_key(true, v, "") == "", "no faction, no speech")

	# Borg cutscene triggers from the briefing text.
	var b := {"events": [{"text": "The Borg have assimilated Vega (Federation)."}, {"text": "Starfleet Intelligence: Borg sphere detected entering the quadrant."}]}
	var pick := App.borg_cutscene_for(b, "Federation")
	check(pick.get("kind") == "arrival" and pick.get("asset") == "ship.borg.sphere", "an arrival wins over an assimilation, the sphere's hull")
	pick = App.borg_cutscene_for({"events": [{"text": "The Borg have assimilated Vega (Federation)."}]}, "Federation")
	check(pick.get("kind") == "assimilation" and str(pick.get("caption")).ends_with("Vega"), "the player's world assimilated")
	check(App.borg_cutscene_for({"events": [{"text": "The Borg have assimilated Vega (Klingon Empire)."}]}, "Federation").is_empty(), "a rival's world: no cutscene")
	check(App.borg_cutscene_for({"events": [{"text": "Starfleet Intelligence: Tholian web detected entering the quadrant."}]}, "Federation").is_empty(), "a non-Borg threat: no cutscene")
	check(App.borg_cutscene_for(null, "Federation").is_empty(), "no briefing")

	# The beacon sits on the hull under it, not on a tall fin to the side.
	var root := Node3D.new()
	get_root().add_child(root)
	var body := MeshInstance3D.new()
	var bm := BoxMesh.new()
	bm.size = Vector3(4, 1, 8)
	body.mesh = bm
	root.add_child(body)
	var fin := MeshInstance3D.new()
	var fm := BoxMesh.new()
	fm.size = Vector3(0.2, 6, 1)
	fin.mesh = fm
	fin.position = Vector3(1.9, 3.0, 3.0)   # a tall fin at the side and aft: the bounding box's top is 6
	root.add_child(fin)
	var top := BattleShip.top_near_centre(root, 1.2, 99.0)
	check(is_equal_approx(top, 0.5), "beacon height from the hull top at the middle (0.5), not the fin (6): %s" % top)
	check(BattleShip.top_near_centre(Node3D.new(), 1.0, 7.0) == 7.0, "no mesh: the fallback height")
	root.queue_free()

	# Cinematics stay off unless main.gd switches them on (tests and tools never see a movie).
	var app: App = load("res://scenes/ui/app.tscn").instantiate()
	check(not app.cinematics_live and not app.cinematics_on(), "cinematics off outside a real run")
	app.free()

	# F4 polish and F5 (20 Nov): the assimilation beam's shaders compile from code; autosaves rotate through three slots;
	# a headless run takes no thumbnail; a slot without one draws no texture.
	for sh in [Cinema._beam_shader(), Cinema._beam_ground_shader(), Cinema._beam_halo_shader()]:
		check(sh.code.contains("intensity"), "beam shader carries its intensity uniform")
	check(AppSaves.autosave_name(0) == "Autosave 1" and AppSaves.autosave_name(1) == "Autosave 2" and AppSaves.autosave_name(2) == "Autosave 3" and AppSaves.autosave_name(3) == "Autosave 1", "autosaves rotate through three slots")
	check(AppSaves.thumbnail(root.get_viewport()) == "", "headless: no thumbnail")
	check(AppSaves.thumb_texture({}) == null and AppSaves.thumb_texture({"thumb": "bm90IGEganBlZw=="}) == null, "no thumb or a broken one: no texture")

	print("%d checks, %d failures" % [checks, failures])
	quit(1 if failures > 0 else 0)
