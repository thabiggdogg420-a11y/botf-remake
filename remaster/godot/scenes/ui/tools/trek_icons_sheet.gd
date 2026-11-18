extends SceneTree
## Trek icon pass proof sheet (18 Nov): every SVG icon of TrekIconSvg at 16, 18, 24 and 48 px in the LCARS tints on the panel colour,
## so legibility at status-bar size can be judged. Prints "TREKICON <name> ok|FAIL"; exit 1 if any icon fails to rasterise.
##   godot --path remaster/godot --audio-driver Dummy --resolution 1400x1000 -s res://scenes/ui/tools/trek_icons_sheet.gd -- <out.png>
##   web: prints "WEBSHOT trek-icons" and holds the frame.

var out := "user://trek-icons.png"

class Sheet extends Control:
	var names: Array = []
	func _draw() -> void:
		draw_rect(Rect2(Vector2.ZERO, size), Lcars.BG)
		var font := Lcars.font("ui", 500)
		var tints: Array = [Lcars.accent(), Lcars.GOOD, Lcars.WARN, Lcars.TEXT2]
		var col_w := 340.0
		var row_h := 58.0
		var per_col := int(floor((size.y - 20.0) / row_h))
		for i in names.size():
			var x0: float = 14.0 + floor(float(i) / per_col) * col_w
			var y0: float = 14.0 + (i % per_col) * row_h
			var n: String = names[i]
			draw_string(font, Vector2(x0, y0 + 12), n, HORIZONTAL_ALIGNMENT_LEFT, -1, 12, Lcars.TEXT3)
			var x: float = x0
			for px in [16, 18, 24]:
				UiIcon.draw_icon(self, n, Rect2(Vector2(x, y0 + 18 + (24.0 - px) / 2.0), Vector2(px, px)), tints[i % tints.size()])
				x += px + 10
			UiIcon.draw_icon(self, n, Rect2(Vector2(x, y0 + 4), Vector2(48, 48)), tints[i % tints.size()])
			x += 60
			draw_rect(Rect2(Vector2(x, y0 + 14), Vector2(110, 32)), Lcars.accent2())
			UiIcon.draw_icon(self, n, Rect2(Vector2(x + 8, y0 + 21), Vector2(18, 18)), Color(Lcars.ink(), 0.85))

func _initialize() -> void:
	var a := OS.get_cmdline_user_args()
	if a.size() > 0:
		out = a[0]
	var s := Sheet.new()
	s.names = TrekIconSvg.SVG.keys()
	s.set_anchors_preset(Control.PRESET_FULL_RECT)
	root.add_child(s)
	var bad := 0
	for n in s.names:
		var ok := UiIcon.svg_texture(n, 24) != null
		print("TREKICON ", n, " ", "ok" if ok else "FAIL")
		bad += 0 if ok else 1
	_shot.call_deferred(bad)

func _shot(bad: int) -> void:
	for i in 10:
		await process_frame
	if OS.has_feature("web"):
		print("WEBSHOT trek-icons")
	else:
		root.get_viewport().get_texture().get_image().save_png(out)
		print("saved ", out)
	quit(1 if bad > 0 else 0)
