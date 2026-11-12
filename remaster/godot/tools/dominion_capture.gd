extends SceneTree
## MOD-3 captures: a Dominion War game (the Dominion as the player's empire, the preset's overlay stack) at 1920x1080 on a real GPU
## window with the native core: the system screen, its build list, the installed structures, the intel screen and the diplomacy screen.
##   godot --path remaster/godot --resolution 1920x1080 --audio-driver Dummy -s res://tools/dominion_capture.gd -- <out_dir> [seed]
## Writes <out_dir>/dominion-system.png, dominion-buildlist.png, dominion-installed.png, dominion-intel.png, dominion-diplomacy.png and
## prints the Dominion capital's structures and its build choices. The six empires of the lineup are fixed (Federation, Klingon,
## Romulan, Cardassian, Dominion), as in the preset.
var app: App

func wait(k := 2) -> void:
	for i in k:
		await process_frame

func _initialize() -> void:
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred()

func _shot(path: String) -> void:
	await wait(10)
	for t in app.get_node("Toasts").get_children():
		t.queue_free()
	await RenderingServer.frame_post_draw
	root.get_texture().get_image().save_png(path)
	print("DOM wrote ", path)

func _run() -> void:
	var a := OS.get_cmdline_user_args()
	var out := a[0] if a.size() > 0 else "user://dominion"
	var seed_v := int(a[1]) if a.size() > 1 else 424242
	DirAccess.make_dir_recursive_absolute(out)
	await wait(10)
	print("DOM backend: %s" % app.session.core.backend)
	var ok := app.start_campaign({"faction": 6, "seed": seed_v, "era": 3, "ruleset": "reference", "width": 22, "height": 16, "systems": 48,
		"preset": "dominion-war", "lineup": ["federation", "klingon", "romulan", "cardassian", "dominion"], "minorDensity": "many",
		"difficulty": "hard", "events": true, "aiAggression": true})
	print("DOM started: %s, preset %s" % [ok, str(app.view.get("meta", {}).get("preset", "?"))])
	if not ok:
		quit(1)
		return
	if app.core.is_native():
		for n in 6:
			app.end_turn()
			app.close_all_modals()
	app.close_all_modals()
	await wait(10)
	# The capital's detail: built structures and the build list (structures tab).
	app.goto("system")
	await wait(8)
	var sc: Node = app.current_screen()
	var d: Dictionary = sc.vm.detail
	print("DOM capital structures: %s" % str(d.get("raceStructures", []).map(func(x): return x["name"])))
	print("DOM build choices: %s" % str(d.get("choices", []).filter(func(x): return x["product"] == "Structure").map(func(x): return "%s [%s]" % [x["name"], x["blocker"]])))
	await _shot("%s/dominion-system.png" % out)
	sc.tab = "structures"
	sc.refresh()
	await wait(6)
	await _shot("%s/dominion-buildlist.png" % out)
	sc.tab = "installed"
	sc.refresh()
	await wait(6)
	await _shot("%s/dominion-installed.png" % out)
	app.goto("intel")
	await _shot("%s/dominion-intel.png" % out)
	app.goto("diplomacy")
	await _shot("%s/dominion-diplomacy.png" % out)
	app.goto("empire")
	await _shot("%s/dominion-empire.png" % out)
	quit(0)
