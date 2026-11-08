class_name LcarsElbow
extends Control
## The LCARS elbow: the rail column sweeping into the top bar, with the faction emblem.

var empire := 1

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE

func _draw() -> void:
	var a := Lcars.accent()
	var w := App.RAIL_W
	var h := App.TOP_H - 14
	draw_rect(Rect2(0, 0, w, h + 30), a)
	draw_rect(Rect2(w, 0, 40, h), a)
	draw_rect(Rect2(w, h, 30, 30), a)
	# inner corner cut
	var pts := PackedVector2Array()
	for k in range(0, 17):
		var ang := PI + PI / 2.0 * k / 16.0
		pts.append(Vector2(w + 30, h + 30) + Vector2(cos(ang), sin(ang)) * 30)
	pts.append(Vector2(w + 30, h + 30))
	draw_colored_polygon(pts, Lcars.BG)
	Lcars.draw_emblem(self, empire, Vector2(34, 34), 40, Lcars.ink())
	draw_string(Lcars.font("display", 600), Vector2(62, 44), "LCARS", HORIZONTAL_ALIGNMENT_LEFT, -1, 22, Lcars.ink())
