extends SceneTree
## F2 combat probe (FINISH-PLAN F2): plays a drill battle through the game's own screens (title, drill, End turn, Take command,
## Engage each round) in real time on a GPU window and measures what the F2 gate asks for:
##   - frame times: the worst frame in the 3 s after the battle screen opens (the first-open hitch), and per round the worst
##     frame and the count of frames over 33 ms (the per-round hitch);
##   - beams: per round, the beam jobs in the timeline and the frames on which a beam ribbon was actually drawn;
##   - camera: per round, the frames on which no ship was inside the camera's view (empty space);
##   - chrome: whether the app's top bar and rail were hidden while the round played and shown between rounds.
##   godot --path remaster/godot --resolution 1600x900 --audio-driver Dummy -s res://scenes/battle/tools/f2_probe.gd -- \
##       <out_dir> [tactical|starbase|fleetN] [--rounds=6] [--shots]
## --shots saves one 960x540 frame per round while a beam is drawn (<out_dir>/beam_rNN.jpg).
## Prints F2 lines and writes <out_dir>/f2-probe.txt.
var app: Node
var out := ""
var lines: PackedStringArray = []

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

func _log(s: String) -> void:
	print(s)
	lines.append(s)

func _initialize() -> void:
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred()

func _beam_drawn(vn: Node) -> bool:
	var fx = vn.get("fx")
	if fx == null or vn.timeline == null:
		return false
	for i in fx._active.keys():
		var j: Dictionary = vn.timeline.jobs[i]
		if j.kind != BattleTimeline.Job.BEAM or vn.time < j.start:
			continue
		var ns: Array = fx._active[i].nodes
		if ns.size() > 0 and is_instance_valid(ns[0]) and ns[0] is MeshInstance3D and ns[0].is_visible_in_tree():
			return true
	return false

func _ships_in_view(vn: Node) -> int:
	var cam: Camera3D = vn.get_viewport().get_camera_3d()
	if cam == null:
		return -1
	var n := 0
	for s in vn._ships.values():
		if is_instance_valid(s) and s.visible and cam.is_position_in_frustum(s.global_position):
			n += 1
	return n

func _run() -> void:
	var a := OS.get_cmdline_user_args()
	out = a[0]
	DirAccess.make_dir_recursive_absolute(out)
	var mode := "tactical"
	var max_rounds := 6
	var shots := false
	for x in a.slice(1):
		if x.begins_with("--rounds="):
			max_rounds = int(x.get_slice("=", 1))
		elif x == "--shots":
			shots = true
		elif not x.begins_with("--"):
			mode = x
	await wait(10)
	app.view = {}
	app.goto("title")
	# Let the title's idle battle warm-up run, as a player sitting on the title would.
	await wait(240)
	if mode == "starbase":
		app.start_drill("newBattleDrill", "Starbase drill", {"station": true})
	elif mode.begins_with("fleet"):
		app.start_drill("newBattleDrill", "Tactical drill", {"ships": int(mode.trim_prefix("fleet"))})
	else:
		app.start_drill("newBattleDrill", "Tactical drill")
	await wait(30)
	_perf()
	var t_open := Time.get_ticks_usec()
	_t_open = t_open
	RenderingServer.frame_pre_draw.connect(_pre_draw)
	RenderingServer.frame_post_draw.connect(_post_draw)
	var st: String = app.end_turn()
	print("F2 end_turn returned at %.1f ms" % ((Time.get_ticks_usec() - t_open) / 1000.0))
	_log("F2 end_turn -> %s screen %s" % [st, app.screen])
	var open_worst := 0.0
	var open_slow := 0
	var last := Time.get_ticks_usec()
	var open_first := (last - t_open) / 1000.0
	open_worst = open_first
	while Time.get_ticks_usec() - t_open < 3000000:
		await process_frame
		var now := Time.get_ticks_usec()
		var d := (now - last) / 1000.0
		last = now
		open_worst = maxf(open_worst, d)
		if d > 33.0:
			open_slow += 1
			_log("F2 SLOW open frame %d: %.1f ms %s" % [Engine.get_process_frames(), d, _perf()])
	_log("F2 OPEN end_turn_call=%.1f ms worst_frame_3s=%.1f ms frames_over_33ms=%d" % [open_first, open_worst, open_slow])
	var t_load := Time.get_ticks_usec()
	while BattleAssets.loading() and Time.get_ticks_usec() - t_load < 30000000:
		await process_frame
	_log("F2 HULLS loaded %.1f s after the 3 s window (still loading: %s)" % [(Time.get_ticks_usec() - t_load) / 1e6, BattleAssets.loading()])
	await wait(30)
	RenderingServer.frame_pre_draw.disconnect(_pre_draw)
	RenderingServer.frame_post_draw.disconnect(_post_draw)
	var take := _find(app, "TakeCommand")
	if take and take is BaseButton:
		take.emit_signal("pressed")
		app.close_modal()
	await wait(20)
	var cs: Node = app.current_screen()
	var rounds := 0
	var seen := 0
	var cur := {}
	var all_worst := 0.0
	var guard := 0
	last = Time.get_ticks_usec()
	while rounds <= max_rounds and guard < 60 * 400:
		guard += 1
		var vn: Node = cs.host.view_node if cs.get("host") != null else null
		var playing: bool = vn != null and vn.has_method("is_playing") and vn.is_playing()
		if not playing and not cs.get("_playing_layout") and not app.encounter.get("finished", false):
			if not cur.is_empty():
				_finish_round(cur)
				cur = {}
				_log("F2 BETWEEN chrome_visible=%s" % _chrome_visible())
			if rounds >= max_rounds:
				break
			cs.order("Engage")
			await process_frame
			last = Time.get_ticks_usec()
			var t0 := Time.get_ticks_usec()
			cs._round()
			rounds += 1
			cur = {"n": rounds, "press_ms": (Time.get_ticks_usec() - t0) / 1000.0, "worst": 0.0, "slow": 0, "frames": 0,
				"beam_frames": 0, "empty_frames": 0, "play_frames": 0, "chrome_shown": 0, "beam_jobs": -1, "shot": false, "worst_at": 0}
		if app.encounter.get("finished", false) and not playing:
			break
		await process_frame
		var now := Time.get_ticks_usec()
		var d := (now - last) / 1000.0
		last = now
		if cur.is_empty():
			continue
		cur.frames += 1
		if d > cur.worst:
			cur.worst = d
			cur.worst_at = cur.frames
		all_worst = maxf(all_worst, d)
		if d > 33.0:
			cur.slow += 1
			_log("F2 SLOW round %d frame %d (%d into the round): %.1f ms %s" % [cur.n, Engine.get_process_frames(), cur.frames, d, _perf()])
		vn = cs.host.view_node if cs.get("host") != null else null
		if vn != null and vn.is_playing() and vn.timeline != null:
			if int(cur.beam_jobs) < 0:
				var b := 0
				for j in vn.timeline.jobs:
					if j.kind == BattleTimeline.Job.BEAM:
						b += 1
				cur.beam_jobs = b
			cur.play_frames += 1
			if _chrome_visible():
				cur.chrome_shown += 1
			if _beam_drawn(vn):
				cur.beam_frames += 1
				if shots and not cur.shot and int(cur.beam_frames) == 8:
					cur.shot = true
					var img := root.get_viewport().get_texture().get_image()
					img.resize(960, 540)
					img.save_jpg(out + "/beam_r%02d.jpg" % int(cur.n), 0.8)
			if _ships_in_view(vn) == 0:
				cur.empty_frames += 1
	if not cur.is_empty():
		_finish_round(cur)
	_log("F2 DONE rounds=%d worst_frame_all=%.1f ms finished=%s" % [rounds, all_worst, app.encounter.get("finished", false)])
	var f := FileAccess.open(out + "/f2-probe.txt", FileAccess.WRITE)
	f.store_string("\n".join(lines) + "\n")
	f.close()
	quit(0)

var _pipes := {}
var _t_open := 0
var _t_pre := 0
## Open window: when each frame's processing (with deferred calls) ended and its draw finished, from the End turn call.
func _pre_draw() -> void:
	_t_pre = Time.get_ticks_usec()
	if _t_pre - _t_open < 600000:
		print("F2 frame %d process done at %.1f ms" % [Engine.get_process_frames(), (_t_pre - _t_open) / 1000.0])
func _post_draw() -> void:
	var t := Time.get_ticks_usec()
	if t - _t_open < 600000:
		print("F2 frame %d drawn at %.1f ms (draw %.1f ms)" % [Engine.get_process_frames(), (t - _t_open) / 1000.0, (t - _t_pre) / 1000.0])
## Process time of the last frame and the pipeline compilations since the last call (what a slow frame spent its time on).
func _perf() -> String:
	var out := "process=%.1f ms" % (Performance.get_monitor(Performance.TIME_PROCESS) * 1000.0)
	for k in ["canvas", "mesh", "surface", "draw", "specialization"]:
		var m: int = {"canvas": Performance.PIPELINE_COMPILATIONS_CANVAS, "mesh": Performance.PIPELINE_COMPILATIONS_MESH,
			"surface": Performance.PIPELINE_COMPILATIONS_SURFACE, "draw": Performance.PIPELINE_COMPILATIONS_DRAW,
			"specialization": Performance.PIPELINE_COMPILATIONS_SPECIALIZATION}[k]
		var v := int(Performance.get_monitor(m))
		if v != int(_pipes.get(k, 0)):
			out += " %s+%d" % [k, v - int(_pipes.get(k, 0))]
		_pipes[k] = v
	return out

func _chrome_visible() -> bool:
	var c: Control = app.get("_chrome")
	return c != null and c.is_visible_in_tree() and c.modulate.a > 0.05

func _finish_round(c: Dictionary) -> void:
	_log("F2 ROUND %d press=%.1f ms worst=%.1f ms (frame %d) over33=%d frames=%d play=%d beam_jobs=%d beam_frames=%d empty_view=%d chrome_in_round=%d" % [
		c.n, c.press_ms, c.worst, c.worst_at, c.slow, c.frames, c.play_frames, c.beam_jobs, c.beam_frames, c.empty_frames, c.chrome_shown])
