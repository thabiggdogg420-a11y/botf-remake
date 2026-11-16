extends SceneTree
## Portrait look-test capture (real GPU): the race portraits as they appear in the real screens.
##   godot --path remaster/godot --audio-driver Dummy --resolution 1920x1080 -s res://scenes/ui/tools/portrait_capture.gd -- <out_dir> <tag> [<variant_dir> [majors|minors|all [legacy]]]
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
	if a.size() > 3:
		a_what = a[3]
	PortraitSlot.stretch_legacy = a.size() > 4 and a[4] == "legacy"
	## a[2] (optional): a folder of <key>.png variants. They go straight into AppLoader's art cache, which wins over the
	## portraits in the shared user://packs/art_desktop.pck (that pack must stay untouched: other lanes use it).
	if a.size() > 2 and DirAccess.dir_exists_absolute(a[2]):
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
	var what := "majors" if a_what == "" else a_what
	if what != "minors":
		await _majors()
	if what != "majors":
		await _minors()
	quit(0)

var a_what := ""

## Majors 1..8 (Federation, Klingon, Romulan, Cardassian, Ferengi, Dominion, Borg, Breen): race card, hail panel, Database entry, Empire banner.
func _majors() -> void:
	var race_of := {1: 1, 2: 3, 3: 4, 4: 0, 5: 2, 6: 35, 7: 36, 8: 37}
	for id in range(1, 9):
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
	for id in range(1, 9):
		app.goto("database")
		await _wait(6)
		var ds = app.current_screen()
		ds.tab = "races"
		ds.selected = race_of[id]
		ds.refresh()
		await _wait(6)
		await _save("db-%d" % id, _slot(ds))
	# Player = Federation: hail 2..8 and the Empire banner. Player = Klingon: hail the Federation and the banner.
	for pair in [[1, [2, 3, 4, 5, 6, 7, 8]], [2, [1]]]:
		_campaign(pair[0])
		await _wait(6)
		app.goto("empire")
		await _wait(8)
		await _save("empire-%d" % pair[0], _slot(app.current_screen()))
		for t in pair[1]:
			app.goto("diplomacy")
			await _wait(6)
			var d = app.current_screen()
			d.vm.select(false, t)
			d.refresh()
			await _wait(8)
			await _save("hail-%d" % t, _slot(d))

## Every minor race in Database > Minor races (the 420x236 slot).
func _minors() -> void:
	app.goto("database")
	await _wait(6)
	var ds = app.current_screen()
	ds.tab = "minors"
	ds.refresh()
	await _wait(6)
	var rows: Array = ds.vm.rows()
	for r in rows:
		ds.selected = int(r["id"])
		ds.refresh()
		await _wait(5)
		await _save("minor-%s" % str(r["title"]).to_lower().replace(" ", "").replace("'", "").replace("-", ""), _slot(ds))
