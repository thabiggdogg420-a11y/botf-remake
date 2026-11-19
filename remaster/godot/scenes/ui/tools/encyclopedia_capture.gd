extends SceneTree
## F5 encyclopedia frame proof: Database > Ships entries with the 3D viewer, through the real App.
##   godot --path remaster/godot --audio-driver Dummy --resolution 1920x1080 -s res://scenes/ui/tools/encyclopedia_capture.gd -- <out_dir> [count]
## Picks roster ships across the powers (first with each faction), shoots each entry twice (the model turns between),
## prints "ENCY <class>: key <assetKey> hero <hero key>" and exits 1 when a roster ship has no viewer.

var app: App
var out_dir := "user://ency"
var code := 0


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() > 0:
		out_dir = str(args[0])
	DirAccess.make_dir_recursive_absolute(out_dir)
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred(int(args[1]) if args.size() > 1 else 10)


func _wait(n: int) -> void:
	for i in n:
		await process_frame


func _shot(name: String) -> void:
	await _wait(2)
	var img := root.get_viewport().get_texture().get_image()
	img.resize(960, 540, Image.INTERPOLATE_LANCZOS)
	img.save_png("%s/%s.png" % [out_dir, name])


func _run(count: int) -> void:
	await _wait(10)
	app.goto("database")
	await _wait(6)
	var ds = app.current_screen()
	ds.tab = "ships"
	ds.refresh()
	await _wait(6)
	var seen := {}
	var picks: Array = []
	for r in ds.vm.rows():
		var e: Dictionary = ds.vm.entry("ships", int(r["id"]))
		var ro: Variant = e.get("roster")
		if not (ro is Dictionary) or str(ro.get("assetKey", "")) == "":
			continue
		var f := str(ro.get("faction", ""))
		if seen.has(f):
			continue
		seen[f] = true
		picks.append(int(r["id"]))
		if picks.size() >= count:
			break
	for i in picks.size():
		ds.selected = picks[i]
		ds.refresh()
		await _wait(30)
		var v: ShipViewer3D = ds.find_child("ShipViewer", true, false)
		var e: Dictionary = ds.vm.entry("ships", picks[i])
		if v == null:
			print("ENCY %s: NO VIEWER" % e["designClass"])
			code = 1
			continue
		print("ENCY %s: key %s hero %s" % [e["designClass"], v.asset_key, v.hero_key()])
		await _shot("ency-%02d-a" % i)
		await create_timer(3.0).timeout
		await _shot("ency-%02d-b" % i)
	print("ENCY done, %d entries, code %d" % [picks.size(), code])
	quit(code)
