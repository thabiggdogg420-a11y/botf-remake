extends SceneTree
## Proof frames for the trektext lane (a real GPU window; always --audio-driver Dummy).
##   godot --path remaster/godot --resolution 1920x1080 --audio-driver Dummy -s res://scenes/ui/tools/trektext_capture.gd -- <out_dir> <mode> [args]
## Modes:
##   marks              ours.png: each major insignia at 84 / 30 / 14 px, one row each, for the compare sheet
##   screens <faction>  <tag>-<screen>.png: galaxy, galaxy-whole, fleets, events (after 10 autoplay turns), chronicle, diplomacy, title, new

var out_dir := ""
var mode := ""
var args2: Array = []

func _initialize() -> void:
	var a := OS.get_cmdline_user_args()
	out_dir = a[0]
	mode = a[1]
	args2 = a.slice(2)
	DirAccess.make_dir_recursive_absolute(out_dir)
	_run.call_deferred()

func _wait(n: int = 6) -> void:
	for i in n:
		await process_frame

func _save(name_: String) -> void:
	await _wait(6)
	var img := root.get_viewport().get_texture().get_image()
	img.save_png(out_dir.path_join(name_))
	print("saved ", out_dir.path_join(name_), " ", img.get_size())

func _run() -> void:
	match mode:
		"marks":
			await _marks()
		"screens":
			await _screens(int(args2[0]) if args2.size() > 0 else 1)
	quit(0)

const MARKS := [["federation", 1, "auto"], ["federation", 1, "seal"], ["klingon", 2, "auto"], ["romulan", 3, "auto"], ["cardassian", 4, "auto"], ["ferengi", 5, "auto"], ["dominion", -1, "auto"], ["borg", -2, "auto"]]

class MarkSheet extends Control:
	func _draw() -> void:
		draw_rect(Rect2(Vector2.ZERO, size), Lcars.BG)
		for i in MARKS.size():
			var m: Array = MARKS[i]
			var y := 8.0 + i * 100.0
			var tint: Color = Lcars.owner_color(int(m[1])) if int(m[1]) > 0 else (Color("#9a6fd0") if int(m[1]) == -1 else Color("#c8403a"))
			Insignia.draw(self, m[0], Rect2(Vector2(8, y), Vector2(84, 84)), tint, m[2])
			Insignia.draw(self, m[0], Rect2(Vector2(104, y + 27), Vector2(30, 30)), tint, m[2])
			Insignia.draw(self, m[0], Rect2(Vector2(146, y + 35), Vector2(14, 14)), tint, m[2])

func _marks() -> void:
	var c := MarkSheet.new()
	c.set_anchors_preset(Control.PRESET_FULL_RECT)
	root.add_child(c)
	await _save("ours.png")

func _screens(faction: int) -> void:
	var tag: String = ["x", "fed", "kli", "rom", "car", "fer"][faction]
	OS.set_environment("BOTF_TITLE_EMPIRE", str(faction))
	var app: App = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	await _wait(6)
	app.start_campaign({"faction": faction, "seed": 424242, "era": 3, "ruleset": "reference", "width": 22, "height": 16, "systems": 48, "preset": "remastered", "events": true, "aiAggression": true})
	var best: Variant = null
	var best_n := -1
	if app.core.is_native():
		for n in 10:
			app.end_turn()
			app.close_all_modals()
			var b: Variant = app.briefing
			if b is Dictionary and b.get("events", []).size() >= best_n:
				best = b
				best_n = b.get("events", []).size()
	app.close_all_modals()
	if best is Dictionary:
		app.briefing = best
	print(tag, " briefing events: ", best_n)
	var shots: Array = args2.slice(1) if args2.size() > 1 else ["galaxy", "galaxy-whole", "fleets", "report", "events", "chronicle", "diplomacy", "title", "new"]
	for sh in shots:
		match sh:
			"galaxy":
				app.goto("galaxy")
				var cols: Array = app.view.get("fleets", [])
				if cols.size() > 0:
					app.select_fleet(int(cols[0]["id"]))
			"galaxy-whole":
				app.goto("galaxy")
				await _wait(4)
				var gm: GalaxyMap = app.current_screen().map
				var fl: Array = app.view.get("fleets", [])
				gm.focus_cell(int(fl[0]["x"]) if fl.size() > 0 else 11, int(fl[0]["y"]) if fl.size() > 0 else 8, 0.7, false)
			"report":
				app.goto("galaxy")
				app.open_overlay("briefing")
			"chronicle":
				app.goto("galaxy")
				Overlays.briefing(app, "diplomacy")
			"events":
				app.goto("galaxy")
				app.open_overlay("events")
			"title":
				var keep_t: Dictionary = app.view
				app.view = {}
				app.goto("title")
				await _wait(14)
				await _save("%s-title.png" % tag)
				app.view = keep_t
				continue
			"new":
				var keep: Dictionary = app.view
				app.view = {}
				app.goto("new")
				await _wait(4)
				app.current_screen()._pick_faction(faction)
				await _wait(10)
				for t in app.get_node("Toasts").get_children():
					t.queue_free()
				await _save("%s-new.png" % tag)
				app.view = keep
				continue
			_:
				app.goto(sh)
		await _wait(10)
		for t in app.get_node("Toasts").get_children():
			t.queue_free()
		await _save("%s-%s.png" % [tag, sh])
		app.close_all_modals()
	app.queue_free()
