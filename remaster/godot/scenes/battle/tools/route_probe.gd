extends SceneTree
## Route probe (M4 combat 2): plays the game's own route (drill, End turn, Take command, Engage) through the app screens
## and prints what each round carries: event counts, jobs, ship keys. Optional mode: starbase | tactical | classic.
##   godot --headless --path remaster/godot --audio-driver Dummy -s res://scenes/battle/tools/route_probe.gd -- starbase 6
var app: Node

func wait(k := 2) -> void:
	for i in k:
		await process_frame

func _find(node: Node, needle: String) -> Control:
	if node is Control and node.name == needle:
		return node as Control
	for c in node.get_children():
		var r := _find(c, needle)
		if r != null:
			return r
	return null

func _initialize() -> void:
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred()

func _run() -> void:
	var a := OS.get_cmdline_user_args()
	var mode: String = a[0] if a.size() > 0 else "starbase"
	var n_rounds := int(a[1]) if a.size() > 1 else 5
	await wait(10)
	app.view = {}
	app.goto("title")
	await wait(6)
	if mode == "starbase":
		app.start_drill("newBattleDrill", "Starbase drill", {"station": true})
	else:
		app.start_drill("newBattleDrill", "Tactical drill")
	await wait(4)
	print("ROUTE end_turn -> ", app.end_turn(), " screen ", app.screen)
	await wait(30)
	var take := _find(app, "TakeCommand")
	if take and take is BaseButton:
		take.emit_signal("pressed")
		app.close_modal()
	await wait(10)
	var cs: Node = app.current_screen()
	var e: Dictionary = app.encounter
	print("ROUTE encounter keys ", e.keys(), " rules ", e.get("rules"), " ships ", e.get("ships", []).size())
	print("ROUTE ship[0] keys ", e.get("ships", [{}])[0].keys())
	for r in n_rounds:
		cs.order("Engage")
		await process_frame
		cs._round()
		await wait(8)
		var rs: Array = app.encounter.get("rounds", [])
		var last: Dictionary = rs.back() if rs.size() > 0 else {}
		var kinds := {}
		for ev in last.get("events", []):
			kinds[str(ev.get("kind", "?"))] = kinds.get(str(ev.get("kind", "?")), 0) + 1
		print("ROUTE round ", r + 1, " keys ", last.keys(), " events ", kinds)
		if r == 0:
			print("ROUTE round sample ", JSON.stringify(last).substr(0, 1800))
		var vn: Node = cs.host.view_node if cs.get("host") != null else null
		if r >= 2:
			var fl := []
			for ev in last.get("events", []):
				if ev.get("kind") == "Fire":
					fl.append("%s/%s t%s hit=%s dmg=%s/%s" % [ev.get("weapon"), ev.get("family"), ev.get("tick"), ev.get("hit"), ev.get("shieldDamage"), ev.get("hullDamage")])
			print("ROUTE   fires ", fl)
		if vn != null and vn.get("timeline") != null:
			var tl = vn.timeline
			var counts := {}
			for j in tl.jobs:
				counts[BattleTimeline.JOB_NAMES[j.kind]] = counts.get(BattleTimeline.JOB_NAMES[j.kind], 0) + 1
			print("ROUTE   jobs ", counts, " end ", tl.end_seconds)
		if vn != null and vn.has_method("skip_round"):
			vn.skip_round()
		await wait(5)
		if app.encounter.get("finished", false):
			print("ROUTE finished")
			break
	quit(0)
