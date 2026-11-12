extends SceneTree
## MOD-8 captures: the scenario picker, the Dominion War galaxy at turn 1, a Klingon civil war split, a Mirror Universe game and a flagship,
## at 1920x1080 on a real GPU window with the native core.
##   godot --path remaster/godot --resolution 1920x1080 --audio-driver Dummy -s res://tools/scenarios_capture.gd -- <out_dir>
## Writes <out_dir>/scenario-picker.png, scenario-picker-bottom.png, dominion-war-turn1.png, civil-war-split.png, civil-war-empire.png,
## mirror-galaxy.png, mirror-empire.png, flagship-before.png and flagship-after.png, and prints the facts behind each frame.
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
	print("SCN wrote ", path)

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
	var out := a[0] if a.size() > 0 else "user://scenarios"
	DirAccess.make_dir_recursive_absolute(out)
	await wait(10)
	print("SCN backend: %s" % app.session.core.backend)
	# 1. The picker on the new-game screen.
	app.goto("new")
	await wait(10)
	var sc: Node = app.current_screen()
	print("SCN scenarios offered: %s" % str(sc.vm.scenarios().map(func(s): return s["id"])))
	sc.vm.pick_scenario("dominion-war")
	sc.refresh()
	await wait(8)
	await _shot("%s/scenario-picker.png" % out)
	_reveal(sc, "Launch")
	await wait(4)
	await _shot("%s/scenario-picker-bottom.png" % out)
	# 2. Dominion War, turn 1 (the Federation's galaxy).
	var ok := app.start_campaign(sc.vm.options())
	print("SCN dominion-war started: %s turn %s year %s" % [ok, str(app.view.get("turn")), str(app.view.get("meta", {}).get("year"))])
	app.close_all_modals()
	await wait(10)
	app.goto("galaxy")
	await _shot("%s/dominion-war-turn1.png" % out)
	# 3. Klingon Civil War: play to the split (turn 4) and show the galaxy, then the empire screen.
	ok = app.start_campaign({"scenario": "klingon-civil-war", "seed": 2367})
	app.close_all_modals()
	var guard := 0
	while app.view.get("meta", {}).get("secessions", []).is_empty() and guard < 12:
		app.end_turn()
		app.close_all_modals()
		guard += 1
	print("SCN civil war: started %s, secessions %s after %d turns, turn %s" % [ok, str(app.view.get("meta", {}).get("secessions", [])), guard, str(app.view.get("turn"))])
	await wait(10)
	app.goto("galaxy")
	await _shot("%s/civil-war-split.png" % out)
	app.goto("empire")
	await _shot("%s/civil-war-empire.png" % out)
	# 4. Mirror Universe.
	ok = app.start_campaign({"scenario": "mirror-universe", "seed": 1})
	app.close_all_modals()
	await wait(10)
	print("SCN mirror: started %s names %s" % [ok, str(app.view.get("meta", {}).get("empireNames", {}))])
	app.goto("galaxy")
	await _shot("%s/mirror-galaxy.png" % out)
	app.goto("empire")
	await _shot("%s/mirror-empire.png" % out)
	# 5. A flagship: the Terran Empire's scenario-placed I.S.S. Enterprise, then a commission by the Cardassian Alliance on the fleets screen.
	var placed := {}
	for f in app.view.get("fleets", []):
		if int(f.get("flagship", 0)) > 0:
			placed = f
	app.select_fleet(int(placed.get("id", 0)))
	app.goto("fleets")
	await wait(8)
	print("SCN placed flagship: %s name %s hull %s/%s" % [str(placed.get("name")), str(placed.get("flagshipName")), str(placed.get("hull")), str(placed.get("maxHull"))])
	await _shot("%s/flagship-placed.png" % out)
	ok = app.start_campaign({"scenario": "mirror-universe", "seed": 1, "faction": 4})
	app.close_all_modals()
	await wait(10)
	var fl := {}
	for f in app.view.get("fleets", []):
		if int(f.get("attack", 0)) > 0 and int(f.get("flagship", 0)) == 0:
			fl = f
			break
	app.select_fleet(int(fl.get("id", 0)))
	app.goto("fleets")
	await wait(8)
	print("SCN cardassian fleet before: %s blocker=%s credits=%s" % [str(fl.get("name")), str(fl.get("flagshipBlocker")), str(app.view.get("empire", {}).get("credits"))])
	await _shot("%s/flagship-before.png" % out)
	var vm: Variant = app.current_screen().vm
	var done: Variant = vm.commission_flagship(fl)
	print("SCN commission: %s" % str(done))
	await wait(8)
	app.end_turn()
	app.close_all_modals()
	app.select_fleet(int(fl.get("id", 0)))
	app.goto("galaxy")
	app.goto("fleets")
	await wait(10)
	for f in app.view.get("fleets", []):
		if int(f.get("flagship", 0)) > 0:
			print("SCN flagship: %s hull %s/%s" % [str(f.get("flagshipName", f.get("name"))), str(f.get("hull")), str(f.get("maxHull"))])
	await _shot("%s/flagship-after.png" % out)
	quit(0)
