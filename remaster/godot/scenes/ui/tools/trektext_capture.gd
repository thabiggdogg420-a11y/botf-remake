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
	pass
