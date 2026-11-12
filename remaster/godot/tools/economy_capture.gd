extends SceneTree
## MOD-6 captures: the economy drill (Dominion War preset, every UDM economy flag) at 1920x1080 on a real GPU window with the native core:
## the capital's build list with the structure upgrades, a Protectorate HQ system, the intercept and raid orders on the fleets screen,
## crew experience, the turn after (the raid report) and the battle's ship systems panel with the crew rating.
##   godot --path remaster/godot --resolution 1920x1080 --audio-driver Dummy -s res://tools/economy_capture.gd -- <out_dir>
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
	print("ECO wrote ", path)

func _fleet(order: int) -> Dictionary:
	for f in app.view.get("fleets", []):
		if int(f.get("order", 0)) == order and (order != 0 or int(f.get("experience", 25)) != 25):
			return f
	return {}

func _run() -> void:
	var a := OS.get_cmdline_user_args()
	var out := a[0] if a.size() > 0 else "user://economy"
	DirAccess.make_dir_recursive_absolute(out)
	await wait(10)
	print("ECO backend: %s" % app.session.core.backend)
	app.start_drill("newEconomyDrill", "Economy drill")
	app.close_all_modals()
	await wait(10)
	var rules: Dictionary = app.view.get("meta", {}).get("rules", {})
	print("ECO rules: ", rules)
	# The capital: upgrades panel, queue, then the build list.
	var cap := 0
	var hq := 0
	for c in app.view.get("colonies", []):
		if bool(c.get("isCapital", false)) or cap == 0:
			cap = int(c["systemId"])
			if bool(c.get("isCapital", false)):
				break
	app.select_system(cap)
	app.goto("system")
	await wait(8)
	var sc: Node = app.current_screen()
	print("ECO capital queue: %s" % str(sc.vm.detail.get("queue", []).map(func(x): return x.get("name", "?"))))
	await _shot("%s/economy-capital.png" % out)
	sc.tab = "structures"
	sc.refresh()
	await wait(6)
	print("ECO choices: %s" % str(sc.vm.detail.get("choices", []).filter(func(x): return x["product"] == "Structure").map(func(x): return "%s [%s]" % [x["name"], x["blocker"]])))
	await _shot("%s/economy-buildlist.png" % out)
	# The Protectorate HQ world.
	for c in app.view.get("colonies", []):
		var sid := int(c["systemId"])
		if sid == cap:
			continue
		app.select_system(sid)
		app.goto("system")
		await wait(8)
		var sc2: Node = app.current_screen()
		if bool(sc2.vm.colony().get("economyRules", {}).get("subjugated", false)):
			hq = sid
			print("ECO protectorate world %d: %s" % [sid, str(sc2.vm.colony().get("economyRules", {}))])
			await _shot("%s/economy-protectorate.png" % out)
			break
	# Fleets: the interceptor, a veteran and the raiders.
	var ic := _fleet(1)
	app.select_fleet(int(ic.get("id", 0)))
	app.goto("fleets")
	await wait(8)
	print("ECO interceptor: order %s crew %s xp %s" % [str(ic.get("orderName")), str(ic.get("crewRating")), str(ic.get("experience"))])
	await _shot("%s/economy-intercept.png" % out)
	var rd := _fleet(2)
	app.select_fleet(int(rd.get("id", 0)))
	app.goto("fleets")
	await wait(8)
	print("ECO raider: order %s target %s crew %s xp %s" % [str(rd.get("orderName")), str(rd.get("raidSystemId")), str(rd.get("crewRating")), str(rd.get("experience"))])
	await _shot("%s/economy-raid.png" % out)
	# The turn: raid plunder and the intercept, which opens a battle.
	var credits_before := int(app.view.get("empire", {}).get("credits", 0))
	var res := app.end_turn()
	print("ECO end turn: %s" % res)
	await wait(12)
	if res == "battle":
		await _shot("%s/economy-battle.png" % out)
		var sc3: Node = app.current_screen()
		print("ECO combat screen: %s" % sc3.name)
	else:
		app.close_all_modals()
		app.goto("galaxy")
		await _shot("%s/economy-turn.png" % out)
	quit(0)
