extends SceneTree
## Route clip (M4 combat 2; from O'Brien's v6 smoke): plays a drill battle through the game's own screens in real time for N seconds of
## game time (run with --write-movie --fixed-fps 30), as the packaged exe does. Prints one ROUTE line per round with its jobs.
## args after --: <out_dir> <tactical|starbase|classic|fleetN> <seconds> [start_round_skip]
## args after --: <out_dir> <tactical|starbase> <seconds>
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
	var out: String = a[0]
	var mode: String = a[1]
	var secs := float(a[2])
	print("BATTLE args ", a, " secs ", secs)
	await wait(10)
	app.view = {}
	app.goto("title")
	await wait(6)
	if mode == "starbase":
		app.start_drill("newBattleDrill", "Starbase drill", {"station": true})
	elif mode.begins_with("fleet"):
		# fleet24: an N v N tactical drill (M4 flight stage 2 gate, the fleet battle).
		app.start_drill("newBattleDrill", "Tactical drill", {"ships": int(mode.trim_prefix("fleet"))})
	elif mode == "classic":
		app.start_drill("newBattleDrill", "Tactical drill", {"rules": 2})
	else:
		app.start_drill("newBattleDrill", "Tactical drill")
	await wait(4)
	var st: String = app.end_turn()
	print("BATTLE end_turn -> ", st, " screen ", app.screen)
	await wait(30)
	var take := _find(app, "TakeCommand")
	if take:
		take.emit_signal("pressed") if take is BaseButton else null
		app.close_modal()
	await wait(10)
	var cs: Node = app.current_screen()
	var frames := 0
	var target := int(secs * 30.0)
	var rounds := 0
	var started := false
	var seen := 0
	while frames < target:
		var vn: Node = cs.host.view_node if cs.get("host") != null else null
		var playing: bool = vn != null and vn.has_method("is_playing") and vn.is_playing()
		# The player's turn: nothing playing and the plan layout is back (a chained round is the game's own, not ours).
		if not playing and not cs.get("_playing_layout") and not app.encounter.get("finished", false) and (frames % 15 == 0):
			cs.order("Engage")
			await process_frame
			cs._round()
			rounds += 1
			print("BATTLE Engage pressed at frame ", frames)
		var n_rounds: int = app.encounter.get("rounds", []).size()
		if n_rounds != seen and vn != null and vn.get("timeline") != null:
			await wait(3)
			seen = n_rounds
			var counts := {}
			for j in vn.timeline.jobs:
				counts[BattleTimeline.JOB_NAMES[j.kind]] = counts.get(BattleTimeline.JOB_NAMES[j.kind], 0) + 1
			print("BATTLE round ", seen, " at frame ", frames, " jobs ", counts, " end ", vn.timeline.end_seconds, " idle ", vn.last_round_idle)
		await process_frame
		frames += 1
	print("BATTLE done frames=", frames, " rounds=", rounds, " finished=", app.encounter.get("finished", false))
	var f := FileAccess.open(out + "/battle-done.txt", FileAccess.WRITE)
	f.store_string("done %d %d\n" % [frames, rounds])
	f.close()
	quit(0)
