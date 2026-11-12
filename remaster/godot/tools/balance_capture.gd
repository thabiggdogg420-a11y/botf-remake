extends SceneTree
## MOD-7 captures at 1920x1080 on a real GPU window with the native core: the New campaign screen with each mod preset's options
## (planets per star, start systems, star types, difficulty with Expert), a Balance of Power game's capital build list (scaled
## costs) and the galaxy with race star types under Galaxies.
##   godot --path remaster/godot --resolution 1920x1080 --audio-driver Dummy -s res://tools/balance_capture.gd -- <out_dir>
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
	print("BAL wrote ", path)

func _find_text(node: Node, needle: String) -> Control:
	if node is Control and (node.name == needle or (node is Label and needle in (node as Label).text)):
		return node as Control
	for c in node.get_children():
		var r := _find_text(c, needle)
		if r != null:
			return r
	return null

func _reveal(node: Node, needle: String) -> bool:
	var hit := _find_text(node, needle)
	if hit == null:
		return false
	var p: Node = hit.get_parent()
	while p != null:
		if p is ScrollContainer:
			(p as ScrollContainer).ensure_control_visible(hit)
		p = p.get_parent()
	return true

func _run() -> void:
	var a := OS.get_cmdline_user_args()
	var out := a[0] if a.size() > 0 else "user://balance"
	DirAccess.make_dir_recursive_absolute(out)
	await wait(10)
	print("BAL backend: %s" % app.session.core.backend)
	for id in ["balance-of-power", "ultimate", "dominion-war", "ages", "galaxies"]:
		app.goto("new")
		await wait(6)
		var nc := app.current_screen()
		nc.vm.pick_preset(id)
		nc.seed_value = 424242
		nc.refresh()
		await wait(10)
		print("BAL %s: difficulty %s, start systems %d, planets %s, star types %s" % [id, nc.vm.difficulty, nc.vm.start_systems, nc.vm.planets_id, str(nc.vm.star_types)])
		await _shot("%s/new-%s.png" % [out, id])
		_reveal(nc, "StarTypes")
		await wait(6)
		await _shot("%s/new-%s-rules.png" % [out, id])
	# Expert selected on the Ultimate preset.
	app.goto("new")
	await wait(6)
	var ne := app.current_screen()
	ne.vm.pick_preset("ultimate")
	ne.vm.difficulty = "expert"
	ne.refresh()
	await wait(10)
	_reveal(ne, "StarTypes")
	await wait(6)
	await _shot("%s/new-ultimate-expert.png" % out)
	# A Balance of Power game: the capital's build list under the scaled costs.
	app.goto("new")
	await wait(6)
	var nb := app.current_screen()
	nb.vm.pick_preset("balance-of-power")
	nb.seed_value = 424242
	nb.refresh()
	await wait(8)
	nb.start()
	await wait(40)
	var cap := 0
	for c in app.view.get("colonies", []):
		cap = int(c["systemId"])
		if bool(c.get("isCapital", false)):
			break
	app.select_system(cap)
	app.goto("system")
	await wait(8)
	var sc: Node = app.current_screen()
	sc.tab = "structures"
	sc.refresh()
	await wait(6)
	print("BAL BoP build list: %s" % str(sc.vm.detail.get("choices", []).filter(func(x): return x["blocker"] == "").slice(0, 14).map(func(x): return "%s %d" % [x["name"], int(x["cost"])])))
	await _shot("%s/buildlist-balance-of-power.png" % out)
	quit(0)
