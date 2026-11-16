extends SceneTree
## Picture-led UI look test (16 Nov 2026): the three screens a new player uses most, with a word count of what is on screen.
##   godot --path remaster/godot --audio-driver Dummy --resolution 1920x1080 -s res://scenes/ui/tools/pictures_capture.gd -- <out_dir> [shot ...]
## Shots: colony (system screen, workforce + structures), build (structures with a queue of four), build-ships (starships tab),
## galaxy-fleet (a ship selected), galaxy-system (our colony's sector selected), galaxy-contact (not used when no contact is near).
## Prints "WORDS <shot>: <words> (<numbers> numbers)" counting visible Label/Button/RichTextLabel/LineEdit text under the screen:
## a word is a token with two or more letters, a number a token with a digit and fewer (1T, 240, +80). Tooltips are not counted.

var app: App
var out_dir := "res://docs/screens"

func _initialize() -> void:
	var args := Array(OS.get_cmdline_user_args())
	if args.size() > 0:
		out_dir = args[0]
	var shots: Array = args.slice(1) if args.size() > 1 else ["colony", "build", "build-ships", "galaxy-fleet", "galaxy-system"]
	DirAccess.make_dir_recursive_absolute(out_dir)
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred(shots)

func _wait(frames: int = 8) -> void:
	for n in frames:
		await process_frame

func _wait_ms(ms: int) -> void:
	var t := Time.get_ticks_msec()
	while Time.get_ticks_msec() - t < ms:
		await _wait(4)

func _campaign() -> void:
	if app.view.is_empty() or app.view.get("session", {}).get("active", false):
		app.start_campaign({"faction": 1, "seed": 424242, "era": 3, "ruleset": "reference", "width": 22, "height": 16, "systems": 48, "preset": "remastered", "events": true, "aiAggression": true})
		if app.core.is_native():
			for n in 12:
				app.end_turn()
				app.close_all_modals()
	app.close_all_modals()

func _press_choice(scr: Control, max_n: int) -> int:
	var n := 0
	for b in scr.find_children("Choice_*", "Button", true, false):
		if n >= max_n:
			break
		if not (b as Button).disabled:
			(b as Button).pressed.emit()
			n += 1
	return n

func _run(shots: Array) -> void:
	await _wait(6)
	_campaign()
	for s in shots:
		match s:
			"colony", "build", "build-ships":
				app.goto("system")
				await _wait(4)
				var scr := app.current_screen()
				scr.tab = "structures"
				scr.refresh()
				await _wait(3)
				if s == "build-ships":
					scr.tab = "ships"
					scr.refresh()
				elif s == "build":
					# four items in the queue: three structures, then a ship when the shipyard allows it. The queue governor would
					# rewrite a hand-made queue, so it is switched to Manual first.
					var c0: Dictionary = app.view.get("colonies", [])[0]
					app.issue({"type": "SetColonyGovernor", "systemId": int(c0["systemId"]), "governorWorkforce": "Balanced", "governorQueue": "Manual"}, [], "")
					app.end_turn()
					app.close_all_modals()
					await _wait(6)
					scr = app.current_screen()
					scr.refresh()
					await _wait(3)
					_press_choice(scr, 3)
					await _wait(4)
					scr = app.current_screen()
					scr.tab = "ships"
					scr.refresh()
					await _wait(3)
					_press_choice(scr, 1)
					await _wait(4)
					app.end_turn() # the queue shows committed items: play the turn so the four orders are in it
					app.close_all_modals()
					await _wait(6)
					app.goto("system")
					await _wait(4)
					scr = app.current_screen()
					scr.tab = "structures"
					scr.refresh()
				await _wait_ms(3500) # the planet globes are SubViewports that load their textures on first use
			"galaxy-fleet":
				app.goto("galaxy")
				await _wait(4)
				var cols: Array = app.view.get("fleets", [])
				if cols.size() > 0:
					app.select_fleet(int(cols[0]["id"]))
				await _wait_ms(1500)
			"galaxy-system":
				app.goto("galaxy")
				await _wait(4)
				var col: Dictionary = app.view.get("colonies", [])[0]
				app.select_system(int(col["systemId"]))
				await _wait_ms(1500)
		for t in app.get_node("Toasts").get_children():
			t.queue_free()
		await _wait(6)
		var wc := _count(app.current_screen())
		print("WORDS ", s, ": ", wc[0], " (", wc[1], " numbers)")
		var img := root.get_viewport().get_texture().get_image()
		img.save_png("%s/pic-%s.png" % [out_dir, s])
		print("saved ", out_dir, "/pic-", s, ".png")
	quit(0)

## [words, numbers] of the visible text under `n`.
func _count(n: Node) -> Array:
	var out := [0, 0]
	_walk(n, out, Rect2(Vector2.ZERO, Vector2(root.size)))
	return out

## `clip` is the part of the window not cut off by an enclosing scroll area: text scrolled out of sight is not on screen.
func _walk(n: Node, out: Array, clip: Rect2) -> void:
	var ci := n as CanvasItem
	if ci != null and not ci.visible:
		return
	var ctl := n as Control
	if ctl != null:
		if n is ScrollContainer:
			clip = clip.intersection(ctl.get_global_rect())
		if not clip.intersects(ctl.get_global_rect()):
			return
	var txt := ""
	if n is Label:
		txt = (n as Label).text
	elif n is Button:
		txt = (n as Button).text
	elif n is RichTextLabel:
		txt = (n as RichTextLabel).get_parsed_text()
	elif n is LineEdit:
		txt = (n as LineEdit).text if (n as LineEdit).text != "" else (n as LineEdit).placeholder_text
	if txt != "":
		for tok in txt.replace("\n", " ").split(" ", false):
			var letters := 0
			var has_digit := false
			for ch in tok:
				if (ch >= "a" and ch <= "z") or (ch >= "A" and ch <= "Z"):
					letters += 1
				elif ch >= "0" and ch <= "9":
					has_digit = true
			if letters >= 2:
				out[0] += 1
			elif has_digit:
				out[1] += 1
	for c in n.get_children():
		_walk(c, out, clip)
