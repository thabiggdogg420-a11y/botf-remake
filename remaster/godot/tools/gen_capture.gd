extends SceneTree
## M2-GEN captures: for each galaxy shape, the New campaign screen (live preview from the core) and the turn-1 galaxy of the
## campaign those options start. Needs a window (GPU) and the native core.
##   godot --path remaster/godot --resolution 1920x1080 --audio-driver Dummy -s res://tools/gen_capture.gd -- <out_dir> [seed]
## Writes <out_dir>/new-<shape>.png and <out_dir>/galaxy-<shape>.png, and prints the preview and campaign star counts.
var app: App

func wait(k := 2) -> void:
	for i in k:
		await process_frame

func _initialize() -> void:
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred()

func _shot(path: String) -> void:
	await wait(8)
	await RenderingServer.frame_post_draw
	root.get_texture().get_image().save_png(path)
	print("GEN wrote ", path)

func _run() -> void:
	var a := OS.get_cmdline_user_args()
	var out := a[0] if a.size() > 0 else "user://gen"
	var seed_v := int(a[1]) if a.size() > 1 else 424242
	DirAccess.make_dir_recursive_absolute(out)
	await wait(10)
	print("GEN backend: %s" % app.session.core.backend)
	var combos := [["spiral", "some", "normal"], ["ring", "many", "hard"], ["irregular", "few", "impossible"], ["elliptical", "none", "simple"]]
	for c in combos:
		app.goto("new")
		await wait(6)
		var nc := app.current_screen()
		nc.vm.shape = c[0]
		nc.vm.minor_density = c[1]
		nc.vm.difficulty = c[2]
		nc.seed_value = seed_v
		nc.refresh()
		await wait(10)
		var pv: Dictionary = nc.vm.preview_galaxy()
		var preview_minors: int = pv["stars"].filter(func(s): return int(s[2]) == 6).size() if not pv.is_empty() else -1
		print("GEN preview %s/%s/%s: %d stars, %d minors" % [c[0], c[1], c[2], pv["stars"].size() if not pv.is_empty() else -1, preview_minors])
		await _shot("%s/new-%s.png" % [out, c[0]])
		var opts: Dictionary = nc.vm.options()
		nc.start()
		await wait(30)
		var v: Dictionary = app.view
		var minors: int = v.get("minors", []).size() if v.get("minors") is Array else -1
		print("GEN campaign %s: %d systems, shape %s, density %s, difficulty %s, turn %d" % [c[0], v["systems"].size(), v["meta"].get("galaxyShape", "?"), v["meta"].get("minorDensity", "?"), v["meta"].get("difficulty", "?"), int(v["meta"]["turn"])])
		await create_timer(4.0).timeout
		await _shot("%s/galaxy-%s.png" % [out, c[0]])
	quit(0)
