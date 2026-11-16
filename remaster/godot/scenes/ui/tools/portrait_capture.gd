extends SceneTree
## Portrait look-test capture (real GPU): the race portraits as they appear in the real screens.
##   godot --path remaster/godot --audio-driver Dummy --resolution 1920x1080 -s res://scenes/ui/tools/portrait_capture.gd -- <out_dir> <tag> [<variant_dir>]
## Writes <out_dir>/<tag>-<screen>-<race>.png (full 1920x1080) and <tag>-<screen>-<race>-crop.png (the portrait slot, 2x),
## for the Federation (1), Klingon (2) and Cardassian (4):
##   new   the race-select card (76x62) with that power picked
##   hail  the Diplomacy hail panel (300x170) with that race selected
##   db    the Database > Races entry (420x236)
##   empire the player's Empire banner portrait (112x112), player = that race
## Portraits come from res://art/portrait/<key>.png (gitignored; the look test stages each variant there).

var app: App
var out_dir := ""
var tag := ""

func _wait(n: int = 8) -> void:
	for i in n:
		await process_frame

func _initialize() -> void:
	var a := Array(OS.get_cmdline_user_args())
	out_dir = a[0]
	tag = a[1]
	## a[2] (optional): a folder of <key>.png variants. They go straight into AppLoader's art cache, which wins over the
	## portraits in the shared user://packs/art_desktop.pck (that pack must stay untouched: other lanes use it).
	if a.size() > 2:
		for f in DirAccess.get_files_at(a[2]):
			if f.ends_with(".png"):
				var img := Image.load_from_file(a[2].path_join(f))
				img.generate_mipmaps()
				AppLoader._art_cache["portrait/" + f.get_basename()] = ImageTexture.create_from_image(img)
	DirAccess.make_dir_recursive_absolute(out_dir)
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred()

func _save(name_: String, crop_of: Control = null) -> void:
	await _wait(10)
	var img := root.get_viewport().get_texture().get_image()
	img.save_png("%s/%s-%s.png" % [out_dir, tag, name_])
	if crop_of != null and is_instance_valid(crop_of):
		var r := crop_of.get_global_rect()
		var sc := Vector2(img.get_width(), img.get_height()) / Vector2(root.get_visible_rect().size)
		var reg := Rect2i(Vector2i(r.position * sc), Vector2i(r.size * sc))
		var c := img.get_region(reg)
		c.resize(c.get_width() * 2, c.get_height() * 2, Image.INTERPOLATE_LANCZOS)
		c.save_png("%s/%s-%s-crop.png" % [out_dir, tag, name_])
	print("saved ", name_)

func _slot(node: Node) -> PortraitSlot:
	for n in node.find_children("*", "PortraitSlot", true, false):
		return n
	return null

func _campaign(player: int) -> void:
	app.view = {}
	var keys := ["federation", "klingon", "romulan", "cardassian", "ferengi"]
	app.start_campaign({"faction": player, "seed": 424242, "era": 3, "ruleset": "reference", "width": 22, "height": 16, "systems": 48,
		"preset": "remastered", "events": true, "aiAggression": true, "lineup": keys})
	for n in 6:
		app.end_turn()
		app.close_all_modals()
	app.close_all_modals()

func _run() -> void:
	await _wait(6)
	for id in [1, 2, 4]:
		app.view = {}
		app.goto("new")
		await _wait(6)
		app._current.vm.faction = id
		app._current.refresh()
		await _wait(6)
		var ps: PortraitSlot = null
		for n in app._current.find_children("*", "PortraitSlot", true, false):
			if (n as PortraitSlot).empire == id:
				ps = n
		await _save("new-%d" % id, ps)
	for id in [1, 2, 4]:
		var race: int = {1: 1, 2: 3, 4: 0}[id]
		app.goto("database")
		await _wait(6)
		var ds = app.current_screen()
		ds.tab = "races"
		ds.selected = race
		ds.refresh()
		await _wait(6)
		await _save("db-%d" % id, _slot(ds))
	# Player = Federation: hail the Klingons and Cardassians, and the Empire banner. Player = Klingon: hail the Federation.
	for pair in [[1, [2, 4]], [2, [1]]]:
		_campaign(pair[0])
		await _wait(6)
		if pair[0] == 1:
			app.goto("empire")
			await _wait(8)
			await _save("empire-1", _slot(app.current_screen()))
		else:
			app.goto("empire")
			await _wait(8)
			await _save("empire-2", _slot(app.current_screen()))
		for t in pair[1]:
			app.goto("diplomacy")
			await _wait(6)
			var d = app.current_screen()
			d.vm.select(false, t)
			d.refresh()
			await _wait(8)
			await _save("hail-%d" % t, _slot(d))
	quit(0)
